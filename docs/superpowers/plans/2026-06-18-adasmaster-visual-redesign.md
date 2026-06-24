# ADASMaster 视觉层改造实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 ADASMaster 的视觉层升级为「指挥控制中心」风格（B 方向），不改后端与布局结构。

**Architecture:** 自底向上分层推进——先扩展 `Theme.qml` 设计 token（所有视觉依赖它），再建图标基建与可复用组件（PanelFrame/StatusCard/MetricCell/StateFlow/Icon/IconButton），然后升级现有组件与 8 个页面套用新组件，最后全局构建验证。

**Tech Stack:** C++17 + Qt 6.11.1 (Quick/Qml) + 手写 QML + CMake + Ninja（MinGW）

## Global Constraints

- **构建/运行**：`cmake --build build` 增量构建；运行 `./build/ADASMaster.exe` 视觉确认。环境为 Qt 6.11.1 MinGW + Ninja，`build/` 已配置。
- **验证方式**：本项目**无单元测试框架**，且本计划是 **QML 视觉改造**——每个任务的验证 = `cmake --build build` 成功 + 运行应用视觉检查对应页面，**不是 TDD 断言**。
- **CMake 登记（易踩坑）**：新增任何 `.qml` 必须登记到 `CMakeLists.txt` 的 `qt_add_qml_module ... QML_FILES`；新增 `qml/icons/*.svg` 等资源登记到 `RESOURCES`；新增含 `Q_OBJECT` 的 C++ 头文件登记到 `qt_add_executable`。**未登记会导致运行时找不到类型/资源。**
- **git**：项目当前**非 git 仓库**。每个任务末尾的 `git commit` 步骤为**可选**——若用户已 `git init` 则执行，否则跳过（构建验证通过即视为任务完成）。
- **不改后端**：`src/` 下所有 C++（Manager/Engine/Model）与 `main.cpp`、`Main.qml` 的页面结构/导航逻辑**不得改动**，仅改 QML 视觉属性（颜色/字体/圆角/边框/组件样式）。
- **状态色约定（不可违反）**：`0=绿 1=黄 2=橙 3=红`，复用 `Theme.levelColor(level)`。急停为最高级。
- **代码规范**：代码英文、注释中文；新增 QML 文件头必须有块注释说明功能/输入/输出（参考现有文件）。
- **数值字体**：数值用等宽数字字体族（`Theme.fontNumeric`），保证刷新时不跳动；不依赖 `font.features`。

---

## File Structure

**Modify（视觉 token/样式）：**
- `qml/theme/Theme.qml` — token 扩展（Task 1）
- `qml/components/TopStatusBar.qml` — Task 6
- `qml/components/SideNavBar.qml` — Task 7
- `qml/components/BottomLogBar.qml` — Task 8
- `qml/components/StatusLamp.qml` — Task 8（微调）
- `qml/components/EmergencyButton.qml` — Task 8（微调）
- `qml/components/DeviceCard.qml` — Task 8
- `qml/pages/DashboardPage.qml` — Task 9
- `qml/pages/TestPage.qml` — Task 10
- `qml/pages/DevicePage.qml` — Task 11
- `qml/pages/AlarmPage.qml` — Task 11
- `qml/pages/MapPage.qml` / `ScenarioPage.qml` / `DataReplayPage.qml` / `SettingsPage.qml` — Task 12
- `CMakeLists.txt` — 各任务按需登记新文件
- `CLAUDE.md` — Task 13 补充视觉规范引用

**Create（新可复用组件/资源）：**
- `qml/icons/*.svg` — 线性图标（Task 2）
- `qml/components/Icon.qml` — SVG + 着色（Task 2）
- `qml/components/IconButton.qml` — 图标+文字按钮（Task 2）
- `qml/components/PanelFrame.qml` — 标题栏面板容器（Task 3）
- `qml/components/StatusCard.qml` — 状态卡（Task 4）
- `qml/components/MetricCell.qml` — 指标格（Task 4）
- `qml/components/StateFlow.qml` — 流程条（Task 5）

**文件职责原则**：每个新组件单一职责（容器/卡片/指标/流程/图标），通过属性暴露接口，页面通过属性注入数据，不持有业务逻辑。

---

## Task 1: Theme.qml 设计 token 扩展

**Files:**
- Modify: `qml/theme/Theme.qml`

**Interfaces:**
- Produces（后续所有任务消费）：新增 token —— `colorTitleBar`、`colorTitleBarActive`、`colorEmergencyHi`、`fontFamily`、`fontNumeric`、`iconSm`、`iconMd`、`iconLg`、`iconPrefix`；调整值 —— `colorBg`、`colorPanel`、`colorPanelAlt`、`radius`、`radiusLg`。

- [ ] **Step 1: 替换 Theme.qml 的背景/面板/圆角 token 值**

把 `qml/theme/Theme.qml` 中以下 `readonly property` 的值改为：

```qml
    readonly property color colorBg:         "#12161d"   // 最外层应用背景（原 #1e1f22）
    readonly property color colorPanel:      "#1b212b"   // 面板/卡片（原 #2b2d31）
    readonly property color colorPanelAlt:   "#161a22"   // 凹陷/交替区（原 #232428）
    readonly property color colorBorder:     "#2a3340"   // 边框（原 #3f4147）
    readonly property color colorDivider:    "#2a3340"   // 分割线
```

```qml
    readonly property int radius:      5      // 更硬朗（原 6）
    readonly property int radiusLg:    8      // （原 10）
```

- [ ] **Step 2: 在 Theme.qml 文字色块后新增字体族 token**

在 `// ---- 文字 ----` 块之后新增：

```qml
    // ---- 字体族 ----
    readonly property string fontFamily:  "'Microsoft YaHei','Segoe UI',system-ui,sans-serif"
    readonly property string fontNumeric: "'Cascadia Code','Consolas','Courier New',monospace"  // 数值等宽
```

- [ ] **Step 3: 在主色块后新增活跃面板/急停高亮 token**

在 `colorDisable` 定义之后新增：

```qml
    readonly property color colorTitleBar:       "#232a35"   // 面板标题栏
    readonly property color colorTitleBarActive: "#2a3a4a"   // 活跃面板标题栏
    readonly property color colorEmergencyHi:    "#ff6b5e"   // 急停高亮描边
```

- [ ] **Step 4: 在间距块后新增图标尺寸/前缀 token**

在 `spacingXl` 之后新增：

```qml
    // ---- 图标 ----
    readonly property int iconSm: 16
    readonly property int iconMd: 20
    readonly property int iconLg: 24
    readonly property string iconPrefix: "qrc:/qt/qml/ADASMaster/icons/"  // SVG 图标资源前缀
```

- [ ] **Step 5: 构建并运行验证**

Run: `cmake --build build`
Expected: 构建成功（Theme.qml 无语法错误）

Run: `./build/ADASMaster.exe`
Expected: 应用启动，整体配色变为更深的蓝灰（背景 #12161d），无 QML 报错，原有功能正常。

- [ ] **Step 6（可选）: Commit**

```bash
git add qml/theme/Theme.qml
git commit -m "style: 扩展 Theme 设计 token（指挥控制中心配色/字体/图标）"
```

---

## Task 2: 图标体系基建（Icon + IconButton + SVG 资源）

**Files:**
- Create: `qml/components/Icon.qml`
- Create: `qml/components/IconButton.qml`
- Create: `qml/icons/*.svg`（19 个：导航 8 + 操作 7 + 状态 4）
- Modify: `CMakeLists.txt`（登记新 QML + svg 资源）

**Interfaces:**
- Consumes: `Theme.iconPrefix`、`Theme.iconSm/Md/Lg`、`Theme.fontFamily`、`Theme.colorText/TextDim`、状态色
- Produces:
  - `Icon` 组件：属性 `source: string`（图标名，如 `"dashboard"`，自动拼前缀）、`color: color`、`size: int`
  - `IconButton` 组件：属性 `iconName: string`、`text: string`、`accent: color`、`enabled: bool`；信号 `clicked()`

**图标选型**：采用 **Lucide**（MIT，线性图标集）。所有 SVG 统一改为 `fill="none" stroke="#ffffff" stroke-width="2"`（白色描边），由 `Icon` 组件用 `MultiEffect` 着色。

**图标清单（iconName → Lucide 名）**：
- 导航：`dashboard`(layout-dashboard)、`device`(cpu)、`map`(map)、`scenario`(list-checks)、`test`(play)、`replay`(history)、`alarm`(bell)、`settings`(settings)
- 操作：`load`(refresh-cw)、`check`(wrench)、`arm`(chevrons-right)、`start`(play)、`stop`(square)、`estop`(alert-triangle)、`reset`(rotate-ccw)
- 状态：`normal`(check-circle)、`warning`(alert-circle)、`error`(octagon-alert)、`emergency`(siren)

- [ ] **Step 1: 创建一个代表性 SVG 确认格式（以 dashboard 为例）**

Create `qml/icons/dashboard.svg`（白色描边 Lucide `layout-dashboard` 路径）：

```xml
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <rect width="7" height="9" x="3" y="3" rx="1"/>
  <rect width="7" height="5" x="14" y="3" rx="1"/>
  <rect width="7" height="9" x="14" y="12" rx="1"/>
  <rect width="7" height="5" x="3" y="16" rx="1"/>
</svg>
```

Create `qml/icons/estop.svg`（`alert-triangle`，急停/警告用）：

```xml
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <path d="m21.73 18-8-14a2 2 0 0 0-3.48 0l-8 14A2 2 0 0 0 4 21h16a2 2 0 0 0 1.73-3Z"/>
  <path d="M12 9v4"/><path d="M12 17h.01"/>
</svg>
```

- [ ] **Step 2: 按清单补齐其余 17 个 SVG**

按上方清单的 Lucide 名，从 `https://lucide.dev/icons/<name>` 取 `<path>/<rect>/<circle>` 等几何元素，套入与 Step 1 相同的 `<svg ... stroke="#ffffff" stroke-width="2" ...>` 外壳，分别创建：
`device.svg map.svg scenario.svg test.svg replay.svg alarm.svg settings.svg load.svg check.svg arm.svg start.svg stop.svg reset.svg normal.svg warning.svg error.svg emergency.svg`

> 执行提示：每个 Lucide 图标页 "Copy SVG" 给出的就是几何元素；替换外壳的 `stroke` 为 `#ffffff`、`fill="none"` 即可，不改路径数据。

- [ ] **Step 3: 创建 Icon.qml（SVG + MultiEffect 着色）**

Create `qml/components/Icon.qml`：

```qml
// =============================================================================
// Icon.qml
// 功能：线性图标显示组件。加载白色描边 SVG，用 MultiEffect 着成指定颜色。
// 输入：source(图标名，自动拼 Theme.iconPrefix 前缀)、color、size。
// 输出：着色后的图标。
// =============================================================================
import QtQuick
import QtQuick.Effects
import ADASMaster

Item {
    id: root
    property string source: ""          // 图标名，如 "dashboard"
    property color color: Theme.colorTextDim
    property int size: Theme.iconMd

    implicitWidth: root.size
    implicitHeight: root.size

    Image {
        id: img
        anchors.fill: parent
        source: Theme.iconPrefix + root.source + ".svg"
        fillMode: Image.Pad
        visible: false
    }

    MultiEffect {
        anchors.fill: img
        source: img
        colorizationColor: root.color
        colorization: 1.0
    }
}
```

- [ ] **Step 4: 创建 IconButton.qml（图标 + 文字）**

Create `qml/components/IconButton.qml`：

```qml
// =============================================================================
// IconButton.qml
// 功能：图标 + 文字按钮。用于导航项、操作按钮。颜色由 accent 决定。
// 输入：iconName、text、accent(图标/文字色)、enabled、size。
// 输出：clicked() 信号。
// =============================================================================
import QtQuick
import QtQuick.Layouts
import ADASMaster

Item {
    id: root
    property string iconName: ""
    property string text: ""
    property color accent: Theme.colorTextDim
    property bool enabled: true
    property int size: Theme.iconMd
    signal clicked()

    implicitHeight: row.implicitHeight
    implicitWidth: row.implicitWidth
    opacity: root.enabled ? 1.0 : 0.4

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: Theme.spacingSm
        Icon { source: root.iconName; color: root.accent; size: root.size; Layout.alignment: Qt.AlignVCenter }
        Text {
            text: root.text
            color: root.accent
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontNormal
            font.bold: true
            visible: root.text.length > 0
            Layout.alignment: Qt.AlignVCenter
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.enabled
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }
}
```

- [ ] **Step 5: 在 CMakeLists.txt 登记新文件与资源**

Modify `CMakeLists.txt`：
- 在 `qt_add_qml_module(ADASMaster ... QML_FILES ...)` 列表里追加：
```cmake
        qml/components/Icon.qml
        qml/components/IconButton.qml
```
- 在 `RESOURCES` 列表里追加图标目录（用通配需 CMake 支持，这里逐个列出确定性更高）：
```cmake
        qml/icons/dashboard.svg
        qml/icons/device.svg
        qml/icons/map.svg
        qml/icons/scenario.svg
        qml/icons/test.svg
        qml/icons/replay.svg
        qml/icons/alarm.svg
        qml/icons/settings.svg
        qml/icons/load.svg
        qml/icons/check.svg
        qml/icons/arm.svg
        qml/icons/start.svg
        qml/icons/stop.svg
        qml/icons/estop.svg
        qml/icons/reset.svg
        qml/icons/normal.svg
        qml/icons/warning.svg
        qml/icons/error.svg
        qml/icons/emergency.svg
```

- [ ] **Step 6: 构建并运行验证**

Run: `cmake --build build`
Expected: 成功，无 "file not found" 或 QML 注册错误。

Run: `./build/ADASMaster.exe`
Expected: 应用启动无报错（图标暂未在界面使用，仅验证资源加载与组件编译通过）。

- [ ] **Step 7（可选）: Commit**

```bash
git add qml/components/Icon.qml qml/components/IconButton.qml qml/icons/ CMakeLists.txt
git commit -m "feat: 图标体系基建（Icon/IconButton + 19 线性 SVG）"
```

---

## Task 3: PanelFrame 可复用面板容器

**Files:**
- Create: `qml/components/PanelFrame.qml`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `Theme.colorPanel/colorTitleBar/colorTitleBarActive/colorAccent/colorBorder/colorText/TextDim`、`spacing/fontSmall`
- Produces:
  - `PanelFrame`：属性 `title:string`、`titleEn:string`、`liveText:string`、`liveColor:color`、`accent:bool`、`contentPadding:int`；默认 children 槽注入面板内容区。

- [ ] **Step 1: 创建 PanelFrame.qml**

Create `qml/components/PanelFrame.qml`：

```qml
// =============================================================================
// PanelFrame.qml
// 功能：B 方向核心容器——带标题栏的面板。标题栏左侧 3px 工业蓝竖条 + 标签，
//       右侧可挂实时指示；内容通过默认 children 注入。
// 输入：title/titleEn(标题)、liveText/liveColor(右侧指示)、accent(活跃面板)、
//       默认槽(内容)。
// 输出：统一外观面板。
// =============================================================================
import QtQuick
import QtQuick.Layouts
import ADASMaster

Rectangle {
    id: root
    property string title: ""
    property string titleEn: ""
    property string liveText: ""
    property color liveColor: Theme.colorNormal
    property bool accent: false
    property int contentPadding: Theme.spacing

    default property alias content: contentCol.children

    implicitHeight: col.implicitHeight
    color: Theme.colorPanel
    radius: Theme.radius
    border.color: root.accent ? Theme.colorAccent : Theme.colorBorder
    border.width: root.accent ? 2 : 1

    ColumnLayout {
        id: col
        anchors.fill: parent
        spacing: 0

        // 标题栏（仅当有标题时显示）
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            visible: root.title.length > 0 || root.titleEn.length > 0
            color: root.accent ? Theme.colorTitleBarActive : Theme.colorTitleBar

            Rectangle {  // 左侧 3px 工业蓝竖条
                width: 3
                height: parent.height - 12
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.colorAccent
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacing
                anchors.rightMargin: Theme.spacing
                spacing: Theme.spacingSm

                Text {
                    text: root.title
                    visible: root.title.length > 0
                    color: Theme.colorText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                    font.bold: true
                }
                Text {
                    text: root.titleEn.toUpperCase()
                    visible: root.titleEn.length > 0
                    color: Theme.colorTextDim
                    font.family: Theme.fontFamily
                    font.pixelSize: 11
                    font.letterSpacing: 1
                }
                Item { Layout.fillWidth: true }

                RowLayout {  // 右侧实时指示
                    visible: root.liveText.length > 0
                    spacing: 4
                    Rectangle { width: 6; height: 6; radius: 3; color: root.liveColor }
                    Text {
                        text: root.liveText
                        color: root.liveColor
                        font.family: Theme.fontFamily
                        font.pixelSize: 10
                        font.bold: true
                    }
                }
            }
        }

        Rectangle {  // 标题栏底分隔
            Layout.fillWidth: true
            height: 1
            color: Theme.colorBorder
            visible: root.title.length > 0 || root.titleEn.length > 0
        }

        ColumnLayout {  // 内容区
            id: contentCol
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: root.contentPadding
            spacing: Theme.spacing
        }
    }
}
```

- [ ] **Step 2: CMakeLists.txt 登记**

在 `QML_FILES` 追加 `qml/components/PanelFrame.qml`。

- [ ] **Step 3: 构建并运行验证**

Run: `cmake --build build` → 成功。
Run: `./build/ADASMaster.exe` → 应用启动无报错（PanelFrame 暂未被使用，验证编译注册）。

- [ ] **Step 4（可选）: Commit**

```bash
git add qml/components/PanelFrame.qml CMakeLists.txt
git commit -m "feat: PanelFrame 标题栏面板容器"
```

---

## Task 4: StatusCard + MetricCell 组件

**Files:**
- Create: `qml/components/StatusCard.qml`
- Create: `qml/components/MetricCell.qml`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: Theme 色板/字体/圆角
- Produces:
  - `StatusCard`：属性 `label/value/sub/valueColor/accent`；`Layout.fillWidth` 由调用方设。
  - `MetricCell`：属性 `label/value/unit/valueColor`；置于 1px 网格容器内。

- [ ] **Step 1: 创建 StatusCard.qml**

Create `qml/components/StatusCard.qml`：

```qml
// =============================================================================
// StatusCard.qml
// 功能：状态卡。小标签 + 大号状态色数值 + 副信息，无标题栏（运行监控页六卡）。
// 输入：label/value/sub/valueColor/accent。
// =============================================================================
import QtQuick
import QtQuick.Layouts
import ADASMaster

Rectangle {
    id: root
    property string label: ""
    property string value: ""
    property string sub: ""
    property color valueColor: Theme.colorText
    property bool accent: false

    implicitHeight: 92
    color: Theme.colorPanel
    radius: Theme.radius
    border.color: root.accent ? Theme.colorAccent : Theme.colorBorder
    border.width: root.accent ? 2 : 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing
        spacing: 2

        Text {
            text: root.label
            color: Theme.colorTextMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
        }
        Text {
            text: root.value
            color: root.valueColor
            font.family: Theme.fontNumeric
            font.pixelSize: Theme.fontHuge
            font.bold: true
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        Text {
            text: root.sub
            visible: root.sub.length > 0
            color: Theme.colorTextMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontTiny
        }
    }
}
```

- [ ] **Step 2: 创建 MetricCell.qml**

Create `qml/components/MetricCell.qml`：

```qml
// =============================================================================
// MetricCell.qml
// 功能：指标格。小标签 + 大号等宽数值 + 单位。置于 1px 网格容器内（容器背景
//       colorBorder，本格 colorPanel，形成分隔线）。
// 输入：label/value/unit/valueColor。
// =============================================================================
import QtQuick
import QtQuick.Layouts
import ADASMaster

ColumnLayout {
    id: root
    property string label: ""
    property string value: ""
    property string unit: ""
    property color valueColor: Theme.colorText

    spacing: 3

    Text {
        text: root.label
        color: Theme.colorTextMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontTiny
    }
    RowLayout {
        spacing: 4
        Text {
            text: root.value
            color: root.valueColor
            font.family: Theme.fontNumeric
            font.pixelSize: Theme.fontLarge
            font.bold: true
        }
        Text {
            text: root.unit
            color: Theme.colorTextMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
            Layout.alignment: Qt.AlignBottom
        }
    }
}
```

- [ ] **Step 3: CMakeLists.txt 登记**

在 `QML_FILES` 追加 `qml/components/StatusCard.qml` 和 `qml/components/MetricCell.qml`。

- [ ] **Step 4: 构建并运行验证**

Run: `cmake --build build` → 成功。Run: `./build/ADASMaster.exe` → 启动无报错。

- [ ] **Step 5（可选）: Commit**

```bash
git add qml/components/StatusCard.qml qml/components/MetricCell.qml CMakeLists.txt
git commit -m "feat: StatusCard 与 MetricCell 组件"
```

---

## Task 5: StateFlow 流程条组件

**Files:**
- Create: `qml/components/StateFlow.qml`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `appController.stateManager.testState`（int，TestState 枚举），Theme 状态色
- Produces:
  - `StateFlow`：只读可视化组件，绑定 `testState`，自动渲染 6 步流程（空闲/检查中/就绪/预备/运行中/完成），支持故障(6)/急停(7)态着色。

- [ ] **Step 1: 创建 StateFlow.qml**

Create `qml/components/StateFlow.qml`：

```qml
// =============================================================================
// StateFlow.qml
// 功能：测试状态机流程条。6 步（空闲→检查中→就绪→预备→运行中→完成），
//       三态着色：已完成(绿勾)/当前(蓝)/未到(灰)；故障(6)橙、急停(7)红。
// 输入：currentState(int，TestState 枚举值)。
// 输出：只读流程可视化。
// =============================================================================
import QtQuick
import QtQuick.Layouts
import ADASMaster

Item {
    id: root
    property int currentState: 0   // 由调用方绑定 stateManager.testState

    readonly property var steps: [
        { idx: 0, name: "空闲" }, { idx: 1, name: "检查中" },
        { idx: 2, name: "就绪" }, { idx: 3, name: "预备" },
        { idx: 4, name: "运行中" }, { idx: 5, name: "完成" }
    ]

    // 故障/急停时整体覆盖态色
    readonly property color overrideColor: {
        if (currentState === 6) return Theme.colorError;
        if (currentState === 7) return Theme.colorEmergency;
        return Theme.colorAccent;  // 占位，实际由各步判断
    }

    implicitHeight: 80

    RowLayout {
        id: flow
        anchors.centerIn: parent
        spacing: 0

        Repeater {
            model: root.steps.length

            RowLayout {
                id: stepRow
                spacing: Theme.spacingSm
                Layout.alignment: Qt.AlignVCenter

                required property int index
                readonly property var step: root.steps[stepRow.index]   // 内层统一通过 stepRow.step 访问，避免 parent 作用域错误

                Rectangle {
                    id: circle
                    width: 42; height: 42; radius: 21
                    color: {
                        if (root.currentState === 6) return Theme.colorError;
                        if (root.currentState === 7) return Theme.colorEmergency;
                        if (stepRow.step.idx < root.currentState) return Theme.colorBorder;     // 已完成（深底，靠勾显绿）
                        if (stepRow.step.idx === root.currentState) return Theme.colorAccent;   // 当前（蓝）
                        return Theme.colorPanelAlt;                                             // 未到（灰）
                    }
                    border.width: stepRow.step.idx === root.currentState ? 3 : 2
                    border.color: {
                        if (root.currentState >= 6) return Qt.lighter(circle.color, 1.3);
                        if (stepRow.step.idx === root.currentState) return "#5a9fd4";  // 当前高亮边
                        return Theme.colorBorder;
                    }

                    Text {
                        anchors.centerIn: parent
                        text: {
                            if (root.currentState >= 6) return "!";
                            if (stepRow.step.idx < root.currentState) return "✓";
                            return (stepRow.step.idx + 1);
                        }
                        color: {
                            if (root.currentState >= 6) return "white";
                            if (stepRow.step.idx < root.currentState) return Theme.colorNormal;
                            return stepRow.step.idx === root.currentState ? "white" : Theme.colorTextMuted;
                        }
                        font.family: Theme.fontNumeric
                        font.pixelSize: 16
                        font.bold: true
                    }
                }

                Text {
                    text: stepRow.step.name
                    color: {
                        if (root.currentState >= 6) return root.overrideColor;
                        if (stepRow.step.idx === root.currentState) return Theme.colorText;
                        return Theme.colorTextDim;
                    }
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                    font.bold: stepRow.step.idx === root.currentState
                }

                // 连线（最后一步不显示）
                Rectangle {
                    visible: stepRow.index < root.steps.length - 1
                    width: 28; height: 2
                    color: Theme.colorBorder
                    Layout.alignment: Qt.AlignVCenter
                }
            }
        }
    }
}
```

- [ ] **Step 2: CMakeLists.txt 登记**

在 `QML_FILES` 追加 `qml/components/StateFlow.qml`。

- [ ] **Step 3: 构建并运行验证**

Run: `cmake --build build` → 成功。Run: `./build/ADASMaster.exe` → 启动无报错。

- [ ] **Step 4（可选）: Commit**

```bash
git add qml/components/StateFlow.qml CMakeLists.txt
git commit -m "feat: StateFlow 状态机流程条组件"
```

---

## Task 6: 顶部状态栏 TopStatusBar 升级

**Files:**
- Modify: `qml/components/TopStatusBar.qml`

**Interfaces:**
- Consumes: Task 1 的 Theme token（colorBg 已自动生效，本任务补字体族与配色细节）。

- [ ] **Step 1: 给 TopStatusBar 所有 Text 补 font.family 并统一深底**

在 `qml/components/TopStatusBar.qml` 中：
- 根 `Rectangle` 的 `color: Theme.colorPanel` 保持（顶栏用面板色，与 #12161d 背景区分）。
- 给每个 `Text {...}` 加 `font.family: Theme.fontFamily`。
- "设备在线" Text 的 `color` 逻辑保持（>=0.8 绿/>=0.5 黄/否则橙），值不变。
- 高度 `height: 56` 改为 `height: 52`。

- [ ] **Step 2: 急停叠加层配色微调**

急停叠加 `Rectangle` 的 `color: Theme.colorEmergency` 与 `opacity`（lvl>=3 ? 0.18 : 0.0）保持不变（已符合规范）。无需改动。

- [ ] **Step 3: 构建并运行验证**

Run: `cmake --build build` → 成功。Run: `./build/ADASMaster.exe` → 顶部状态栏字体统一、高度 52、深蓝灰底，安全/设备/测试项状态色正常。

- [ ] **Step 4（可选）: Commit**

```bash
git add qml/components/TopStatusBar.qml
git commit -m "style: TopStatusBar 统一字体族与高度"
```

---

## Task 7: 左导航 SideNavBar 升级（加图标）

**Files:**
- Modify: `qml/components/SideNavBar.qml`

**Interfaces:**
- Consumes: `Icon`/`IconButton`（Task 2）、Theme token。

- [ ] **Step 1: 给菜单项加图标 + 统一样式**

在 `qml/components/SideNavBar.qml` 中，把 `menus` 改为带图标名的结构，并在 delegate 里用 `Icon`：

把：
```qml
    readonly property var menus: [
        "运行监控", "设备管理", "场地地图", "工况管理",
        "测试执行", "数据回放", "报警中心", "系统设置"
    ]
```
改为：
```qml
    readonly property var menus: [
        { name: "运行监控", icon: "dashboard" },
        { name: "设备管理", icon: "device" },
        { name: "场地地图", icon: "map" },
        { name: "工况管理", icon: "scenario" },
        { name: "测试执行", icon: "test" },
        { name: "数据回放", icon: "replay" },
        { name: "报警中心", icon: "alarm" },
        { name: "系统设置", icon: "settings" }
    ]
```

把 delegate 内的 `Text`（居中）替换为 `RowLayout`（图标 + 文字）：

```qml
                RowLayout {
                    anchors.centerIn: parent
                    spacing: Theme.spacingSm
                    Icon {
                        source: modelData.icon
                        color: root.currentIndex === index ? "white" : Theme.colorTextDim
                        size: Theme.iconMd
                    }
                    Text {
                        text: modelData.name
                        color: root.currentIndex === index ? "white" : Theme.colorText
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontNormal
                        font.bold: root.currentIndex === index
                    }
                }
```

- [ ] **Step 2: 导航宽度和选中态保持**

`width: 180` 改为 `width: 176`；选中项蓝底 + 左侧 4px 绿指示条逻辑保持（已符合规范，`Theme.colorAccent`/`colorNormal` 自动取新值）。

- [ ] **Step 3: 构建并运行验证**

Run: `cmake --build build` → 成功。Run: `./build/ADASMaster.exe` → 左导航每项显示线性图标+文字，选中项蓝底白字图标+绿指示条，宽度 176。

- [ ] **Step 4（可选）: Commit**

```bash
git add qml/components/SideNavBar.qml
git commit -m "style: SideNavBar 加线性图标"
```

---

## Task 8: 底部日志栏 / 状态灯 / 急停按钮 / 设备卡升级

**Files:**
- Modify: `qml/components/BottomLogBar.qml`
- Modify: `qml/components/StatusLamp.qml`
- Modify: `qml/components/EmergencyButton.qml`
- Modify: `qml/components/DeviceCard.qml`

**Interfaces:**
- Consumes: Theme token（字体族、新增色）。

- [ ] **Step 1: BottomLogBar 加字体族 + 级别 pill**

在 `qml/components/BottomLogBar.qml` 中，给所有 `Text` 加 `font.family: Theme.fontFamily`。把"最新事件"区的纯 `StatusLamp` 文本前加一个级别色 pill（可选增强），或保持现状（StatusLamp 已用 levelColor）。本步骤最小改动：仅补 `font.family`，并把根 `height: 44` 保持。`logColor()` 逻辑不变。

- [ ] **Step 2: StatusLamp 补字体族**

在 `qml/components/StatusLamp.qml` 的 `Text` 加 `font.family: Theme.fontFamily`。其余（圆点 levelColor、闪烁）不变。

- [ ] **Step 3: EmergencyButton 加外发光环**

在 `qml/components/EmergencyButton.qml` 的 `background: Rectangle {...}` 内追加一个外发光层（在 border 之后）：

```qml
    background: Rectangle {
        radius: Theme.radiusLg
        color: root.pressed ? Qt.darker(Theme.colorEmergency, 1.3)
             : root.hovered ? Qt.lighter(Theme.colorEmergency, 1.1)
             : Theme.colorEmergency
        border.color: Theme.colorEmergencyHi   // 改为新增的高亮描边
        border.width: 2
        layer.enabled: true
        layer.effect: null
    }
```
并给 `Button` 根加一个外发光矩形（放在 background 之下/同级）：

```qml
    Rectangle {  // 外发光环
        anchors.centerIn: parent
        width: parent.width; height: parent.height
        radius: Theme.radiusLg
        color: "transparent"
        border.color: Theme.colorEmergency
        border.width: 3
        opacity: 0.25
        z: -1
    }
```
呼吸动画（`SequentialAnimation on opacity`）保持。`text` 的 contentItem Text 加 `font.family: Theme.fontFamily`。

- [ ] **Step 4: DeviceCard 补字体族 + 等宽数值**

在 `qml/components/DeviceCard.qml` 中：
- 所有文字 `Text` 加 `font.family: Theme.fontFamily`。
- IP/延迟、电量等数值 Text 的 `font.family` 用 `Theme.fontNumeric`（数值等宽）。具体："延迟 %3 ms"、"电量：%1%"、"故障码"用 fontNumeric。

- [ ] **Step 5: 构建并运行验证**

Run: `cmake --build build` → 成功。Run: `./build/ADASMaster.exe` → 进入设备管理页，设备卡字体统一、数值等宽；底部日志栏字体统一；急停按钮带外发光环。

- [ ] **Step 6（可选）: Commit**

```bash
git add qml/components/BottomLogBar.qml qml/components/StatusLamp.qml qml/components/EmergencyButton.qml qml/components/DeviceCard.qml
git commit -m "style: 日志栏/状态灯/急停/设备卡 统一样式"
```

---

## Task 9: 运行监控页 DashboardPage 升级（样板）

**Files:**
- Modify: `qml/pages/DashboardPage.qml`

**Interfaces:**
- Consumes: `PanelFrame`/`StatusCard`/`MetricCell`（Task 3/4）、Theme token。

- [ ] **Step 1: 六状态卡替换为 StatusCard**

在 `qml/pages/DashboardPage.qml` 中，把顶部 `GridLayout` 内 6 个手写 `Rectangle {...}` 状态卡，逐一替换为 `StatusCard`。以"安全状态"卡为例，替换为：

```qml
                StatusCard {
                    Layout.fillWidth: true
                    label: "安全状态"
                    value: appController.safetyManager.levelText
                    valueColor: Theme.levelColor(appController.safetyManager.safetyLevel)
                    sub: appController.safetyManager.emergencyStopActive ? "急停：" + appController.safetyManager.emergencySource : "无急停 · 通信正常"
                }
```

其余 5 卡同理替换（RTK/设备/工况/测试/进度），保留各自的 valueColor 绑定逻辑（参考现状 DashboardPage.qml 第 49-125 行的值与着色），"测试状态"卡设 `accent: appController.stateManager.testState === 4`（运行中时蓝框高亮）。值文本若非纯数字（如"正常""运行中"），`StatusCard.value` 仍可用（文字）。

- [ ] **Step 2: 运动学指标用 PanelFrame + MetricCell**

把"实时运动学指标"的 `Rectangle {...}` 替换为 `PanelFrame`，标题栏带实时指示：

```qml
        PanelFrame {
            Layout.fillWidth: true
            Layout.margins: Theme.spacing
            title: "实时运动学"
            titleEn: "kinematics"
            liveText: "LIVE 10Hz"
            liveColor: Theme.colorNormal

            // 1px 网格容器：背景 colorBorder，子格 colorPanel
            GridLayout {
                Layout.fillWidth: true
                columns: 4
                rowSpacing: 1
                columnSpacing: 1

                MetricCell { label: "主车速度"; value: appController.stateManager.egoSpeed.toFixed(2); unit: "m/s" }
                MetricCell { label: "目标速度"; value: appController.stateManager.tgtSpeed.toFixed(2); unit: "m/s" }
                MetricCell { label: "相对距离"; value: appController.stateManager.distance.toFixed(2); unit: "m"; valueColor: appController.stateManager.collisionRisk ? Theme.colorEmergency : Theme.colorNormal }
                MetricCell { label: "相对速度"; value: appController.stateManager.relativeSpeed.toFixed(2); unit: "m/s" }
                MetricCell { label: "TTC"; value: appController.stateManager.ttc > 900 ? "∞" : appController.stateManager.ttc.toFixed(2); unit: "s"; valueColor: appController.stateManager.ttc < 2.0 ? Theme.colorEmergency : Theme.colorNormal }
                MetricCell { label: "主车 X"; value: appController.stateManager.egoX.toFixed(1); unit: "m" }
                MetricCell { label: "主车 Y"; value: appController.stateManager.egoY.toFixed(1); unit: "m" }
                MetricCell { label: "航向角"; value: appController.stateManager.egoYaw.toFixed(1); unit: "°" }
            }
        }
```

> 注：1px 网格分隔效果——可在 PanelFrame 内容区放一个 `Rectangle { color: Theme.colorBorder }` 作网格背景，MetricCell 自带透明背景；或直接给每个 MetricCell 包一层 `Rectangle { color: Theme.colorPanel }`。执行时二选一实现分隔线效果。

- [ ] **Step 3: 快捷操作按钮补字体族 + 图标**

底部快捷操作按钮组保留现有 `Button` 结构，给 `contentItem` 的 Text 加 `font.family: Theme.fontFamily`。按钮文字前可加对应 Icon（开始→`start`、停止→`stop`、急停已是 EmergencyButton）。最小改动：仅补 font.family，确保配色用新 token（开始绿框/停止黄框逻辑已对）。

- [ ] **Step 4: 删除页面内不再使用的内联 MetricText 组件**

页面末尾的 `component MetricText : ColumnLayout {...}`（第 197-210 行）已被 `MetricCell` 取代，删除。

- [ ] **Step 5: 构建并运行验证**

Run: `cmake --build build` → 成功。Run: `./build/ADASMaster.exe` → 运行监控页呈现 B 方向完整样态：六状态卡（运行中卡蓝框）、指标面板带标题栏与 LIVE 指示、数值等宽、危险量红色。

- [ ] **Step 6（可选）: Commit**

```bash
git add qml/pages/DashboardPage.qml
git commit -m "style: 运行监控页套用 PanelFrame/StatusCard/MetricCell"
```

---

## Task 10: 测试执行页 TestPage 升级

**Files:**
- Modify: `qml/pages/TestPage.qml`

**Interfaces:**
- Consumes: `StateFlow`（Task 5）、`PanelFrame`（Task 3）、`IconButton`（Task 2）、Theme token。

- [ ] **Step 1: 状态机流程条替换为 StateFlow**

把 `qml/pages/TestPage.qml` 顶部手写的"状态机流程条" `Rectangle {...}`（第 19-61 行）替换为 PanelFrame + StateFlow：

```qml
        PanelFrame {
            Layout.fillWidth: true
            Layout.margins: Theme.spacing
            Layout.preferredHeight: 110
            title: "测试流程"
            titleEn: "state machine"
            liveText: appController.stateManager.testState === 4 ? "运行中" : ""
            liveColor: Theme.colorNormal

            StateFlow {
                anchors.centerIn: parent
                currentState: appController.stateManager.testState
            }
        }
```

- [ ] **Step 2: 工况选择与操作区包 PanelFrame**

把"工况选择" `Rectangle`（第 83-102 行）改为 `PanelFrame { title: "工况选择"; titleEn: "scenario"; ...内容不变... }`；把"操作按钮区" `GridLayout`（第 105-157 行）改为 `PanelFrame { title: "操作"; titleEn: "controls"; ...按钮 GridLayout 不变... }`。操作按钮的 `contentItem` Text 补 `font.family: Theme.fontFamily`。

- [ ] **Step 3: 异常提示条与结果补字体族**

异常提示条（第 64-80 行）的 Text 补 `font.family: Theme.fontFamily`，配色用新 token（急停 colorEmergency、故障 colorError，已对）。复位 + 结果行（第 160-180 行）同理补字体族。

- [ ] **Step 4: 构建并运行验证**

Run: `cmake --build build` → 成功。Run: `./build/ADASMaster.exe` → 测试执行页：流程条为 StateFlow 三态（走一遍状态机：加载工况→检查→预备→开始→停止，确认✓/蓝/灰着色正确），各区块带标题栏。

- [ ] **Step 5（可选）: Commit**

```bash
git add qml/pages/TestPage.qml
git commit -m "style: 测试执行页套用 StateFlow/PanelFrame"
```

---

## Task 11: 设备管理页 DevicePage + 报警中心 AlarmPage 升级

**Files:**
- Modify: `qml/pages/DevicePage.qml`
- Modify: `qml/pages/AlarmPage.qml`

**Interfaces:**
- Consumes: `PanelFrame`（Task 3）、Theme token。

- [ ] **Step 1: DevicePage 工具栏与字体**

在 `qml/pages/DevicePage.qml` 中：顶部工具栏 `Rectangle`（第 18-56 行）可整体用 `PanelFrame { title:"设备管理"; titleEn:"devices"; ... }` 替换，标题栏右侧放在线统计与"全部连接/断开/复位"按钮。所有 Text 补 `font.family: Theme.fontFamily`，数值（在线/总数）补 `font.family: Theme.fontNumeric`。`GridView` 的 `delegate: DeviceCard` 保持（DeviceCard 已在 Task 8 升级）。

- [ ] **Step 2: AlarmPage 工具栏与列表样式**

在 `qml/pages/AlarmPage.qml` 中：工具栏 `Rectangle`（第 18-40 行）用 `PanelFrame { title:"报警中心"; titleEn:"alarms"; ... "全部确认"按钮 ... }` 替换。列表 delegate（第 53-113 行）：
- 行交替色用 `color: index % 2 === 0 ? Theme.colorPanelAlt : Theme.colorBg`（已在用，token 已自动更新）。
- 等级色条 `Theme.levelColor(alarmLevel)` 保持。
- 所有 Text 补 `font.family: Theme.fontFamily`；时间 Text 补 `font.family: Theme.fontNumeric`。

- [ ] **Step 3: 构建并运行验证**

Run: `cmake --build build` → 成功。Run: `./build/ADASMaster.exe` → 设备管理页工具栏带标题栏、卡片样式统一；报警中心列表交替行、等级色条、等宽时间正常。

- [ ] **Step 4（可选）: Commit**

```bash
git add qml/pages/DevicePage.qml qml/pages/AlarmPage.qml
git commit -m "style: 设备管理页与报警中心套用 PanelFrame"
```

---

## Task 12: 其余页面升级（MapPage / ScenarioPage / DataReplayPage / SettingsPage）

**Files:**
- Modify: `qml/pages/MapPage.qml`
- Modify: `qml/pages/ScenarioPage.qml`
- Modify: `qml/pages/DataReplayPage.qml`
- Modify: `qml/pages/SettingsPage.qml`

**Interfaces:**
- Consumes: `PanelFrame`（Task 3）、Theme token。

> 这四个页面执行前先各自 `Read` 现状，套用统一规则：把外层裸 `Rectangle` 内容区包进 `PanelFrame`（带标题栏），所有 `Text` 补 `font.family: Theme.fontFamily`，数值补 `Theme.fontNumeric`，按钮配色用新 token，状态量用 `levelColor`。

- [ ] **Step 1: MapPage 升级**

Read `qml/pages/MapPage.qml`，把地图占位/控件外包 `PanelFrame { title:"场地地图"; titleEn:"map" }`，叠加的状态信息用 TopStatusBar 风格小条；所有 Text 补字体族。

- [ ] **Step 2: ScenarioPage 升级**

Read `qml/pages/ScenarioPage.qml`，工况列表/卡片用 `PanelFrame` 容器，选中态用 `colorAccent`；Text 补字体族，数值（速度/TTC）用 fontNumeric + levelColor。

- [ ] **Step 3: DataReplayPage 升级**

Read `qml/pages/DataReplayPage.qml`，回放控件 + 时间轴外包 `PanelFrame { title:"数据回放"; titleEn:"replay" }`；Text 补字体族，时间数值用 fontNumeric。

- [ ] **Step 4: SettingsPage 升级**

Read `qml/pages/SettingsPage.qml`，设置分组用 `PanelFrame` 容器；输入控件统一深色描边（背景 `colorHover`、边框 `colorBorder`、圆角 `radius`）；Text 补字体族。

- [ ] **Step 5: 构建并运行验证**

Run: `cmake --build build` → 成功。Run: `./build/ADASMaster.exe` → 逐页点开，四页均带标题栏面板、字体统一、配色一致。

- [ ] **Step 6（可选）: Commit**

```bash
git add qml/pages/MapPage.qml qml/pages/ScenarioPage.qml qml/pages/DataReplayPage.qml qml/pages/SettingsPage.qml
git commit -m "style: 地图/工况/回放/设置页套用 PanelFrame"
```

---

## Task 13: 全局构建验证 + 视觉走查 + CLAUDE.md 更新

**Files:**
- Modify: `CLAUDE.md`（补充视觉规范与组件索引）

- [ ] **Step 1: 全量重新构建**

Run: `cmake --build build`
Expected: 成功，无 warning（除可能的 Qt 自动生成警告）、无错误。

- [ ] **Step 2: 运行并逐页视觉走查**

Run: `./build/ADASMaster.exe`

走查清单（对照 spec 第 12 节验收标准）：
- [ ] 8 页视觉统一为指挥控制中心风格（深蓝灰 #12161d 背景、标题栏面板、工业蓝主色）
- [ ] 关键数据（速度/距离/TTC/安全/设备/测试状态）一眼可读，危险量红色
- [ ] 急停元素全应用一致最醒目（红 + 外发光环 + 呼吸）
- [ ] 图标全套上线，导航/操作识别清晰
- [ ] 数值等宽对齐，刷新不跳动（运行一次测试观察 TTC/距离数字）
- [ ] 状态色 0=绿/1=黄/2=橙/3=红 全应用一致

- [ ] **Step 3: 更新 CLAUDE.md，补充视觉规范与组件索引**

在 `CLAUDE.md` 的 "QML UI 结构" 节之后，新增一节：

```markdown
## 视觉设计规范（B 方向 · 指挥控制中心）

视觉语言定义在 `docs/superpowers/specs/2026-06-18-adasmaster-visual-design.md`，核心要点：
- 配色集中在 `Theme.qml`（深蓝灰背景层级 + 工业蓝主色 + 绿/黄/橙/红状态色）。
- 所有信息区用 `PanelFrame`（标题栏面板，左蓝竖条+标签）作为统一容器。
- 状态卡用 `StatusCard`，指标用 `MetricCell`，流程用 `StateFlow`，图标用 `Icon`/`IconButton`。
- 数值用 `Theme.fontNumeric`（等宽），界面文字用 `Theme.fontFamily`。
- 线性图标（Lucide，白色 SVG + MultiEffect 着色）在 `qml/icons/`，新增图标须登记 CMake `RESOURCES`。
```

- [ ] **Step 4（可选）: Commit**

```bash
git add CLAUDE.md
git commit -m "docs: CLAUDE.md 补充视觉规范与组件索引"
```

---

## 验收

全部任务完成后，应满足 spec 第 12 节验收标准。`docs/superpowers/specs/2026-06-18-adasmaster-visual-design.md` 为权威设计依据，本计划为其实现路径。
