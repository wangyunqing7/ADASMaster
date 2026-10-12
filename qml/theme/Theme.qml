pragma Singleton
// =============================================================================
// Theme.qml
// 功能：全局主题单例。统一管理深色工控风格的颜色、字号、间距。
//       状态颜色严格遵循：正常=绿，警告=黄，错误=橙，急停=红。
// 用法：在任意 QML 文件中 `import ADASMaster`，然后 `Theme.colorPanel` 等。
// =============================================================================
import QtQuick

QtObject {
    // ---- 背景与面板 ----
    readonly property color colorBg:         "#12161d"   // 最外层应用背景
    readonly property color colorPanel:      "#1b212b"   // 面板/卡片
    readonly property color colorPanelAlt:   "#161a22"   // 凹陷/交替区
    readonly property color colorHover:      "#35373c"   // 悬停
    readonly property color colorBorder:     "#2a3340"   // 边框
    readonly property color colorDivider:    "#2a3340"   // 分割线

    // ---- 文字 ----
    readonly property color colorText:       "#e6e6e6"   // 主文字
    readonly property color colorTextDim:    "#9aa3ad"   // 次要文字（标签、副信息）
    readonly property color colorTextMuted:  "#6a6c70"   // 弱化文字（单位、占位）

    // ---- 字体族 ----
    // Qt font.family 接受字体名称，不接受 CSS 字体列表。
    readonly property string fontFamily:  "Microsoft YaHei"
    readonly property string fontNumeric: "Consolas"  // 数值等宽，刷新不跳动

    // ---- 主色 / 操作 ----
    readonly property color colorAccent:     "#2d6a9f"   // 工业蓝（主操作）
    readonly property color colorAccentHover:"#357eb0"
    readonly property color colorDisable:    "#4a4c52"
    readonly property color colorTitleBar:       "#232a35"   // 面板标题栏
    readonly property color colorTitleBarActive: "#2a3a4a"   // 活跃面板标题栏
    readonly property color colorEmergencyHi:    "#ff6b5e"   // 急停高亮描边

    // ---- 状态色（工控约定，醒目为先）----
    readonly property color colorNormal:     "#2ecc71"   // 绿 正常
    readonly property color colorWarning:    "#f1c40f"   // 黄 警告
    readonly property color colorError:      "#e67e22"   // 橙 错误
    readonly property color colorEmergency:  "#e74c3c"   // 红 急停
    readonly property color colorInfo:       "#3498db"   // 蓝 信息

    // ---- 字号 ----
    readonly property int fontTiny:    10
    readonly property int fontSmall:   12
    readonly property int fontNormal:  14
    readonly property int fontMedium:  16
    readonly property int fontLarge:   20
    readonly property int fontHuge:    28

    // ---- 间距与圆角 ----
    readonly property int radius:      5
    readonly property int radiusLg:    8
    readonly property int spacingTiny: 4
    readonly property int spacingSm:   8
    readonly property int spacing:     12
    readonly property int spacingLg:   16
    readonly property int spacingXl:   24

    // ---- 图标 ----
    readonly property int iconSm: 16
    readonly property int iconMd: 20
    readonly property int iconLg: 24
    readonly property string iconPrefix: "qrc:/qt/qml/ADASMaster/qml/icons/"  // SVG 图标资源前缀（qrc alias 含 qml/ 前缀）

    // 按安全/测试等级(0..3)返回对应颜色
    function levelColor(level: int): color {
        switch (level) {
            case 0: return colorNormal;
            case 1: return colorWarning;
            case 2: return colorError;
            case 3: return colorEmergency;
            default: return colorNormal;
        }
    }
}
