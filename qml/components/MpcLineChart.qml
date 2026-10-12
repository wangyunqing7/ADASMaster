// =============================================================================
// 功能：MPC 时间序列图；输入有界样本/曲线键/颜色，输出带时间与数值轴的曲线。
// =============================================================================
import QtQuick
import ADASMaster

Item {
    id: root
    property var samples: []
    property var keys: ["errorM"]
    property var colors: [Theme.colorNormal]
    property real minimumRange: 0.1
    property string unit: "m"
    onSamplesChanged: plot.requestPaint()
    onKeysChanged: plot.requestPaint()

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
            const left = 49, top = 16, right = plot.width - 14, bottom = plot.height - 28
            if (right <= left || bottom <= top) return
            const data = root.samples
            let magnitude = root.minimumRange
            for (let i = 0; i < data.length; ++i)
                for (let j = 0; j < root.keys.length; ++j)
                    magnitude = Math.max(magnitude, Math.abs(Number(data[i][root.keys[j]])))
            magnitude *= 1.15
            const lastTime = data.length ? Number(data[data.length - 1].timeS) : 10
            const firstTime = Math.max(0, lastTime - 20)
            const maxTime = Math.max(firstTime + 1, lastTime)
            const sx = (t) => left + (t - firstTime) / (maxTime - firstTime) * (right - left)
            const sy = (y) => top + (magnitude - y) / (2 * magnitude) * (bottom - top)
            ctx.font = Theme.fontSmall + "px " + Theme.fontNumeric
            ctx.lineWidth = 1
            for (let i = 0; i <= 4; ++i) {
                const y = top + i / 4 * (bottom - top)
                ctx.strokeStyle = Theme.colorBorder
                ctx.beginPath(); ctx.moveTo(left, y); ctx.lineTo(right, y); ctx.stroke()
                ctx.fillStyle = Theme.colorTextDim
                ctx.textAlign = "right"
                ctx.fillText((magnitude * (1 - i / 2)).toFixed(magnitude < 1 ? 2 : 0), left - 7, y + 3)
                const x = left + i / 4 * (right - left)
                ctx.beginPath(); ctx.moveTo(x, top); ctx.lineTo(x, bottom); ctx.stroke()
                ctx.textAlign = "center"
                ctx.fillText(Number(firstTime + i / 4 * (maxTime - firstTime)).toFixed(1), x, bottom + 16)
            }
            ctx.fillStyle = Theme.colorTextDim
            ctx.textAlign = "left"; ctx.fillText(root.unit, 8, 13)
            ctx.textAlign = "right"; ctx.fillText("s", right + 10, bottom + 16)
            ctx.strokeStyle = Theme.colorTextMuted
            ctx.setLineDash([3, 4]); ctx.beginPath(); ctx.moveTo(left, sy(0)); ctx.lineTo(right, sy(0)); ctx.stroke()
            ctx.setLineDash([])
            ctx.save(); ctx.beginPath(); ctx.rect(left, top, right - left, bottom - top); ctx.clip()
            for (let j = 0; j < root.keys.length; ++j) {
                ctx.strokeStyle = root.colors[j]
                ctx.lineWidth = 2
                ctx.beginPath()
                let begun = false
                for (let i = 0; i < data.length; ++i) {
                    if (Number(data[i].timeS) < firstTime) continue
                    const x = sx(Number(data[i].timeS)), y = sy(Number(data[i][root.keys[j]]))
                    if (!begun) { ctx.moveTo(x, y); begun = true } else ctx.lineTo(x, y)
                }
                ctx.stroke()
            }
            ctx.restore()
            if (!data.length) {
                ctx.fillStyle = Theme.colorTextMuted; ctx.textAlign = "center"
                ctx.fillText("运行仿真后显示曲线", (left + right) / 2, (top + bottom) / 2)
            }
        }
    }
}
