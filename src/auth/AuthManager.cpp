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

AuthManager::AuthManager(QObject *parent) : QObject(parent) {}

QString AuthManager::hashPassword(const QString &password)
{
    const QByteArray hash = QCryptographicHash::hash(password.toUtf8(),
                                                     QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex());
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
    const QString hash = hashPassword(password);
    for (const UserAccount &user : m_users) {
        if (user.username == username && user.passwordHash == hash) {
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

    // 失败：统一错误信息，不区分“用户不存在”与“密码错误”，防止账号枚举
    m_loginError = QStringLiteral("用户名或密码错误");
    emit loginErrorChanged();
    if (m_alarm)
        m_alarm->warning(QStringLiteral("登录"),
                         QStringLiteral("用户 %1 登录失败").arg(username));
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
