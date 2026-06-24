# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目简介

ADASMaster 是 ADAS（高级驾驶辅助系统）场地测试主控系统的工控上位机，统一调度驾驶机器人 / 目标车 / RTK / 安全系统，执行 NCAP/ADAS 测试工况。

技术栈：**C++17 + Qt6 (Quick/Qml) + 手写 QML + CMake + Ninja**。当前为**第二阶段**：仍全部使用 `FakeDevice` 模拟设备，但工况参数已由 `ScenarioParser` 从 `scenarios/*.json` 加载，并由 `NcapEvaluator` 按 C-NCAP 2024 / Euro NCAP 规程评定 PASS/FAIL。

## 构建与运行

环境（来自 `build/CMakeCache.txt`）：Qt 6.11.1 (MinGW_64) + MinGW g++（`C:/Qt/Tools/mingw1310_64`）+ Ninja。构建目录 `build/` 已配置完成，通常**增量构建即可**：

```bash
# 增量构建（最常用）
cmake --build build            # 或 ninja -C build

# 首次/重新配置（仅在 CMakeLists.txt 变化或清空 build 时）
cmake -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.11.1/mingw_64 -DCMAKE_BUILD_TYPE=Debug

# 运行
./build/ADASMaster.exe
```

> `CMakeLists.txt` 中 `WIN32_EXECUTABLE TRUE` 使程序无控制台窗口。调试时若要看 `qWarning` 输出，临时改为 `FALSE` 重新构建。

本项目使用 **QtTest** 单元测试框架 + **qmllint** 静态检查门禁：

```bash
# 构建并运行全部测试（ctest）
export PATH="/c/Qt/6.11.1/mingw_64/bin:/c/Qt/Tools/mingw1310_64/bin:$PATH"
cmake --build build
cd build && ctest --output-on-failure
# 注：含 GUI 的 *_gate 测试需要桌面环境，CI/无头环境用 -E 排除
cd build && ctest --output-on-failure -E "login_gate|startup_smoke|scenario_management_gate|status_monitor_gate"
```

> 运行单个 .exe 测试需 Qt DLL 在 PATH，工作目录需在源码根（测试用相对路径 `scenarios/...`）。

## 核心架构

系统是严格的分层结构，**QML 永远只通过 `appController` 访问 C++ 后端**，绝不直接接触设备对象或通信协议。

```
main.cpp
  └─ AppController  ← 唯一的 QML/C++ 桥梁（context property: "appController"）
       ├─ DeviceManager   设备增删/连接/聚合，通过 DeviceListModel 暴露给 QML
       │    └─ IDevice (抽象) ─ FakeDevice (当前) / 未来真实协议客户端
       ├─ SafetyManager   安全大脑：三级安全等级 + 三种急停源(硬/软/无线)
       ├─ ScenarioEngine  工况执行引擎，驱动测试状态机与运动学模拟
       ├─ AlarmManager    事件统一入口，写入 AlarmListModel
       └─ StateManager    实时状态中央仓库（只存状态，不做决策）
```

**几个关键的跨文件协作**（理解这些就理解了系统）：

- **依赖注入**：`AppController` 构造函数创建 5 个 Manager 后，通过 `ScenarioEngine::setXxxManager()` 把 Device/Safety/State/Alarm 注入引擎。新增 Manager 间依赖时在此处接线。
- **两套时钟**：`AppController` 的 `m_deviceTimer`（1Hz）驱动设备刷新；`ScenarioEngine::m_tickTimer`（10Hz）驱动测试运行时的运动学模拟。
- **安全 → 场景的短路**：`SafetyManager::emergencyTriggered` 直接连到 `ScenarioEngine::onSafetyEmergency`，急停时无需 QML 介入即停止所有设备与测试。
- **设备在线数 → 安全降级**：`DeviceManager::onlineCountChanged`（<4 台）触发 `SafetyManager::onCommunicationDegraded`。
- **状态分层原则**：`StateManager` 是纯数据仓库（被 QML 绑定显示），所有**决策**在 `ScenarioEngine` / `SafetyManager` 中完成，再写入 `StateManager`。

## 数据结构约定

- **枚举**（`SafetyLevel` / `TestState` / `AlarmLevel` / `AlarmSource`）通过 `static_cast<int>` 暴露给 QML，`Theme.levelColor(0..3)` 按等级映射颜色。安全/测试等级颜色**必须遵守工控约定**：`0=绿 1=黄 2=橙 3=红`（见 `qml/theme/Theme.qml`）。
- `DeviceStatus` 用 `Q_GADGET`（非 QObject）+ `Q_PROPERTY MEMBER`，可在 C++/QML 间作为 `QVariant` 传递并直接用于 ListModel。
- `DeviceListModel` 是 `QAbstractListModel`，QML 通过 role 名（`NameRole`→`name` 等）只读访问设备，不直接持有设备对象。

## QML UI 结构

`Main.qml` 用 `StackLayout` 按 `currentPage` 索引（0~7）切换 8 个一级页面，顺序与 `SideNavBar` 一致：Dashboard / Device / Map / Scenario / Test / DataReplay / Alarm / Settings。所有视觉规范（颜色、字号、间距、圆角）集中在 `Theme.qml` 单例，**新增样式优先复用 `Theme.*`，禁止硬编码颜色值**。

## 视觉设计规范（B 方向 · 指挥控制中心）

视觉语言定义在 `docs/superpowers/specs/2026-06-18-adasmaster-visual-design.md`，实现计划见 `docs/superpowers/plans/2026-06-18-adasmaster-visual-redesign.md`。核心要点：

- 配色集中在 `Theme.qml`：深蓝灰背景层级 `#12161d`(bg)/`#1b212b`(panel)/`#161a22`(panelAlt) + 工业蓝主色 `#2d6a9f` + 绿/黄/橙/红状态色 + 标题栏色 `colorTitleBar`/`colorTitleBarActive`。
- 所有信息区用 **`PanelFrame`**（标题栏面板：左 3px 蓝竖条 + 标签 + 右侧可选实时指示）作为统一容器；活跃面板设 `accent: true`（蓝边框 + 深蓝标题栏）。
- 复用组件：`StatusCard`（状态卡）、`MetricCell`（指标格，等宽数值）、`StateFlow`（状态机流程条）、`Icon`/`IconButton`（线性图标）。
- 字体：界面文字用 `Theme.fontFamily`（微软雅黑/Segoe UI）；**所有数值用 `Theme.fontNumeric`**（等宽，刷新不跳动）。
- 图标：Lucide 白色描边 SVG 在 `qml/icons/`，通过 `opacity` 控制亮/暗（本机 Qt 无 `QtQuick.Effects`/MultiEffect，故不着色）；资源前缀 `qrc:/qt/qml/ADASMaster/qml/icons/`；新增图标须登记 CMake `RESOURCES`。
- 状态色严格 `0=绿 1=黄 2=橙 3=红`，复用 `Theme.levelColor(level)`；急停最高级，全应用红色叠加 + 锁定操作。

## 必须遵守的构建约束（易踩坑）

1. **新增任何 C++ 源/头文件**（含 `Q_OBJECT`/`Q_GADGET`/`Q_NAMESPACE`）**必须登记到 `CMakeLists.txt` 的 `qt_add_executable` 列表**，否则 AUTOMOC 不生成元对象代码，QML 运行时找不到类型。
2. **新增 `.qml` 文件必须登记到 `qt_add_qml_module` 的 `QML_FILES`**，否则编译注册后运行时无法加载。
3. **新增 `config/*.json` 或 `scenarios/*.json` 必须登记到 `RESOURCES`**。
4. `Theme.qml` 通过 `set_source_files_properties(... QT_QML_SINGLETON_TYPE TRUE)` 显式声明为单例——保留此设置。
5. QML/C++ 后端交互统一用 `Q_PROPERTY` + `NOTIFY` 信号 + `Q_INVOKABLE` + `QAbstractListModel`，遵循现有 Manager 的写法。

## 代码规范

遵循用户全局约定（与现有代码一致）：**代码用英文，注释用中文**；每个 C++/QML 文件开头必须有块注释，说明**功能/用途、输入、输出**（工具类说明用途即可）。参考现有头文件的注释格式。
