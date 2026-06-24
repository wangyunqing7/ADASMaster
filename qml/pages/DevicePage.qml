// =============================================================================
// DevicePage.qml
// 功能：设备管理页面。以卡片列表展示全部设备，连接状态由数据驱动（有数据=绿/在线，
//       无数据=红/离线）。支持添加设备（基本信息+网络配置，添加后自动连接）。
// 数据源：appController.deviceManager.deviceListModel (QAbstractListModel)
// =============================================================================
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import ADASMaster

// Bound 模式：delegate 作为独立作用域，访问外部 grid 与自身 required 角色统一用 id 限定。
pragma ComponentBehavior: Bound

Item {
    id: page

    // appController 由 main.cpp setContextProperty 注入，静态分析工具无法识别其类型，故此处行内抑制
    readonly property var dm: appController.deviceManager // qmllint disable unqualified

    // 添加设备对话框（用 open/close 控制，避免 visible 绑定冲突）
    // 编辑设备状态：当前编辑的设备索引，-1 表示未编辑
    property int editingIndex: -1
    // 删除确认状态：待删除的设备索引，-1 表示未确认
    property int removingIndex: -1

    // 打开编辑对话框：读取现有设备信息填充表单
    function openEditDialog(idx) {
        if (idx < 0 || idx >= page.dm.totalCount) return
        page.editingIndex = idx
        const info = page.dm.deviceInfo(idx)
        editNameField.text = info.name || ""
        editTypeLabel.text = info.type || ""
        editIpField.text = info.ip || ""
        editPortField.text = String(info.port || 0)
        editError.text = ""
        editDevicePopup.open()
    }

    // 打开删除确认
    function openRemoveConfirm(idx) {
        if (idx < 0 || idx >= page.dm.totalCount) return
        page.removingIndex = idx
        const info = page.dm.deviceInfo(idx)
        removeConfirmName.text = info.name || ""
        removeConfirmPopup.open()
    }

    PanelFrame {
        anchors.fill: parent
        title: "设备管理"
        titleEn: "devices"

        // 工具栏：在线统计 + 添加设备
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing

            Text {
                text: "在线 %1 / 总数 %2".arg(page.dm.onlineCount).arg(page.dm.totalCount)
                color: Theme.colorText
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontMedium
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            Button {
                id: addDeviceButton
                text: "+ 添加设备"
                Layout.preferredHeight: 32
                contentItem: Text { text: addDeviceButton.text; color: "white"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: addDeviceButton.down ? Theme.colorAccentHover : Theme.colorAccent; radius: Theme.radius }
                onClicked: {
                    addDeviceError.text = ""
                    addDevicePopup.open()
                }
            }
        }

        // 设备卡片列表（纵向排列）
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ListView {
                id: grid
                width: parent.width
                model: page.dm.deviceListModel
                spacing: Theme.spacing

                delegate: DeviceCard {
                    id: card
                    required property int index
                    required property string deviceName
                    required property string deviceType
                    required property string deviceIp
                    required property int devicePort
                    required property bool deviceConnected
                    required property bool deviceHeartbeat
                    required property bool deviceReady
                    required property bool deviceArmed
                    required property bool deviceRunning
                    required property bool deviceFault
                    required property int deviceFaultCode
                    required property int deviceBattery
                    required property int deviceLatency

                    width: grid.width
                    height: 110
                    deviceIndex: card.index
                    dName: card.deviceName
                    dType: card.deviceType
                    dIp: card.deviceIp
                    dPort: card.devicePort
                    dConnected: card.deviceConnected
                    dHeartbeat: card.deviceHeartbeat
                    dReady: card.deviceReady
                    dArmed: card.deviceArmed
                    dRunning: card.deviceRunning
                    dFault: card.deviceFault
                    dFaultCode: card.deviceFaultCode
                    dBattery: card.deviceBattery
                    dLatency: card.deviceLatency
                    // 接管卡片操作信号
                    onEditRequested: function(idx) { page.openEditDialog(idx) }
                    onRemoveRequested: function(idx) { page.openRemoveConfirm(idx) }
                }
            }
        }
    }

    // 添加设备对话框
    Popup {
        id: addDevicePopup
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 460
        height: 580
        // 不绑 visible，用 open()/close() 控制（Qt 推荐用法，避免绑定冲突）
        padding: 0

        background: Rectangle {
            color: Theme.colorPanel
            radius: Theme.radiusLg
            border.color: Theme.colorBorder
            border.width: 1
        }

        // 顶部说明条
        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            height: 44
            color: Theme.colorTitleBar
            radius: Theme.radiusLg
            Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.colorBorder }
            Text {
                anchors.centerIn: parent
                text: "添加新设备"
                color: Theme.colorText
                font.family: Theme.fontFamily; font.pixelSize: Theme.fontMedium; font.bold: true
            }
        }

        ScrollView {
            anchors.fill: parent
            anchors.topMargin: 44
            anchors.bottomMargin: 64
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: Theme.spacing
                Layout.margins: Theme.spacingLg

                // 说明文字
                Text {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.spacingLg; Layout.rightMargin: Theme.spacingLg
                    Layout.topMargin: Theme.spacing
                    text: "填写设备基本信息与网络配置。带 * 为必填项，添加后设备将自动尝试连接。"
                    color: Theme.colorTextDim
                    font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                }

                // ---- 分组1：基本信息 ----
                Text {
                    Layout.fillWidth: true; Layout.leftMargin: Theme.spacingLg
                    text: "基本信息"
                    color: Theme.colorAccent
                    font.family: Theme.fontFamily; font.pixelSize: Theme.fontTiny; font.bold: true
                }

                // 设备名称
                ColumnLayout {
                    Layout.fillWidth: true; Layout.leftMargin: Theme.spacingLg; Layout.rightMargin: Theme.spacingLg
                    spacing: 2
                    Text {
                        text: "设备名称 *"
                        color: Theme.colorTextDim
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontTiny
                    }
                    TextField {
                        id: nameField
                        Layout.fillWidth: true
                        placeholderText: "如：驾驶机器人 #2"
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
                        color: Theme.colorText
                        background: Rectangle { color: Theme.colorHover; radius: Theme.radius; border.color: nameField.activeFocus ? Theme.colorAccent : Theme.colorBorder; border.width: 1 }
                    }
                    Text {
                        text: "用于界面显示，不可与其他设备重名"
                        color: Theme.colorTextMuted
                        font.family: Theme.fontFamily; font.pixelSize: 10
                    }
                }

                // 设备类型（下拉选择）
                ColumnLayout {
                    Layout.fillWidth: true; Layout.leftMargin: Theme.spacingLg; Layout.rightMargin: Theme.spacingLg
                    spacing: 2
                    Text {
                        text: "设备类型 *"
                        color: Theme.colorTextDim
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontTiny
                    }
                    ComboBox {
                        id: typeField
                        Layout.fillWidth: true
                        model: [
                            "DrivingRobot - 驾驶机器人",
                            "TargetPlatform - 目标车平台",
                            "RtkBase - RTK 基站",
                            "SafetySystem - 安全系统",
                            "DataAcquisition - 数据采集",
                            "Other - 其他"
                        ]
                        contentItem: Text { text: typeField.currentText; color: Theme.colorText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: Theme.colorHover; radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
                    }
                    Text {
                        text: "决定设备在系统中的角色与数据解析方式"
                        color: Theme.colorTextMuted
                        font.family: Theme.fontFamily; font.pixelSize: 10
                    }
                }

                // ---- 分组2：网络配置 ----
                Text {
                    Layout.fillWidth: true; Layout.leftMargin: Theme.spacingLg; Layout.topMargin: Theme.spacing
                    text: "网络配置"
                    color: Theme.colorAccent
                    font.family: Theme.fontFamily; font.pixelSize: Theme.fontTiny; font.bold: true
                }

                // IP 地址
                ColumnLayout {
                    Layout.fillWidth: true; Layout.leftMargin: Theme.spacingLg; Layout.rightMargin: Theme.spacingLg
                    spacing: 2
                    Text {
                        text: "IP 地址 *"
                        color: Theme.colorTextDim
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontTiny
                    }
                    TextField {
                        id: ipField
                        Layout.fillWidth: true
                        placeholderText: "如：192.168.1.100"
                        text: "192.168.1."
                        font.family: Theme.fontNumeric; font.pixelSize: Theme.fontNormal
                        color: Theme.colorText
                        background: Rectangle { color: Theme.colorHover; radius: Theme.radius; border.color: ipField.activeFocus ? Theme.colorAccent : Theme.colorBorder; border.width: 1 }
                    }
                    Text {
                        text: "设备的 IPv4 地址，需与主控在同一网段"
                        color: Theme.colorTextMuted
                        font.family: Theme.fontFamily; font.pixelSize: 10
                    }
                }

                // 端口号
                ColumnLayout {
                    Layout.fillWidth: true; Layout.leftMargin: Theme.spacingLg; Layout.rightMargin: Theme.spacingLg
                    spacing: 2
                    Text {
                        text: "端口号 *"
                        color: Theme.colorTextDim
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontTiny
                    }
                    TextField {
                        id: portField
                        Layout.fillWidth: true
                        placeholderText: "1 - 65535"
                        font.family: Theme.fontNumeric; font.pixelSize: Theme.fontNormal
                        color: Theme.colorText
                        background: Rectangle { color: Theme.colorHover; radius: Theme.radius; border.color: portField.activeFocus ? Theme.colorAccent : Theme.colorBorder; border.width: 1 }
                        validator: IntValidator { bottom: 1; top: 65535 }
                    }
                    Text {
                        text: "设备监听的端口号"
                        color: Theme.colorTextMuted
                        font.family: Theme.fontFamily; font.pixelSize: 10
                    }
                }

                // 协议（下拉选择）
                ColumnLayout {
                    Layout.fillWidth: true; Layout.leftMargin: Theme.spacingLg; Layout.rightMargin: Theme.spacingLg
                    spacing: 2
                    Text {
                        text: "通信协议 *"
                        color: Theme.colorTextDim
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontTiny
                    }
                    ComboBox {
                        id: protocolField
                        Layout.fillWidth: true
                        model: [ "TCP", "UDP", "NTRIP", "Serial", "CAN" ]
                        currentIndex: 0
                        contentItem: Text { text: protocolField.currentText; color: Theme.colorText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: Theme.colorHover; radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
                    }
                    Text {
                        text: "TCP=可靠传输 / UDP=低延迟 / NTRIP=RTK 差分 / Serial=串口 / CAN=总线"
                        color: Theme.colorTextMuted
                        font.family: Theme.fontFamily; font.pixelSize: 10
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }

                // 错误提示
                Rectangle {
                    Layout.fillWidth: true; Layout.leftMargin: Theme.spacingLg; Layout.rightMargin: Theme.spacingLg
                    Layout.preferredHeight: addDeviceError.implicitHeight + 12
                    visible: addDeviceError.text.length > 0
                    color: Theme.colorEmergency
                    radius: Theme.radius
                    opacity: 0.15
                    Text {
                        id: addDeviceError
                        anchors.centerIn: parent
                        text: ""
                        color: Theme.colorEmergency
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; font.bold: true
                        wrapMode: Text.WordWrap
                    }
                }

                Item { Layout.fillHeight: true; Layout.fillWidth: true }
            }
        }

        // 底部按钮栏（固定在底部）
        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: 64
            color: Theme.colorPanelAlt
            Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; height: 1; color: Theme.colorBorder }

            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacing
                spacing: Theme.spacing

                Item { Layout.fillWidth: true }

                Button {
                    id: cancelButton
                    text: "取消"
                    Layout.preferredWidth: 100
                    Layout.preferredHeight: 36
                    contentItem: Text { text: cancelButton.text; color: Theme.colorText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: cancelButton.down ? Theme.colorHover : Theme.colorPanel; radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
                    onClicked: {
                        addDeviceError.text = ""
                        addDevicePopup.close()
                    }
                }

                Button {
                    id: addButton
                    text: "✓ 添加设备"
                    Layout.preferredWidth: 120
                    Layout.preferredHeight: 36
                    contentItem: Text { text: addButton.text; color: "white"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: addButton.down ? Theme.colorAccentHover : Theme.colorAccent; radius: Theme.radius }
                    onClicked: {
                        // 从下拉类型提取实际类型标识（取 " - " 前的部分）
                        const typeRaw = typeField.currentValue || typeField.currentText
                        const typeCode = typeRaw.split(" - ")[0].trim()
                        const protoRaw = protocolField.currentValue || protocolField.currentText
                        // 校验必填项
                        if (nameField.text.trim() === "") {
                            addDeviceError.text = "请填写设备名称"
                            return
                        }
                        if (ipField.text.trim() === "" || portField.text.trim() === "") {
                            addDeviceError.text = "请填写 IP 地址和端口号"
                            return
                        }
                        const port = parseInt(portField.text)
                        if (isNaN(port) || port <= 0 || port > 65535) {
                            addDeviceError.text = "端口号必须是 1-65535 的数字"
                            return
                        }
                        // 调用后端添加设备
                        const idx = page.dm.addDevice(nameField.text.trim(), typeCode, ipField.text.trim(), port, protoRaw.toLowerCase())
                        if (idx < 0) {
                            addDeviceError.text = "添加失败：设备名称已存在或参数无效"
                            return
                        }
                        // 成功：清空字段并关闭
                        addDeviceError.text = ""
                        nameField.text = ""
                        ipField.text = "192.168.1."
                        portField.text = ""
                        addDevicePopup.close()
                    }
                }
            }
        }
    }

    // 编辑设备对话框
    Popup {
        id: editDevicePopup
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 460
        height: 460
        padding: 0

        background: Rectangle {
            color: Theme.colorPanel
            radius: Theme.radiusLg
            border.color: Theme.colorBorder
            border.width: 1
        }

        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            height: 44
            color: Theme.colorTitleBar
            radius: Theme.radiusLg
            Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: Theme.colorBorder }
            Text {
                anchors.centerIn: parent
                text: "编辑设备信息"
                color: Theme.colorText
                font.family: Theme.fontFamily; font.pixelSize: Theme.fontMedium; font.bold: true
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.topMargin: 56
            anchors.bottomMargin: 64
            anchors.leftMargin: Theme.spacingLg
            anchors.rightMargin: Theme.spacingLg
            spacing: Theme.spacing

            // 设备类型（只读显示）
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: "设备类型（不可修改）"
                    color: Theme.colorTextMuted
                    font.family: Theme.fontFamily; font.pixelSize: Theme.fontTiny
                }
                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 36
                    color: Theme.colorPanelAlt; radius: Theme.radius
                    border.color: Theme.colorBorder; border.width: 1
                    Text {
                        id: editTypeLabel
                        anchors.left: parent.left; anchors.leftMargin: Theme.spacing
                        anchors.verticalCenter: parent.verticalCenter
                        text: ""
                        color: Theme.colorTextDim
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
                    }
                }
            }

            // 设备名称
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text { text: "设备名称 *"; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontTiny }
                TextField {
                    id: editNameField
                    Layout.fillWidth: true
                    font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
                    color: Theme.colorText
                    background: Rectangle { color: Theme.colorHover; radius: Theme.radius; border.color: editNameField.activeFocus ? Theme.colorAccent : Theme.colorBorder; border.width: 1 }
                }
            }

            // IP 地址
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text { text: "IP 地址 *"; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontTiny }
                TextField {
                    id: editIpField
                    Layout.fillWidth: true
                    font.family: Theme.fontNumeric; font.pixelSize: Theme.fontNormal
                    color: Theme.colorText
                    background: Rectangle { color: Theme.colorHover; radius: Theme.radius; border.color: editIpField.activeFocus ? Theme.colorAccent : Theme.colorBorder; border.width: 1 }
                }
            }

            // 端口号
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text { text: "端口号 *"; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontTiny }
                TextField {
                    id: editPortField
                    Layout.fillWidth: true
                    font.family: Theme.fontNumeric; font.pixelSize: Theme.fontNormal
                    color: Theme.colorText
                    background: Rectangle { color: Theme.colorHover; radius: Theme.radius; border.color: editPortField.activeFocus ? Theme.colorAccent : Theme.colorBorder; border.width: 1 }
                    validator: IntValidator { bottom: 1; top: 65535 }
                }
            }

            // 错误提示
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: editError.implicitHeight + 12
                visible: editError.text.length > 0
                color: Theme.colorEmergency; radius: Theme.radius; opacity: 0.15
                Text {
                    id: editError
                    anchors.centerIn: parent
                    text: ""
                    color: Theme.colorEmergency
                    font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; font.bold: true
                    wrapMode: Text.WordWrap
                }
            }

            Item { Layout.fillHeight: true }
        }

        // 底部按钮栏
        Rectangle {
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            height: 64
            color: Theme.colorPanelAlt
            Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; height: 1; color: Theme.colorBorder }

            RowLayout {
                anchors.fill: parent; anchors.margins: Theme.spacing
                spacing: Theme.spacing
                Item { Layout.fillWidth: true }
                Button {
                    id: editCancelBtn
                    text: "取消"
                    Layout.preferredWidth: 100; Layout.preferredHeight: 36
                    contentItem: Text { text: editCancelBtn.text; color: Theme.colorText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: editCancelBtn.down ? Theme.colorHover : Theme.colorPanel; radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
                    onClicked: editDevicePopup.close()
                }
                Button {
                    id: editSaveBtn
                    text: "✓ 保存"
                    Layout.preferredWidth: 120; Layout.preferredHeight: 36
                    contentItem: Text { text: editSaveBtn.text; color: "white"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: editSaveBtn.down ? Theme.colorAccentHover : Theme.colorAccent; radius: Theme.radius }
                    onClicked: {
                        if (editNameField.text.trim() === "") {
                            editError.text = "请填写设备名称"
                            return
                        }
                        if (editIpField.text.trim() === "" || editPortField.text.trim() === "") {
                            editError.text = "请填写 IP 地址和端口号"
                            return
                        }
                        const port = parseInt(editPortField.text)
                        if (isNaN(port) || port <= 0 || port > 65535) {
                            editError.text = "端口号必须是 1-65535 的数字"
                            return
                        }
                        const ok = page.dm.updateDevice(page.editingIndex, editNameField.text.trim(),
                                                         editIpField.text.trim(), port)
                        if (!ok) {
                            editError.text = "保存失败：名称已存在或参数无效"
                            return
                        }
                        editDevicePopup.close()
                    }
                }
            }
        }
    }

    // 删除确认对话框
    Popup {
        id: removeConfirmPopup
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 400
        height: 200
        padding: Theme.spacingLg

        background: Rectangle {
            color: Theme.colorPanel
            radius: Theme.radiusLg
            border.color: Theme.colorBorder
            border.width: 1
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.spacing

            Text {
                text: "确认删除设备"
                color: Theme.colorEmergency
                font.family: Theme.fontFamily; font.pixelSize: Theme.fontMedium; font.bold: true
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "即将删除设备「%1」。此操作不可撤销，删除后设备将从列表移除。".arg(removeConfirmName.text)
                color: Theme.colorTextDim
                font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing
                Item { Layout.fillWidth: true }
                Button {
                    id: removeCancelBtn
                    text: "取消"
                    Layout.preferredWidth: 100; Layout.preferredHeight: 36
                    contentItem: Text { text: removeCancelBtn.text; color: Theme.colorText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: removeCancelBtn.down ? Theme.colorHover : Theme.colorPanelAlt; radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
                    onClicked: removeConfirmPopup.close()
                }
                Button {
                    id: removeConfirmBtn
                    text: "✕ 删除"
                    Layout.preferredWidth: 100; Layout.preferredHeight: 36
                    contentItem: Text { text: removeConfirmBtn.text; color: "white"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: removeConfirmBtn.down ? Qt.darker(Theme.colorEmergency, 1.2) : Theme.colorEmergency; radius: Theme.radius }
                    onClicked: {
                        page.dm.removeDevice(page.removingIndex)
                        removeConfirmPopup.close()
                    }
                }
            }
        }
    }

    // 隐藏的 Text 用于暂存删除设备名（removeConfirmName）
    Text { id: removeConfirmName; visible: false; text: "" }
}
