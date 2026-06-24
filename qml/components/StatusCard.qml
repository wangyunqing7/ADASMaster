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
            font.family: Theme.fontFamily
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
