// =============================================================================
// DeviceCard.qml
// 功能：设备卡片组件，用于设备管理页面。显示一台设备的全部状态字段。
// 输入：由 DevicePage ListView 注入 model.* 角色。
// =============================================================================
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import ADASMaster

Rectangle {
    id: root
    color: Theme.colorPanelAlt
    radius: Theme.radius
    border.color: Theme.colorBorder
    border.width: 1
    implicitHeight: 110
    clip: true

    // 由父级 ListView delegate 绑定各角色
    property int deviceIndex: -1
    property string dName: ""
    property string dType: ""
    property string dIp: ""
    property int dPort: 0
    property bool dConnected: false
    property bool dHeartbeat: false
    property bool dReady: false
    property bool dArmed: false
    property bool dRunning: false
    property bool dFault: false
    property int dFaultCode: 0
    property int dBattery: 100
    property int dLatency: 0

    // 操作信号：由 DevicePage 接管，弹出编辑对话框或确认删除
    signal editRequested(int index)
    signal removeRequested(int index)

    // appController 由 main.cpp setContextProperty 注入，静态分析工具无法识别其类型，故此处行内抑制
    readonly property var dm: appController.deviceManager // qmllint disable unqualified

    // 整体状态级别，决定左边色条
    function statusLevel(): int {
        if (root.dFault) return 3
        if (root.dConnected) return 0
        return 3
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing
        spacing: Theme.spacing

        // 状态色条
        Rectangle {
            Layout.preferredWidth: 6
            Layout.minimumWidth: 6
            Layout.maximumWidth: 6
            Layout.fillHeight: true
            color: Theme.levelColor(root.statusLevel())
            radius: 3
        }

        // 左侧设备信息区
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 0
            spacing: Theme.spacingTiny

            // 名称 + 类型 + 在线状态
            RowLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: Theme.spacing

                Text {
                    text: root.dName
                    color: Theme.colorText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMedium
                    font.bold: true

                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    elide: Text.ElideRight
                }

                Rectangle {
                    color: Theme.colorHover
                    radius: 3
                    Layout.preferredHeight: 18
                    Layout.minimumHeight: 18
                    Layout.maximumHeight: 18
                    Layout.preferredWidth: Math.min(typeText.implicitWidth + 12, 120)
                    Layout.minimumWidth: 42
                    Layout.maximumWidth: 120

                    Text {
                        id: typeText
                        anchors.centerIn: parent
                        width: parent.width - 8
                        text: root.dType
                        color: Theme.colorTextDim
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontTiny
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }

                Text {
                    text: root.dConnected ? "● 在线" : "● 离线"
                    color: root.dConnected ? Theme.colorNormal : Theme.colorEmergency
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                    font.bold: true
                    Layout.preferredWidth: 54
                    Layout.minimumWidth: 54
                    Layout.maximumWidth: 54
                    horizontalAlignment: Text.AlignRight
                }
            }

            // IP / 端口 / 延迟
            Text {
                text: "%1:%2   延迟 %3 ms".arg(root.dIp).arg(root.dPort).arg(root.dLatency)
                color: Theme.colorTextDim
                font.family: Theme.fontNumeric
                font.pixelSize: Theme.fontSmall

                Layout.fillWidth: true
                Layout.minimumWidth: 0
                elide: Text.ElideRight
            }

            // 通信指标
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacingLg

                StatusLamp {
                    level: root.dConnected && root.dHeartbeat ? 0 : 3
                    text: "心跳"
                    dotSize: 8
                    Layout.preferredWidth: 70
                    Layout.minimumWidth: 70
                    Layout.maximumWidth: 70
                }

                StatusLamp {
                    level: root.dConnected ? 0 : 3
                    text: "数据"
                    dotSize: 8
                    Layout.preferredWidth: 70
                    Layout.minimumWidth: 70
                    Layout.maximumWidth: 70
                }
            }

            // 故障 + 电量
            RowLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: Theme.spacingLg

                Text {
                    text: root.dFault ? "故障码：" + root.dFaultCode : "无故障"
                    color: root.dFault ? Theme.colorEmergency : Theme.colorNormal
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                    font.bold: root.dFault

                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    elide: Text.ElideRight
                }

                // 电量可视化：标签 + 进度条 + 百分比
                RowLayout {
                    spacing: Theme.spacingSm
                    Layout.preferredWidth: 150
                    Layout.minimumWidth: 150
                    Layout.maximumWidth: 150

                    Text {
                        text: "电量"
                        color: Theme.colorTextMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontTiny
                    }

                    // 进度条背景
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 10
                        radius: 5
                        color: Theme.colorPanelAlt
                        border.color: Theme.colorBorder
                        border.width: 1
                        clip: true

                        // 进度条填充（颜色随电量变化：>50绿 / 20-50黄 / <20红）
                        Rectangle {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.max(0, (parent.width - 2) * root.dBattery / 100.0)
                            height: parent.height - 2
                            radius: 4
                            color: root.dBattery > 50 ? Theme.colorNormal
                                 : (root.dBattery > 20 ? Theme.colorWarning : Theme.colorError)
                            Behavior on width { NumberAnimation { duration: 200 } }
                        }
                    }

                    // 百分比数值
                    Text {
                        text: root.dBattery + "%"
                        color: root.dBattery > 20 ? Theme.colorTextDim : Theme.colorError
                        font.family: Theme.fontNumeric
                        font.pixelSize: Theme.fontSmall
                        font.bold: root.dBattery <= 20
                        Layout.preferredWidth: 36
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }
        }

        // 右侧操作按钮列：固定宽度，避免不同设备行按钮大小不一致
        ColumnLayout {
            Layout.preferredWidth: 72
            Layout.minimumWidth: 72
            Layout.maximumWidth: 72
            Layout.fillHeight: true
            spacing: Theme.spacingSm

            Item {
                Layout.fillHeight: true
            }

            Button {
                id: editBtn
                text: "✎ 编辑"

                Layout.fillWidth: true
                Layout.preferredHeight: 30
                Layout.minimumHeight: 30
                Layout.maximumHeight: 30

                contentItem: Text {
                    text: editBtn.text
                    color: Theme.colorText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontTiny
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    color: editBtn.down ? Theme.colorAccentHover : Theme.colorHover
                    radius: Theme.radius
                    border.color: Theme.colorBorder
                    border.width: 1
                }

                onClicked: root.editRequested(root.deviceIndex)
            }

            Button {
                id: removeBtn
                text: "✕ 删除"

                Layout.fillWidth: true
                Layout.preferredHeight: 30
                Layout.minimumHeight: 30
                Layout.maximumHeight: 30

                contentItem: Text {
                    text: removeBtn.text
                    color: Theme.colorEmergency
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontTiny
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    color: removeBtn.down ? Theme.colorEmergency : Theme.colorPanelAlt
                    radius: Theme.radius
                    border.color: Theme.colorEmergency
                    border.width: 1
                    opacity: removeBtn.down ? 1.0 : 0.6
                }

                onClicked: root.removeRequested(root.deviceIndex)
            }
        }
    }
}