// =============================================================================
// IconButton.qml
// 功能：图标 + 文字按钮。用于导航项、操作按钮。颜色由 accent 决定，
//       通过 MultiEffect 着色，故图标颜色随 accent 变化。
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
    property int size: Theme.iconMd
    signal clicked()

    implicitHeight: row.implicitHeight
    implicitWidth: row.implicitWidth
    opacity: root.enabled ? 1.0 : 0.4

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: Theme.spacingSm

        Icon {
            source: root.iconName
            color: root.accent
            size: root.size
            Layout.alignment: Qt.AlignVCenter
        }

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
