// =============================================================================
// BottomLogBar.qml
// 功能：底部信息栏。常驻显示最新报警、最新事件、通信状态、简要日志。
// 数据源：appController.alarmManager.latestEvent / safetyManager / deviceManager
// =============================================================================
import QtQuick
import QtQuick.Layouts
import ADASMaster

Rectangle {
    id: root
    color: Theme.colorPanel
    height: 44

    // appController 由 main.cpp setContextProperty 注入，静态分析工具无法识别其类型，
    // 故 ctrl 定义处行内抑制；am/sm/dm 经 ctrl 派生，不触达 appController。
    readonly property var ctrl: appController // qmllint disable unqualified
    readonly property var am: root.ctrl.alarmManager
    readonly property var sm: root.ctrl.safetyManager
    readonly property var dm: root.ctrl.deviceManager

    // 最新报警着色：Emergency 红，Error 橙，Warning 黄，Info 蓝
    function logColor(): color {
        const s = root.am.latestEvent;
        if (s.indexOf("[急停]") >= 0) return Theme.colorEmergency;
        if (s.indexOf("[错误]") >= 0) return Theme.colorError;
        if (s.indexOf("[警告]") >= 0) return Theme.colorWarning;
        return Theme.colorInfo;
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingLg
        anchors.rightMargin: Theme.spacingLg
        spacing: Theme.spacingLg

        // 最新事件
        StatusLamp {
            level: root.sm.safetyLevel >= 2 ? root.sm.safetyLevel : 0
            text: root.am.latestEvent.length > 0
                  ? root.am.latestEvent : "系统就绪"
            Layout.fillWidth: true
        }

        // 通信状态
        Text {
            text: "通信：" + (root.dm.onlineCount >= 4 ? "正常" : "异常")
            color: root.dm.onlineCount >= 4 ? Theme.colorNormal : Theme.colorError
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
            Layout.preferredWidth: 120
        }

        // 报警计数
        Text {
            text: "告警：" + root.am.alarmListModel.count + " 条"
            color: Theme.colorTextDim
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall
            Layout.preferredWidth: 120
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Theme.colorBorder
    }
}
