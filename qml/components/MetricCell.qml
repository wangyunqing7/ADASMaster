// =============================================================================
// MetricCell.qml
// 功能：指标格。自带 colorPanel 背景，置于 1px 网格容器（背景 colorBorder、
//       spacing 1）内即形成分隔线。小标签 + 大号等宽数值 + 单位。
// 输入：label/value/unit/valueColor。
// =============================================================================
import QtQuick
import QtQuick.Layouts
import ADASMaster

Rectangle {
    id: root
    property string label: ""
    property string value: ""
    property string unit: ""
    property color valueColor: Theme.colorText

    color: Theme.colorPanel

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing
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
}
