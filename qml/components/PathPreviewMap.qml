// =============================================================================
// PathPreviewMap.qml
// 功能：只读轨迹预览/回放 Canvas。支持主车+平板双路径同时显示。
//   - path 模式：同时绘制主车航点路径（蓝色实线）与平板航点路径（黄色虚线），
//     高亮当前编辑中的路径（activeTarget），带图例。
//   - replay 模式：接收 samples，播放历史采样轨迹。
// 输入：mode、egoWaypoints、targetWaypoints、activeTarget("ego"|"target")、samples、replayIndex。
// 输出：只读轨迹可视化。
// =============================================================================
import QtQuick
import ADASMaster

Canvas {
    id: canvas
    property string mode: "path"                 // "path" | "replay"
    property var egoWaypoints: []                 // 主车航点 QVariantList of {x,y,speed}
    property var targetWaypoints: []              // 平板航点 QVariantList of {x,y,speed}
    property string activeTarget: "target"        // 当前编辑的路径：高亮显示
    property var waypoints: []                    // 兼容旧绑定（= active 对应的路径）
    property var samples: []                      // replay 模式采样数据
    property int replayIndex: 0
    property real pixelsPerMeter: 3.0

    onEgoWaypointsChanged: requestPaint()
    onTargetWaypointsChanged: requestPaint()
    onActiveTargetChanged: requestPaint()
    onWaypointsChanged: requestPaint()
    onSamplesChanged: requestPaint()
    onReplayIndexChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    Component.onCompleted: requestPaint()

    onPaint: {
        const ctx = getContext("2d");
        ctx.reset();
        const cx = canvas.width / 2;
        const cy = canvas.height / 2;
        const ppm = canvas.pixelsPerMeter;
        const w2sX = (wx) => cx + wx * ppm;
        const w2sY = (wy) => cy - wy * ppm;

        // 背景网格（5m 间距）
        ctx.strokeStyle = Theme.colorDivider; ctx.lineWidth = 1;
        const gridPx = 5 * ppm;
        if (gridPx > 4) {
            for (let gx = cx % gridPx; gx < canvas.width; gx += gridPx) {
                ctx.beginPath(); ctx.moveTo(gx, 0); ctx.lineTo(gx, canvas.height); ctx.stroke();
            }
            for (let gy = cy % gridPx; gy < canvas.height; gy += gridPx) {
                ctx.beginPath(); ctx.moveTo(0, gy); ctx.lineTo(canvas.width, gy); ctx.stroke();
            }
        }
        // 坐标轴
        ctx.strokeStyle = Theme.colorBorder; ctx.lineWidth = 1;
        ctx.beginPath(); ctx.moveTo(0, cy); ctx.lineTo(canvas.width, cy); ctx.stroke();
        ctx.beginPath(); ctx.moveTo(cx, 0); ctx.lineTo(cx, canvas.height); ctx.stroke();

        if (canvas.mode === "path") {
            // 先画非活动路径（虚线淡显），再画活动路径（实线高亮）
            drawPath(ctx, canvas.targetWaypoints, w2sX, w2sY,
                     Theme.colorWarning, canvas.activeTarget === "target");
            drawPath(ctx, canvas.egoWaypoints, w2sX, w2sY,
                     Theme.colorAccent, canvas.activeTarget === "ego");
            drawLegend(ctx);
        } else {
            // replay 模式：播放采样轨迹
            const sm = canvas.samples;
            const end = Math.min(canvas.replayIndex, sm.length);
            ctx.strokeStyle = Theme.colorAccent; ctx.lineWidth = 1.5;
            ctx.beginPath();
            for (let i = 0; i < end; ++i) {
                const x = w2sX(sm[i].egoX), y = w2sY(sm[i].egoY);
                if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
            }
            ctx.stroke();
            ctx.strokeStyle = Theme.colorWarning;
            ctx.beginPath();
            for (let i = 0; i < end; ++i) {
                const x = w2sX(sm[i].targetX), y = w2sY(sm[i].targetY);
                if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
            }
            ctx.stroke();
            if (end > 0) {
                const last = sm[end - 1];
                const ex = w2sX(last.egoX), ey = w2sY(last.egoY);
                const tx = w2sX(last.targetX), ty = w2sY(last.targetY);
                ctx.fillStyle = Theme.colorAccent;
                ctx.beginPath(); ctx.moveTo(ex, ey - 7); ctx.lineTo(ex - 5, ey + 5); ctx.lineTo(ex + 5, ey + 5); ctx.closePath(); ctx.fill();
                ctx.fillStyle = last.collisionRisk ? Theme.colorEmergency : Theme.colorWarning;
                ctx.fillRect(tx - 6, ty - 6, 12, 12);
            }
        }
    }

    // 绘制单条路径：连线 + 圆点 + 序号（活动路径高亮）
    function drawPath(ctx, pts, w2sX, w2sY, color, isActive) {
        if (!pts || pts.length === 0) return;
        if (pts.length >= 2) {
            ctx.strokeStyle = color;
            ctx.lineWidth = isActive ? 2.5 : 1.5;
            if (!isActive) ctx.setLineDash([6, 4]);
            ctx.beginPath();
            ctx.moveTo(w2sX(pts[0].x), w2sY(pts[0].y));
            for (let i = 1; i < pts.length; ++i) ctx.lineTo(w2sX(pts[i].x), w2sY(pts[i].y));
            ctx.stroke();
            ctx.setLineDash([]);
        }
        ctx.fillStyle = color;
        for (let i = 0; i < pts.length; ++i) {
            const r = isActive ? 6 : 4;
            ctx.beginPath(); ctx.arc(w2sX(pts[i].x), w2sY(pts[i].y), r, 0, 2 * Math.PI); ctx.fill();
        }
        if (isActive) {
            ctx.fillStyle = Theme.colorText; ctx.font = "10px monospace";
            for (let i = 0; i < pts.length; ++i) {
                ctx.fillText(String(i + 1), w2sX(pts[i].x) + 8, w2sY(pts[i].y) - 8);
            }
        }
    }

    // 绘制图例
    function drawLegend(ctx) {
        const x = 10, y = canvas.height - 50;
        ctx.font = "11px sans-serif";
        ctx.fillStyle = Theme.colorAccent;
        ctx.fillRect(x, y, 12, 3);
        ctx.fillStyle = Theme.colorTextDim;
        ctx.fillText("主车", x + 16, y + 4);
        ctx.fillStyle = Theme.colorWarning;
        ctx.fillRect(x, y + 16, 12, 3);
        ctx.fillStyle = Theme.colorTextDim;
        ctx.fillText("平板", x + 16, y + 20);
    }
}
