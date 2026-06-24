// =============================================================================
// TrajectorySegmentEditor.qml
// 功能：用连续直线段/圆弧段参数建立轨迹；地图仅接收预览点，不参与编辑。
// 输入：segments。输出：previewRequested、saveRequested。
// =============================================================================
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import ADASMaster

pragma ComponentBehavior: Bound

Item {
    id: root
    readonly property var ctrl: appController // qmllint disable unqualified
    property var segments: []
    property var editing: []
    property real rotateDegrees: 0
    signal previewRequested(var points)
    signal saveRequested(var segments)

    function clone(value) { return JSON.parse(JSON.stringify(value || [])) }
    function normalize(value) { return root.ctrl.normalizeTrajectory(value) }
    function sync() {
        root.editing = root.normalize(root.clone(root.segments))
        root.previewRequested(root.ctrl.previewTrajectory(root.editing))
    }
    function replaceField(index, key, value) {
        const copy = root.clone(root.editing)
        copy[index][key] = value
        root.editing = root.normalize(copy)
        root.previewRequested(root.ctrl.previewTrajectory(root.editing))
    }
    function replaceHeading(index, degrees) {
        const copy = root.clone(root.editing)
        const segment = copy[index]
        const dx = segment.endX - segment.startX
        const dy = segment.endY - segment.startY
        const length = Math.sqrt(dx * dx + dy * dy)
        const radians = degrees * Math.PI / 180
        segment.endX = segment.startX + length * Math.cos(radians)
        segment.endY = segment.startY + length * Math.sin(radians)
        root.editing = root.normalize(copy)
        root.previewRequested(root.ctrl.previewTrajectory(root.editing))
    }
    function addSegment(kind) {
        const copy = root.clone(root.editing)
        const previous = copy.length > 0 ? copy[copy.length - 1] : null
        const sx = previous ? previous.endX : 0
        const sy = previous ? previous.endY : 0
        const speed = previous ? previous.endSpeedKph : 0
        copy.push({
            kind: kind,
            startX: sx, startY: sy,
            endX: sx + (kind === "arc" ? 20 : 50),
            endY: sy + (kind === "arc" ? 20 : 0),
            startSpeedKph: speed,
            endSpeedKph: Math.max(speed, 10),
            radiusM: kind === "arc" ? 20 : 0,
            turnDirection: "left"
        })
        root.editing = root.normalize(copy)
        root.previewRequested(root.ctrl.previewTrajectory(root.editing))
    }
    function removeSegment(index) {
        const copy = root.clone(root.editing); copy.splice(index, 1)
        root.editing = root.normalize(copy)
        root.previewRequested(root.ctrl.previewTrajectory(root.editing))
    }
    function moveSegment(index, delta) {
        const target = index + delta
        if (target < 0 || target >= root.editing.length) return
        const copy = root.clone(root.editing)
        const item = copy.splice(index, 1)[0]; copy.splice(target, 0, item)
        root.editing = root.normalize(copy)
        root.previewRequested(root.ctrl.previewTrajectory(root.editing))
    }
    function duplicateSegment(index) {
        const copy = root.clone(root.editing); copy.splice(index + 1, 0, root.clone([copy[index]])[0])
        root.editing = root.normalize(copy)
        root.previewRequested(root.ctrl.previewTrajectory(root.editing))
    }
    function applyRotation() {
        if (root.editing.length === 0) return
        const copy = root.clone(root.editing)
        const originX = copy[0].startX; const originY = copy[0].startY
        const radians = root.rotateDegrees * Math.PI / 180
        const c = Math.cos(radians); const s = Math.sin(radians)
        for (let i = 0; i < copy.length; ++i) {
            const points = [["startX", "startY"], ["endX", "endY"]]
            for (let p = 0; p < points.length; ++p) {
                const xKey = points[p][0]; const yKey = points[p][1]
                const dx = copy[i][xKey] - originX; const dy = copy[i][yKey] - originY
                copy[i][xKey] = originX + dx * c - dy * s
                copy[i][yKey] = originY + dx * s + dy * c
            }
        }
        root.editing = root.normalize(copy)
        root.previewRequested(root.ctrl.previewTrajectory(root.editing))
    }

    Component.onCompleted: sync()
    onSegmentsChanged: sync()

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacingSm

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSm
            Button { text: "+ 直线段"; onClicked: root.addSegment("straight") }
            Button { text: "+ 圆弧段"; onClicked: root.addSegment("arc") }
            Item { Layout.fillWidth: true }
            Text { text: "整轨旋转"; color: Theme.colorTextDim; font.pixelSize: Theme.fontTiny }
            TextField {
                Layout.preferredWidth: 64; text: String(root.rotateDegrees)
                validator: DoubleValidator { bottom: -360; top: 360 }
                onEditingFinished: root.rotateDegrees = Number(text)
            }
            Button { text: "应用"; onClicked: root.applyRotation() }
        }

        ScrollView {
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            Column {
                width: parent.width
                spacing: Theme.spacingSm
                Repeater {
                    model: root.editing
                    Rectangle {
                        id: segmentCard
                        required property var modelData
                        required property int index
                        width: parent.width; height: segmentColumn.implicitHeight + 16
                        color: Theme.colorPanelAlt; radius: Theme.radius
                        border.color: segmentCard.modelData.valid ? Theme.colorBorder : Theme.colorEmergency

                        ColumnLayout {
                            id: segmentColumn
                            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                            anchors.margins: 8; spacing: 6
                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: "%1 · %2".arg(segmentCard.index + 1).arg(segmentCard.modelData.kind === "arc" ? "圆弧段" : "直线段")
                                    color: Theme.colorText; font.bold: true; font.pixelSize: Theme.fontSmall
                                }
                                Text {
                                    text: segmentCard.modelData.valid ? "%1 m  ·  %2°  ·  %3 s".arg(Number(segmentCard.modelData.lengthM).toFixed(1)).arg(Number(segmentCard.modelData.headingDeg).toFixed(1)).arg(Number(segmentCard.modelData.durationSec).toFixed(1)) : segmentCard.modelData.error
                                    color: segmentCard.modelData.valid ? Theme.colorTextMuted : Theme.colorEmergency
                                    font.pixelSize: Theme.fontTiny; Layout.fillWidth: true; elide: Text.ElideRight
                                }
                                Button { text: "↑"; enabled: segmentCard.index > 0; onClicked: root.moveSegment(segmentCard.index, -1) }
                                Button { text: "↓"; enabled: segmentCard.index < root.editing.length - 1; onClicked: root.moveSegment(segmentCard.index, 1) }
                                Button { text: "复制"; onClicked: root.duplicateSegment(segmentCard.index) }
                                Button { text: "×"; onClicked: root.removeSegment(segmentCard.index) }
                            }
                            GridLayout {
                                Layout.fillWidth: true; columns: 8; columnSpacing: 5; rowSpacing: 3
                                Text { text: "起点 X"; color: Theme.colorTextMuted; font.pixelSize: 10 }
                                TextField { Layout.fillWidth: true; text: Number(segmentCard.modelData.startX).toFixed(2); enabled: segmentCard.index === 0; onEditingFinished: root.replaceField(segmentCard.index, "startX", Number(text)) }
                                Text { text: "起点 Y"; color: Theme.colorTextMuted; font.pixelSize: 10 }
                                TextField { Layout.fillWidth: true; text: Number(segmentCard.modelData.startY).toFixed(2); enabled: segmentCard.index === 0; onEditingFinished: root.replaceField(segmentCard.index, "startY", Number(text)) }
                                Text { text: "起速 km/h"; color: Theme.colorTextMuted; font.pixelSize: 10 }
                                TextField { Layout.fillWidth: true; text: Number(segmentCard.modelData.startSpeedKph).toFixed(1); enabled: segmentCard.index === 0; onEditingFinished: root.replaceField(segmentCard.index, "startSpeedKph", Number(text)) }
                                Text { text: "加速度"; color: Theme.colorTextMuted; font.pixelSize: 10 }
                                Text { text: Number(segmentCard.modelData.accelerationMps2).toFixed(3) + " m/s²"; color: Theme.colorInfo; font.pixelSize: 10 }

                                Text { text: "终点 X"; color: Theme.colorTextMuted; font.pixelSize: 10 }
                                TextField { Layout.fillWidth: true; text: Number(segmentCard.modelData.endX).toFixed(2); onEditingFinished: root.replaceField(segmentCard.index, "endX", Number(text)) }
                                Text { text: "终点 Y"; color: Theme.colorTextMuted; font.pixelSize: 10 }
                                TextField { Layout.fillWidth: true; text: Number(segmentCard.modelData.endY).toFixed(2); onEditingFinished: root.replaceField(segmentCard.index, "endY", Number(text)) }
                                Text { text: "末速 km/h"; color: Theme.colorTextMuted; font.pixelSize: 10 }
                                TextField { Layout.fillWidth: true; text: Number(segmentCard.modelData.endSpeedKph).toFixed(1); onEditingFinished: root.replaceField(segmentCard.index, "endSpeedKph", Number(text)) }
                                Text { text: "航向角"; color: Theme.colorTextMuted; font.pixelSize: 10 }
                                TextField {
                                    Layout.fillWidth: true
                                    text: Number(segmentCard.modelData.headingDeg).toFixed(1)
                                    validator: DoubleValidator { bottom: -360; top: 360 }
                                    onEditingFinished: root.replaceHeading(segmentCard.index, Number(text))
                                }
                            }
                            RowLayout {
                                visible: segmentCard.modelData.kind === "arc"
                                Text { text: "转弯半径 (m)"; color: Theme.colorTextMuted; font.pixelSize: 10 }
                                TextField { Layout.preferredWidth: 80; text: Number(segmentCard.modelData.radiusM).toFixed(1); onEditingFinished: root.replaceField(segmentCard.index, "radiusM", Number(text)) }
                                Text { text: "转向"; color: Theme.colorTextMuted; font.pixelSize: 10 }
                                ComboBox { model: ["left", "right"]; currentIndex: segmentCard.modelData.turnDirection === "right" ? 1 : 0; onActivated: (i) => root.replaceField(segmentCard.index, "turnDirection", i === 1 ? "right" : "left") }
                                Text { text: "横向加速度 %1 m/s²".arg(Number(segmentCard.modelData.lateralAccelerationMps2).toFixed(3)); color: Theme.colorInfo; font.pixelSize: 10 }
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text { text: root.editing.length === 0 ? "暂无轨迹段，请添加直线段或圆弧段" : "共 %1 段".arg(root.editing.length); color: Theme.colorTextMuted; font.pixelSize: Theme.fontTiny }
            Item { Layout.fillWidth: true }
            Button {
                text: "保存轨迹"
                enabled: root.editing.length > 0 && root.editing.every(function(item) { return item.valid })
                onClicked: root.saveRequested(root.editing)
            }
        }
    }
}
