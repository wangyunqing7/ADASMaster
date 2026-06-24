# 页面合并设计 · 运行监控/场地地图/工况管理/数据回放

- **日期**：2026-06-20
- **状态**：已定稿 / 待实现
- **范围**：信息架构重排（导航 8→5）+ 两处页面合并 + 航点路径数据结构 + 轨迹回放
- **视觉规范**：沿用 `docs/superpowers/specs/2026-06-18-adasmaster-visual-design.md`（B 方向 · 指挥控制中心），不新增 Theme token

## 1. 目标与动机

当前 ADASMaster 有一级导航 8 项，存在两处职责割裂：

1. **运行监控页（Dashboard）冗余**：顶部状态栏已有全局安全/测试/设备状态，底部 `VehiclePlatformStatusMonitor` 已有主车/目标车平台状态。运行监控页重复展示同类信息。
2. **场地地图与测试操作分离**：操作员执行测试时需在「场地地图」看实时位置、「测试执行」点操作按钮，频繁切换。
3. **工况管理与数据回放割裂**：查看某工况历史回放需先去工况管理找名称，再去数据回放翻列表过滤。
4. **工况只有单点目标**：`ScenarioConfig` 仅支持目标物单一起点 + 恒定速度，无法描述"目标物按路径行进"的真实工况。

本次将 8 个页面合并为 5 个，并新增航点路径数据结构与轨迹回放能力。

## 2. 信息架构（导航 8→5）

| 序号 | 新导航 | 来源 | 说明 |
|---|---|---|---|
| 0 | 设备管理 | `DevicePage` | 不变 |
| 1 | **场地地图** | `MapPage` + `TestPage` 合并 | 地图实时显示 + 工况选择 + 操作按钮 + 状态流程条 |
| 2 | **工况管理** | `ScenarioPage` + `DataReplayPage` 合并 | 工况列表 + 路径编辑（航点表格+地图预览）+ 该工况历史回放 + 轨迹回放 |
| 3 | 报警中心 | `AlarmPage` | 不变 |
| 4 | 系统设置 | `SettingsPage` | 不变 |

**删除的页面**：

- `DashboardPage`（运行监控）——彻底删除。全局状态已在顶部状态栏，车辆状态已在底部 `VehiclePlatformStatusMonitor`。
- `TestPage`（测试执行）——其内容（状态流程条 + 工况选择 + 6 操作按钮 + 急停）全部迁移到场地地图页。
- `DataReplayPage`（数据回放）——其内容（历史记录列表）迁移到工况管理页右下，并增加按工况过滤 + 点击轨迹回放。

**`Main.qml` 改动**：`StackLayout` 从 8 个子项变 5 个；`SideNavBar.menus` 从 8 项变 5 项；`currentPage` 索引范围 0~4。

## 3. 场地地图页详细设计

**文件**：`qml/pages/MapPage.qml`（重写，吸收原 TestPage 内容）

### 3.1 布局骨架（A1 · 垂直堆叠）

页面整体为 `ColumnLayout`，自上而下三个 `PanelFrame`：

```
┌─────────────────────────────────────────────────────────────┐
│ 顶部：状态流程条（横跨全宽，高约 50px）                          │
│ ●Idle ─ ○Check ─ ○Ready ─ ○Armed ─ ○Running    倒计时: 3s   │
├──────────────────────────────────────────┬──────────────────┤
│                                          │  工况选择          │
│           场地地图（Canvas）              │  [AEB CCRs 50 ▾] │
│                                          │ ─────────────────│
│      ▲ 主车         ■ 目标物              │  操作             │
│                                          │  [加载工况]       │
│      图例(左上)    坐标(右下)             │  [检查设备]       │
│                                          │  [预备]           │
│                                          │  [▶ 开始] (绿)    │
│                                          │  [■ 停止] (黄)    │
│                                          │ ─────────────────│
│                                          │  [⚠ 软件急停] (红) │
└──────────────────────────────────────────┴──────────────────┘
```

### 3.2 三个区块

1. **顶部流程条面板**（高 50px，横跨全宽）
   - 复用现有 `StateFlow` 组件，`currentState` 绑定 `stateManager.testState`。
   - 右侧 `liveText` 区显示倒计时（`countdown > 0` 时）/ 结果文本（Finished/Fault/Emergency 时）。
   - 异常状态横幅（原 TestPage 的红/橙提示条）并入此区，`testState === 6/7` 时显示。

2. **左侧地图区**（`Layout.fillWidth: true`，占剩余宽度）
   - 沿用现有 Canvas 绘制：背景网格、坐标轴、安全边界、起点区域、主车三角、目标物方块、连线、距离标注。
   - 左上角图例、右下角坐标显示保留。
   - `Connections` 监听 `stm.stateChanged` 触发重绘。

3. **右侧操作栏**（`Layout.preferredWidth: 170`，固定）
   - **工况选择区**：`ComboBox`（model 来自工况列表）+ "已加载：xxx" 文本。
   - **操作区**：6 个按钮（加载/检查/预备/开始/停止 + 复位），`enabled` 门控逻辑完全沿用原 TestPage：
     - 加载：`operationalReady`
     - 检查：`operationalReady && testState === 0`
     - 预备：`operationalReady && testState === 2`
     - 开始：`operationalReady && testState === 3 && !countdownActive`
     - 停止：`operationalReady && testState === 4`
     - 复位：`testState === 5/6/7`
   - **急停区**：`EmergencyButton`，固定底部，调用 `scenarioEngine.softwareEmergencyStop()`。

### 3.3 后端无改动

所有按钮调用与原 TestPage 完全一致（`scenarioEngine.loadScenario/checkDevices/arm/start/stop/reset/softwareEmergencyStop`），只是换了宿主页面。

## 4. 工况管理页详细设计

**文件**：`qml/pages/ScenarioPage.qml`（重写）

### 4.1 布局骨架（B1 · 左列表 + 右上下分区）

页面整体为 `RowLayout`：左侧工况列表 + 右侧上下分区。

```
┌────────────┬─────────────────────────────────────────────┐
│ 工况列表    │  右上：路径编辑（航点表格 + 地图预览）          │
│            │  ┌──────────────┬──────────────────────────┐ │
│ ▸ AEB CCRs │  │ 航点表格      │  地图预览(只读)           │ │
│   RCTA 01  │  │ # X   Y  速度 │     ●──●──● 轨迹线       │ │
│   LKA 01   │  │ 1 0   0  0.0  │                          │ │
│            │  │ 2 20  0  3.0  │                          │ │
│ [+ 新建]   │  │ 3 40  2  3.0  │                          │ │
│            │  │ [+航点][保存] │                          │ │
│            │  └──────────────┴──────────────────────────┘ │
│            ├─────────────────────────────────────────────┤
│            │  右下：数据回放（该工况的历史记录）             │
│            │  开始时间    状态    结果        操作员        │
│            │  06-20 14:32 PASS   无碰撞      admin         │
│            │  06-20 14:28 EMERG  软件急停    admin         │
│            │  [▶ 选中记录在此播放轨迹回放]                  │
└────────────┴─────────────────────────────────────────────┘
```

### 4.2 左侧工况列表（宽 200px）

- `PanelFrame`，标题"工况列表"。
- `ListView` 展示工况名称 + 类型，选中态用工业蓝高亮（沿用现状）。
- 底部"+ 新建工况"按钮：弹出输入名称 → 创建空航点列表工况 → 进入编辑。

### 4.3 右上：路径编辑区

`PanelFrame` 内左右分栏（`RowLayout`）：

- **左侧航点表格**（`WaypointEditor` 新组件）：序号 / X / Y / 速度 四列，行尾删除按钮，底部"+ 添加航点"和"保存"按钮。编辑触发 `pathChanged` 信号。
- **右侧地图预览**（`PathPreviewMap` 新组件）：只读 Canvas，接收航点列表绘制轨迹线 + 航点圆点。轨迹回放模式下接收采样数据播放动画。

### 4.4 右下：数据回放区

`PanelFrame` 内：

- 标题栏显示"共 N 条记录（当前工况）"。
- `ListView` 展示该工况的历史记录（开始时间 / 状态 / 结果 / 操作员），列定义沿用原 `DataReplayPage`。
- 选中某条记录后，右上方地图预览切换为"轨迹回放"模式，加载该 run 的 `SampleRecord` 播放。
- 回放控件：播放/暂停按钮（复用 `IconButton`）。

### 4.5 交互流程

1. **编辑工况**：左列表选中工况 → 右上表格显示其 `targetPath` → 编辑航点 → 点"保存"写回 `scenarios/*.json`。
2. **查看回放**：右下选中某条历史记录 → 上方地图切换为"轨迹回放"模式 → 加载该 run 的采样 → 播放/暂停。
3. **新建工况**：左列表底部"+ 新建" → 弹出输入名称 → 创建空航点列表的工况 → 进入编辑。

## 5. 后端数据结构扩展

### 5.1 新增 Waypoint 结构

`src/scenario/ScenarioConfig.h`：

```cpp
struct Waypoint {
    double x = 0.0;        // 航点 X 坐标（米）
    double y = 0.0;        // 航点 Y 坐标（米）
    double speed = 0.0;    // 到达该航点的目标速度（m/s）
};
```

`Waypoint` 用 `Q_GADGET` + `Q_PROPERTY MEMBER`，参照现有 `DeviceStatus` 写法，可在 C++/QML 间作为 `QVariant` 传递。

### 5.2 ScenarioConfig 新增字段

```cpp
QList<Waypoint> targetPath;   // 目标物航点路径（空则用单点 targetStart*）
```

向后兼容：`targetPath` 为空时，`ScenarioEngine` 退回原有的单点 + 恒定速度逻辑。

### 5.3 JSON 扩展（scenarios/*.json）

为 `target` 增加 `waypoints` 字段：

```json
"target": {
    "type": "静止车辆",
    "start_x": 0.0,
    "start_y": 0.0,
    "start_yaw": 0.0,
    "speed_mps": 0.0,
    "waypoints": [
        { "x": 0.0,  "y": 0.0, "speed": 0.0 },
        { "x": 20.0, "y": 0.0, "speed": 3.0 }
    ]
}
```

`start_x/y/yaw/speed_mps` 保留作为起点（向后兼容）。`waypoints` 为可选字段，缺失时退回单点逻辑。

### 5.4 引擎运动学改造

`ScenarioEngine::stepSimulation` 改为航点插值：

- 维护 `m_currentWaypoint` 索引，目标物沿 `targetPath` 顺序行进。
- 每帧根据"当前位置 → 下一航点"方向移动，速度取当前段的目标速度。
- 到达航点（距离 < 阈值，如 0.5m）则切换到下一航点。
- `targetPath` 为空时，保持原单点静止/恒速逻辑。

### 5.5 回放查询扩展

`DatabaseManager` 新增：

```cpp
QList<TestRunSummary> listRunsByScenario(const QString &scenarioName, int limit = 200) const;
QList<SampleRecord> listSamples(const QString &runId) const;
```

`ReplayListModel` 新增：

```cpp
Q_INVOKABLE void filterByScenario(const QString &scenarioName);
```

`filterByScenario("")` 返回全部记录（等价原 `refresh()`）。

## 6. 新增 QML 组件

### 6.1 WaypointEditor.qml

- 位置：`qml/components/WaypointEditor.qml`
- 职责：航点表格编辑器。四列（序号 / X / Y / 速度），行尾删除，底部"+ 添加航点"和"保存"按钮。
- 输入：`waypoints` 属性（`QVariantList`）。
- 输出：`pathChanged(var newPath)` 信号。
- 数据：编辑中的航点为本地副本，"保存"时才提交。

### 6.2 PathPreviewMap.qml

- 位置：`qml/components/PathPreviewMap.qml`
- 职责：只读 Canvas，两种模式：
  - **路径模式**：接收航点列表，绘制轨迹线（黄色虚线）+ 航点圆点（黄色）。
  - **回放模式**：接收 `SampleRecord` 列表，按时间戳驱动主车/目标物位置重绘，叠加轨迹尾迹。
- 输入：`waypoints` 或 `samples` + `mode` 属性。
- 复用 `MapPage` 的坐标系转换（`w2sX/w2sY`、`pixelsPerMeter`）。

## 7. 构建约束与迁移清单

### 7.1 QML 文件增删（同步 CMakeLists.txt）

**新增**（登记到 `qt_add_qml_module` 的 `QML_FILES`）：
- `qml/components/WaypointEditor.qml`
- `qml/components/PathPreviewMap.qml`

**删除**（从 `QML_FILES` 移除）：
- `qml/pages/DashboardPage.qml`
- `qml/pages/TestPage.qml`
- `qml/pages/DataReplayPage.qml`

**重写**（文件名不变，内容大改）：
- `qml/pages/MapPage.qml`
- `qml/pages/ScenarioPage.qml`
- `qml/components/SideNavBar.qml`（menus 8→5）
- `qml/Main.qml`（StackLayout 8→5）

### 7.2 C++ 文件增删（登记 `qt_add_executable`）

**修改**（已在构建列表中）：
- `src/scenario/ScenarioConfig.h`（新增 `Waypoint`、`targetPath`）
- `src/scenario/ScenarioEngine.cpp/.h`（`stepSimulation` 航点插值）
- `src/persistence/DatabaseManager.cpp/.h`（`listRunsByScenario` / `listSamples`）
- `src/persistence/ReplayListModel.cpp/.h`（`filterByScenario`）
- `src/app/AppController.cpp/.h`（接线工况列表模型、航点读写、回放过滤给 QML）

> 轨迹回放的分工：**数据查询在 C++**（`DatabaseManager::listSamples` 返回 `QList<SampleRecord>`，由 `AppController` 暴露给 QML），**时序驱动用 QML Timer**（按采样时间戳推进重绘）。本次不新增 `ReplayController` C++ 类，避免增加构建复杂度。

### 7.3 JSON 资源（登记 `RESOURCES`）

修改 3 个文件：`scenarios/AEB_CCR_50.json`、`scenarios/LKA_01.json`、`scenarios/RCTA_01.json`，为 `target` 增加 `waypoints` 字段。

### 7.4 状态色与视觉规范

- 全部沿用现有 `Theme.qml`，**不新增 token**。
- 航点圆点用 `colorWarning`（黄，与目标物方块一致）。
- 轨迹线用 `colorWarning` + 虚线（编辑态）/ 实线（回放态）。
- 回放播放/暂停按钮复用 `IconButton`。
- 状态流程条、操作按钮、急停的视觉完全沿用原 TestPage 已验证的样式。

## 8. 向后兼容与降级

- `ScenarioConfig.targetPath` 为空 → 引擎退回单点逻辑（旧 JSON 不加 `waypoints` 也能跑）。
- `ReplayListModel.filterByScenario("")` → 返回全部记录（等价原 `refresh()`）。
- 轨迹回放数据缺失（`sampleCount === 0`）→ 地图显示"无采样数据"占位，不崩溃。

## 9. 不在本次范围

- 真实设备协议接入（仍是 FakeDevice）。
- 工况的导入/导出（除 JSON 文件保存外）。
- 多目标物路径（单一目标物路径）。
- 轨迹回放的时间轴精确拖动（本次只做播放/暂停）。
- `ScenarioParser` 的完整实现（航点解析作为最小子集先做）。

## 10. 测试与验证

- 构建：`cmake --build build`，确认新增/修改文件全部通过 AUTOMOC 和 QML 注册。
- QML lint：`qmllint` gate 通过。
- 启动冒烟：`./build/ADASMaster.exe`，确认 5 项导航切换正常、无 QML 加载错误。
- 场地地图页：状态流程条显示、工况选择、6 个操作按钮 enabled 门控、急停可用、地图实时刷新。
- 工况管理页：
  - 选中工况 → 右上表格显示航点 → 编辑 → 保存 → 重开仍持久化。
  - 右下显示该工况历史记录（按工况过滤生效）。
  - 选中记录 → 地图切换回放模式 → 播放/暂停。
- 向后兼容：删除某 JSON 的 `waypoints` 字段，引擎仍能跑单点工况。
