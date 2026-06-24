// =============================================================================
// AuthManager.cpp —— 见头文件注释
// =============================================================================

#include "AuthManager.h"
#include "../alarm/AlarmManager.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QPasswordDigestor>
#include <QDateTime>

namespace {
// 常量时间比较：无论内容是否相同，遍历耗时一致，防止时序侧信道泄漏哈希信息。
// 长度不同直接返回 false（长度本身不算敏感信息）。
bool constantTimeEquals(const QByteArray &a, const QByteArray &b)
{
    if (a.size() != b.size()) return false;
    char diff = 0;
    for (int i = 0; i < a.size(); ++i)
        diff |= a.at(i) ^ b.at(i);
    return diff == 0;
}
}

AuthManager::AuthManager(QObject *parent) : QObject(parent) {}

QString AuthManager::hashPassword(const QString &password)
{
    // 旧式无盐 SHA256（向后兼容，保留用于识别未迁移的旧哈希）
    const QByteArray hash = QCryptographicHash::hash(password.toUtf8(),
                                                     QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex());
}

QString AuthManager::hashPasswordSalted(const QString &password, const QString &saltHex,
                                        int iterations)
{
    // PBKDF2-HMAC-SHA256 加盐迭代哈希，抵抗彩虹表与暴力破解。
    // 存储格式：pbkdf2$iterations$saltHex$hashHex（便于 verifyPassword 解析）
    const QByteArray salt = QByteArray::fromHex(saltHex.toUtf8());
    const QByteArray derived = QPasswordDigestor::deriveKeyPbkdf2(
        QCryptographicHash::Sha256, password.toUtf8(), salt, iterations, 32);
    return QStringLiteral("pbkdf2$%1$%2$%3")
        .arg(iterations)
        .arg(saltHex)
        .arg(QString::fromLatin1(derived.toHex()));
}

bool AuthManager::verifyPassword(const QString &password, const QString &storedHash)
{
    // 新格式：pbkdf2$iterations$saltHex$hashHex
    if (storedHash.startsWith(QStringLiteral("pbkdf2$"))) {
        const QStringList parts = storedHash.split(QLatin1Char('$'));
        if (parts.size() != 4) return false;
        bool ok = false;
        const int iterations = parts.at(1).toInt(&ok);
        if (!ok || iterations <= 0) return false;
        const QString recomputed = hashPasswordSalted(password, parts.at(2), iterations);
        return constantTimeEquals(recomputed.toUtf8(), storedHash.toUtf8());
    }
    // 旧格式：无盐 SHA256 hex（64 字符）——向后兼容
    const QString legacy = hashPassword(password);
    return constantTimeEquals(legacy.toUtf8(), storedHash.toUtf8());
}

int AuthManager::roleIndex(const QString &role)
{
    if (role == QLatin1String("admin"))    return 3;
    if (role == QLatin1String("engineer")) return 2;
    if (role == QLatin1String("operator")) return 1;
    return 0;  // visitor / unknown
}

bool AuthManager::loadUsers(const QString &configDir)
{
    QFile file(configDir + QStringLiteral("/users.json"));
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    const QJsonArray arr = doc.object().value(QStringLiteral("users")).toArray();
    m_users.clear();
    for (const QJsonValue &value : arr) {
        const QJsonObject obj = value.toObject();
        UserAccount user;
        user.username     = obj.value(QStringLiteral("username")).toString();
        user.displayName  = obj.value(QStringLiteral("displayName")).toString();
        user.role         = obj.value(QStringLiteral("role")).toString();
        user.passwordHash = obj.value(QStringLiteral("passwordHash")).toString();
        m_users.append(user);
    }
    return !m_users.isEmpty();
}

bool AuthManager::login(const QString &username, const QString &password)
{
    // 锁定检查：连续失败达上限后锁定一段时间，防暴力破解
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lockUntilMs > 0 && now < m_lockUntilMs) {
        const int remainSec = static_cast<int>((m_lockUntilMs - now) / 1000) + 1;
        m_loginError = QStringLiteral("账户已锁定，请 %1 秒后重试").arg(remainSec);
        emit loginErrorChanged();
        return false;
    }
    // 锁定期已过，重置计数
    if (m_lockUntilMs > 0 && now >= m_lockUntilMs) {
        m_lockUntilMs = 0;
        m_failedAttempts = 0;
    }

    for (const UserAccount &user : m_users) {
        if (user.username == username && verifyPassword(password, user.passwordHash)) {
            // 登录成功：重置失败计数
            m_failedAttempts = 0;
            m_lockUntilMs = 0;
            m_loggedIn = true;
            m_currentUser = user.username;
            m_currentDisplayName = user.displayName;
            m_currentRole = roleIndex(user.role);
            m_loginError.clear();
            emit loginStateChanged();
            emit loginErrorChanged();
            if (m_alarm)
                m_alarm->info(QStringLiteral("登录"),
                              QStringLiteral("%1（%2）登录成功").arg(user.displayName, user.role));
            return true;
        }
    }

    // 失败：累加计数，达上限则锁定；统一错误信息防账号枚举
    ++m_failedAttempts;
    if (m_failedAttempts >= MaxFailedAttempts) {
        m_lockUntilMs = now + 30000;   // 锁定 30 秒
        m_loginError = QStringLiteral("登录失败次数过多，账户已锁定 30 秒");
    } else {
        m_loginError = QStringLiteral("用户名或密码错误");
    }
    emit loginErrorChanged();
    if (m_alarm)
        m_alarm->warning(QStringLiteral("登录"),
                         QStringLiteral("用户 %1 登录失败（第 %2 次）").arg(username).arg(m_failedAttempts));
    return false;
}

void AuthManager::logout()
{
    if (!m_loggedIn)
        return;
    const QString who = m_currentDisplayName;
    m_loggedIn = false;
    m_currentUser.clear();
    m_currentDisplayName.clear();
    m_currentRole = 0;
    emit loginStateChanged();
    if (m_alarm)
        m_alarm->info(QStringLiteral("注销"), QStringLiteral("%1 已注销").arg(who));
}
