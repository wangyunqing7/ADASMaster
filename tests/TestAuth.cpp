// =============================================================================
// TestAuth.cpp
// 功能：验证 AuthManager 的账号加载、登录成功/失败、注销与 SHA256 哈希。
// 输入：项目根下 config/users.json（CTest 工作目录 = 项目根）。
// 输出：QtTest 断言。
// =============================================================================

#include <QtTest>
#include "src/auth/AuthManager.h"

class TestAuth : public QObject
{
    Q_OBJECT

private slots:
    void loadsUsersFromConfig()
    {
        AuthManager auth;
        QVERIFY(auth.loadUsers(QStringLiteral("config")));
        QVERIFY(!auth.isLoggedIn());
        QCOMPARE(auth.currentUser(), QString());
        QCOMPARE(auth.currentRole(), 0);
    }

    void loginSucceedsWithCorrectPassword()
    {
        AuthManager auth;
        QVERIFY(auth.loadUsers(QStringLiteral("config")));
        QVERIFY(auth.login(QStringLiteral("admin"), QStringLiteral("admin123")));
        QVERIFY(auth.isLoggedIn());
        QCOMPARE(auth.currentUser(), QStringLiteral("admin"));
        QCOMPARE(auth.currentDisplayName(), QStringLiteral("系统管理员"));
        QCOMPARE(auth.currentRole(), 3);  // admin
    }

    void loginFailsWithWrongPassword()
    {
        AuthManager auth;
        QVERIFY(auth.loadUsers(QStringLiteral("config")));
        QVERIFY(!auth.login(QStringLiteral("admin"), QStringLiteral("wrong")));
        QVERIFY(!auth.isLoggedIn());
        QCOMPARE(auth.loginError(), QStringLiteral("用户名或密码错误"));
    }

    void loginFailsWithUnknownUser()
    {
        AuthManager auth;
        QVERIFY(auth.loadUsers(QStringLiteral("config")));
        QVERIFY(!auth.login(QStringLiteral("nobody"), QStringLiteral("x")));
        // 不区分“用户不存在”与“密码错误”，统一提示，防账号枚举
        QCOMPARE(auth.loginError(), QStringLiteral("用户名或密码错误"));
    }

    void logoutClearsSession()
    {
        AuthManager auth;
        QVERIFY(auth.loadUsers(QStringLiteral("config")));
        QVERIFY(auth.login(QStringLiteral("lisi"), QStringLiteral("li123")));
        QCOMPARE(auth.currentRole(), 1);  // operator
        auth.logout();
        QVERIFY(!auth.isLoggedIn());
        QCOMPARE(auth.currentUser(), QString());
    }

    void hashPasswordIsSha256Hex()
    {
        // 旧式无盐 SHA256（向后兼容，用于迁移判定）
        QCOMPARE(AuthManager::hashPassword(QStringLiteral("admin123")),
                 QStringLiteral("240be518fabd2724ddb6f04eeb1da5967448d7e831c08c8fa822809f74c720a9"));
    }

    // ---- 安全加固：PBKDF2 加盐哈希 ----
    void pbkdf2HashHasSaltAndIterations()
    {
        const QString hashed = AuthManager::hashPasswordSalted(
            QStringLiteral("secret"), QStringLiteral("0123456789abcdef"), 10000);
        // 格式：pbkdf2$iterations$saltHex$hashHex
        QVERIFY(hashed.startsWith(QStringLiteral("pbkdf2$10000$")));
        const QStringList parts = hashed.split(QLatin1Char('$'));
        QCOMPARE(parts.size(), 4);
        QCOMPARE(parts.at(2), QStringLiteral("0123456789abcdef"));
    }

    void pbkdf2HashIsDeterministicForSameSalt()
    {
        const QString h1 = AuthManager::hashPasswordSalted(
            QStringLiteral("secret"), QStringLiteral("salt1"), 10000);
        const QString h2 = AuthManager::hashPasswordSalted(
            QStringLiteral("secret"), QStringLiteral("salt1"), 10000);
        QCOMPARE(h1, h2);   // 相同盐+密码+迭代 → 相同哈希
    }

    void pbkdf2HashDiffersBySalt()
    {
        const QString h1 = AuthManager::hashPasswordSalted(
            QStringLiteral("secret"), QStringLiteral("salt1"), 10000);
        const QString h2 = AuthManager::hashPasswordSalted(
            QStringLiteral("secret"), QStringLiteral("salt2"), 10000);
        QVERIFY(h1 != h2);  // 不同盐 → 不同哈希（防彩虹表）
    }

    void pbkdf2HashDiffersByIterations()
    {
        const QString h1 = AuthManager::hashPasswordSalted(
            QStringLiteral("secret"), QStringLiteral("salt"), 10000);
        const QString h2 = AuthManager::hashPasswordSalted(
            QStringLiteral("secret"), QStringLiteral("salt"), 20000);
        QVERIFY(h1 != h2);
    }

    // ---- 常量时间比较 + 验证函数 ----
    void verifyPasswordAcceptsOldSha256()
    {
        // 旧式 SHA256 哈希（无盐）应能验证通过（迁移兼容）
        QVERIFY(AuthManager::verifyPassword(
            QStringLiteral("admin123"),
            QStringLiteral("240be518fabd2724ddb6f04eeb1da5967448d7e831c08c8fa822809f74c720a9")));
    }

    void verifyPasswordAcceptsPbkdf2()
    {
        const QString hashed = AuthManager::hashPasswordSalted(
            QStringLiteral("mypass"), QStringLiteral("abcd"), 10000);
        QVERIFY(AuthManager::verifyPassword(QStringLiteral("mypass"), hashed));
        QVERIFY(!AuthManager::verifyPassword(QStringLiteral("wrongpass"), hashed));
    }

    void verifyPasswordIsConstantTime()
    {
        // 常量时间比较：相同前缀不同长度的哈希不应泄漏信息。
        // 这里验证 verifyPassword 对错误密码始终返回 false（行为正确性）
        QVERIFY(!AuthManager::verifyPassword(
            QStringLiteral("x"),
            QStringLiteral("0000000000000000000000000000000000000000000000000000000000000000")));
    }

    // ---- 登录失败锁定 ----
    void loginLocksAfterRepeatedFailures()
    {
        AuthManager auth;
        QVERIFY(auth.loadUsers(QStringLiteral("config")));
        // 连续失败 5 次后应被锁定
        for (int i = 0; i < 5; ++i)
            QVERIFY(!auth.login(QStringLiteral("admin"), QStringLiteral("wrong")));
        // 第 6 次：即使密码正确也应因锁定失败
        QVERIFY(!auth.login(QStringLiteral("admin"), QStringLiteral("admin123")));
        QVERIFY(auth.loginError().contains(QStringLiteral("锁定")));
    }

    void loginFailsWhenLocked()
    {
        AuthManager auth;
        QVERIFY(auth.loadUsers(QStringLiteral("config")));
        // 触发锁定
        for (int i = 0; i < 5; ++i)
            auth.login(QStringLiteral("admin"), QStringLiteral("wrong"));
        // 锁定状态下提示锁定
        QVERIFY(!auth.login(QStringLiteral("admin"), QStringLiteral("admin123")));
        QVERIFY(auth.loginError().contains(QStringLiteral("锁定")));
    }

    void roleIndexMapsKnownRoles()
    {
        QCOMPARE(AuthManager::roleIndex(QStringLiteral("admin")), 3);
        QCOMPARE(AuthManager::roleIndex(QStringLiteral("engineer")), 2);
        QCOMPARE(AuthManager::roleIndex(QStringLiteral("operator")), 1);
        QCOMPARE(AuthManager::roleIndex(QStringLiteral("visitor")), 0);
    }
};

QTEST_GUILESS_MAIN(TestAuth)
#include "TestAuth.moc"
