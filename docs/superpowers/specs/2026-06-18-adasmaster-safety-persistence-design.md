# ADASMaster 安全状态机、配置解析与 SQLite 持久化设计

- 日期：2026-06-18
- 状态：待用户审核
- 范围：第一阶段原型的安全闭环、配置加载、数据持久化、回放与自动化测试
- 不包含：真实 TCP/UDP/CAN/串口/厂商 SDK 协议接入

## 1. 目标

在保留现有 `AppController + Manager + QML` 分层结构的前提下，完成以下改进：

1. 修复测试状态机防重入、运行中换工况、设备未启动、自动完成未停机等缺陷。
2. 将设备、安全和测试许可从“数量判断”升级为基于必需设备与安全源状态的明确联锁。
3. 从 JSON 加载应用、设备与工况配置，删除 C++ 和 QML 中重复的工况硬编码。
4. 使用 SQLite 保存测试记录、10Hz 采样和事件，回放页面读取真实数据。
5. 引入 Qt Test、CTest 和关键 QML 静态检查，形成可重复验证流程。

完成后，软件仍是模拟设备环境，但测试流程、数据来源和审计记录应具备一致、可验证的闭环。

## 2. 约束与原则

- QML 只通过 `appController` 访问 C++ 后端，不直接读取文件、数据库或设备对象。
- 所有控制决策位于 C++；QML 只负责显示状态和提交操作请求。
- 安全相关失败采用 fail-closed：条件不明确、数据库不可写、必需设备离线时，不允许预备或开始。
- 硬件急停和无线急停只能由相应输入状态解除，软件复位不得伪造物理释放。
- SQLite 写入不能阻塞 10Hz 控制/界面循环；采样批量提交，控制命令不依赖数据库写入延迟。
- 保留现有枚举数值，避免破坏 QML 中已存在的状态映射；倒计时作为 Armed 的受控子状态。

## 3. 总体架构

```text
AppController
├─ ConfigManager
│  ├─ AppConfig
│  ├─ DeviceConfig[]
│  └─ ScenarioConfig[]
├─ DatabaseManager
│  ├─ TestRunRepository
│  ├─ SampleRepository
│  └─ EventRepository
├─ DeviceManager ─ IDevice ─ FakeDevice
├─ SafetyManager
├─ ScenarioEngine
├─ DataLogger
├─ AlarmManager ─ AlarmListModel
├─ ReplayListModel
└─ StateManager
```

依赖方向：

- `ConfigManager` 和 `DatabaseManager` 不依赖业务 Manager。
- `DeviceManager` 根据 `DeviceConfig` 创建设备，并提供必需设备健康检查。
- `ScenarioEngine` 依赖设备、安全、状态、报警和数据记录接口。
- `DataLogger` 监听测试生命周期和状态快照，统一写入 SQLite。
- `AlarmManager` 同时写内存模型与 `EventRepository`。
- `ReplayListModel` 只查询数据库，不直接依赖运行中的测试对象。

## 4. 配置加载

### 4.1 路径优先级

1. 环境变量 `ADASMASTER_DATA_DIR`，用于开发、测试和受控部署。
2. `QStandardPaths::AppConfigLocation` 下的外部配置。
3. QRC 内置默认配置，首次运行时复制到外部配置目录。

SQLite 数据库位于：

1. `ADASMASTER_DATA_DIR/adasmaster.db`；若未设置环境变量，
2. `QStandardPaths::AppDataLocation/adasmaster.db`。

### 4.2 ConfigManager

新增 `ConfigManager`，职责为：

- 加载并校验 `app_config.json`、`devices.json` 和 `scenarios/*.json`。
- 对缺失字段、错误类型、重复设备名、重复工况名给出结构化错误。
- 提供只读配置集合，不允许业务模块自行重新解析 JSON。
- 支持显式 `reload()`；仅在测试状态为 Idle 时允许重新加载。

设备配置增加字段：

```json
{
  "name": "安全系统",
  "type": "SafetySystem",
  "required": true,
  "enabled": true,
  "protocol": "tcp",
  "ip": "192.168.1.30",
  "port": 5020
}
```

所有现有五台设备默认 `required: true`。后续若允许非关键数据采集设备离线，应通过配置明确设为 `false`，而不是使用在线数量阈值。

### 4.3 ScenarioParser

新增 `ScenarioParser`，将现有三个 JSON 文件转换为完整 `ScenarioConfig`：

- 主车、目标物起始位姿与速度。
- 触发条件、停止条件、安全边界和最大时间。
- 重复次数和 pass criteria。
- 保留原始 JSON 文本作为测试记录的配置快照。

本批次不实现完整 NCAP 物理模型。AEB 使用现有一维模型；RCTA 和 LKA 使用各自独立的模拟策略，并明确标记为模拟结果，禁止复用“AEB 碰撞风险”结果文案。

## 5. 测试状态机与设备闭环

### 5.1 合法状态转换

```text
Idle -> Checking -> Ready -> Armed -> Running -> Finished
  |         |         |        |         |
  +---------+---------+--------+---------+-> Fault / Emergency

Finished / Fault / Emergency -> Idle（满足复位条件后）
```

规则：

- `loadScenario()` 只允许在 Idle。
- `checkDevices()` 需要已成功加载工况、数据库可写、安全状态正常、全部 required 设备在线且 ready。
- `arm()` 再次检查全部许可条件，成功后调用 `armAll()` 并验证所有 required 设备 armed。
- `start()` 只允许调用一次。倒计时期间设置 `countdownActive=true`，禁止再次开始、加载或预备。
- 倒计时结束前再次检查安全与设备状态；通过后调用 `startAll()`，验证 required 设备 running，再进入 Running。
- `stop()`、自动完成、故障和急停均通过统一终止函数停止定时器并控制设备。
- `finishTest()` 始终调用 `stopAll()`，关闭采样并提交数据库事务。

### 5.2 倒计时

- 将倒计时定时器改为 `ScenarioEngine` 的成员，不再每次动态创建。
- 新增只读属性 `countdownActive`。
- 倒计时中普通停止操作可取消测试并进入 Finished；急停进入 Emergency。
- QML 的开始按钮在 `countdownActive` 或倒计时大于 0 时禁用。

### 5.3 DeviceManager

新增：

- `startAll()`。
- `allRequiredOnline()`、`allRequiredReady()`、`allRequiredArmed()`、`allRequiredRunning()`。
- `requiredHealthSummary()`，用于界面和报警显示具体失败设备。

`DeviceListModel` 订阅每台设备的 `statusChanged`，立即刷新对应行；1Hz tick 仅负责模拟量和超时检查，不再承担所有 UI 刷新。

## 6. 安全管理

安全源拆分为“输入状态”和“锁存状态”：

- `updateHardwareEStop(bool active)`：由安全设备适配器调用。
- `updateWirelessEStop(bool active)`：由无线急停适配器调用。
- `triggerSoftwareEStop()`：由界面调用并锁存。
- `requestReset()` 返回 `bool` 和失败原因。

复位规则：

- 硬件或无线输入仍 active 时复位失败。
- required 设备或安全通信异常时复位失败。
- 软件急停锁存只能在上述条件正常后清除。
- 复位失败必须写入报警和 SQLite 事件表。
- 安全系统设备离线立即进入 Error；运行中 required 设备离线触发 Emergency 终止流程。

## 7. SQLite 持久化

### 7.1 DatabaseManager

使用 Qt6 Sql 和 `QSQLITE`：

- 启动时创建数据库目录并打开连接。
- 启用 `PRAGMA foreign_keys=ON`、`journal_mode=WAL` 和合理的 `busy_timeout`。
- 在事务中执行版本迁移。
- 暴露 `ready`、`writable`、`lastError`。
- 迁移或写入失败时保留查看功能，但禁止开始新的测试。

### 7.2 数据库结构

```sql
CREATE TABLE schema_version (
    version INTEGER NOT NULL
);

CREATE TABLE test_runs (
    id TEXT PRIMARY KEY,
    scenario_name TEXT NOT NULL,
    scenario_type TEXT NOT NULL,
    started_at_ms INTEGER NOT NULL,
    finished_at_ms INTEGER,
    status TEXT NOT NULL,
    result_text TEXT,
    operator_name TEXT NOT NULL,
    app_version TEXT NOT NULL,
    config_snapshot TEXT NOT NULL
);

CREATE TABLE samples (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id TEXT NOT NULL REFERENCES test_runs(id) ON DELETE CASCADE,
    timestamp_ms INTEGER NOT NULL,
    ego_x REAL NOT NULL,
    ego_y REAL NOT NULL,
    ego_yaw REAL NOT NULL,
    ego_speed REAL NOT NULL,
    target_x REAL NOT NULL,
    target_y REAL NOT NULL,
    target_yaw REAL NOT NULL,
    target_speed REAL NOT NULL,
    distance REAL NOT NULL,
    relative_speed REAL NOT NULL,
    ttc REAL NOT NULL,
    collision_risk INTEGER NOT NULL
);

CREATE INDEX idx_samples_run_time
ON samples(run_id, timestamp_ms);

CREATE TABLE events (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    run_id TEXT REFERENCES test_runs(id) ON DELETE SET NULL,
    timestamp_ms INTEGER NOT NULL,
    level INTEGER NOT NULL,
    source INTEGER NOT NULL,
    title TEXT NOT NULL,
    message TEXT NOT NULL,
    acknowledged INTEGER NOT NULL DEFAULT 0,
    recovered INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX idx_events_time ON events(timestamp_ms DESC);
CREATE INDEX idx_events_run_time ON events(run_id, timestamp_ms);
```

### 7.3 DataLogger

- `beginRun()` 创建 UUID，并插入 `test_runs`。
- Running 时接收 10Hz 状态快照；每 10 条或每秒批量事务提交。
- `finishRun()` 刷新剩余样本并更新结果、状态和结束时间。
- Emergency/Fault 仍需提交已有样本及终止原因。
- 数据库写入失败时停止新测试；若运行中失败，触发 Error 并执行受控停止，同时记录到 Qt 日志。

## 8. 报警与回放

### 8.1 AlarmManager

- 每个事件同时进入 `AlarmListModel` 和 `EventRepository`。
- 便捷方法接收准确的 `AlarmSource`，不再默认全部归类为 System。
- 确认和恢复状态同步更新数据库。
- 修复容量超过 1000 时嵌套 begin/end model notification 的问题：先完成插入，再独立删除最旧行。

### 8.2 ReplayListModel

新增 `ReplayListModel : QAbstractListModel`：

- 分页查询 `test_runs`，默认按开始时间倒序。
- 暴露工况、时间、结果、操作员和持续时间。
- 选择 run 后查询采样与事件摘要。
- DataReplayPage 删除内置 records，改为真实模型；导出报告仍不在本批次实现，但按钮必须明确显示“尚未实现”并禁用。

## 9. QML 与性能修复

本批次只处理明确问题：

- 将 `MapPage.scale` 改为 `pixelsPerMeter`，避免覆盖 QQuickItem 内置属性。
- 修复布局内直接设置 width/height 的 18 处警告。
- 按钮设置 id，使用 `button.text/down/enabled`，消除 `parent` 的 missing-property 警告。
- 移除未使用 import，修复 `IconButton.enabled` 重复声明。
- 对 Repeater/ListView delegate 增加 id 或 `pragma ComponentBehavior: Bound`，逐步消除 unqualified access。
- `StateManager` 增加一次性 `setKinematicsSnapshot()`，每个 10Hz tick 只发一次运动学通知，地图每 tick 最多重绘一次。

验收要求不是立即清零所有提示，而是：

- missing-property = 0。
- layout-positioning = 0。
- property-override = 0。
- 未处理的 unqualified access 必须记录原因，并不得涉及控制按钮或状态条件。

## 10. 错误处理

- 配置加载失败：应用进入配置错误状态，不创建设备，不允许测试；界面显示具体文件和字段错误。
- SQLite 初始化失败：允许查看基础界面与设备状态，但禁用检查、预备、开始；报警栏显示持久化错误。
- 运行中数据库写入失败：执行受控停止，结果标记为 `DATA_ERROR`。
- 工况不支持：加载失败并保持原状态，不创建部分配置。
- 非法状态操作：返回失败原因并记录 Warning，不静默忽略。
- required 设备掉线：Idle/Ready/Armed 进入 Fault；Running 进入 Emergency。

## 11. 自动化测试

新增 Qt Test 测试目标并注册到 CTest：

### 11.1 状态机

- 未加载工况不能检查。
- 非 Idle 不能加载工况。
- 连续调用 start 只产生一个倒计时。
- 倒计时中设备掉线不会进入 Running。
- 进入 Running 前 required 设备均已调用 start。
- 普通停止、自动完成、故障和急停均停止设备和采样。

### 11.2 安全

- 硬件急停 active 时复位失败。
- 无线急停 active 时复位失败。
- required 安全设备离线时不能预备或开始。
- 软件急停在安全条件恢复后可复位。

### 11.3 配置

- 三个现有工况 JSON 全部可解析。
- 缺字段、错误类型、重复名称解析失败并返回字段路径。
- 设备 required 属性生效。

### 11.4 SQLite

- 空数据库可迁移到当前 schema。
- 创建 run、批量样本、事件、完成 run 可在事务后正确查询。
- 写入失败不会留下半完成事务。
- ReplayListModel 返回真实测试记录。

### 11.5 UI 静态检查

- 保留侧栏布局回归检查。
- CMake 增加 qmllint 质量门槛脚本，验证关键警告计数为 0。

## 12. 构建与部署

- CMake 增加 `Qt6::Sql` 和 `Qt6::Test`。
- 测试目标不链接 QML UI，优先测试 C++ 业务与数据库。
- Windows 部署必须包含 `plugins/sqldrivers/qsqlite.dll`；构建验证检查驱动可加载。
- Debug 用于开发；交付产物使用 Release，并保留可定位版本号和数据库 schema 版本。

## 13. 验收标准

1. 全量构建成功，CTest 全部通过。
2. 重复点击开始不会创建多个倒计时或重复启动设备。
3. 非 Idle 状态无法加载新工况。
4. 任一 required 设备离线时不能开始；运行中掉线会立即终止。
5. 硬件/无线急停仍 active 时软件复位失败。
6. 三个 JSON 工况均由解析器加载，QML 不再维护重复工况列表。
7. 每次测试在 SQLite 中生成 run、samples 和 events，回放页显示真实记录。
8. 自动完成、普通停止、故障和急停均停止设备并关闭数据记录。
9. qmllint 的 missing-property、layout-positioning、property-override 警告均为 0。
10. 新 EXE 可加载 qsqlite 驱动并通过无界面启动检查。

