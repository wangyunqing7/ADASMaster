# 页面合并与航点路径实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 ADASMaster 的一级导航从 8 项合并为 5 项——删除运行监控页、场地地图融合测试执行、工况管理融合数据回放——并新增目标物航点路径数据结构、解析、引擎插值运动学、按工况过滤的回放查询与轨迹回放。

**Architecture:** 自底向上分六阶段实施。先扩展 C++ 数据结构（`Waypoint`/`ScenarioConfig.targetPath`）与解析器（`ScenarioParser` 读 JSON `waypoints`），再改引擎运动学（航点插值），再加持久化查询（`listRunsByScenario`/`listSamples`）与 `ReplayListModel` 过滤、`AppController` 接线暴露给 QML，然后重写两个 QML 页面与导航，最后删除废弃页面、更新 CMakeLists 与受影响测试、跑全量验证。

**Tech Stack:** C++17 + Qt6 (Quick/Qml/Sql/Test) + 手写 QML + CMake + Ninja + QtTest（`add_adasmaster_test`）+ PowerShell QML lint gate。

**关键约束（务必遵守）：**
- 新增 C++ 头文件含 `Q_OBJECT`/`Q_GADGET`/`Q_NAMESPACE` 必须登记到 `CMakeLists.txt` 的 `adasmaster_core` 库列表，否则 AUTOMOC 失败。
- 新增 `.qml` 必须登记到 `qt_add_qml_module` 的 `QML_FILES`。
- 代码用英文，注释用中文；每个文件开头块注释说明功能/输入/输出。
- 视觉全部沿用 `Theme.qml`，禁止硬编码颜色值。
- 状态色严格 `0=绿 1=黄 2=橙 3=红`。
- 后端交互统一用 `Q_PROPERTY` + `NOTIFY` + `Q_INVOKABLE` + `QAbstractListModel`。

**参考文件（理解现状必读）：**
- 设计规范：`docs/superpowers/specs/2026-06-20-page-merge-design.md`
- 解析现状：`src/config/ScenarioParser.cpp`（`requireDouble`/`requireObject` 工具函数）
- 引擎现状：`src/scenario/ScenarioEngine.cpp`（`beginSimulation`/`stepSimulation`）
- 持久化现状：`src/persistence/DatabaseManager.cpp`（`listRuns`）、`src/persistence/ReplayListModel.cpp`
- 接线现状：`src/app/AppController.cpp`（构造函数接线）
- 页面现状：`qml/pages/MapPage.qml`、`qml/pages/ScenarioPage.qml`、`qml/pages/TestPage.qml`、`qml/pages/DataReplayPage.qml`、`qml/components/SideNavBar.qml`、`qml/Main.qml`

**构建与测试命令：**
```bash
cmake --build build                                    # 增量构建
ctest --test-dir build --output-on-failure            # 全量测试
ctest --test-dir build -R test_config -V               # 单个测试
ctest --test-dir build -R qml_lint_gate -V             # QML lint
```

---

## 文件结构总览

**新增 C++（登记 `adasmaster_core`）：**
- `src/scenario/Waypoint.h` — 航点结构（`Q_GADGET`，x/y/speed）

**修改 C++（已在构建列表）：**
- `src/scenario/ScenarioConfig.h` — 新增 `targetPath` 字段
- `src/config/ScenarioParser.cpp` — 解析 `target.waypoints`
- `src/scenario/ScenarioEngine.h/.cpp` — 航点插值运动学
- `src/persistence/DatabaseManager.h/.cpp` — `listRunsByScenario`/`listSamples`
- `src/persistence/ReplayListModel.h/.cpp` — `filterByScenario`
- `src/app/AppController.h/.cpp` — 接线工况列表模型、航点读写、回放查询

**新增 QML（登记 `QML_FILES`）：**
- `qml/components/WaypointEditor.qml` — 航点表格编辑器
- `qml/components/PathPreviewMap.qml` — 轨迹预览/回放 Canvas

**重写 QML（文件名不变）：**
- `qml/pages/MapPage.qml` — 融合 TestPage
- `qml/pages/ScenarioPage.qml` — 融合 DataReplayPage + 航点编辑 + 回放
- `qml/components/SideNavBar.qml` — menus 8→5
- `qml/Main.qml` — StackLayout 8→5

**删除 QML（从 `QML_FILES` 移除并删文件）：**
- `qml/pages/DashboardPage.qml`、`qml/pages/TestPage.qml`、`qml/pages/DataReplayPage.qml`

**修改 JSON（已在 RESOURCES）：**
- `scenarios/AEB_CCR_50.json`、`scenarios/RCTA_01.json`、`scenarios/LKA_01.json`

**新增测试（登记 `add_adasmaster_test`）：**
- `tests/TestScenarioParser.cpp` — 航点解析
- `tests/TestWaypointSimulation.cpp` — 航点插值运动学

**修改测试：**
- `tests/TestReplayListModel.cpp` — 按工况过滤

---

## 阶段 1 · 数据结构与解析

### Task 1: 新增 Waypoint 结构

**Files:**
- Create: `src/scenario/Waypoint.h`

- [ ] **Step 1: 创建 Waypoint.h**

```cpp
#pragma once

// =============================================================================
// Waypoint.h
// 功能：定义目标物航点结构。一个航点表示目标物路径上的一个途经点。
// 输入：由 ScenarioParser 从 JSON 的 target.waypoints 数组填充。
// 输出：供 ScenarioConfig.targetPath 组装、ScenarioEngine 航点插值使用。
// =============================================================================

#include <QMetaType>
#include <QString>

struct Waypoint
{
    double x = 0.0;       // 航点 X 坐标（米，场地坐标系）
    double y = 0.0;       // 航点 Y 坐标（米，场地坐标系）
    double speed = 0.0;   // 到达该航点时应达到的目标速度（m/s）

    // 比较运算符，便于测试断言
    bool operator==(const Waypoint &other) const
    {
        return qFuzzyCompare(x, other.x) && qFuzzyCompare(y, other.y)
               && qFuzzyCompare(speed, other.speed);
    }
};

Q_DECLARE_METATYPE(Waypoint)
```

- [ ] **Step 2: 登记到 CMakeLists.txt 的 adasmaster_core 库**

在 `CMakeLists.txt` 的 `add_library(adasmaster_core STATIC ...)` 列表中，在 `src/scenario/ScenarioConfig.h` 这一行之前插入：

```
    src/scenario/Waypoint.h
```

- [ ] **Step 3: 构建验证**

Run: `cmake --build build`
Expected: 构建成功（Waypoint.h 是纯头文件，无符号需链接）

- [ ] **Step 4: Commit**

```bash
git add src/scenario/Waypoint.h CMakeLists.txt
git commit -m "feat(scenario): add Waypoint struct for target path"
```

---

### Task 2: ScenarioConfig 新增 targetPath 字段

**Files:**
- Modify: `src/scenario/ScenarioConfig.h`

- [ ] **Step 1: 在 ScenarioConfig.h 顶部 include Waypoint.h 并新增字段**

在 `#include <QString>` 之后加：

```cpp
#include "Waypoint.h"
#include <QList>
```

在 `ScenarioConfig` 结构体内，`double targetStartYaw = 0.0;` 这一行之后插入：

```cpp
    QList<Waypoint> targetPath;     // 目标物航点路径；为空时退回单点 targetStart* 逻辑（向后兼容）
```

- [ ] **Step 2: 构建验证**

Run: `cmake --build build`
Expected: 构建成功（仅添加字段，未改逻辑）

- [ ] **Step 3: Commit**

```bash
git add src/scenario/ScenarioConfig.h
git commit -m "feat(scenario): add targetPath field to ScenarioConfig"
```

---

### Task 3: ScenarioParser 解析 waypoints 字段

**Files:**
- Modify: `src/config/ScenarioParser.cpp`
- Create: `tests/TestScenarioParser.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 先写失败测试**

创建 `tests/TestScenarioParser.cpp`：

```cpp
#include <QtTest>
#include "src/config/ScenarioParser.h"

class TestScenarioParser : public QObject
{
    Q_OBJECT

private slots:
    // 有 waypoints 时，解析填充 targetPath，并以第一个航点作为 targetStart
    void parsesWaypoints()
    {
        const QByteArray json = R"({
            "name":"WP测试", "type":"AEB",
            "ego":{"target_speed_mps":13.89,"start_x":-50,"start_y":0,"start_yaw":0},
            "target":{"type":"车","speed_mps":3.0,"start_x":0,"start_y":0,"start_yaw":0,
                "waypoints":[
                    {"x":0,"y":0,"speed":0},
                    {"x":20,"y":0,"speed":3},
                    {"x":40,"y":2,"speed":3}
                ]},
            "max_test_time_sec":20, "repeat_count":1
        })";
        ScenarioConfig config;
        QString error;
        QVERIFY2(ScenarioParser::parseJson(json, QStringLiteral("memory"), &config, &error),
                 qPrintable(error));
        QCOMPARE(config.targetPath.size(), 3);
        QCOMPARE(config.targetPath.at(0).x, 0.0);
        QCOMPARE(config.targetPath.at(1).x, 20.0);
        QCOMPARE(config.targetPath.at(2).y, 2.0);
        QCOMPARE(config.targetPath.at(2).speed, 3.0);
    }

    // 缺少 waypoints 字段时，targetPath 为空（向后兼容）
    void emptyWaypointsWhenAbsent()
    {
        const QByteArray json = R"({
            "name":"无航点", "type":"AEB",
            "ego":{"target_speed_mps":13.89,"start_x":-50,"start_y":0,"start_yaw":0},
            "target":{"type":"车","speed_mps":0,"start_x":0,"start_y":0,"start_yaw":0},
            "max_test_time_sec":20, "repeat_count":1
        })";
        ScenarioConfig config;
        QString error;
        QVERIFY2(ScenarioParser::parseJson(json, QStringLiteral("memory"), &config, &error),
                 qPrintable(error));
        QVERIFY(config.targetPath.isEmpty());
    }

    // waypoints 存在但不是数组时报错
    void rejectsNonArrayWaypoints()
    {
        const QByteArray json = R"({
            "name":"坏航点", "type":"AEB",
            "ego":{"target_speed_mps":13.89,"start_x":-50,"start_y":0,"start_yaw":0},
            "target":{"type":"车","speed_mps":0,"start_x":0,"start_y":0,"start_yaw":0,
                "waypoints":"not-an-array"},
            "max_test_time_sec":20, "repeat_count":1
        })";
        ScenarioConfig config;
        QString error;
        QVERIFY(!ScenarioParser::parseJson(json, QStringLiteral("memory"), &config, &error));
        QVERIFY2(error.contains(QStringLiteral("waypoints")), qPrintable(error));
    }
};
```

- [ ] **Step 2: 登记测试到 CMakeLists.txt**

在 `add_adasmaster_test(test_auth tests/TestAuth.cpp)` 之后加：

```
add_adasmaster_test(test_scenario_parser tests/TestScenarioParser.cpp)
```

- [ ] **Step 3: 构建并运行测试，确认失败**

Run: `cmake --build build && ctest --test-dir build -R test_scenario_parser -V`
Expected: FAIL —— `targetPath.size()` 为 0（解析器还没读 waypoints），`parsesWaypoints` 失败

- [ ] **Step 4: 实现 ScenarioParser 解析 waypoints**

在 `src/config/ScenarioParser.cpp` 的匿名命名空间内（`requireObject` 函数之后）加一个 `requireWaypoints` 工具：

```cpp
// 解析 target.waypoints 数组；字段缺失时返回空列表（向后兼容），格式错误时报错。
bool parseWaypoints(const QJsonObject &target, const QString &path,
                    QList<Waypoint> *waypoints, QString *error)
{
    waypoints->clear();
    const QJsonValue item = target.value(QStringLiteral("waypoints"));
    if (item.isUndefined() || item.isNull()) return true;   // 可选字段
    if (!item.isArray()) {
        if (error) *error = QStringLiteral("%1.waypoints must be an array").arg(path);
        return false;
    }
    const QJsonArray array = item.toArray();
    for (int i = 0; i < array.size(); ++i) {
        const QJsonObject wp = array.at(i).toObject();
        const QString wpPath = QStringLiteral("%1.waypoints[%2]").arg(path).arg(i);
        Waypoint point;
        if (!requireDouble(wp, QStringLiteral("x"), wpPath, &point.x, error) ||
            !requireDouble(wp, QStringLiteral("y"), wpPath, &point.y, error) ||
            !requireDouble(wp, QStringLiteral("speed"), wpPath, &point.speed, error)) {
            return false;
        }
        waypoints->append(point);
    }
    return true;
}
```

然后在 `parseJson` 函数内，解析完 target 的 start_x/y/yaw 块之后（即 `} else if (...)` 块结束之后），在 `parsed.description = ...` 之前插入：

```cpp
    if (!parseWaypoints(target, QStringLiteral("target"), &parsed.targetPath, error)) {
        return false;
    }
```

注意：需要在文件顶部加 `#include <QJsonArray>`（现有 include 只有 `QJsonDocument`/`QJsonObject`），并在 `#include "../scenario/ScenarioConfig.h"` 之后加 `#include "../scenario/Waypoint.h"`（Waypoint 已通过 ScenarioConfig.h 间接 include，但显式更清晰）。

- [ ] **Step 5: 构建并运行测试，确认通过**

Run: `cmake --build build && ctest --test-dir build -R test_scenario_parser -V`
Expected: PASS（3 个测试全过）

- [ ] **Step 6: 运行现有 TestConfig 确认未破坏**

Run: `ctest --test-dir build -R test_config -V`
Expected: PASS（现有 3 个测试仍通过，因为它们不涉及 waypoints）

- [ ] **Step 7: Commit**

```bash
git add src/config/ScenarioParser.cpp tests/TestScenarioParser.cpp CMakeLists.txt
git commit -m "feat(config): parse target.waypoints into ScenarioConfig.targetPath"
```

---

### Task 4: 为三个内置工况 JSON 添加 waypoints

**Files:**
- Modify: `scenarios/AEB_CCR_50.json`
- Modify: `scenarios/RCTA_01.json`
- Modify: `scenarios/LKA_01.json`

- [ ] **Step 1: AEB_CCR_50.json 添加单点 waypoints（静止目标）**

AEB 是车对静止车辆，目标不动。在 `target` 对象的 `start_yaw` 之后加：

```json
        "waypoints": [
            { "x": 0.0, "y": 0.0, "speed": 0.0 }
        ]
```

- [ ] **Step 2: RCTA_01.json 添加横穿路径 waypoints**

先读取现状：`cat scenarios/RCTA_01.json` 确认 target 的 start_x/y。RCTA 目标横穿，从右侧驶入。在 `target` 对象内加：

```json
        "waypoints": [
            { "x": 5.0,  "y": -10.0, "speed": 3.0 },
            { "x": 0.0,  "y": 0.0,    "speed": 3.0 },
            { "x": -5.0, "y": 10.0,   "speed": 3.0 }
        ]
```

- [ ] **Step 3: LKA_01.json 保留无 waypoints**

LKA 无物理目标物（车道线偏移），不加 waypoints。保持现状以验证向后兼容（targetPath 为空）。

- [ ] **Step 4: 运行 TestConfig 验证解析**

Run: `cmake --build build && ctest --test-dir build -R test_config -V`
Expected: PASS（`parsesAllProjectConfiguration` 仍 3 个工况）

- [ ] **Step 5: 运行 TestScenarioParser**

Run: `ctest --test-dir build -R test_scenario_parser -V`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add scenarios/AEB_CCR_50.json scenarios/RCTA_01.json
git commit -m "feat(scenarios): add target waypoints to AEB and RCTA scenarios"
```

---

## 阶段 2 · 引擎航点插值运动学

### Task 5: ScenarioEngine 航点插值

**Files:**
- Modify: `src/scenario/ScenarioEngine.h`
- Modify: `src/scenario/ScenarioEngine.cpp`
- Create: `tests/TestWaypointSimulation.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 先写失败测试**

创建 `tests/TestWaypointSimulation.cpp`。由于 `stepSimulation` 是 private，测试通过 `tick`/`start` 链路较难驱动；改为直接验证公开行为：`loadScenario` 后 `beginSimulation` 初始化的目标位置等于第一个航点。但 `beginSimulation` 也是 private。

采用方案：测试 `ScenarioEngine` 通过 `loadScenario` + 检查加载后 `scenarioName`，并新增一个 `Q_INVOKABLE` 只读调试方法供测试与 QML 预览使用：`currentTargetWaypointCount()`。同时验证插值核心逻辑用一个可测的辅助函数。

先在测试里验证解析后的 config 传入引擎后能取到航点数：

```cpp
#include <QtTest>
#include "src/scenario/ScenarioEngine.h"
#include "src/scenario/ScenarioConfig.h"
#include "src/config/ScenarioParser.h"

class TestWaypointSimulation : public QObject
{
    Q_OBJECT

private slots:
    // 引擎能报告加载工况的航点数（供 QML 预览与测试使用）
    void reportsWaypointCount()
    {
        const QByteArray json = R"({
            "name":"WP", "type":"AEB",
            "ego":{"target_speed_mps":13.89,"start_x":-50,"start_y":0,"start_yaw":0},
            "target":{"type":"车","speed_mps":3.0,"start_x":0,"start_y":0,"start_yaw":0,
                "waypoints":[
                    {"x":0,"y":0,"speed":0},
                    {"x":20,"y":0,"speed":3},
                    {"x":40,"y":2,"speed":3}
                ]},
            "max_test_time_sec":20, "repeat_count":1
        })";
        ScenarioConfig config;
        QString error;
        QVERIFY2(ScenarioParser::parseJson(json, QStringLiteral("memory"), &config, &error),
                 qPrintable(error));

        ScenarioEngine engine;
        engine.setScenarios({config});
        QVERIFY(engine.loadScenario(QStringLiteral("WP")));
        QCOMPARE(engine.currentTargetWaypointCount(), 3);
        QCOMPARE(engine.currentWaypoint(0).x, 0.0);
        QCOMPARE(engine.currentWaypoint(1).x, 20.0);
        QCOMPARE(engine.currentWaypoint(2).y, 2.0);
    }

    // 无航点工况（LKA）航点数为 0
    void zeroWaypointsForLegacyScenario()
    {
        const QByteArray json = R"({
            "name":"Legacy", "type":"LKA",
            "ego":{"target_speed_mps":20,"start_x":0,"start_y":0,"start_yaw":0},
            "target":{"type":"车道线","speed_mps":0},
            "max_test_time_sec":30, "repeat_count":1
        })";
        ScenarioConfig config;
        QString error;
        QVERIFY2(ScenarioParser::parseJson(json, QStringLiteral("memory"), &config, &error),
                 qPrintable(error));

        ScenarioEngine engine;
        engine.setScenarios({config});
        QVERIFY(engine.loadScenario(QStringLiteral("Legacy")));
        QCOMPARE(engine.currentTargetWaypointCount(), 0);
    }
};
```

- [ ] **Step 2: 登记测试到 CMakeLists.txt**

在 `add_adasmaster_test(test_scenario_parser tests/TestScenarioParser.cpp)` 之后加：

```
add_adasmaster_test(test_waypoint_simulation tests/TestWaypointSimulation.cpp)
```

- [ ] **Step 3: 构建并运行测试，确认失败**

Run: `cmake --build build && ctest --test-dir build -R test_waypoint_simulation -V`
Expected: FAIL —— 编译错误，`currentTargetWaypointCount`/`currentWaypoint` 方法不存在

- [ ] **Step 4: 在 ScenarioEngine.h 加只读访问方法**

在 `ScenarioEngine.h` 的 public 区（`bool countdownActive() const` 之后）加：

```cpp
    // 当前加载工况的目标物航点路径（供 QML 预览与测试读取；未加载时返回空）
    int currentTargetWaypointCount() const { return m_config.targetPath.size(); }
    Waypoint currentWaypoint(int index) const { return m_config.targetPath.value(index); }
    const QList<Waypoint> &currentTargetPath() const { return m_config.targetPath; }
```

并在头部 include 区加 `#include "Waypoint.h"`（Waypoint 已通过 ScenarioConfig.h 间接 include）。

- [ ] **Step 5: 修改 beginSimulation 用第一个航点初始化目标位置**

在 `src/scenario/ScenarioEngine.cpp` 的 `beginSimulation()` 内，把：

```cpp
    m_tgtX = m_config.targetStartX; m_tgtY = m_config.targetStartY;
    m_tgtYaw = m_config.targetStartYaw; m_tgtSpeed = m_config.targetSpeed;
```

替换为：

```cpp
    // 目标物起点：优先取第一个航点，无航点时退回 targetStart*
    if (!m_config.targetPath.isEmpty()) {
        const Waypoint &wp0 = m_config.targetPath.first();
        m_tgtX = wp0.x; m_tgtY = wp0.y; m_tgtSpeed = wp0.speed;
        m_tgtYaw = m_config.targetStartYaw;
        m_currentWaypoint = 0;
    } else {
        m_tgtX = m_config.targetStartX; m_tgtY = m_config.targetStartY;
        m_tgtYaw = m_config.targetStartYaw; m_tgtSpeed = m_config.targetSpeed;
        m_currentWaypoint = -1;
    }
```

- [ ] **Step 6: 在 ScenarioEngine.h 加航点索引成员**

在 private 区（`bool m_simInited = false;` 之后）加：

```cpp
    int m_currentWaypoint = -1;     // 当前正在前往的航点索引；-1 表示单点模式
```

- [ ] **Step 7: 重写 stepSimulation 的目标物运动段**

在 `src/scenario/ScenarioEngine.cpp` 的 `stepSimulation(double dt)` 内，把这段目标物运动：

```cpp
    // 目标物运动
    const double targetRadians = m_tgtYaw * 3.14159265358979323846 / 180.0;
    m_tgtX += m_tgtSpeed * std::cos(targetRadians) * dt;
    m_tgtY += m_tgtSpeed * std::sin(targetRadians) * dt;
```

替换为航点插值：

```cpp
    // 目标物运动：有航点时按航点插值，无航点时保持单点恒速/静止
    if (m_currentWaypoint >= 0 && m_currentWaypoint < m_config.targetPath.size()) {
        const Waypoint &goal = m_config.targetPath.at(m_currentWaypoint);
        const double dxw = goal.x - m_tgtX;
        const double dyw = goal.y - m_tgtY;
        const double distToGoal = std::hypot(dxw, dyw);
        const double move = m_tgtSpeed * dt;

        if (distToGoal <= 0.5 || move >= distToGoal) {
            // 到达当前航点，切换到下一个（或停留）
            m_tgtX = goal.x; m_tgtY = goal.y;
            if (m_currentWaypoint + 1 < m_config.targetPath.size()) {
                ++m_currentWaypoint;
                m_tgtSpeed = m_config.targetPath.at(m_currentWaypoint).speed;
            } else {
                // 最后航点：保持其速度（可能为 0，即静止）
                m_tgtSpeed = goal.speed;
                m_currentWaypoint = m_config.targetPath.size();  // 标记已走完
            }
        } else {
            // 朝航点方向移动
            m_tgtX += dxw / distToGoal * move;
            m_tgtY += dyw / distToGoal * move;
        }
    } else if (m_currentWaypoint < 0) {
        // 单点模式：沿 yaw 方向恒速
        const double targetRadians = m_tgtYaw * 3.14159265358979323846 / 180.0;
        m_tgtX += m_tgtSpeed * std::cos(targetRadians) * dt;
        m_tgtY += m_tgtSpeed * std::sin(targetRadians) * dt;
    }
    // m_currentWaypoint >= size()：已走完所有航点，目标物静止不动
```

- [ ] **Step 8: 构建并运行测试，确认通过**

Run: `cmake --build build && ctest --test-dir build -R test_waypoint_simulation -V`
Expected: PASS（2 个测试全过）

- [ ] **Step 9: 运行现有 TestStateMachine 确认未破坏**

Run: `ctest --test-dir build -R test_state_machine -V`
Expected: PASS

- [ ] **Step 10: Commit**

```bash
git add src/scenario/ScenarioEngine.h src/scenario/ScenarioEngine.cpp tests/TestWaypointSimulation.cpp CMakeLists.txt
git commit -m "feat(scenario): waypoint-interpolated target motion in ScenarioEngine"
```

---

## 阶段 3 · 持久化查询扩展

### Task 6: DatabaseManager 新增按工况过滤与采样查询

**Files:**
- Modify: `src/persistence/DatabaseManager.h`
- Modify: `src/persistence/DatabaseManager.cpp`
- Modify: `tests/TestDatabase.cpp`

- [ ] **Step 1: 先看 TestDatabase.cpp 现有结构**

Run: `cat tests/TestDatabase.cpp`（理解现有测试如何构造 DatabaseManager 与插入数据）

- [ ] **Step 2: 在 TestDatabase.cpp 加失败测试**

在 TestDatabase 类的 private slots 末尾加两个测试（参照现有 `listRuns` 测试的 fixture 写法插入 run + sample）：

```cpp
    void listsRunsFilteredByScenario()
    {
        // 自包含 fixture：插入 AEB 和 RCTA 各一条
        TestRunRecord aebRun;
        aebRun.id = QStringLiteral("aeb-filter-1");
        aebRun.scenarioName = QStringLiteral("AEB CCRs 50");
        aebRun.scenarioType = QStringLiteral("AEB");
        aebRun.startedAtMs = 1000;
        aebRun.status = QStringLiteral("FINISHED");
        aebRun.operatorName = QStringLiteral("tester");
        aebRun.appVersion = QStringLiteral("test");
        aebRun.configSnapshot = QStringLiteral("{}");
        QVERIFY(m_db.createRun(aebRun));

        TestRunRecord rctaRun;
        rctaRun.id = QStringLiteral("rcta-filter-1");
        rctaRun.scenarioName = QStringLiteral("RCTA 01");
        rctaRun.scenarioType = QStringLiteral("RCTA");
        rctaRun.startedAtMs = 2000;
        rctaRun.status = QStringLiteral("FINISHED");
        rctaRun.operatorName = QStringLiteral("tester");
        rctaRun.appVersion = QStringLiteral("test");
        rctaRun.configSnapshot = QStringLiteral("{}");
        QVERIFY(m_db.createRun(rctaRun));

        const QList<TestRunSummary> aebOnly = m_db.listRunsByScenario(QStringLiteral("AEB CCRs 50"));
        QVERIFY(aebOnly.size() >= 1);
        QVERIFY(std::all_of(aebOnly.cbegin(), aebOnly.cend(),
                            [](const TestRunSummary &s) { return s.scenarioName == QStringLiteral("AEB CCRs 50"); }));
        const QList<TestRunSummary> rctaOnly = m_db.listRunsByScenario(QStringLiteral("RCTA 01"));
        QCOMPARE(rctaOnly.size(), 1);
        QCOMPARE(rctaOnly.first().id, QStringLiteral("rcta-filter-1"));
    }

    void listsSamplesForRun()
    {
        // 自包含 fixture：新建一个 run，插入两个采样，再查回
        TestRunRecord run;
        run.id = QStringLiteral("sample-run-1");
        run.scenarioName = QStringLiteral("AEB CCRs 50");
        run.scenarioType = QStringLiteral("AEB");
        run.startedAtMs = 3000;
        run.status = QStringLiteral("FINISHED");
        run.operatorName = QStringLiteral("tester");
        run.appVersion = QStringLiteral("test");
        run.configSnapshot = QStringLiteral("{}");
        QVERIFY(m_db.createRun(run));

        const QString runId = run.id;
        SampleRecord s1; s1.runId = runId; s1.timestampMs = 100; s1.egoX = 1.0; s1.egoY = 0.0;
        SampleRecord s2; s2.runId = runId; s2.timestampMs = 200; s2.egoX = 2.0; s2.egoY = 0.0;
        QVERIFY(m_db.appendSamples({s1, s2}));
        const QList<SampleRecord> samples = m_db.listSamples(runId);
        QCOMPARE(samples.size(), 2);
        QCOMPARE(samples.at(0).timestampMs, 100);
        QCOMPARE(samples.at(1).timestampMs, 200);
    }
```

> 注：执行此步前先读 `tests/TestDatabase.cpp` 确认 `m_db` 成员名与 setup/teardown（每个测试是否独立 DB）。若 `listsRunsFilteredByScenario` 与本测试共享 fixture 导致计数不稳定，两个测试都改用各自自包含的 run（如上），不依赖其他测试插入的数据。

- [ ] **Step 3: 在 DatabaseManager.h 声明新方法**

在 `QList<TestRunSummary> listRuns(int limit = 200) const;` 之后加：

```cpp
    QList<TestRunSummary> listRunsByScenario(const QString &scenarioName, int limit = 200) const;
    QList<SampleRecord> listSamples(const QString &runId) const;
```

- [ ] **Step 4: 在 DatabaseManager.cpp 实现**

在 `listRuns` 函数之后加：

```cpp
QList<TestRunSummary> DatabaseManager::listRunsByScenario(const QString &scenarioName, int limit) const
{
    QList<TestRunSummary> result;
    QSqlQuery query(database());
    query.prepare(QStringLiteral("SELECT id,scenario_name,scenario_type,started_at_ms,"
                                 "COALESCE(finished_at_ms,0),status,COALESCE(result_text,''),operator_name "
                                 "FROM test_runs WHERE scenario_name=? ORDER BY started_at_ms DESC LIMIT ?"));
    query.addBindValue(scenarioName);
    query.addBindValue(limit);
    if (!query.exec()) return result;
    while (query.next()) {
        TestRunSummary summary;
        summary.id = query.value(0).toString();
        summary.scenarioName = query.value(1).toString();
        summary.scenarioType = query.value(2).toString();
        summary.startedAtMs = query.value(3).toLongLong();
        summary.finishedAtMs = query.value(4).toLongLong();
        summary.status = query.value(5).toString();
        summary.resultText = query.value(6).toString();
        summary.operatorName = query.value(7).toString();
        result.append(summary);
    }
    return result;
}

QList<SampleRecord> DatabaseManager::listSamples(const QString &runId) const
{
    QList<SampleRecord> result;
    QSqlQuery query(database());
    query.prepare(QStringLiteral("SELECT run_id,timestamp_ms,ego_x,ego_y,ego_yaw,ego_speed,"
                                 "target_x,target_y,target_yaw,target_speed,distance,relative_speed,ttc,collision_risk "
                                 "FROM samples WHERE run_id=? ORDER BY timestamp_ms ASC"));
    query.addBindValue(runId);
    if (!query.exec()) return result;
    while (query.next()) {
        SampleRecord s;
        s.runId = query.value(0).toString();
        s.timestampMs = query.value(1).toLongLong();
        s.egoX = query.value(2).toDouble();
        s.egoY = query.value(3).toDouble();
        s.egoYaw = query.value(4).toDouble();
        s.egoSpeed = query.value(5).toDouble();
        s.targetX = query.value(6).toDouble();
        s.targetY = query.value(7).toDouble();
        s.targetYaw = query.value(8).toDouble();
        s.targetSpeed = query.value(9).toDouble();
        s.distance = query.value(10).toDouble();
        s.relativeSpeed = query.value(11).toDouble();
        s.ttc = query.value(12).toDouble();
        s.collisionRisk = query.value(13).toInt() != 0;
        result.append(s);
    }
    return result;
}
```

- [ ] **Step 5: 构建并运行测试，确认通过**

Run: `cmake --build build && ctest --test-dir build -R test_database -V`
Expected: PASS（含新增 2 个测试）

- [ ] **Step 6: Commit**

```bash
git add src/persistence/DatabaseManager.h src/persistence/DatabaseManager.cpp tests/TestDatabase.cpp
git commit -m "feat(persistence): listRunsByScenario and listSamples queries"
```

---

### Task 7: ReplayListModel 按工况过滤

**Files:**
- Modify: `src/persistence/ReplayListModel.h`
- Modify: `src/persistence/ReplayListModel.cpp`
- Modify: `tests/TestReplayListModel.cpp`

- [ ] **Step 1: 读 TestReplayListModel.cpp 现有结构**

Run: `cat tests/TestReplayListModel.cpp`（理解 fixture：如何注入 DatabaseManager mock 或真实 DB）

- [ ] **Step 2: 在 TestReplayListModel.cpp 加失败测试**

在 private slots 末尾加：

```cpp
    void filtersByScenario()
    {
        // 假设 fixture 已插入 AEB 和 RCTA 各一条 run
        m_model.filterByScenario(QStringLiteral("AEB CCRs 50"));
        QVERIFY(m_model.count() >= 1);
        for (int i = 0; i < m_model.count(); ++i) {
            QModelIndex idx = m_model.index(i);
            QCOMPARE(m_model.data(idx, ReplayListModel::ScenarioNameRole).toString(),
                     QStringLiteral("AEB CCRs 50"));
        }
        // 空字符串恢复全部
        m_model.filterByScenario(QString());
        QVERIFY(m_model.count() >= 2);
    }
```

> 注：执行时需按实际 fixture 的成员名（`m_model`/DB 注入方式）调整。

- [ ] **Step 3: 在 ReplayListModel.h 加过滤状态与方法**

在 `Q_INVOKABLE void refresh();` 之后加：

```cpp
    Q_INVOKABLE void filterByScenario(const QString &scenarioName);
```

在 private 区 `QList<TestRunSummary> m_runs;` 之前加：

```cpp
    QString m_scenarioFilter;   // 非空时仅显示该工况的记录
```

- [ ] **Step 4: 在 ReplayListModel.cpp 实现**

修改 `refresh()`，把：

```cpp
    m_runs = m_database && m_database->ready()
                 ? m_database->listRuns()
                 : QList<TestRunSummary>{};
```

替换为：

```cpp
    if (m_database && m_database->ready()) {
        m_runs = m_scenarioFilter.isEmpty()
                     ? m_database->listRuns()
                     : m_database->listRunsByScenario(m_scenarioFilter);
    } else {
        m_runs.clear();
    }
```

并实现 `filterByScenario`：

```cpp
void ReplayListModel::filterByScenario(const QString &scenarioName)
{
    m_scenarioFilter = scenarioName;
    refresh();
}
```

- [ ] **Step 5: 构建并运行测试，确认通过**

Run: `cmake --build build && ctest --test-dir build -R test_replay_list_model -V`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add src/persistence/ReplayListModel.h src/persistence/ReplayListModel.cpp tests/TestReplayListModel.cpp
git commit -m "feat(replay): filter replay records by scenario name"
```

---

## 阶段 4 · AppController 接线

### Task 8: AppController 暴露工况列表、航点与回放采样给 QML

**Files:**
- Modify: `src/app/AppController.h`
- Modify: `src/app/AppController.cpp`

- [ ] **Step 1: 在 AppController.h 加 Q_INVOKABLE 接口**

在 `Q_INVOKABLE void startup();` 之后加：

```cpp
    // 工况列表（名称数组，供 QML 工况选择下拉与工况管理列表）
    Q_INVOKABLE QStringList scenarioNames() const;
    // 取某工况的航点路径（ QVariantList of QVariantMap{x,y,speed}）；无则空
    Q_INVOKABLE QVariantList scenarioWaypoints(const QString &name) const;
    // 加载某 run 的采样数据，供轨迹回放（QVariantList of QVariantMap）
    Q_INVOKABLE QVariantList runSamples(const QString &runId) const;
```

- [ ] **Step 2: 在 AppController.cpp 实现**

在文件末尾加：

```cpp
QStringList AppController::scenarioNames() const
{
    QStringList names;
    if (m_config) {
        for (const ScenarioConfig &s : m_config->scenarios())
            names.append(s.name);
    }
    return names;
}

QVariantList AppController::scenarioWaypoints(const QString &name) const
{
    QVariantList list;
    if (!m_config) return list;
    const ScenarioConfig *s = m_config->scenarioByName(name);
    if (!s) return list;
    for (const Waypoint &wp : s->targetPath) {
        QVariantMap point;
        point.insert(QStringLiteral("x"), wp.x);
        point.insert(QStringLiteral("y"), wp.y);
        point.insert(QStringLiteral("speed"), wp.speed);
        list.append(point);
    }
    return list;
}

QVariantList AppController::runSamples(const QString &runId) const
{
    QVariantList list;
    if (!m_database || !m_database->ready() || runId.isEmpty()) return list;
    const QList<SampleRecord> samples = m_database->listSamples(runId);
    for (const SampleRecord &s : samples) {
        QVariantMap m;
        m.insert(QStringLiteral("timestampMs"), s.timestampMs);
        m.insert(QStringLiteral("egoX"), s.egoX);
        m.insert(QStringLiteral("egoY"), s.egoY);
        m.insert(QStringLiteral("egoSpeed"), s.egoSpeed);
        m.insert(QStringLiteral("targetX"), s.targetX);
        m.insert(QStringLiteral("targetY"), s.targetY);
        m.insert(QStringLiteral("targetSpeed"), s.targetSpeed);
        m.insert(QStringLiteral("distance"), s.distance);
        m.insert(QStringLiteral("ttc"), s.ttc);
        m.insert(QStringLiteral("collisionRisk"), s.collisionRisk);
        list.append(m);
    }
    return list;
}
```

注意在 AppController.cpp 顶部确保已 include 所需类型（`ScenarioConfig.h`/`Waypoint.h`/`PersistenceTypes.h` 已通过 AppController.h 间接 include）。

- [ ] **Step 3: 构建验证**

Run: `cmake --build build`
Expected: 构建成功

- [ ] **Step 4: 运行 TestAppController 确认未破坏**

Run: `ctest --test-dir build -R test_app_controller -V`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add src/app/AppController.h src/app/AppController.cpp
git commit -m "feat(app): expose scenario names, waypoints, run samples to QML"
```

---

## 阶段 5 · QML 页面重写

### Task 9: SideNavBar 菜单从 8 项改 5 项

**Files:**
- Modify: `qml/components/SideNavBar.qml`
- Modify: `qml/Main.qml`

- [ ] **Step 1: 修改 SideNavBar.qml 的 menus 数组**

把 `readonly property var menus:` 数组改为：

```qml
    readonly property var menus: [
        { name: "设备管理", icon: "device" },
        { name: "场地地图", icon: "map" },
        { name: "工况管理", icon: "scenario" },
        { name: "报警中心", icon: "alarm" },
        { name: "系统设置", icon: "settings" }
    ]
```

- [ ] **Step 2: 修改 Main.qml 的 StackLayout**

把 StackLayout 的 8 个子项改为 5 个（顺序与导航一致）：

```qml
                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: rightContent.height * 3 / 4
                    currentIndex: window.currentPage

                    DevicePage { Layout.fillWidth: true; Layout.fillHeight: true }
                    MapPage { Layout.fillWidth: true; Layout.fillHeight: true }
                    ScenarioPage { Layout.fillWidth: true; Layout.fillHeight: true }
                    AlarmPage { Layout.fillWidth: true; Layout.fillHeight: true }
                    SettingsPage { Layout.fillWidth: true; Layout.fillHeight: true }
                }
```

同时更新 `property int currentPage: 0` 上方注释为 `// 当前页面索引（0~4）`。

- [ ] **Step 3: QML lint 验证**

Run: `cmake --build build && ctest --test-dir build -R qml_lint_gate -V`
Expected: PASS（注意：此时 DashboardPage/TestPage/DataReplayPage 仍在 QML_FILES，未删除，lint 不会因引用缺失报错，因为 Main.qml 已不再引用它们）

> ⚠️ 若 qmllint 报“unused import”等，按提示修复。

- [ ] **Step 4: 冒烟启动验证（此时旧页面文件还在但未被引用）**

Run: `cmake --build build && ctest --test-dir build -R startup_smoke -V`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add qml/components/SideNavBar.qml qml/Main.qml
git commit -m "refactor(nav): merge navigation from 8 to 5 items"
```

---

### Task 10: 重写 MapPage 融合测试执行

**Files:**
- Modify: `qml/pages/MapPage.qml`

- [ ] **Step 1: 重写 MapPage.qml**

完整替换为（吸收 TestPage 的流程条、工况选择、操作按钮、急停，保留原地图 Canvas）：

```qml
// =============================================================================
// MapPage.qml
// 功能：场地地图页面（融合原测试执行页）。顶部状态流程条 + 左侧实时地图 +
//       右侧工况选择与操作按钮（加载/检查/预备/开始/停止/复位）+ 软件急停。
//       运行监控页已删除，全局状态由顶部状态栏与底部车辆平台卡承担。
// 数据源：appController.scenarioEngine / stateManager
// =============================================================================
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import ADASMaster

Item {
    id: page

    readonly property var ctrl: appController // qmllint disable unqualified
    readonly property var stm: page.ctrl.stateManager
    property real pixelsPerMeter: 4.0
    function w2sX(wx: real): real { return mapCanvas.width / 2 + wx * page.pixelsPerMeter }
    function w2sY(wy: real): real { return mapCanvas.height / 2 - wy * page.pixelsPerMeter }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacing

        // ---- 顶部：状态流程条（横跨全宽）----
        PanelFrame {
            Layout.fillWidth: true
            Layout.margins: Theme.spacing
            Layout.preferredHeight: 96
            title: "测试流程"
            titleEn: "state machine"
            liveText: page.stm.testState === 4 ? "运行中"
                    : (page.stm.testState === 3 ? "倒计时 " + page.stm.countdown + "s"
                    : (page.stm.result.length > 0 ? page.stm.result : ""))
            liveColor: page.stm.testState === 7 ? Theme.colorEmergency
                    : (page.stm.testState === 6 ? Theme.colorError
                    : (page.stm.testState === 4 ? Theme.colorNormal : Theme.colorAccent))

            StateFlow {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
                currentState: page.stm.testState
            }
        }

        // ---- 异常状态横幅 ----
        Rectangle {
            Layout.fillWidth: true
            Layout.margins: Theme.spacing
            Layout.preferredHeight: 36
            visible: page.stm.testState === 6 || page.stm.testState === 7
            color: page.stm.testState === 7 ? Theme.colorEmergency : Theme.colorError
            radius: Theme.radius
            Text {
                anchors.centerIn: parent
                text: page.stm.testState === 7
                      ? "⚠ 急停已激活：%1（请复位后继续）".arg(page.ctrl.safetyManager.emergencySource)
                      : "⚠ 故障状态：%1（请检查并复位）".arg(page.stm.result)
                color: "white"
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontNormal
                font.bold: true
            }
        }

        // ---- 主体：左地图 + 右操作栏 ----
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Theme.spacing
            spacing: Theme.spacing

            // 左：地图
            PanelFrame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: "场地地图"
                titleEn: "live map"
                liveText: "10Hz"
                liveColor: Theme.colorNormal

                Canvas {
                    id: mapCanvas
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Connections {
                        target: page.stm
                        function onStateChanged() { mapCanvas.requestPaint() }
                    }
                    Component.onCompleted: requestPaint()

                    onPaint: {
                        const ctx = getContext("2d");
                        ctx.reset();
                        const cx = mapCanvas.width / 2;
                        const cy = mapCanvas.height / 2;
                        const ppm = page.pixelsPerMeter;
                        const W = mapCanvas.width;
                        const H = mapCanvas.height;

                        // 背景网格
                        ctx.strokeStyle = Theme.colorDivider; ctx.lineWidth = 1;
                        for (let gx = cx % (20 * ppm / 4); gx < W; gx += 20 * (ppm / 4)) {
                            ctx.beginPath(); ctx.moveTo(gx, 0); ctx.lineTo(gx, H); ctx.stroke();
                        }
                        for (let gy = cy % (20 * ppm / 4); gy < H; gy += 20 * (ppm / 4)) {
                            ctx.beginPath(); ctx.moveTo(0, gy); ctx.lineTo(W, gy); ctx.stroke();
                        }
                        // 坐标轴
                        ctx.strokeStyle = Theme.colorBorder; ctx.lineWidth = 2;
                        ctx.beginPath(); ctx.moveTo(0, cy); ctx.lineTo(W, cy); ctx.stroke();
                        ctx.beginPath(); ctx.moveTo(cx, 0); ctx.lineTo(cx, H); ctx.stroke();
                        // 安全边界
                        ctx.strokeStyle = Theme.colorError; ctx.lineWidth = 2; ctx.setLineDash([8, 6]);
                        ctx.strokeRect(cx - 60 * ppm, cy - 60 * ppm, 120 * ppm, 120 * ppm);
                        ctx.setLineDash([]);
                        // 起点区域
                        ctx.fillStyle = Theme.colorNormal; ctx.globalAlpha = 0.25;
                        ctx.beginPath(); ctx.arc(cx - 50 * ppm, cy, 16, 0, 2 * Math.PI); ctx.fill();
                        ctx.globalAlpha = 1.0;
                        // 主车
                        const ex = page.w2sX(page.stm.egoX);
                        const ey = page.w2sY(page.stm.egoY);
                        ctx.fillStyle = Theme.colorAccent;
                        ctx.beginPath(); ctx.moveTo(ex, ey - 8); ctx.lineTo(ex - 6, ey + 6); ctx.lineTo(ex + 6, ey + 6); ctx.closePath(); ctx.fill();
                        // 目标物
                        const tx = page.w2sX(page.stm.tgtX);
                        const ty = page.w2sY(page.stm.tgtY);
                        ctx.fillStyle = page.stm.collisionRisk ? Theme.colorEmergency : Theme.colorWarning;
                        ctx.fillRect(tx - 7, ty - 7, 14, 14);
                        // 连线 + 距离
                        ctx.strokeStyle = Theme.colorTextMuted; ctx.lineWidth = 1;
                        ctx.beginPath(); ctx.moveTo(ex, ey); ctx.lineTo(tx, ty); ctx.stroke();
                        ctx.fillStyle = Theme.colorText; ctx.font = "12px sans-serif";
                        ctx.fillText(page.stm.distance.toFixed(1) + " m", (ex + tx) / 2 + 6, (ey + ty) / 2 - 6);
                    }
                }
            }

            // 右：工况选择 + 操作 + 急停
            ColumnLayout {
                Layout.preferredWidth: 180
                Layout.fillHeight: true
                spacing: Theme.spacing

                // 工况选择
                PanelFrame {
                    Layout.fillWidth: true
                    title: "工况选择"
                    titleEn: "scenario"

                    ComboBox {
                        id: scenarioCombo
                        Layout.fillWidth: true
                        model: page.ctrl.scenarioNames()
                        contentItem: Text { text: scenarioCombo.currentText; color: Theme.colorText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: Theme.colorHover; radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
                    }
                    Text {
                        text: "已加载：" + page.stm.scenarioName
                        color: Theme.colorTextDim
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSmall
                    }
                }

                // 操作按钮
                PanelFrame {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: "操作"
                    titleEn: "controls"

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingSm

                        Button {
                            id: btnLoad; text: "加载工况"
                            Layout.fillWidth: true; Layout.preferredHeight: 44
                            enabled: page.ctrl.operationalReady
                            contentItem: Text { text: btnLoad.text; color: Theme.colorText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: btnLoad.down ? Theme.colorAccentHover : (btnLoad.enabled ? Theme.colorHover : Theme.colorDisable); radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
                            onClicked: page.ctrl.scenarioEngine.loadScenario(scenarioCombo.currentText)
                        }
                        Button {
                            id: btnCheck; text: "检查设备"
                            Layout.fillWidth: true; Layout.preferredHeight: 44
                            enabled: page.ctrl.operationalReady && page.stm.testState === 0
                            contentItem: Text { text: btnCheck.text; color: Theme.colorText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: btnCheck.down ? Theme.colorAccentHover : (btnCheck.enabled ? Theme.colorHover : Theme.colorDisable); radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
                            onClicked: page.ctrl.scenarioEngine.checkDevices()
                        }
                        Button {
                            id: btnArm; text: "预备"
                            Layout.fillWidth: true; Layout.preferredHeight: 44
                            enabled: page.ctrl.operationalReady && page.stm.testState === 2
                            contentItem: Text { text: btnArm.text; color: Theme.colorText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: btnArm.down ? Theme.colorAccentHover : (btnArm.enabled ? Theme.colorHover : Theme.colorDisable); radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
                            onClicked: page.ctrl.scenarioEngine.arm()
                        }
                        Button {
                            id: btnStart; text: "开始"
                            Layout.fillWidth: true; Layout.preferredHeight: 48
                            enabled: page.ctrl.operationalReady && page.stm.testState === 3 && !page.ctrl.scenarioEngine.countdownActive
                            contentItem: Text { text: btnStart.text; color: Theme.colorNormal; font.family: Theme.fontFamily; font.pixelSize: Theme.fontMedium; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: btnStart.down ? Qt.darker(Theme.colorNormal, 1.2) : (btnStart.enabled ? Theme.colorHover : Theme.colorDisable); radius: Theme.radius; border.color: Theme.colorNormal; border.width: 2 }
                            onClicked: page.ctrl.scenarioEngine.start()
                        }
                        Button {
                            id: btnStop; text: "停止"
                            Layout.fillWidth: true; Layout.preferredHeight: 44
                            enabled: page.ctrl.operationalReady && page.stm.testState === 4
                            contentItem: Text { text: btnStop.text; color: Theme.colorWarning; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: btnStop.down ? Qt.darker(Theme.colorWarning, 1.2) : (btnStop.enabled ? Theme.colorHover : Theme.colorDisable); radius: Theme.radius; border.color: Theme.colorWarning; border.width: 2 }
                            onClicked: page.ctrl.scenarioEngine.stop()
                        }
                        Button {
                            id: btnReset; text: "复位"
                            Layout.fillWidth: true; Layout.preferredHeight: 44
                            enabled: page.stm.testState === 5 || page.stm.testState === 6 || page.stm.testState === 7
                            contentItem: Text { text: btnReset.text; color: Theme.colorText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: btnReset.down ? Theme.colorAccentHover : (btnReset.enabled ? Theme.colorHover : Theme.colorDisable); radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
                            onClicked: page.ctrl.scenarioEngine.reset()
                        }
                    }
                }

                // 急停
                EmergencyButton {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56
                    onClicked: page.ctrl.scenarioEngine.softwareEmergencyStop()
                }
            }
        }
    }
}
```

- [ ] **Step 2: QML lint 验证**

Run: `cmake --build build && ctest --test-dir build -R qml_lint_gate -V`
Expected: PASS

- [ ] **Step 3: 冒烟启动验证**

Run: `ctest --test-dir build -R startup_smoke -V`
Expected: PASS

- [ ] **Step 4: Commit**

```bash
git add qml/pages/MapPage.qml
git commit -m "feat(map): rewrite MapPage to merge test execution controls"
```

---

### Task 11: 新增 PathPreviewMap 组件

**Files:**
- Create: `qml/components/PathPreviewMap.qml`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 创建 PathPreviewMap.qml**

```qml
// =============================================================================
// PathPreviewMap.qml
// 功能：只读轨迹预览/回放 Canvas。两种模式：
//   - path 模式：接收 waypoints（QVariantList of {x,y,speed}），绘制航点连线与圆点。
//   - replay 模式：接收 samples（QVariantList of {egoX,egoY,targetX,targetY,...}），
//     由外部 Timer 驱动 replayIndex 递增，重绘主车/目标物历史位置与轨迹尾迹。
// 输入：mode("path"|"replay")、waypoints、samples、replayIndex。
// 输出：只读轨迹可视化。
// =============================================================================
import QtQuick
import ADASMaster

Canvas {
    id: canvas
    property string mode: "path"                 // "path" | "replay"
    property var waypoints: []                    // QVariantList of {x,y,speed}
    property var samples: []                      // QVariantList of sample maps
    property int replayIndex: 0                   // replay 模式当前帧
    property real pixelsPerMeter: 3.0

    onWaypointsChanged: requestPaint()
    onSamplesChanged: requestPaint()
    onReplayIndexChanged: requestPaint()
    Component.onCompleted: requestPaint()

    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        const cx = canvas.width / 2;
        const cy = canvas.height / 2;
        const ppm = canvas.pixelsPerMeter;
        const w2sX = (wx) => cx + wx * ppm;
        const w2sY = (wy) => cy - wy * ppm;

        // 背景网格
        ctx.strokeStyle = Theme.colorDivider; ctx.lineWidth = 1;
        for (let gx = cx % 20; gx < canvas.width; gx += 20) {
            ctx.beginPath(); ctx.moveTo(gx, 0); ctx.lineTo(gx, canvas.height); ctx.stroke();
        }
        for (let gy = cy % 20; gy < canvas.height; gy += 20) {
            ctx.beginPath(); ctx.moveTo(0, gy); ctx.lineTo(canvas.width, gy); ctx.stroke();
        }
        // 坐标轴
        ctx.strokeStyle = Theme.colorBorder; ctx.lineWidth = 1;
        ctx.beginPath(); ctx.moveTo(0, cy); ctx.lineTo(canvas.width, cy); ctx.stroke();
        ctx.beginPath(); ctx.moveTo(cx, 0); ctx.lineTo(cx, canvas.height); ctx.stroke();

        if (canvas.mode === "path") {
            // 航点连线（黄色虚线）+ 圆点
            const pts = canvas.waypoints;
            if (pts.length >= 2) {
                ctx.strokeStyle = Theme.colorWarning; ctx.lineWidth = 2; ctx.setLineDash([6, 4]);
                ctx.beginPath();
                ctx.moveTo(w2sX(pts[0].x), w2sY(pts[0].y));
                for (let i = 1; i < pts.length; ++i) ctx.lineTo(w2sX(pts[i].x), w2sY(pts[i].y));
                ctx.stroke();
                ctx.setLineDash([]);
            }
            ctx.fillStyle = Theme.colorWarning;
            for (let i = 0; i < pts.length; ++i) {
                ctx.beginPath(); ctx.arc(w2sX(pts[i].x), w2sY(pts[i].y), 5, 0, 2 * Math.PI); ctx.fill();
            }
            // 序号
            ctx.fillStyle = Theme.colorText; ctx.font = "10px monospace";
            for (let i = 0; i < pts.length; ++i) {
                ctx.fillText(String(i + 1), w2sX(pts[i].x) + 7, w2sY(pts[i].y) - 7);
            }
        } else {
            // replay 模式：绘制截至 replayIndex 的轨迹
            const sm = canvas.samples;
            const end = Math.min(canvas.replayIndex, sm.length);
            // 主车轨迹尾迹（蓝）
            ctx.strokeStyle = Theme.colorAccent; ctx.lineWidth = 1.5;
            ctx.beginPath();
            for (let i = 0; i < end; ++i) {
                const x = w2sX(sm[i].egoX), y = w2sY(sm[i].egoY);
                if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
            }
            ctx.stroke();
            // 目标轨迹尾迹（黄）
            ctx.strokeStyle = Theme.colorWarning;
            ctx.beginPath();
            for (let i = 0; i < end; ++i) {
                const x = w2sX(sm[i].targetX), y = w2sY(sm[i].targetY);
                if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
            }
            ctx.stroke();
            // 当前主车/目标位置
            if (end > 0) {
                const last = sm[end - 1];
                const ex = w2sX(last.egoX), ey = w2sY(last.egoY);
                const tx = w2sX(last.targetX), ty = w2sY(last.targetY);
                ctx.fillStyle = Theme.colorAccent;
                ctx.beginPath(); ctx.moveTo(ex, ey - 7); ctx.lineTo(ex - 5, ey + 5); ctx.lineTo(ex + 5, ey + 5); ctx.closePath(); ctx.fill();
                ctx.fillStyle = last.collisionRisk ? Theme.colorEmergency : Theme.colorWarning;
                ctx.fillRect(tx - 6, ty - 6, 12, 12);
            }
        }
    }
}
```

- [ ] **Step 2: 登记到 CMakeLists.txt 的 QML_FILES**

在 `qml/components/VehiclePlatformStatusMonitor.qml` 之后加：

```
        qml/components/PathPreviewMap.qml
```

- [ ] **Step 3: 构建并 lint**

Run: `cmake --build build && ctest --test-dir build -R qml_lint_gate -V`
Expected: PASS

- [ ] **Step 4: Commit**

```bash
git add qml/components/PathPreviewMap.qml CMakeLists.txt
git commit -m "feat(qml): add PathPreviewMap component for path/replay visualization"
```

---

### Task 12: 新增 WaypointEditor 组件

**Files:**
- Create: `qml/components/WaypointEditor.qml`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 创建 WaypointEditor.qml**

```qml
// =============================================================================
// WaypointEditor.qml
// 功能：航点表格编辑器。四列（序号/X/Y/速度），行尾删除按钮，
//       底部"添加航点"与"保存"按钮。编辑中维护本地副本，保存时发 pathChanged。
// 输入：waypoints（QVariantList of {x,y,speed}）。
// 输出：pathChanged(var newPath) 信号。
// =============================================================================
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import ADASMaster

pragma ComponentBehavior: Bound

Item {
    id: root
    property var waypoints: []        // QVariantList of {x,y,speed}
    signal pathChanged(var newPath)

    // 本地可编辑副本
    property var editing: []

    function syncFromInput() {
        const copy = [];
        for (let i = 0; i < root.waypoints.length; ++i) {
            copy.push({ x: root.waypoints[i].x, y: root.waypoints[i].y, speed: root.waypoints[i].speed });
        }
        root.editing = copy;
    }
    Component.onCompleted: syncFromInput()
    onWaypointsChanged: syncFromInput()

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacingSm

        // 表头
        RowLayout {
            Layout.fillWidth: true
            spacing: 0
            Rectangle { Layout.preferredWidth: 28; height: 24; color: Theme.colorTitleBar; Text { anchors.centerIn: parent; text: "#"; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall } }
            Rectangle { Layout.fillWidth: true; height: 24; color: Theme.colorTitleBar; Text { anchors.centerIn: parent; text: "X (m)"; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall } }
            Rectangle { Layout.fillWidth: true; height: 24; color: Theme.colorTitleBar; Text { anchors.centerIn: parent; text: "Y (m)"; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall } }
            Rectangle { Layout.fillWidth: true; height: 24; color: Theme.colorTitleBar; Text { anchors.centerIn: parent; text: "速度 (m/s)"; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall } }
            Rectangle { Layout.preferredWidth: 36; height: 24; color: Theme.colorTitleBar }
        }

        // 行列表
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.editing.length
            spacing: 1
            delegate: RowLayout {
                id: row
                required property int index
                spacing: 0

                Rectangle {
                    Layout.preferredWidth: 28; Layout.fillHeight: true
                    color: Theme.colorPanelAlt
                    Text { anchors.centerIn: parent; text: String(row.index + 1); color: Theme.colorTextDim; font.family: Theme.fontNumeric; font.pixelSize: Theme.fontSmall }
                }
                TextField {
                    Layout.fillWidth: true
                    text: root.editing[row.index].x.toFixed(1)
                    font.family: Theme.fontNumeric; font.pixelSize: Theme.fontSmall; color: Theme.colorText
                    background: Rectangle { color: Theme.colorPanelAlt; border.color: Theme.colorBorder; border.width: 1 }
                    onTextEdited: { root.editing[row.index].x = parseFloat(text) || 0; }
                }
                TextField {
                    Layout.fillWidth: true
                    text: root.editing[row.index].y.toFixed(1)
                    font.family: Theme.fontNumeric; font.pixelSize: Theme.fontSmall; color: Theme.colorText
                    background: Rectangle { color: Theme.colorPanelAlt; border.color: Theme.colorBorder; border.width: 1 }
                    onTextEdited: { root.editing[row.index].y = parseFloat(text) || 0; }
                }
                TextField {
                    Layout.fillWidth: true
                    text: root.editing[row.index].speed.toFixed(1)
                    font.family: Theme.fontNumeric; font.pixelSize: Theme.fontSmall; color: Theme.colorText
                    background: Rectangle { color: Theme.colorPanelAlt; border.color: Theme.colorBorder; border.width: 1 }
                    onTextEdited: { root.editing[row.index].speed = parseFloat(text) || 0; }
                }
                Button {
                    Layout.preferredWidth: 36
                    text: "×"
                    contentItem: Text { text: "×"; color: Theme.colorEmergency; font.bold: true; font.pixelSize: Theme.fontMedium; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: parent.down ? Theme.colorHover : Theme.colorPanelAlt }
                    onClicked: {
                        const copy = root.editing.slice();
                        copy.splice(row.index, 1);
                        root.editing = copy;
                    }
                }
            }
        }

        Text {
            visible: root.editing.length === 0
            text: "暂无航点，点击下方添加"
            color: Theme.colorTextDim
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall
            Layout.alignment: Qt.AlignCenter
        }

        // 底部按钮
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSm
            Button {
                text: "+ 添加航点"
                Layout.preferredHeight: 32
                contentItem: Text { text: "+ 添加航点"; color: Theme.colorText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: parent.down ? Theme.colorAccentHover : Theme.colorHover; radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
                onClicked: {
                    const copy = root.editing.slice();
                    const last = copy.length > 0 ? copy[copy.length - 1] : { x: 0, y: 0, speed: 0 };
                    copy.push({ x: last.x, y: last.y, speed: last.speed });
                    root.editing = copy;
                }
            }
            Item { Layout.fillWidth: true }
            Button {
                id: btnSave
                text: "保存"
                Layout.preferredHeight: 32
                contentItem: Text { text: "保存"; color: Theme.colorNormal; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: btnSave.down ? Qt.darker(Theme.colorNormal, 1.2) : Theme.colorHover; radius: Theme.radius; border.color: Theme.colorNormal; border.width: 2 }
                onClicked: root.pathChanged(root.editing)
            }
        }
    }
}
```

- [ ] **Step 2: 登记到 CMakeLists.txt 的 QML_FILES**

在 `qml/components/PathPreviewMap.qml` 之后加：

```
        qml/components/WaypointEditor.qml
```

- [ ] **Step 3: 构建并 lint**

Run: `cmake --build build && ctest --test-dir build -R qml_lint_gate -V`
Expected: PASS

- [ ] **Step 4: Commit**

```bash
git add qml/components/WaypointEditor.qml CMakeLists.txt
git commit -m "feat(qml): add WaypointEditor component for waypoint table editing"
```

---

### Task 13: 重写 ScenarioPage 融合数据回放 + 航点编辑

**Files:**
- Modify: `qml/pages/ScenarioPage.qml`

- [ ] **Step 1: 重写 ScenarioPage.qml**

```qml
// =============================================================================
// ScenarioPage.qml
// 功能：工况管理页面（融合原数据回放页）。左侧工况列表 + 右上路径编辑
//       （航点表格 + 地图预览）+ 右下该工况历史回放记录。选中记录可轨迹回放。
// 数据源：appController.scenarioNames/scenarioWaypoints/runSamples、replayListModel
// =============================================================================
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import ADASMaster

pragma ComponentBehavior: Bound

Item {
    id: page
    readonly property var ctrl: appController // qmllint disable unqualified

    property int selected: 0
    property var scenarioNames: page.ctrl.scenarioNames()
    property var currentWaypoints: []
    property string currentReplayRunId: ""
    property var currentSamples: []
    property int replayIndex: 0
    property bool replayPlaying: false

    // 选中工况变化时加载航点
    function loadSelectedWaypoints() {
        if (page.scenarioNames.length === 0) { page.currentWaypoints = []; return; }
        const name = page.scenarioNames[page.selected];
        page.currentWaypoints = page.ctrl.scenarioWaypoints(name);
        page.ctrl.replayListModel.filterByScenario(name);
        // 切换工况时清空回放
        page.currentReplayRunId = "";
        page.currentSamples = [];
        page.replayIndex = 0;
        page.replayPlaying = false;
        replayTimer.stop();
        pathPreview.mode = "path";
    }

    // 选中某条记录后加载采样
    function loadRunSamples(runId: string) {
        page.currentReplayRunId = runId;
        page.currentSamples = page.ctrl.runSamples(runId);
        page.replayIndex = 0;
        page.replayPlaying = page.currentSamples.length > 0;
        pathPreview.mode = "replay";
        if (page.replayPlaying) replayTimer.start();
    }

    Component.onCompleted: loadSelectedWaypoints()

    Timer {
        id: replayTimer
        interval: 100   // 10Hz，与采样频率一致
        repeat: true
        onTriggered: {
            if (page.replayIndex < page.currentSamples.length - 1) {
                page.replayIndex++;
            } else {
                page.replayPlaying = false;
                replayTimer.stop();
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: Theme.spacing

        // 左：工况列表
        PanelFrame {
            Layout.fillHeight: true
            Layout.preferredWidth: 220
            title: "工况列表"
            titleEn: "scenarios"

            ListView {
                id: scenarioList
                Layout.fillWidth: true; Layout.fillHeight: true
                model: page.scenarioNames.length
                spacing: Theme.spacingSm
                delegate: Rectangle {
                    id: scenarioItem
                    required property int index
                    width: scenarioList.width; height: 52
                    radius: Theme.radius
                    color: page.selected === scenarioItem.index ? Theme.colorAccent : Theme.colorPanelAlt
                    border.color: Theme.colorBorder; border.width: 1
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: { page.selected = scenarioItem.index; page.loadSelectedWaypoints(); }
                    }
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: Theme.spacing; spacing: 2
                        Text {
                            text: page.scenarioNames[scenarioItem.index] || ""
                            color: page.selected === scenarioItem.index ? "white" : Theme.colorText
                            font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; font.bold: true
                        }
                        Text {
                            text: page.ctrl.scenarioWaypoints(page.scenarioNames[scenarioItem.index]).length + " 航点"
                            color: page.selected === scenarioItem.index ? "#cccccc" : Theme.colorTextDim
                            font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall
                        }
                    }
                }
            }
        }

        // 右：上下分区
        ColumnLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            spacing: Theme.spacing

            // 右上：路径编辑 + 地图预览
            PanelFrame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: parent.height * 0.55
                title: page.scenarioNames.length > 0 ? page.scenarioNames[page.selected] + " · 路径" + (page.currentReplayRunId.length > 0 ? "（回放中）" : "") : "路径"
                titleEn: page.currentReplayRunId.length > 0 ? "replay" : "waypoints"

                RowLayout {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    spacing: Theme.spacing

                    WaypointEditor {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        waypoints: page.currentWaypoints
                        onPathChanged: (newPath) => {
                            page.currentWaypoints = newPath;
                            pathPreview.waypoints = newPath;
                        }
                    }

                    PathPreviewMap {
                        id: pathPreview
                        Layout.fillWidth: true; Layout.fillHeight: true
                        Layout.preferredWidth: parent.width * 0.5
                        mode: "path"
                        waypoints: page.currentWaypoints
                        samples: page.currentSamples
                        replayIndex: page.replayIndex
                    }
                }

                // 回放控件（仅回放模式显示）
                RowLayout {
                    Layout.fillWidth: true
                    visible: page.currentReplayRunId.length > 0
                    spacing: Theme.spacingSm
                    Button {
                        id: btnPlay
                        text: page.replayPlaying ? "暂停" : "播放"
                        Layout.preferredHeight: 32
                        enabled: page.currentSamples.length > 0
                        contentItem: Text { text: btnPlay.text; color: Theme.colorNormal; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: btnPlay.down ? Qt.darker(Theme.colorNormal, 1.2) : Theme.colorHover; radius: Theme.radius; border.color: Theme.colorNormal; border.width: 1 }
                        onClicked: {
                            if (page.replayPlaying) { page.replayPlaying = false; replayTimer.stop(); }
                            else {
                                if (page.replayIndex >= page.currentSamples.length - 1) page.replayIndex = 0;
                                page.replayPlaying = true; replayTimer.start();
                            }
                        }
                    }
                    Text {
                        text: page.currentSamples.length > 0 ? "帧 %1 / %2".arg(page.replayIndex + 1).arg(page.currentSamples.length) : "无采样数据"
                        color: Theme.colorTextDim
                        font.family: Theme.fontNumeric; font.pixelSize: Theme.fontSmall
                    }
                    Item { Layout.fillWidth: true }
                    Button {
                        text: "退出回放"
                        Layout.preferredHeight: 32
                        contentItem: Text { text: "退出回放"; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: Theme.colorHover; radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
                        onClicked: {
                            page.currentReplayRunId = ""; page.currentSamples = [];
                            page.replayIndex = 0; page.replayPlaying = false;
                            replayTimer.stop(); pathPreview.mode = "path";
                        }
                    }
                }
            }

            // 右下：数据回放记录
            PanelFrame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: parent.height * 0.45
                title: "数据回放"
                titleEn: "replay"
                liveText: "共 %1 条".arg(page.ctrl.replayListModel.count)
                liveColor: Theme.colorInfo

                ColumnLayout {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    spacing: 1

                    // 表头
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 30
                        color: Theme.colorTitleBar
                        RowLayout {
                            anchors.fill: parent; anchors.leftMargin: Theme.spacing; anchors.rightMargin: Theme.spacing
                            spacing: 0
                            Text { text: "开始时间"; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; Layout.preferredWidth: 150 }
                            Text { text: "状态"; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; Layout.preferredWidth: 90 }
                            Text { text: "结果"; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; Layout.fillWidth: true }
                            Text { text: "操作员"; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; Layout.preferredWidth: 80 }
                        }
                    }

                    ListView {
                        id: recordList
                        Layout.fillWidth: true; Layout.fillHeight: true
                        clip: true
                        model: page.ctrl.replayListModel
                        spacing: 1
                        delegate: Rectangle {
                            id: recordRow
                            required property int index
                            required property string runId
                            required property string startedAt
                            required property string runStatus
                            required property string resultText
                            required property string operatorName

                            width: recordList.width; height: 36
                            color: page.currentReplayRunId === recordRow.runId ? Theme.colorAccent : (recordRow.index % 2 === 0 ? Theme.colorPanelAlt : Theme.colorBg)
                            border.color: Theme.colorBorder; border.width: page.currentReplayRunId === recordRow.runId ? 2 : 0
                            MouseArea {
                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onClicked: page.loadRunSamples(recordRow.runId)
                            }
                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: Theme.spacing; anchors.rightMargin: Theme.spacing
                                spacing: 0
                                Text { text: recordRow.startedAt; color: page.currentReplayRunId === recordRow.runId ? "white" : Theme.colorText; font.family: Theme.fontNumeric; font.pixelSize: Theme.fontSmall; Layout.preferredWidth: 150 }
                                Text {
                                    text: recordRow.runStatus
                                    color: page.currentReplayRunId === recordRow.runId ? "white" : (recordRow.runStatus === "EMERGENCY" || recordRow.runStatus === "FAULT" ? Theme.colorEmergency : (recordRow.runStatus === "RUNNING" ? Theme.colorWarning : Theme.colorNormal))
                                    font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; font.bold: true; Layout.preferredWidth: 90
                                }
                                Text { text: recordRow.resultText.length > 0 ? recordRow.resultText : "—"; color: page.currentReplayRunId === recordRow.runId ? "#cccccc" : Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; elide: Text.ElideRight; Layout.fillWidth: true }
                                Text { text: recordRow.operatorName; color: page.currentReplayRunId === recordRow.runId ? "#cccccc" : Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; Layout.preferredWidth: 80 }
                            }
                        }
                    }

                    Text {
                        visible: page.ctrl.replayListModel.count === 0
                        text: "该工况暂无测试记录"
                        color: Theme.colorTextDim
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall
                        Layout.alignment: Qt.AlignCenter
                    }
                }
            }
        }
    }
}
```

- [ ] **Step 2: 检查 ReplayListModel role 名**

Run: `grep "roleNames" src/persistence/ReplayListModel.cpp`
确认 `runId` role 存在（Step 1 中 delegate 用了 `required property string runId`）。现有 roleNames 已含 `{RunIdRole, "runId"}`，匹配。

- [ ] **Step 3: 构建并 lint**

Run: `cmake --build build && ctest --test-dir build -R qml_lint_gate -V`
Expected: PASS

- [ ] **Step 4: Commit**

```bash
git add qml/pages/ScenarioPage.qml
git commit -m "feat(scenario): rewrite ScenarioPage to merge replay + waypoint editing"
```

---

## 阶段 6 · 清理与全量验证

### Task 14: 删除废弃页面并更新 CMakeLists

**Files:**
- Delete: `qml/pages/DashboardPage.qml`
- Delete: `qml/pages/TestPage.qml`
- Delete: `qml/pages/DataReplayPage.qml`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 从 CMakeLists.txt 的 QML_FILES 移除三个页面**

删除这三行：

```
        qml/pages/DashboardPage.qml
        qml/pages/TestPage.qml
        qml/pages/DataReplayPage.qml
```

- [ ] **Step 2: 删除三个 QML 文件**

Run:
```bash
rm qml/pages/DashboardPage.qml qml/pages/TestPage.qml qml/pages/DataReplayPage.qml
```

- [ ] **Step 3: 构建验证**

Run: `cmake --build build`
Expected: 构建成功（无残留引用）

- [ ] **Step 4: 全量测试**

Run: `ctest --test-dir build --output-on-failure`
Expected: 全部 PASS（注意 qml_lint_gate、status_monitor_gate、login_gate、startup_smoke）

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "refactor: remove obsolete Dashboard/Test/DataReplay pages"
```

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
## 🗑️ 删除操作摘要
**删除目标**: qml/pages/DashboardPage.qml、qml/pages/TestPage.qml、qml/pages/DataReplayPage.qml
**删除原因**: 页面合并后这三个页面已被 MapPage/ScenarioPage 吸收，不再需要独立文件
**影响范围**: 信息架构精简（导航 8→5），CMakeLists QML_FILES 同步移除
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

---

### Task 15: 手动冒烟验证与最终全量回归

**Files:** 无（验证任务）

- [ ] **Step 1: 启动应用**

Run: `./build/ADASMaster.exe`
手动验证：
- 登录后左侧导航显示 5 项（设备管理/场地地图/工况管理/报警中心/系统设置）
- 点击"场地地图"：顶部状态流程条、左侧地图、右侧工况选择+操作按钮+急停
- 点击"工况管理"：左侧工况列表（3 项）、右上航点表格+预览、右下历史记录
- 选中 AEB 工况 → 右上显示 1 航点
- 选中 RCTA 工况 → 右上显示 3 航点，预览有轨迹线
- 选中 LKA 工况 → 航点表格空（向后兼容验证）

- [ ] **Step 2: 运行一次完整测试（验证航点插值）**

在场地地图页：加载工况（AEB CCRs 50）→ 检查设备 → 预备 → 开始 → 观察地图目标物行为（AEB 目标静止）。停止后回到工况管理页查看新记录。

- [ ] **Step 3: 验证轨迹回放**

在工况管理页选中刚生成的记录 → 右上切换回放模式 → 播放 → 观察主车/目标物历史轨迹。

- [ ] **Step 4: 最终全量回归测试**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: 全部 PASS（10 个 C++ 测试 + 4 个 PowerShell gate）

- [ ] **Step 5: 最终 Commit（如有手动验证发现的修复）**

```bash
git add -A
git commit -m "test: verify page merge end-to-end"
```

（如无修复则跳过此 commit）

---

## 完成标准

- [ ] 导航 5 项，三处合并完成（运行监控删除、场地地图含测试操作、工况管理含数据回放）
- [ ] 航点数据结构、解析、引擎插值运动学全部就绪
- [ ] 工况管理页航点表格编辑 + 地图预览 + 该工况历史记录 + 轨迹回放可用
- [ ] 向后兼容：LKA 无航点仍正常运行
- [ ] 全量 `ctest` 通过（含 qml_lint_gate、startup_smoke）
- [ ] 手动冒烟：导航、地图、工况编辑、回放均正常
