# 登录界面设计

## 目标

为 ADASMaster 增加启动准入登录：程序启动时先认证，认证通过才进入主界面；主界面支持注销换人；登录与注销事件留痕审计。对应工控操作员轮班准入与操作可追溯的要求。

## 登录流程

- 程序启动 → 显示 `LoginView`（居中卡片登录界面）
- 用户输入用户名 + 密码 → `AuthManager.login()` 校验
- 校验成功 → `isLoggedIn = true`，QML 切换到主界面（`Main.qml`）
- 注销 → `AuthManager.logout()`，`isLoggedIn = false`，QML 自动回到 `LoginView`

启动准入意味着未登录时无法进入主界面、无法操作设备。

## 账号体系

预置 3 个具名账号，对应工控现场轮班角色。不设访客登录账号（启动准入，最低权限为操作员）：

| 用户名   | 显示名     | 角色      | roleIndex | 初始密码   |
|----------|------------|-----------|-----------|------------|
| admin    | 系统管理员 | admin     | 3         | admin123   |
| zhangsan | 张工       | engineer  | 2         | zhang123   |
| lisi     | 李工       | operator  | 1         | li123      |

`role` 字段为英文键（visitor/operator/engineer/admin），映射到现有 `roleIndex`（0/1/2/3），与 `SettingsPage` 的 4 级角色一致。密码仅以 SHA256 哈希存储，文件中不含明文。

## 账号存储

账号列表存于 `config/users.json`，由现有 `ConfigManager` 模式加载（与 `devices.json` / `app_config.json` 一致的配置即数据做法）：

```json
{
  "users": [
    { "username": "admin",    "displayName": "系统管理员", "role": "admin",    "passwordHash": "<SHA256(初始密码)>" },
    { "username": "zhangsan", "displayName": "张工",       "role": "engineer", "passwordHash": "<SHA256(初始密码)>" },
    { "username": "lisi",     "displayName": "李工",       "role": "operator", "passwordHash": "<SHA256(初始密码)>" }
  ]
}
```

`passwordHash` 存密码的 SHA256 哈希值（用 `QCryptographicHash::Sha256` 生成），不存明文。改账号或重置密码即改此文件。

## 认证方式

用户名 + 密码。`AuthManager::login(user, pass)` 流程：

1. 在 `users.json` 中查找 `username`
2. 计算 `SHA256(pass)`，与该用户的 `passwordHash` 比对
3. 成功：设置 `currentUser` / `currentRole`，写一条「登录成功」审计事件（info），`isLoggedIn = true`
4. 失败：写一条「登录失败」审计事件（warning），返回错误，`isLoggedIn` 保持 false

## 界面布局

居中卡片，深色工业风（复用 `Theme.qml` 与现有组件）：

```
全屏 Theme.colorBg 深色背景，中央一张卡片
        ┌──────────────────────────┐
        │  ADAS 场地测试主控系统    │   标题 fontLarge 粗体
        │  用户登录                 │   副标题 colorTextDim
        │  ────────────────────     │   分隔线
        │  用户名                   │
        │  [ ____________________ ] │   TextField
        │  密 码                    │
        │  [ ____________________ ] │   TextField，echoMode: Password
        │                           │
        │  ⚠ 用户名或密码错误        │   仅失败时显示，红色
        │                           │
        │  [       登  录       ]   │   主按钮 colorAccent
        │  ● 等待登录 / 正在验证     │   StatusLamp 风格状态行
        └──────────────────────────┘
```

- 回车键触发登录（TextField `onAccepted`）
- 卡片自绘 `Rectangle`（`colorPanel`、圆角、1px 边框）；不用 `PanelFrame`，因其自带标题栏不适合登录卡片样式
- 所有颜色 / 字号 / 字体复用 `Theme.*`，不硬编码
- 窗口保持现有固定 1280×1000 无边框（已实现），登录界面在此尺寸内居中

## 注销

主界面 `TopStatusBar` 退出按钮（power 图标）左侧新增一个注销按钮（`logout.svg`，Lucide log-out 图标），hover 高亮、点击弹确认对话框，确认后调用 `AuthManager.logout()` 回到登录页。防误触。

## 审计

登录与注销事件复用现有 `AlarmManager`（info / warning 接口）写入 `EventLog`，报警中心可查：

| 事件     | 级别    | 记录内容                         |
|----------|---------|----------------------------------|
| 登录成功 | info    | 时间、用户名、角色               |
| 登录失败 | warning | 时间、尝试的用户名、失败原因     |
| 注销     | info    | 时间、用户名                     |

## 错误处理

- 用户名不存在 / 密码错误 → 统一提示「用户名或密码错误」，**不区分**两种情况，防止账号枚举
- 密码用 SHA256 哈希比对，不明文存储、不明文比较
- 连续失败仅计数 + 审计记录，首版**不做账号锁定**

## 与现有代码的衔接

- `AppController`：构造 `AuthManager` 并通过 context property 注入 QML；`isLoggedIn` 变化驱动 QML 在 `LoginView` 与主界面之间切换
- `Main.qml`：根据 `appController.authManager.isLoggedIn` 决定显示 `LoginView` 还是主界面框架
- `TopStatusBar.qml`：退出按钮左侧加注销按钮
- `SettingsPage.qml`：角色 `ComboBox` 改为**只读显示**当前登录用户名与角色（角色由账号绑定，去掉手动切换，避免与登录态冲突）
- 本次仅把 `SettingsPage` 的角色显示接到 `AuthManager.currentRole`；其他页面暂不新增角色权限拦截（后续按需扩展）

## 组件边界

- **`AuthManager`**：账号加载、登录校验、持有当前用户 / 角色、注销、审计。系统唯一的认证入口，QML 只通过它访问登录态。
- **`LoginView.qml`**：仅登录表单 UI，通过 `appController.authManager` 调用认证，不直接接触 `users.json`。
- **`config/users.json`**：账号数据，不含业务逻辑。
- **角色权限执行**：本次仅 `SettingsPage` 读 `AuthManager.currentRole` 显示当前角色；认证逻辑集中在 `AuthManager`，不分散到 UI。其他页面的操作级权限拦截不在本次范围。

## 涉及文件

| 操作 | 文件                                                                                  |
|------|---------------------------------------------------------------------------------------|
| 新增 | `src/auth/AuthManager.h` / `.cpp`、`config/users.json`、`qml/pages/LoginView.qml`、`qml/icons/logout.svg`、`tests/TestAuth.cpp`、`tests/CheckLogin.ps1` |
| 修改 | `AppController`（接线 + 启动流程）、`Main.qml`（登录态切换）、`TopStatusBar.qml`（注销按钮）、`SettingsPage.qml`（角色只读化）、`CMakeLists.txt`（登记新文件） |

## 测试与验证

- `TestAuth.cpp`：正确凭证登录成功、错误密码失败、不存在用户失败、注销后 `isLoggedIn = false`、SHA256 哈希校验、`users.json` 正确加载
- `CheckLogin.ps1`：静态 gate，确认 `LoginView` 字段与绑定齐全、`Main.qml` 有登录态切换、CMake 已登记新文件
- 全量构建 + 完整 CTest 套件通过
- 手动验证：启动即见登录页；三个账号登录后权限与角色一致；注销回到登录页；错误密码给出统一提示；登录 / 注销事件出现在报警中心

## 不在本次范围内

- 账号锁定 / 防暴力（连续失败锁定账号或限流）
- 动态增删用户的管理界面
- 密码加盐或更高级哈希（bcrypt / argon2 等）
- 「记住我」/ 免重复登录
- 访客登录账号
- 真实工牌 / IC 卡等硬件认证
