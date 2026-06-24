// =============================================================================
// Icon.qml
// 功能：线性图标显示组件。加载白色描边 SVG。
//       注：当前 Qt 安装无 QtQuick.Effects（MultiEffect）模块，故不做运行时
//       着色；图标以白色显示，调用方通过 opacity 控制"亮/暗"
//       （如导航选中 1.0 / 未选中 0.5）。
// 输入：source(图标名，自动拼 Theme.iconPrefix 前缀)、size。
//       color 仅作接口占位（保留以兼容），不影响显示。
// 输出：白色图标，亮度由父级 opacity 控制。
// =============================================================================
import QtQuick
import ADASMaster

Item {
    id: root
    property string source: ""          // 图标名，如 "dashboard"
    property color color: Theme.colorTextDim   // 接口占位（无着色，亮度靠 opacity）
    property int size: Theme.iconMd

    implicitWidth: root.size
    implicitHeight: root.size

    Image {
        anchors.fill: parent
        source: Theme.iconPrefix + root.source + ".svg"
        // PreserveAspectFit：等比缩放到 size 框内完整显示。
        // 曾用 Pad（不缩放），导致 24×24 的 SVG 原样画进 20×20 框被裁掉边缘、只露中间。
        fillMode: Image.PreserveAspectFit
        sourceSize.width: root.size * 2   // 2x 提升清晰度
        sourceSize.height: root.size * 2
    }
}
