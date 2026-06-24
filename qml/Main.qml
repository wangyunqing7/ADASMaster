// =============================================================================
// Main.qml
// 功能：主界面框架。整合顶部状态栏、左侧导航栏、中间页面切换区、底部日志栏。
// 页面切换：SideNavBar.navClicked -> 更新 currentIndex -> StackLayout 切换。
// 后端入口：全局 appController（在 main.cpp 中注入 context property）。
// =============================================================================

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import ADASMaster

ApplicationWindow {
    id: window
    // 固定窗口尺寸：令 minimum == maximum，用户无法拖拽缩放，最大化也保持该尺寸。
    // 调整尺寸只需修改下面两个变量。
    readonly property int fixedW: 1280
    readonly property int fixedH: 1000
    width: fixedW
    height: fixedH
    minimumWidth: fixedW
    minimumHeight: fixedH
    maximumWidth: fixedW
    maximumHeight: fixedH
    // 无边框窗口：去掉系统标题栏，由顶部状态栏自绘（空白处可拖动 + 右侧退出按钮）
    flags: Qt.Window | Qt.FramelessWindowHint
    visible: true
    title: "ADAS 场地测试主控系统"
    color: Theme.colorBg

    // 默认深色主题
    palette.window: Theme.colorBg
    palette.windowText: Theme.colorText
    palette.base: Theme.colorPanelAlt
    palette.text: Theme.colorText
    palette.button: Theme.colorHover
    palette.buttonText: Theme.colorText
    palette.highlight: Theme.colorAccent
    palette.highlightedText: "white"

    // 当前页面索引（0~4，与一级菜单顺序一致）
    property int currentPage: 0

    // appController 由 main.cpp setContextProperty 注入；ctrl 统一引用以便 qmllint 抑制
    readonly property var ctrl: appController // qmllint disable unqualified

    // 启动准入：未登录显示 LoginView，登录后由主界面 ColumnLayout 接管
    LoginView {
        anchors.fill: parent
        visible: !window.ctrl.authManager.isLoggedIn
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        visible: window.ctrl.authManager.isLoggedIn

        // 顶部状态栏
        TopStatusBar {
            Layout.fillWidth: true
        }

        // 中间：左导航 + 主内容
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            SideNavBar {
                Layout.preferredWidth: 150
                Layout.fillHeight: true
                currentIndex: window.currentPage
                onNavClicked: function(idx) { window.currentPage = idx }
            }

            // 右内容区：StackLayout 占 3/4，VehiclePlatformStatusMonitor 占 1/4
            ColumnLayout {
                id: rightContent
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // 上：页面切换区（占 3/4）
                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: rightContent.height * 3 / 4
                    currentIndex: window.currentPage

                    DevicePage { Layout.fillWidth: true; Layout.fillHeight: true }
                    MapPage { Layout.fillWidth: true; Layout.fillHeight: true }
                    ScenarioPage { Layout.fillWidth: true; Layout.fillHeight: true }
                    AlarmPage { Layout.fillWidth: true; Layout.fillHeight: true }
                    SettingsPage { Layout.fillWidth: true; Layout.fillHeight: true }
                }

                // 下：主车 / 目标车平台状态监测（占 1/4）
                VehiclePlatformStatusMonitor {
                    Layout.fillWidth: true
                    Layout.preferredHeight: rightContent.height / 4
                }
            }
        }

        // 底部日志栏
        BottomLogBar {
            Layout.fillWidth: true
        }
    }
}
