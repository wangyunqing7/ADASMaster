// =============================================================================
// SideNavBar.qml
// 功能：左侧一级导航栏。中文菜单 + 线性图标：设备管理/场地地图/工况管理/
//       报警中心/系统设置（页面合并后由 8 项精简为 5 项）。
// 输入：currentIndex（Main.qml 中页面索引）
// 输出：通过 navClicked(index) 通知主框架切换页面
// =============================================================================
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import ADASMaster

// Bound 模式：delegate 作为独立作用域，访问外部 root 与自身 required 角色统一用 id 限定。
pragma ComponentBehavior: Bound

Rectangle {
    id: root
    color: Theme.colorPanel

    signal navClicked(int index)

    // appController 由 main.cpp setContextProperty 注入，静态分析工具无法识别其类型，故此处行内抑制
    readonly property var se: appController.scenarioEngine // qmllint disable unqualified

    // 一级菜单（顺序与 Main.qml 页面索引一一对应，含线性图标名）
    readonly property var menus: [
        { name: "设备管理", icon: "device" },
        { name: "场地地图", icon: "map" },
        { name: "工况管理", icon: "scenario" },
        { name: "报警中心", icon: "alarm" },
        { name: "系统设置", icon: "settings" },
        { name: "MPC 调试", icon: "test" }
    ]

    property int currentIndex: 0

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: Theme.spacing
        spacing: 0

        Repeater {
            model: root.menus

            delegate: Rectangle {
                id: navItem
                required property var modelData
                required property int index

                Layout.fillWidth: true
                Layout.preferredHeight: 64
                color: root.currentIndex === navItem.index ? Theme.colorAccent : Theme.colorPanel

                Behavior on color { ColorAnimation { duration: 150 } }

                Rectangle {   // 选中态左侧 4px 绿指示条
                    width: 4
                    height: parent.height
                    color: Theme.colorNormal
                    visible: root.currentIndex === navItem.index
                }

                RowLayout {
                    anchors.centerIn: parent
                    spacing: Theme.spacingSm

                    Icon {
                        source: navItem.modelData.icon
                        size: Theme.iconMd
                        opacity: root.currentIndex === navItem.index ? 1.0 : 0.5
                    }
                    Text {
                        text: navItem.modelData.name
                        color: root.currentIndex === navItem.index ? "white" : Theme.colorText
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontNormal
                        font.bold: root.currentIndex === navItem.index
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.navClicked(navItem.index)
                }
            }
        }

        Item { Layout.fillHeight: true }   // 占位撑到底

        // 底部：复位按钮 + 急停快捷按钮
        Button {
            id: resetBtn
            text: "↻ 复位"
            // 仅在测试结束态（完成/故障/急停）可用
            enabled: root.se.testState === 5 || root.se.testState === 6 || root.se.testState === 7
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacing
            Layout.rightMargin: Theme.spacing
            Layout.preferredHeight: 48
            contentItem: Text {
                text: resetBtn.text
                color: resetBtn.enabled ? Theme.colorText : Theme.colorTextMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontNormal
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: resetBtn.enabled ? (resetBtn.down ? Theme.colorAccentHover : Theme.colorHover) : Theme.colorPanelAlt
                radius: Theme.radius
                border.color: resetBtn.enabled ? Theme.colorBorder : Theme.colorPanelAlt
                border.width: 1
            }
            onClicked: root.se.reset()
        }

        EmergencyButton {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacing
            Layout.rightMargin: Theme.spacing
            Layout.bottomMargin: Theme.spacing
            Layout.preferredHeight: 60
            onClicked: root.se.softwareEmergencyStop()
        }
    }

    // 右侧分割线
    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 1
        color: Theme.colorBorder
    }
}
