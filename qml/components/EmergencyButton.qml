// =============================================================================
// EmergencyButton.qml
// 功能：软件急停按钮。大尺寸、红色、醒目，外层半透明发光环 + 呼吸高亮，
//       符合测试现场"一键停止"要求。
// 重要：软件急停只是软件层停止指令，不能替代硬件急停系统。
// 输入：clicked 信号由父级连接到 ScenarioEngine.softwareEmergencyStop()。
// 说明：改为 Item 自绘（光环+主体+文字+MouseArea），以便叠加外发光环，
//       对外保持 clicked() 信号与 Layout 兼容。
// =============================================================================
import QtQuick
import ADASMaster

Item {
    id: root
    signal clicked()

    implicitWidth: 160
    implicitHeight: 64

    // 外发光环：比主体略大、半透明红，透出边缘光环
    Rectangle {
        anchors.centerIn: parent
        width: parent.width + 4
        height: parent.height + 4
        radius: Theme.radiusLg + 2
        color: Theme.colorEmergency
        opacity: 0.25
    }

    // 主体
    Rectangle {
        id: body
        anchors.fill: parent
        radius: Theme.radiusLg
        color: ma.pressed ? Qt.darker(Theme.colorEmergency, 1.3)
             : ma.containsMouse ? Qt.lighter(Theme.colorEmergency, 1.1)
             : Theme.colorEmergency
        border.color: Theme.colorEmergencyHi
        border.width: 2

        Text {
            anchors.centerIn: parent
            text: "软件急停"
            color: "white"
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontLarge
            font.bold: true
        }
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    // 非按下时持续呼吸式高亮，吸引注意
    SequentialAnimation on opacity {
        loops: Animation.Infinite
        NumberAnimation { to: 0.75; duration: 900 }
        NumberAnimation { to: 1.0;  duration: 900 }
    }
}
