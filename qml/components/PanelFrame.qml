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
                    Rectangle { Layout.preferredWidth: 6; Layout.preferredHeight: 6; radius: 3; color: root.liveColor }
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
            Layout.preferredHeight: 1
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
