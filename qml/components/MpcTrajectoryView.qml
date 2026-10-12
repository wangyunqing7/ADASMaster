// =============================================================================
// 功能：参考/实际质心轨迹和当前车辆方向；输入仿真数据，支持跟随和尺度调整。
// =============================================================================
import QtQuick
import ADASMaster

Item {
    id: root
    property var reference: []
    property var samples: []
    property var pose: ({xM: 0, yM: 0, yawRad: 0})
    property bool followVehicle: true
    property real spanM: 60
    onReferenceChanged: plot.requestPaint()
    onSamplesChanged: plot.requestPaint()
    onPoseChanged: plot.requestPaint()
    onFollowVehicleChanged: plot.requestPaint()
    onSpanMChanged: plot.requestPaint()

    Rectangle { anchors.fill: parent; color: Theme.colorPanelAlt; radius: Theme.radius }
    Canvas {
        id: plot
        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Component.onCompleted: requestPaint()
        onPaint: {
            const ctx = plot.getContext("2d")
            ctx.clearRect(0, 0, plot.width, plot.height)
            const padding = 25
            const w = plot.width - 2 * padding, h = plot.height - 2 * padding
            if (w <= 0 || h <= 0) return
            let centerX = Number(root.pose.xM), centerY = Number(root.pose.yM)
            let scale = w / root.spanM
            if (!root.followVehicle && root.reference.length) {
                let minX = 0, maxX = 0, minY = 0, maxY = 0
                const all = root.reference.concat(root.samples)
                for (let i = 0; i < all.length; ++i) {
                    minX = Math.min(minX, Number(all[i].xM)); maxX = Math.max(maxX, Number(all[i].xM))
                    minY = Math.min(minY, Number(all[i].yM)); maxY = Math.max(maxY, Number(all[i].yM))
                }
                centerX = (minX + maxX) / 2; centerY = (minY + maxY) / 2
                scale = Math.min(w / Math.max(10, maxX - minX + 8), h / Math.max(10, maxY - minY + 8))
            }
            const sx = (x) => plot.width / 2 + (x - centerX) * scale
            const sy = (y) => plot.height / 2 - (y - centerY) * scale
            const grid = scale > 10 ? 5 : 10
            const x0 = centerX - w / 2 / scale, x1 = centerX + w / 2 / scale
            const y0 = centerY - h / 2 / scale, y1 = centerY + h / 2 / scale
            ctx.font = Theme.fontSmall + "px " + Theme.fontNumeric; ctx.lineWidth = 1
            ctx.strokeStyle = Theme.colorBorder; ctx.fillStyle = Theme.colorTextDim
            for (let x = Math.ceil(x0 / grid) * grid; x <= x1; x += grid) {
                ctx.beginPath(); ctx.moveTo(sx(x), padding); ctx.lineTo(sx(x), plot.height - padding); ctx.stroke()
                ctx.textAlign = "center"; ctx.fillText(x.toFixed(0), sx(x), plot.height - 9)
            }
            for (let y = Math.ceil(y0 / grid) * grid; y <= y1; y += grid) {
                ctx.beginPath(); ctx.moveTo(padding, sy(y)); ctx.lineTo(plot.width - padding, sy(y)); ctx.stroke()
                ctx.textAlign = "left"; ctx.fillText(y.toFixed(0), 3, sy(y) + 3)
            }
            ctx.save(); ctx.beginPath(); ctx.rect(padding, padding, w, h); ctx.clip()
            const draw = (data, color, dashed) => {
                ctx.strokeStyle = color; ctx.lineWidth = dashed ? 2 : 2.5
                ctx.setLineDash(dashed ? [7, 5] : [])
                ctx.beginPath()
                for (let i = 0; i < data.length; ++i) {
                    const x = sx(Number(data[i].xM)), y = sy(Number(data[i].yM))
                    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
                }
                ctx.stroke(); ctx.setLineDash([])
            }
            draw(root.reference, Theme.colorInfo, true)
            draw(root.samples, Theme.colorNormal, false)
            ctx.translate(sx(Number(root.pose.xM)), sy(Number(root.pose.yM)))
            ctx.rotate(-Number(root.pose.yawRad))
            ctx.fillStyle = Theme.colorNormal; ctx.strokeStyle = Theme.colorText
            ctx.beginPath(); ctx.moveTo(12, 0); ctx.lineTo(-8, -6); ctx.lineTo(-5, 0); ctx.lineTo(-8, 6); ctx.closePath()
            ctx.fill(); ctx.stroke(); ctx.restore()
            ctx.fillStyle = Theme.colorTextDim; ctx.textAlign = "left"
            ctx.fillText("Y / m", 8, 14)
            ctx.textAlign = "right"; ctx.fillText("X / m", plot.width - 8, plot.height - 9)
        }
    }
}
