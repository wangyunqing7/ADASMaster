# ADASMaster 视觉设计规范

- **日期**：2026-06-18
- **状态**：待评审 / 待实现
- **方向**：B · 指挥控制中心（SCADA HMI / 航天指挥气质），吸收 A · 精密仪表盘的数据可读性
- **主题**：深色单主题
- **落地范围**：视觉层改造（Theme.qml + 现有组件/页面样式 + 全套线性图标），不动布局结构与后端

> 本文件是 ADASMaster 视觉设计探索（brainstorming）的产出，指导后续 Theme.qml 与 QML 组件样式实现。配套可视化 mockup 见 `.superpowers/brainstorm/`（运行监控页 dashboard-b、测试执行页 testpage-b）。

---

## 1. 背景与目标

ADASMaster 是 ADAS 场地测试工控上位机。当前视觉偏扁平朴素：裸色块 + 纯文字、全程无图标、关键数据（速度/距离/TTC）以裸数字呈现、卡片样式高度同质。

本次探索在三个方向（A 精密仪表盘 / B 指挥控制中心 / C 现代科技深色）中选定 **B**，并融合 A 的数据可读性。目标：在不改动布局结构与后端的前提下，对视觉层系统升级，呈现专业、稳重、**状态与安全优先**的工控指挥气质，同时让关键数据一眼可读。

## 2. 设计哲学

1. **状态与安全优先**：安全等级、急停、设备在线、测试状态是第一视觉层级，用实色徽章 + 状态色着色确保醒目。
2. **模块化分区**：所有信息区以"带标题栏的面板"为统一容器，秩序清晰，如指挥中心的多功能屏。
3. **数据可读性（吸收 A）**：所有数值大号 + 等宽对齐（tabular-nums）+ 状态色着色；危险量（距离/TTC）变红警示。
4. **克制与硬核**：小圆角、细边框、无渐变辉光（区别于 SaaS 风），保持工控硬核感。

## 3. 范围

**包含**
- Theme.qml token 扩展与微调
- 现有 QML 组件样式升级（面板 / 状态卡 / 指标网格 / 按钮 / 流程条 / 状态灯 / 急停 / 导航 / 状态栏 / 日志栏）
- 新增全套线性图标体系
- 抽取可复用组件（PanelHeader / StatusCard / MetricGrid 等）

**不包含**
- 页面布局结构重排、信息架构变更
- 后端逻辑（Manager / 状态机 / 数据）
- 双主题 / 浅色 / 户外高亮模式
- 新增数据可视化（图表 / 仪表盘 / 轨迹回放——属下一阶段）

## 4. 色板（深色单主题）

### 4.1 背景层级
| Token | 值 | 用途 |
|---|---|---|
| `colorBg` | `#12161d` | 最外层应用背景 |
| `colorPanel` | `#1b212b` | 面板 / 卡片 |
| `colorPanelAlt` | `#161a22` | 凹陷 / 交替区（未到步骤、列表交替行） |
| `colorHover` | `#242b36` | 悬停 / 可点击底色 |
| `colorTitleBar` | `#232a35` | 面板标题栏（**新增**） |

### 4.2 边框 / 分隔
| Token | 值 | 用途 |
|---|---|---|
| `colorBorder` | `#2a3340` | 1px 标准边框 |
| `colorDivider` | `#2a3340` | 网格分隔线 |

### 4.3 文字层级
| Token | 值 | 用途 |
|---|---|---|
| `colorText` | `#e6e6e6` | 主文字 |
| `colorTextDim` | `#9aa3ad` | 次要文字（标签、副信息） |
| `colorTextMuted` | `#6a6c70` | 弱化文字（单位、占位） |

### 4.4 主色与状态色（保留现状，强化使用规范）
| Token | 值 | 用途 |
|---|---|---|
| `colorAccent` | `#2d6a9f` | 工业蓝：选中态 / 当前端 / 主操作 / 面板标题竖条 |
| `colorAccentHover` | `#357eb0` | 主色悬停 |
| `colorNormal` | `#2ecc71` | 绿：正常 / 已完成 |
| `colorWarning` | `#f1c40f` | 黄：警告 / 停止操作 |
| `colorError` | `#e67e22` | 橙：错误 / 故障 |
| `colorEmergency` | `#e74c3c` | 红：急停 / 碰撞风险 |
| `colorInfo` | `#3498db` | 蓝：信息 |

> 状态色严格遵循工控约定 `0=绿 1=黄 2=橙 3=红`，`Theme.levelColor(level)` 行为不变。

## 5. 排版

- **界面字体**：`'Microsoft YaHei','Segoe UI',system-ui,sans-serif`
- **数值字体**：叠加等宽数字特性——QML 用 `font.features: ["tnum"]`（Qt 6 支持 OpenType feature）或选用含 tabular figures 的字体，确保数字等宽对齐、不跳动。
- **字号阶梯**（保留 Theme）：tiny 10 / small 12 / normal 14 / medium 16 / large 20 / huge 28。
- **字重**：标签 500，数值 700–800，标题 700。
- **大写英文标签**：面板标题栏 / 指标标签可用全大写 + `letter-spacing`（指挥控制感）；中文标签正常。

## 6. 间距与圆角

- **圆角**（微调，更硬朗）：`radius` 5（面板 / 按钮）、`radiusLg` 8（急停 / 外层容器）。
- **间距**（保留）：spacingTiny 4 / spacingSm 8 / spacing 12 / spacingLg 16 / spacingXl 24。

## 7. 图标体系（全套线性图标）

统一**线性（stroke）**风格，1.5–2px 描边，颜色随文字或状态色。建议以 SVG（`Qt.Svg`）或图标字体实现，集中放 `qml/icons/`。

- **导航**：运行监控(仪表盘) / 设备管理(芯片) / 场地地图(地图) / 工况管理(列表) / 测试执行(播放) / 数据回放(回放) / 报警中心(警铃) / 系统设置(齿轮)
- **操作**：加载(刷新) / 检查(齿轮) / 预备(播放) / 开始(三角▶) / 停止(方块■) / 急停(警告⚠) / 复位(回旋↻)
- **状态**：正常 / 警告 / 错误 / 急停（与状态灯色配合）

图标尺寸 16 / 20 / 24，与文字基线对齐。`Theme` 提供 `iconSm/iconMd/iconLg` 尺寸 token。

## 8. 核心组件规范

### 8.1 面板 Panel（统一容器）
- 背景 `colorPanel`，1px `colorBorder`，圆角 5。
- **标题栏**：高 30，背景 `colorTitleBar`，底部 1px `colorBorder` 分隔；左侧 3px 宽工业蓝竖条（`colorAccent`）；标签 11px 大写 + letter-spacing；右侧可挂状态点 / 实时指示（如 `● LIVE 10Hz`）。
- 内容区内边距 12–16。

### 8.2 状态卡 StatusCard（运行监控六卡）
- 面板结构。内容：小标签（10px muted）+ 大号状态色数值（huge / 800）+ 副信息（small muted）。
- **活跃 / 重点卡**（如"运行中"测试卡）：边框改 `colorAccent` 2px，标题栏改深蓝 `#2a3a4a`，让视线锁定当前重点。

### 8.3 指标网格 MetricGrid（运动学）
- 面板内 1px 网格分隔（容器背景 `colorBorder`，每格 `colorPanel`）。
- 每格：小标签 + 大号数值（large / 700，tabular-nums）+ 小单位（muted）。
- **危险量着色**：距离 < 5m 或 TTC < 2s 用 `colorEmergency`；正常量 `colorText`。

### 8.4 按钮 Button
- **默认**：背景 `colorHover`，1px `colorBorder`，文字 `colorText`，高 44–54，图标 + 文字 gap 8。
- **开始**：2px `colorNormal` 边框，文字 `colorNormal`。
- **停止**：2px `colorWarning` 边框，文字 `colorWarning`，淡黄底 `rgba(241,196,15,.08)`。
- **急停**：背景 `colorEmergency`，白字，1px 高亮边框 `#ff6b5e` + 外发光环（3px `rgba(231,76,60,.18)`），最大尺寸，呼吸动画（保留现状）。
- **禁用**：opacity 0.4，灰边框，不可点击。

### 8.5 流程条 StateFlow（测试执行页）
- 横向：圆形步骤（直径 42）+ 连线。
- **已完成**：深底 + 绿勾 + 绿名。
- **当前**：工业蓝填充 + 3px 高亮边框 `#5a9fd4` + 光晕环 + 白字 + 加粗名。
- **未到**：`colorPanelAlt` 底 + 灰边框 + 灰字。
- **故障态**：步骤变 `colorError`；**急停态**：变 `colorEmergency`。
- 连线统一 `colorBorder`（中性，已完成/未到不强调连线）。

### 8.6 状态灯 StatusLamp
- 保留现状：圆点 + 文字。`level >= 2` 发光，`level >= 3` 闪烁。点色 `levelColor(level)`。

### 8.7 顶部状态栏 TopStatusBar
- 深底 `colorPanel`，高 52，底部 1px 分隔。
- 左标题。右侧状态项序列：安全灯 / RTK / 设备 / 工况 / 测试 / 时间，每项"点 + 文字"。
- 急停激活：整条叠加 `colorEmergency` 透明层（opacity .18，保留现状动效）。

### 8.8 左导航 SideNavBar
- 宽 176，`colorPanel` 底，右侧 1px 分隔。
- 菜单项：高 42，**图标 + 文字**。选中：工业蓝底 + 白字 + 左侧 4px 绿指示条（保留现状好设计）。
- 底部：急停快捷按钮（同 8.4 急停规范）。

### 8.9 底部日志栏 BottomLogBar
- `colorPanel` 底，高 34，顶部 1px 分隔。最新事件：级别标签（彩色小 pill）+ 文字 + 右侧时间（tabular-nums）。

## 9. 状态色应用规则

- 安全等级、测试状态、设备在线比、RTK、TTC、距离等，凡有"正常 → 危险"语义的量，一律按 `levelColor(0..3)` 着色。
- 文字色随状态变（绿=好，橙/红=需关注），不只靠图标。
- 急停为最高级：任何急停触发整页红色叠加 + 锁定操作（保留现状安全逻辑）。

## 10. 页面级落地指引

- **运行监控页、测试执行页**：已用本规范 mockup 验证（dashboard-b / testpage-b），作为样板。
- **设备管理页、报警中心页**（列表型）：行用 `colorPanel` / `colorPanelAlt` 交替，状态列用状态灯 / 徽章，操作列用 8.4 按钮。
- **场地地图页、数据回放页**：地图 / 回放控件外包 8.1 面板；叠加的状态 / 时间轴信息用 8.7 状态栏样式。
- **工况管理页、系统设置页**：表单 / 列表外包面板，输入控件统一深色描边样式。

## 11. Theme.qml 改造点

在现有 `Theme.qml` 基础上：
- 调整：`colorBg → #12161d`、`colorPanel → #1b212b`、`colorPanelAlt → #161a22`。
- 新增：`colorTitleBar = #232a35`、活跃面板标题色 `colorTitleBarActive = #2a3a4a`、急停高亮边 `colorEmergencyHi = #ff6b5e`。
- 微调：`radius → 5`、`radiusLg → 8`。
- 新增：图标相关 token（`iconSm/Md/Lg`、图标路径前缀）。
- 保留并复用：`levelColor()`、全部状态色、字号阶梯、间距阶梯。
- 新增可复用组件（`qml/components/`）：`PanelFrame.qml`（标题栏面板）、`StatusCard.qml`、`MetricCell.qml`、`StateFlow.qml`、`IconButton.qml`。

## 12. 验收标准

- 全应用 8 页视觉统一为 B 方向指挥控制中心风格。
- 关键数据（速度 / 距离 / TTC / 安全 / 设备 / 测试状态）一眼可读，危险量红色警示。
- 急停元素全应用一致、最醒目。
- 图标全套上线，导航 / 操作识别清晰。
- 所有数值等宽对齐，刷新时不跳动。

## 13. 风险与备注

- `font.features: ["tnum"]` 需 Qt6 支持；若目标字体无 tabular figures，退化为选用等宽数字字体或接受轻微跳动。
- 线性图标若用 SVG，需确认 Qt Svg 模块已在构建中（当前 `find_package` 仅 Quick/Qml，可能需补 `Svg`）。
- 本规范仅视觉层，不改变 `DeviceManager`/`SafetyManager`/`ScenarioEngine` 等后端行为与信号槽接线。
