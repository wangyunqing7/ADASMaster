#pragma once

// =============================================================================
// AuthManager.h
// 功能：登录认证管理器。加载 config/users.json，校验用户名+密码(SHA256)，
//       持有当前登录用户与角色，提供登录/注销，并把登录/注销事件交给
//       AlarmManager 写入审计日志。系统唯一的认证入口。
// 输入：loadUsers(configDir) 读取 configDir/users.json；login(user,pass) 校验。
// 输出：isLoggedIn/currentUser/currentDisplayName/currentRole/loginError 供 QML 绑定。
// =============================================================================

#include <QObject>
#include <QString>
#include <QList>
#include "UserAccount.h"

class AlarmManager;

class AuthManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isLoggedIn READ isLoggedIn NOTIFY loginStateChanged)
    Q_PROPERTY(QString currentUser READ currentUser NOTIFY loginStateChanged)
    Q_PROPERTY(QString currentDisplayName READ currentDisplayName NOTIFY loginStateChanged)
    Q_PROPERTY(int currentRole READ currentRole NOTIFY loginStateChanged)
    Q_PROPERTY(QString loginError READ loginError NOTIFY loginErrorChanged)

public:
    explicit AuthManager(QObject *parent = nullptr);

    void setAlarmManager(AlarmManager *alarm) { m_alarm = alarm; }
    bool loadUsers(const QString &configDir);  // 读取 configDir/users.json

    bool isLoggedIn() const { return m_loggedIn; }
    QString currentUser() const { return m_currentUser; }
    QString currentDisplayName() const { return m_currentDisplayName; }
    int currentRole() const { return m_currentRole; }        // 0..3
    QString loginError() const { return m_loginError; }

    Q_INVOKABLE bool login(const QString &username, const QString &password);
    Q_INVOKABLE void logout();

    // ---- 密码哈希（安全加固）----
    // 旧式无盐 SHA256（仅向后兼容/迁移判定，新密码不应使用）
    static QString hashPassword(const QString &password);  // SHA256 hex
    // PBKDF2-HMAC-SHA256 加盐哈希，格式：pbkdf2$iterations$saltHex$hashHex
    static QString hashPasswordSalted(const QString &password, const QString &saltHex,
                                      int iterations = 10000);
    // 统一验证：自动识别旧 SHA256 与新 PBKDF2 格式，常量时间比较
    static bool verifyPassword(const QString &password, const QString &storedHash);
    static int roleIndex(const QString &role);             // "admin"->3 等

signals:
    void loginStateChanged();
    void loginErrorChanged();

private:
    QList<UserAccount> m_users;
    AlarmManager *m_alarm = nullptr;
    bool m_loggedIn = false;
    QString m_currentUser;
    QString m_currentDisplayName;
    int m_currentRole = 0;
    QString m_loginError;

    // 登录失败锁定（防暴力破解）
    int m_failedAttempts = 0;
    qint64 m_lockUntilMs = 0;          // 锁定截止时间戳；0 表示未锁定
    static constexpr int MaxFailedAttempts = 5;
};
