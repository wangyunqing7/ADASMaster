# 登录界面 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 ADASMaster 增加启动准入登录界面：开机先认证（用户名 + 密码），通过后才进主界面；主界面支持注销换人；登录/注销事件写审计日志。

**Architecture:** 新增 `AuthManager`（C++，唯一认证入口，加载 `config/users.json`、SHA256 校验、持有当前用户/角色、对接 `AlarmManager` 审计）。`AppController` 构造并注入它到 QML。`Main.qml` 据 `isLoggedIn` 在 `LoginView` 与主界面间切换；`TopStatusBar` 加注销按钮，`SettingsPage` 角色改只读。

**Tech Stack:** C++17 + Qt6 Core/Qml/Test（QCryptographicHash、QtTest）+ QML（QtQuick.Controls.Basic、QtQuick.Dialogs）+ CMake/Ninja + PowerShell gate。

## Global Constraints

- 工作区**无 `.git` 目录**，不执行 `git commit`；变更直接保留在工作区，每步以"验证"替代"提交"。
- **代码用英文，注释用中文**；每个新建 C++/QML 文件开头必须有块注释（功能/输入/输出）。
- 新增含 `Q_OBJECT` 的 C++ 头/源必须登记到 `CMakeLists.txt` 的 `adasmaster_core` 列表，否则 AUTOMOC 不生成元对象代码。
- 新增 `.qml` 必须登记到 `qt_add_qml_module` 的 `QML_FILES`；新增 `config/*.json` 必须登记到 `RESOURCES`。
- 构建：`cmake --build build`；测试：CTest 测试运行目录 = 项目根（`CMAKE_CURRENT_SOURCE_DIR}`），故测试中可用相对路径 `config/users.json`。
- 状态色严格 `0=绿 1=黄 2=橙 3=红`；QML 颜色/字号/字体复用 `Theme.*`，禁止硬编码。
- 角色映射：`visitor/operator/engineer/admin` ↔ `roleIndex 0/1/2/3`。

---

## File map

- **Create** `src/auth/UserAccount.h` — 账号数据结构
- **Create** `src/auth/AuthManager.h` / `.cpp` — 认证管理器（加载/登录/注销/审计）
- **Create** `config/users.json` — 预置账号（SHA256 哈希）
- **Create** `qml/pages/LoginView.qml` — 居中卡片登录界面
- **Create** `qml/icons/logout.svg` — 注销图标（Lucide log-out）
- **Create** `tests/TestAuth.cpp` — AuthManager 单元测试
- **Create** `tests/CheckLogin.ps1` — 登录集成静态 gate
- **Modify** `src/app/AppController.h` / `.cpp` — 注入 AuthManager
- **Modify** `qml/Main.qml` — 登录态切换
- **Modify** `qml/components/TopStatusBar.qml` — 注销按钮
- **Modify** `qml/pages/SettingsPage.qml` — 角色只读化
- **Modify** `CMakeLists.txt` — 登记全部新文件

---

### Task 1: AuthManager 认证核心（含账号数据与单元测试）

**Files:**
- Create: `src/auth/UserAccount.h`
- Create: `src/auth/AuthManager.h`
- Create: `src/auth/AuthManager.cpp`
- Create: `config/users.json`
- Create: `tests/TestAuth.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces: `AuthManager` —— `bool loadUsers(const QString &configDir)`、`Q_INVOKABLE bool login(const QString &user, const QString &pass)`、`Q_INVOKABLE void logout()`、`bool isLoggedIn()`、`QString currentUser()`、`QString currentDisplayName()`、`int currentRole()`、`QString loginError()`、`void setAlarmManager(AlarmManager*)`、`static QString hashPassword(const QString &)`、`static int roleIndex(const QString &role)`。信号：`loginStateChanged()`、`loginErrorChanged()`。

- [ ] **Step 1: 创建账号数据文件 `config/users.json`**

密码哈希为 SHA256（由 `printf '%s' <pwd> | sha256sum` 得到，与 `QCryptographicHash::Sha256(password.toUtf8())` 一致）。

```json
{
    "//说明": "登录账号配置。passwordHash 为密码的 SHA256 哈希，非明文。初始密码：admin/admin123、zhangsan/zhang123、lisi/li123。",
    "users": [
        { "username": "admin",    "displayName": "系统管理员", "role": "admin",    "passwordHash": "240be518fabd2724ddb6f04eeb1da5967448d7e831c08c8fa822809f74c720a9" },
        { "username": "zhangsan", "displayName": "张工",       "role": "engineer", "passwordHash": "81358f158c88145a6e30b9d68abdfb7e2cf92fa2e2fc8b70c3f0ed79035cc7a0" },
        { "username": "lisi",     "displayName": "李工",       "role": "operator", "passwordHash": "876d3762ec42ddb14b1c6b1338059129683a6a2b02f3014ab77bed35298405e1" }
    ]
}
```

- [ ] **Step 2: 创建 `src/auth/UserAccount.h`**

```cpp
#pragma once

// =============================================================================
// UserAccount.h
// 功能：登录账号的强类型表示（用户名、显示名、角色、密码哈希）。
//       供 AuthManager 从 users.json 加载后持有。
// =============================================================================

#include <QString>

struct UserAccount
{
    QString username;
    QString displayName;
    QString role;          // "visitor"/"operator"/"engineer"/"admin"
    QString passwordHash;  // SHA256 十六进制
};
```

- [ ] **Step 3: 写失败测试 `tests/TestAuth.cpp`**

```cpp
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
```

- [ ] **Step 4: 在 `CMakeLists.txt` 登记新文件**

在 `adasmaster_core` 静态库源码列表（紧跟 `src/device/DeviceManager.cpp` 之后，按目录分组）加入：

```cmake
    src/auth/UserAccount.h
    src/auth/AuthManager.h
    src/auth/AuthManager.cpp
```

在 `qt_add_qml_module` 的 `RESOURCES` 列表加入：

```cmake
        config/users.json
```

在测试列表（紧跟 `add_adasmaster_test(test_device_telemetry ...)` 之后）加入：

```cmake
add_adasmaster_test(test_auth tests/TestAuth.cpp)
```

- [ ] **Step 5: 构建测试目标，验证 RED**

```powershell
cmake --build build --target test_auth
```

Expected: 编译失败 —— `AuthManager` 未定义（`src/auth/AuthManager.h` 找不到）。

- [ ] **Step 6: 创建 `src/auth/AuthManager.h`**

```cpp
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

    static QString hashPassword(const QString &password);  // SHA256 hex
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
};
```

- [ ] **Step 7: 创建 `src/auth/AuthManager.cpp`**

```cpp
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
```

- [ ] **Step 8: 构建并运行测试，验证 GREEN**

```powershell
cmake --build build --target test_auth
$env:PATH="C:\Qt\6.11.1\mingw_64\bin;$PWD\build;$env:PATH"
ctest --test-dir build -R test_auth --output-on-failure
```

Expected: `test_auth` 全部用例通过。

---

### Task 2: AuthManager 接入 AppController

**Files:**
- Modify: `src/app/AppController.h`
- Modify: `src/app/AppController.cpp`

**Interfaces:**
- Consumes: Task 1 的 `AuthManager`。
- Produces: `AppController::authManager()`（`AuthManager*`），QML 通过 `appController.authManager` 访问。`AppController` 构造时加载 `config/users.json` 并把 `AlarmManager` 注入 `AuthManager`（使登录/注销能写审计）。

> 说明：本任务为集成接线，无独立新增测试；以现有 `test_app_controller` 回归通过 + 手动确认 `authManager()` 非空 作为验证。

- [ ] **Step 1: 修改 `src/app/AppController.h`**

在 `#include` 区加入（按字母序，`alarm` 之前）：

```cpp
#include "../auth/AuthManager.h"
```

在 `Q_PROPERTY` 区（`configManager` 一行下方）加入：

```cpp
    Q_PROPERTY(AuthManager* authManager READ authManager CONSTANT)
```

在 `public` 的 getter 区（`configManager()` 下方）加入：

```cpp
    AuthManager *authManager() const { return m_auth; }
```

在 `private` 成员区（`m_config` 上方）加入：

```cpp
    AuthManager *m_auth = nullptr;
```

- [ ] **Step 2: 修改 `src/app/AppController.cpp` 构造函数**

在 `m_alarm = new AlarmManager(this);` 之后加入：

```cpp
    m_auth = new AuthManager(this);
```

在 `if (m_config->ready()) { ... }` 块内（`m_scenario->setScenarios(...)` 之后）加入：

```cpp
        m_auth->loadUsers(m_config->dataRoot());
```

在 `m_alarm->setDatabaseManager(m_database);`（`if (m_database->ready())` 内）之后加入：

```cpp
    m_auth->setAlarmManager(m_alarm);
```

- [ ] **Step 3: 构建 + 回归测试**

```powershell
cmake --build build
$env:PATH="C:\Qt\6.11.1\mingw_64\bin;$PWD\build;$env:PATH"
ctest --test-dir build -R "test_auth|test_app_controller" --output-on-failure
```

Expected: 两个测试均通过（`test_app_controller` 回归不受影响，证明接线未破坏）。

- [ ] **Step 4: 手动确认注入**

`grep -n "authManager" src/app/AppController.h` 应看到 Q_PROPERTY 与 getter 各一处。

---

### Task 3: LoginView 登录界面 + Main.qml 登录态切换 + 集成 gate

**Files:**
- Create: `tests/CheckLogin.ps1`
- Create: `qml/icons/logout.svg`
- Create: `qml/pages/LoginView.qml`
- Modify: `qml/Main.qml`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `appController.authManager`（`login`/`logout`/`isLoggedIn`/`loginError`/`currentUser`/`currentDisplayName`/`currentRole`）。
- Produces: `LoginView` 组件；`Main.qml` 据登录态显示 `LoginView` 或主界面。

- [ ] **Step 1: 写失败 gate `tests/CheckLogin.ps1`**

```powershell
$ErrorActionPreference = 'Stop'

$workspace = Split-Path -Parent $PSScriptRoot
$mainPath = Join-Path $workspace 'qml\Main.qml'
$loginPath = Join-Path $workspace 'qml\pages\LoginView.qml'
$cmakePath = Join-Path $workspace 'CMakeLists.txt'

$main = Get-Content -Raw -Encoding UTF8 $mainPath
$cmake = Get-Content -Raw -Encoding UTF8 $cmakePath

if (-not (Test-Path -LiteralPath $loginPath -PathType Leaf)) {
    throw 'LoginView.qml does not exist.'
}

if ($main -notmatch 'LoginView\s*\{') {
    throw 'Main.qml does not place LoginView.'
}
if ($main -notmatch 'authManager\.isLoggedIn') {
    throw 'Main.qml must switch on authManager.isLoggedIn.'
}

$login = Get-Content -Raw -Encoding UTF8 $loginPath
$requiredBindings = @('authManager.login','authManager.loginError','echoMode: TextInput.Password',
                      'TextField','Button')
foreach ($binding in $requiredBindings) {
    if ($login -notmatch [regex]::Escape($binding)) {
        throw "LoginView.qml is missing '$binding'."
    }
}

if ($cmake -notmatch 'qml/pages/LoginView\.qml') {
    throw 'LoginView.qml is not registered in qt_add_qml_module.'
}
if ($cmake -notmatch 'qml/icons/logout\.svg') {
    throw 'logout.svg is not registered in RESOURCES.'
}

Write-Host 'PASS: login screen is integrated.'
```

- [ ] **Step 2: 在 `CMakeLists.txt` 登记 gate 与新资源**

在 `qt_add_qml_module` 的 `QML_FILES` 加入：

```cmake
        qml/pages/LoginView.qml
```

在 `RESOURCES` 加入：

```cmake
        qml/icons/logout.svg
```

在现有 `add_test(... status_monitor_gate ...)` 之后加入：

```cmake
add_test(NAME login_gate
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/CheckLogin.ps1)
set_tests_properties(login_gate PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
```

- [ ] **Step 3: 跑 gate 验证 RED**

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests\CheckLogin.ps1
```

Expected: FAIL with `LoginView.qml does not exist.`

- [ ] **Step 4: 创建 `qml/icons/logout.svg`**（Lucide log-out，与现有图标同规格 24×24/描边2/白）

```svg
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4"/><polyline points="16 17 21 12 16 7"/><line x1="21" x2="9" y1="12" y2="12"/></svg>
```

- [ ] **Step 5: 创建 `qml/pages/LoginView.qml`**

```qml
// =============================================================================
// LoginView.qml
// 功能：启动准入登录界面。居中卡片：用户名 + 密码，校验通过后进入主界面。
// 输入：appController.authManager（login/logout/isLoggedIn/loginError/...）。
// 输出：认证成功后 authManager.isLoggedIn=true，由 Main.qml 切换到主界面。
// =============================================================================
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import ADASMaster

Item {
    id: root

    readonly property var auth: appController.authManager // qmllint disable unqualified

    // 全屏深色背景
    Rectangle {
        anchors.fill: parent
        color: Theme.colorBg
    }

    // 居中登录卡片
    Rectangle {
        id: card
        anchors.centerIn: parent
        width: 380
        height: 360
        color: Theme.colorPanel
        radius: Theme.radiusLg
        border.color: Theme.colorBorder
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacingLg
            spacing: Theme.spacing

            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: "ADAS 场地测试主控系统"
                color: Theme.colorText
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontLarge
                font.bold: true
            }
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: "用户登录"
                color: Theme.colorTextDim
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
            }
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.colorDivider }

            // 用户名
            TextField {
                id: userField
                Layout.fillWidth: true
                placeholderText: "用户名"
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontNormal
                color: Theme.colorText
                background: Rectangle { color: Theme.colorHover; radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
                onAccepted: passField.forceActiveFocus()
            }
            // 密码
            TextField {
                id: passField
                Layout.fillWidth: true
                placeholderText: "密码"
                echoMode: TextInput.Password
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontNormal
                color: Theme.colorText
                background: Rectangle { color: Theme.colorHover; radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
                onAccepted: loginButton.clicked()
            }

            // 错误提示（仅失败时显示）
            Text {
                Layout.fillWidth: true
                visible: root.auth.loginError.length > 0
                text: "⚠ " + root.auth.loginError
                color: Theme.colorEmergency
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
            }

            Button {
                id: loginButton
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                text: "登 录"
                contentItem: Text { text: loginButton.text; color: "white"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: loginButton.down ? Theme.colorAccentHover : Theme.colorAccent; radius: Theme.radius }
                onClicked: root.auth.login(userField.text, passField.text)
            }

            Item { Layout.fillHeight: true }

            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: "初始账号：admin / admin123"
                color: Theme.colorTextMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontTiny
            }
        }
    }
}
```

- [ ] **Step 6: 修改 `qml/Main.qml` 加登录态切换**

在 `ApplicationWindow` 内、现有 `ColumnLayout`（主界面）之前，加入 `LoginView`；并给现有主界面 `ColumnLayout` 加 `visible` 绑定。主界面 `ColumnLayout` 的开头改为：

```qml
        ColumnLayout {
            anchors.fill: parent
            spacing: 0
            visible: appController.authManager.isLoggedIn   // 発录后才显示主界面
```

在 `ApplicationWindow` 内（`ColumnLayout` 之前）加入：

```qml
    // 启动准入：未登录显示 LoginView，登录后由主 ColumnLayout 接管
    LoginView {
        anchors.fill: parent
        visible: !appController.authManager.isLoggedIn
    }
```

- [ ] **Step 7: 构建 + qml_lint + gate 验证 GREEN**

```powershell
cmake --build build
$env:PATH="C:\Qt\6.11.1\mingw_64\bin;$PWD\build;$env:PATH"
ctest --test-dir build -R "login_gate|qml_lint_gate" --output-on-failure
```

Expected: `login_gate` 与 `qml_lint_gate` 均通过。

---

### Task 4: 注销按钮（TopStatusBar）+ SettingsPage 角色只读化

**Files:**
- Modify: `qml/components/TopStatusBar.qml`
- Modify: `qml/pages/SettingsPage.qml`
- Modify: `tests/CheckLogin.ps1`（扩展）

**Interfaces:**
- Consumes: `appController.authManager.logout()`、`currentDisplayName`、`currentRole`。
- Produces: 顶栏注销入口（含确认框）；设置页只读显示当前登录用户与角色。

- [ ] **Step 1: 在 `qml/components/TopStatusBar.qml` 加注销按钮**

在文件顶部 import 区加入：

```qml
import QtQuick.Dialogs
```

在退出按钮（`Rectangle { ... id: quitMa ... onClicked: Qt.quit() }`）**之前**插入注销按钮（同样式，log-out 图标 + 确认框）：

```qml
        // 注销按钮：回登录页换人；点击弹确认框防误触
        Rectangle {
            Layout.preferredWidth: 40
            Layout.fillHeight: true
            color: logoutMa.containsMouse ? Theme.colorError : "transparent"
            Behavior on color { ColorAnimation { duration: 120 } }

            Icon {
                anchors.centerIn: parent
                source: "logout"
                size: Theme.iconMd
                opacity: logoutMa.containsMouse ? 1.0 : 0.7
            }
            MouseArea {
                id: logoutMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: logoutConfirm.open()
            }
        }
```

在 `TopStatusBar` 根 `Rectangle` 内、`RowLayout` 之外（例如底部分割线 `Rectangle` 之前）加入确认对话框：

```qml
    // 注销确认：点 Ok(accepted) 触发注销，Cancel(rejected) 仅关闭
    MessageDialog {
        id: logoutConfirm
        title: "注销"
        text: "确定要注销当前用户并返回登录界面吗？"
        buttons: MessageDialog.Ok | MessageDialog.Cancel
        onAccepted: root.ctrl.authManager.logout()
    }
```

> 注意：`root.ctrl` 即 `appController`（TopStatusBar 已有 `readonly property var ctrl: appController`），`ctrl.authManager.logout()` 触发注销，`isLoggedIn` 变 false 后 `Main.qml` 自动回 `LoginView`。

- [ ] **Step 2: 修改 `qml/pages/SettingsPage.qml` 角色只读化**

把「用户与权限」面板里的角色 `ComboBox` + 描述行，替换为只读显示当前登录用户与角色。该 `PanelFrame` 的 `RowLayout` 内容改为：

```qml
                Text { text: "当前用户："; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal }
                Text {
                    text: appController.authManager.currentDisplayName
                    color: Theme.colorText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontNormal
                    font.bold: true
                }
                Item { Layout.fillWidth: true }
                StatusLamp {
                    level: appController.authManager.isLoggedIn ? 0 : 3
                    text: appController.authManager.isLoggedIn ? "已登录" : "未登录"
                    dotSize: 9
                }
```

> 删掉原 `roleCombo`（ComboBox）与 `roleDesc` 数组相关引用；`数据备份` 面板里 `backupButton.enabled: roleCombo.currentIndex >= 2` 改为 `appController.authManager.currentRole >= 2`。同时移除顶部不再使用的 `roles`/`roleDesc`/`roleIndex` 属性（YAGNI）。

- [ ] **Step 3: 扩展 `tests/CheckLogin.ps1`**

在 `Write-Host` 之前追加对注销与角色只读的检查：

```powershell
$topbar = Get-Content -Raw -Encoding UTF8 (Join-Path $workspace 'qml\components\TopStatusBar.qml')
if ($topbar -notmatch 'authManager\.logout') {
    throw 'TopStatusBar.qml must call authManager.logout for the logout button.'
}
$settings = Get-Content -Raw -Encoding UTF8 (Join-Path $workspace 'qml\pages\SettingsPage.qml')
if ($settings -notmatch 'authManager\.currentRole') {
    throw 'SettingsPage.qml must bind permissions to authManager.currentRole.'
}
```

并把结尾的成功信息改为 `Write-Host 'PASS: login screen and logout are integrated.'`。

- [ ] **Step 4: 构建 + qml_lint + gate**

```powershell
cmake --build build
$env:PATH="C:\Qt\6.11.1\mingw_64\bin;$PWD\build;$env:PATH"
ctest --test-dir build -R "login_gate|qml_lint_gate" --output-on-failure
```

Expected: 全部通过。

---

### Task 5: 全量验证

**Files:**
- Verify all modified files.

- [ ] **Step 1: 全量构建**

```powershell
cmake --build build
```

Expected: 构建成功，退出码 0。

- [ ] **Step 2: 完整 CTest 套件**

```powershell
$env:PATH="C:\Qt\6.11.1\mingw_64\bin;$PWD\build;$env:PATH"
ctest --test-dir build --output-on-failure
```

Expected: 原有全部测试 + 新增 `test_auth`、`login_gate` 均通过。

- [ ] **Step 3: 启动并手动验证**

启动 `build\ADASMaster.exe`（1280×1000），逐项核对：

- 启动直接显示登录卡片（无主界面）。
- `admin / admin123` 登录 → 进入主界面。
- 错误密码（如 `admin / x`）→ 卡片显示「⚠ 用户名或密码错误」，不进入主界面。
- `zhangsan/zhang123`（工程师）、`lisi/li123`（操作员）均可登录。
- 主界面顶栏退出按钮左侧出现注销按钮，hover 变橙；点击弹确认框，确认后回到登录卡片。
- 报警中心能看到「登录」「注销」「登录失败」事件记录。
- 系统设置页显示当前登录用户名与「已登录」，不再有角色下拉框。
- 现有导航、急停、设备管理等不受影响。

- [ ] **Step 4: 记录验证证据**

报告构建退出码、CTest 通过数（应为原有数 +2）、以及手动验证清单中任何仍存在的问题。本工作区无 `.git`，不提交，变更保留在工作区。
