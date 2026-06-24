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

        // ---- 第 0 层：背景网格（降低存在感，避免与轨迹混淆）----
        // 网格颜色更淡 + 透明度，让轨迹线成为视觉焦点
        const gridPx = 5 * ppm;
        if (gridPx > 4) {
            ctx.strokeStyle = Theme.colorDivider; ctx.lineWidth = 1; ctx.globalAlpha = 0.35;
            for (let gx = cx % gridPx; gx < canvas.width; gx += gridPx) {
                ctx.beginPath(); ctx.moveTo(gx, 0); ctx.lineTo(gx, canvas.height); ctx.stroke();
            }
            for (let gy = cy % gridPx; gy < canvas.height; gy += gridPx) {
                ctx.beginPath(); ctx.moveTo(0, gy); ctx.lineTo(canvas.width, gy); ctx.stroke();
            }
            ctx.globalAlpha = 1.0;
        }
        // 坐标轴（淡，仅作参照）
        ctx.strokeStyle = Theme.colorBorder; ctx.lineWidth = 1; ctx.globalAlpha = 0.6;
        ctx.beginPath(); ctx.moveTo(0, cy); ctx.lineTo(canvas.width, cy); ctx.stroke();
        ctx.beginPath(); ctx.moveTo(cx, 0); ctx.lineTo(cx, canvas.height); ctx.stroke();
        ctx.globalAlpha = 1.0;

        if (canvas.mode === "path") {
            // ---- 第 1 层：非活动路径（灰色低透明度，作为参照背景）----
            // 先画非活动的那条，让活动路径叠在上层更突出
            if (canvas.activeTarget !== "target")
                drawPath(ctx, canvas.targetWaypoints, w2sX, w2sY,
                         Theme.colorWarning, false);
            if (canvas.activeTarget !== "ego")
                drawPath(ctx, canvas.egoWaypoints, w2sX, w2sY,
                         Theme.colorAccent, false);

            // ---- 第 2 层：活动路径（高亮 + 光晕，视觉焦点）----
            // 活动路径先画一圈半透明粗光晕（halo），再画实线，让它在网格上跳出来
            if (canvas.activeTarget === "target")
                drawPath(ctx, canvas.targetWaypoints, w2sX, w2sY,
                         Theme.colorWarning, true);
            if (canvas.activeTarget === "ego")
                drawPath(ctx, canvas.egoWaypoints, w2sX, w2sY,
                         Theme.colorAccent, true);

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

    // 绘制单条路径。isActive=true 时画半透明光晕底 + 高亮实线 + 序号；
    // isActive=false 时画淡灰色参照线（透明度低，避免与活动路径争抢注意力）。
    function drawPath(ctx, pts, w2sX, w2sY, color, isActive) {
        if (!pts || pts.length === 0) return;
        if (pts.length >= 2) {
            if (isActive) {
                // 活动路径：先画光晕（半透明粗线），再画清晰实线
                ctx.strokeStyle = color; ctx.lineWidth = 7; ctx.globalAlpha = 0.18;
                ctx.setLineDash([]);
                ctx.beginPath();
                ctx.moveTo(w2sX(pts[0].x), w2sY(pts[0].y));
                for (let i = 1; i < pts.length; ++i) ctx.lineTo(w2sX(pts[i].x), w2sY(pts[i].y));
                ctx.stroke();
                ctx.globalAlpha = 1.0;
                ctx.lineWidth = 2.5;
            } else {
                // 非活动参照：虚线 + 低透明度
                ctx.strokeStyle = color; ctx.lineWidth = 1.5; ctx.globalAlpha = 0.4;
                ctx.setLineDash([6, 4]);
            }
            ctx.beginPath();
            ctx.moveTo(w2sX(pts[0].x), w2sY(pts[0].y));
            for (let i = 1; i < pts.length; ++i) ctx.lineTo(w2sX(pts[i].x), w2sY(pts[i].y));
            ctx.stroke();
            ctx.setLineDash([]);
            ctx.globalAlpha = 1.0;
        }
        // 航点圆点
        ctx.fillStyle = color;
        ctx.globalAlpha = isActive ? 1.0 : 0.5;
        for (let i = 0; i < pts.length; ++i) {
            const r = isActive ? 6 : 4;
            ctx.beginPath(); ctx.arc(w2sX(pts[i].x), w2sY(pts[i].y), r, 0, 2 * Math.PI); ctx.fill();
        }
        ctx.globalAlpha = 1.0;
        if (isActive) {
            // 起终点标注 + 序号（仅活动路径，避免文字泛滥）
            ctx.fillStyle = Theme.colorText; ctx.font = "bold 10px monospace";
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
