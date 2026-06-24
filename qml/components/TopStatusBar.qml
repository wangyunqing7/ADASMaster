// =============================================================================
// TopStatusBar.qml
// 功能：常驻顶部状态栏。显示安全状态、RTK状态、设备在线数、当前工况、
//       测试状态、当前时间。安全状态醒目着色，急停时整条变红。
//       无边框窗口下：顶栏空白处可拖动移动窗口，最右侧为退出按钮。
// 数据源：appController.safetyManager / stateManager / deviceManager
// =============================================================================
import QtQuick
import QtQuick.Window
import QtQuick.Dialogs
import QtQuick.Layouts
import ADASMaster

Rectangle {
    id: root
    color: Theme.colorPanel
    height: 52
    // 防御性裁剪：状态块收缩到底线仍不够的极端情况下，不让内容画出顶栏边界
    clip: true

    // appController 由 main.cpp setContextProperty 注入，静态分析工具无法识别其类型，
    // 故 ctrl 定义处行内抑制；sm/stm/dm 经 ctrl 派生，不触达 appController。
    readonly property var ctrl: appController // qmllint disable unqualified
    readonly property var sm: root.ctrl.safetyManager
    readonly property var stm: root.ctrl.stateManager
    readonly property var dm: root.ctrl.deviceManager

    // 无边框窗口拖动区：顶栏空白处按下即触发系统级窗口移动(startSystemMove)。
    // 退出按钮等交互控件在更上层（后声明），其点击不会被这里拦截。
    MouseArea {
        anchors.fill: parent
        onPressed: {
            if (Window.window)
                Window.window.startSystemMove();
        }
    }

    // 急停时整条高亮红色，强化警示
    Rectangle {
        anchors.fill: parent
        color: Theme.colorEmergency
        opacity: {
            const lvl = root.sm.safetyLevel;
            return lvl >= 3 ? 0.18 : 0.0;
        }
        Behavior on opacity { NumberAnimation { duration: 200 } }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingLg
        anchors.rightMargin: Theme.spacingLg
        // 间距收紧(24->16)，单这一项即可腾出 56px 布局空间
        spacing: Theme.spacingLg

        // 系统标题（不参与伸缩，按内容宽度贴左）
        Text {
            text: "ADAS 场地测试主控系统"
            color: Theme.colorText
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontMedium
            font.bold: true
        }

        // 右侧状态块组：全部 fillWidth 按比例瓜分标题之后的剩余宽度。
        // 空间不足时整体按比例收缩(minimumWidth 兜底)，任何情况下都不溢出。

        // 安全状态
        StatusLamp {
            level: root.sm.safetyLevel
            text: "安全：" + root.sm.levelText
            Layout.fillWidth: true
            Layout.preferredWidth: 150
            Layout.minimumWidth: 100   // 收缩底线
        }

        // RTK 状态
        StatusLamp {
            level: {
                const m = root.stm.rtkMode;
                if (m === "Fixed") return 0;
                if (m === "Float") return 1;
                return 2;
            }
            text: "RTK：" + root.stm.rtkMode
                       + " (" + root.stm.satelliteCount + ")"
            Layout.fillWidth: true
            Layout.preferredWidth: 190
            Layout.minimumWidth: 130   // 收缩底线
        }

        // 设备在线
        Text {
            text: "设备：%1 / %2".arg(root.dm.onlineCount)
                       .arg(root.dm.totalCount)
            color: {
                const ratio = root.dm.totalCount > 0
                    ? root.dm.onlineCount / root.dm.totalCount : 0;
                return ratio >= 0.8 ? Theme.colorNormal
                     : ratio >= 0.5 ? Theme.colorWarning : Theme.colorError;
            }
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontNormal
            font.bold: true
            Layout.fillWidth: true
            Layout.preferredWidth: 100
            Layout.minimumWidth: 60    // 收缩底线
        }

        // 当前工况
        Text {
            text: "工况：" + root.stm.scenarioName
            color: Theme.colorText
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontNormal
            Layout.fillWidth: true
            Layout.preferredWidth: 200
            Layout.minimumWidth: 110   // 收缩底线；配 elide 超长省略
            elide: Text.ElideRight
        }

        // 测试状态
        Text {
            text: "测试：" + root.stm.testStateText
            color: {
                const s = root.stm.testState;
                if (s === 4) return Theme.colorNormal;   // Running
                if (s === 6) return Theme.colorError;    // Fault
                if (s === 7) return Theme.colorEmergency;// Emergency
                return Theme.colorTextDim;
            }
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontNormal
            font.bold: true
            Layout.fillWidth: true
            Layout.preferredWidth: 140
            Layout.minimumWidth: 95    // 收缩底线
            elide: Text.ElideRight
        }

        // 当前时间（等宽，刷新不跳动）
        Text {
            text: root.ctrl.currentTime
            color: Theme.colorTextDim
            font.family: Theme.fontNumeric
            font.pixelSize: Theme.fontNormal
            Layout.fillWidth: true
            Layout.preferredWidth: 170
            Layout.minimumWidth: 115   // 收缩底线（等宽时间，固定长度）
            horizontalAlignment: Text.AlignRight
        }

        // 注销按钮：回登录页换人；点击弹确认框防误触
        Rectangle {
            Layout.preferredWidth: 40
            Layout.fillHeight: true
            color: logoutMa.containsMouse ? Theme.colorError : "transparent"
            Behavior on color { ColorAnimation { duration: 120 } }

            Icon {
                anchors.centerIn: parent
                source: "logout"
                size: Theme.iconMd
                opacity: logoutMa.containsMouse ? 1.0 : 0.7
            }
            MouseArea {
                id: logoutMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: logoutConfirm.open()
            }
        }

        // 退出按钮：无边框窗口的自定义关闭控件；hover 红底提示，点击关闭应用
        Rectangle {
            Layout.preferredWidth: 40
            Layout.fillHeight: true
            color: quitMa.containsMouse ? Theme.colorEmergency : "transparent"
            Behavior on color { ColorAnimation { duration: 120 } }

            Icon {
                anchors.centerIn: parent
                source: "power"
                size: Theme.iconMd
                opacity: quitMa.containsMouse ? 1.0 : 0.7
            }

            MouseArea {
                id: quitMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.quit()
            }
        }
    }

    // 注销确认：点 Ok(accepted) 触发注销，Cancel(rejected) 仅关闭
    MessageDialog {
        id: logoutConfirm
        title: "注销"
        text: "确定要注销当前用户并返回登录界面吗？"
        buttons: MessageDialog.Ok | MessageDialog.Cancel
        onAccepted: root.ctrl.authManager.logout()
    }

    // 底部分割线
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.colorBorder
    }
}
