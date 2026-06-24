// =============================================================================
// SettingsPage.qml
// 功能：系统设置页面。展示用户角色、路径、网络、版本等信息。
// 布局：单列 ColumnLayout 直接排列，收紧间距/内边距后 5 个面板一屏内全部展示，
//       无需滚动。信息不减，仅压缩垂直留白。
// 第一阶段：只读展示 + 角色切换。后续扩展权限管理、数据备份。
// =============================================================================
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import ADASMaster

Item {
    id: root

    // appController 由 main.cpp setContextProperty 注入，静态分析工具无法识别其类型，故此处行内抑制
    readonly property var am: appController.alarmManager // qmllint disable unqualified
    readonly property var auth: appController.authManager // qmllint disable unqualified

    // 直接用 ColumnLayout 排列：内容精简(收紧 spacing/contentPadding)后 5 个面板可一屏放下。
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacingSm

        // ---- 用户与权限 ----
        PanelFrame {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacingSm; Layout.rightMargin: Theme.spacingSm
            contentPadding: Theme.spacingSm
            title: "用户与权限"; titleEn: "user"
            RowLayout {
                Layout.fillWidth: true
                Text { text: "当前用户："; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal }
                Text {
                    text: root.auth.currentDisplayName
                    color: Theme.colorText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontNormal
                    font.bold: true
                }
                Item { Layout.fillWidth: true }
                StatusLamp {
                    level: root.auth.isLoggedIn ? 0 : 3
                    text: root.auth.isLoggedIn ? "已登录" : "未登录"
                    dotSize: 9
                }
            }
        }

        // ---- 路径配置 ----
        PanelFrame {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacingSm; Layout.rightMargin: Theme.spacingSm
            contentPadding: Theme.spacingSm
            title: "路径配置"; titleEn: "paths"
            GridLayout {
                Layout.fillWidth: true; columns: 2; rowSpacing: Theme.spacingSm; columnSpacing: Theme.spacingLg
                PathRow { label: "设备配置路径"; value: "config/devices.json" }
                PathRow { label: "工况配置路径"; value: "scenarios/" }
                PathRow { label: "日志保存路径"; value: "logs/" }
                PathRow { label: "数据回放路径"; value: "data/" }
            }
        }

        // ---- 网络参数 ----
        PanelFrame {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacingSm; Layout.rightMargin: Theme.spacingSm
            contentPadding: Theme.spacingSm
            title: "网络参数"; titleEn: "network"
            GridLayout {
                Layout.fillWidth: true; columns: 2; rowSpacing: Theme.spacingSm; columnSpacing: Theme.spacingLg
                PathRow { label: "控制网段"; value: "192.168.1.0/24" }
                PathRow { label: "RTK 差分源"; value: "192.168.1.20:5010 (NTRIP)" }
                PathRow { label: "通信超时"; value: "3000 ms" }
                PathRow { label: "心跳周期"; value: "1000 ms" }
            }
        }

        // ---- 单位与版本 ----
        PanelFrame {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacingSm; Layout.rightMargin: Theme.spacingSm
            contentPadding: Theme.spacingSm
            title: "单位与版本"; titleEn: "version"
            GridLayout {
                Layout.fillWidth: true; columns: 2; rowSpacing: Theme.spacingSm; columnSpacing: Theme.spacingLg
                PathRow { label: "速度单位"; value: "m/s（界面显示 km/h）" }
                PathRow { label: "距离单位"; value: "m" }
                PathRow { label: "软件版本"; value: "ADASMaster v0.1.0" }
                PathRow { label: "协议版本"; value: "Proto v1.0（模拟）" }
            }
        }

        // ---- 数据备份 ----
        PanelFrame {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacingSm; Layout.rightMargin: Theme.spacingSm
            contentPadding: Theme.spacingSm
            title: "数据备份"; titleEn: "backup"
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    id: backupButton
                    text: "备份数据"; Layout.preferredHeight: 36
                    enabled: root.auth.currentRole >= 2
                    contentItem: Text { text: backupButton.text; color: Theme.colorText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: backupButton.enabled ? (backupButton.down ? Theme.colorAccentHover : Theme.colorHover) : Theme.colorDisable; radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
                    onClicked: root.am.info("数据备份", "数据备份功能将在后续版本实现")
                }
            }
        }

        // 底部弹簧：面板顶部对齐，剩余空间在下方留白
        Item { Layout.fillHeight: true }
    }

    component PathRow : RowLayout {
        id: pathRow
        property string label: ""
        property string value: ""
        spacing: Theme.spacing
        Text { text: pathRow.label + "："; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; Layout.preferredWidth: 130 }
        Text { text: pathRow.value; color: Theme.colorText; font.family: Theme.fontNumeric; font.pixelSize: Theme.fontSmall; Layout.fillWidth: true }
    }
}
