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
        QCOMPARE(AuthManager::hashPassword(QStringLiteral("admin123")),
                 QStringLiteral("240be518fabd2724ddb6f04eeb1da5967448d7e831c08c8fa822809f74c720a9"));
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
