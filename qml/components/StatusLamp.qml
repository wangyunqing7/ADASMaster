// =============================================================================
// StatusLamp.qml
// 功能：状态指示灯组件。圆点 + 文字，颜色随等级变化。
// 用途：顶部状态栏、设备状态、安全状态等所有需要醒目状态显示的位置。
// 输入：level(0正常/1警告/2错误/3急停)、text、可选是否闪烁。
// =============================================================================

import QtQuick
import QtQuick.Layouts
import ADASMaster

Item {
    id: root
    // 0=正常绿 1=警告黄 2=错误橙 3=急停红
    property int level: 0
    property string text: ""
    property bool blinking: level >= 3   // 急停默认闪烁
    property int dotSize: 12
    implicitHeight: row.implicitHeight
    implicitWidth: row.implicitWidth

    RowLayout {
        id: row
        anchors.fill: parent
        spacing: Theme.spacingSm

        Rectangle {
            id: dot
            radius: width / 2
            Layout.preferredWidth: root.dotSize
            Layout.preferredHeight: root.dotSize
            color: Theme.levelColor(root.level)
            // 发光效果，急停/错误时更醒目
            layer.enabled: root.level >= 2
            SequentialAnimation on opacity {
                running: root.blinking
                loops: Animation.Infinite
                NumberAnimation { to: 0.25; duration: 400 }
                NumberAnimation { to: 1.0;  duration: 400 }
            }
        }

        Text {
            text: root.text
            color: Theme.colorText
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontNormal
            font.bold: root.level >= 2
            Layout.fillWidth: true
        }
    }
}
