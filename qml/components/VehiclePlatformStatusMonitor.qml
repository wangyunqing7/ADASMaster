// =============================================================================
// VehiclePlatformStatusMonitor.qml
// 功能：两个可配置运动设备监测位，仅列出支持位置遥测的设备。
// 输入：deviceListModel 的 deviceMotionCapable/deviceX/deviceY/deviceSpeed/deviceYaw
//       以及 deviceConnected/deviceHeartbeat/deviceIp/devicePort/deviceBattery/
//       deviceLatency/devicePacketLoss/deviceSignalStrength。
// 输出：两台不同运动设备的位置、速度、航向角与通信状态。
// =============================================================================
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import ADASMaster

PanelFrame {
    id: root
    title: "运动设备状态监测"
    titleEn: "motion telemetry"
    contentPadding: Theme.spacingSm

    readonly property var ctrl: appController // qmllint disable unqualified
    readonly property var dm: root.ctrl.deviceManager
    readonly property var model: root.dm.deviceListModel
    property var motionRows: []
    property int primaryIndex: 0
    property int secondaryIndex: 1

    function roleValue(row: int, role: int, fallback: var): var {
        if (row < 0 || row >= root.dm.totalCount) return fallback
        const value = root.model.data(root.model.index(row), role)
        return value === undefined || value === null ? fallback : value
    }
    function statusValue(status: var, key: string, fallback: var): var {
        if (status && status[key] !== undefined && status[key] !== null) return status[key]
        return fallback
    }
    function packetLossLevel(packetLossPercent: real, online: bool, heartbeatOk: bool): int {
        if (!online || !heartbeatOk) return 3
        if (packetLossPercent <= 1) return 0
        if (packetLossPercent <= 3) return 1
        return 3
    }
    function signalLevel(signalStrengthPercent: int, online: bool, heartbeatOk: bool): int {
        if (!online || !heartbeatOk) return 3
        if (signalStrengthPercent >= 70) return 0
        if (signalStrengthPercent >= 40) return 1
        return 3
    }
    function endpointText(connected: bool, ip: string, port: int): string {
        return connected ? "%1:%2".arg(ip).arg(port) : "—"
    }
    function refreshMotionRows() {
        const rows = []
        for (let row = 0; row < root.dm.totalCount; ++row) {
            if (root.roleValue(row, 273, false)) {
                rows.push({ row: row, name: String(root.roleValue(row, 257, "")) })
            }
        }
        root.motionRows = rows
        let primary = 0; let secondary = rows.length > 1 ? 1 : 0
        for (let i = 0; i < rows.length; ++i) {
            if (rows[i].name === root.ctrl.monitorDeviceSelection(0)) primary = i
            if (rows[i].name === root.ctrl.monitorDeviceSelection(1)) secondary = i
        }
        if (rows.length > 1 && primary === secondary) secondary = primary === 0 ? 1 : 0
        root.primaryIndex = Math.min(primary, Math.max(0, rows.length - 1))
        root.secondaryIndex = Math.min(secondary, Math.max(0, rows.length - 1))
        root.persistSelection()
    }
    function persistSelection() {
        if (root.motionRows.length === 0) return
        root.ctrl.setMonitorDeviceSelection(0, root.motionRows[root.primaryIndex].name)
        root.ctrl.setMonitorDeviceSelection(1, root.motionRows[root.secondaryIndex].name)
    }
    function activatePrimary(index: int) {
        if (root.motionRows.length > 1 && index === root.secondaryIndex) {
            const old = root.primaryIndex; root.primaryIndex = index; root.secondaryIndex = old
        } else root.primaryIndex = index
        root.persistSelection()
    }
    function activateSecondary(index: int) {
        if (root.motionRows.length > 1 && index === root.primaryIndex) {
            const old = root.secondaryIndex; root.secondaryIndex = index; root.primaryIndex = old
        } else root.secondaryIndex = index
        root.persistSelection()
    }
    function sourceRow(index: int): int {
        return index >= 0 && index < root.motionRows.length ? root.motionRows[index].row : -1
    }

    Component.onCompleted: refreshMotionRows()
    property Connections dmConnections: Connections {
        target: root.dm
        function onTotalCountChanged() { root.refreshMotionRows() }
    }

    component MotionCard: Rectangle {
        id: card
        required property int sourceRow
        required property string slotTitle
        color: Theme.colorPanelAlt; radius: Theme.radius
        border.color: connected ? Theme.colorAccent : Theme.colorEmergency
        border.width: 1

        readonly property string deviceName: String(root.roleValue(sourceRow, 257, "未选择"))
        readonly property bool connected: Boolean(root.roleValue(sourceRow, 261, false))
        readonly property bool heartbeatOk: connected && Boolean(root.roleValue(sourceRow, 262, false))
        readonly property real xM: Number(root.roleValue(sourceRow, 274, 0))
        readonly property real yM: Number(root.roleValue(sourceRow, 275, 0))
        readonly property real speedMps: Number(root.roleValue(sourceRow, 276, 0))
        readonly property real yawDeg: ((Number(root.roleValue(sourceRow, 277, 0)) % 360) + 360) % 360
        readonly property int latencyMs: Number(root.roleValue(sourceRow, 269, 0))
        readonly property real packetLossPercent: Number(root.roleValue(sourceRow, 271, 0))
        readonly property int signalStrengthPercent: Number(root.roleValue(sourceRow, 272, 0))
        readonly property string ip: String(root.roleValue(sourceRow, 259, ""))
        readonly property int port: Number(root.roleValue(sourceRow, 260, 0))

        ColumnLayout {
            anchors.fill: parent; anchors.margins: Theme.spacingSm; spacing: Theme.spacingTiny
            RowLayout {
                Layout.fillWidth: true
                Text { text: card.slotTitle + " · " + card.deviceName; color: Theme.colorText; font.bold: true; font.pixelSize: Theme.fontSmall; Layout.fillWidth: true }
                StatusLamp { level: card.heartbeatOk ? 0 : 3; text: !card.connected ? "离线" : (card.heartbeatOk ? "在线" : "心跳异常"); dotSize: 8 }
            }
            GridLayout {
                Layout.fillWidth: true; Layout.fillHeight: true; columns: 4; columnSpacing: 1; rowSpacing: 1
                MetricCell { Layout.fillWidth: true; Layout.fillHeight: true; label: "X 位置"; value: card.connected ? card.xM.toFixed(2) : "—"; unit: card.connected ? "m" : "" }
                MetricCell { Layout.fillWidth: true; Layout.fillHeight: true; label: "Y 位置"; value: card.connected ? card.yM.toFixed(2) : "—"; unit: card.connected ? "m" : "" }
                MetricCell { Layout.fillWidth: true; Layout.fillHeight: true; label: "速度"; value: card.connected ? (card.speedMps * 3.6).toFixed(1) : "—"; unit: card.connected ? "km/h" : "" }
                MetricCell { Layout.fillWidth: true; Layout.fillHeight: true; label: "航向角"; value: card.connected ? card.yawDeg.toFixed(1) : "—"; unit: card.connected ? "°" : "" }
            }
            RowLayout {
                Layout.fillWidth: true
                Text { text: "延迟 " + (card.connected ? card.latencyMs + " ms" : "—"); color: Theme.colorTextMuted; font.pixelSize: 10 }
                Text { text: "丢包 " + (card.connected ? card.packetLossPercent.toFixed(1) + "%" : "—"); color: Theme.levelColor(root.packetLossLevel(card.packetLossPercent, card.connected, card.heartbeatOk)); font.pixelSize: 10 }
                Text { text: "信号 " + (card.connected ? card.signalStrengthPercent + "%" : "—"); color: Theme.levelColor(root.signalLevel(card.signalStrengthPercent, card.connected, card.heartbeatOk)); font.pixelSize: 10 }
                Item { Layout.fillWidth: true }
                Text { text: root.endpointText(card.connected, card.ip, card.port); color: Theme.colorTextMuted; font.pixelSize: 10 }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true; Layout.fillHeight: true; spacing: Theme.spacingSm
        RowLayout {
            Layout.fillWidth: true
            Text { text: "监测位 1"; color: Theme.colorTextMuted; font.pixelSize: Theme.fontTiny }
            ComboBox {
                id: primaryDeviceCombo
                Layout.fillWidth: true; model: root.motionRows; textRole: "name"; valueRole: "row"
                currentIndex: root.primaryIndex
                onActivated: (index) => root.activatePrimary(index)
            }
            Text { text: "监测位 2"; color: Theme.colorTextMuted; font.pixelSize: Theme.fontTiny }
            ComboBox {
                id: secondaryDeviceCombo
                Layout.fillWidth: true; model: root.motionRows; textRole: "name"; valueRole: "row"
                currentIndex: root.secondaryIndex
                onActivated: (index) => root.activateSecondary(index)
            }
        }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: Theme.spacingSm
            MotionCard { Layout.fillWidth: true; Layout.fillHeight: true; slotTitle: "设备 1"; sourceRow: root.sourceRow(root.primaryIndex) }
            MotionCard { Layout.fillWidth: true; Layout.fillHeight: true; slotTitle: "设备 2"; sourceRow: root.sourceRow(root.secondaryIndex) }
        }
    }
}
