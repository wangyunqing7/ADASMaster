// =============================================================================
// MapPage.qml
// 功能：场地地图页面（融合原测试执行页）。顶部状态流程条 + 左侧实时地图（主导）+
//       右侧精简工况选择与操作按钮 + 软件急停。
//       运行监控页已删除，全局状态由顶部状态栏与底部车辆平台卡承担。
//       布局原则：地图区域占主导，右侧操作栏紧凑不膨胀。
// 数据源：appController.scenarioEngine / stateManager
// =============================================================================
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import ADASMaster

Item {
    id: page

    // appController 由 main.cpp setContextProperty 注入，静态分析工具无法识别其类型，故此处行内抑制
    readonly property var ctrl: appController // qmllint disable unqualified
    readonly property var stm: page.ctrl.stateManager
    // NCAP 裁决颜色：0=通过绿 1=未通过红 2=待评定灰
    readonly property color verdictColor: page.stm.verdictCode === 0 ? Theme.colorNormal
            : (page.stm.verdictCode === 1 ? Theme.colorEmergency : Theme.colorTextDim)
    // 地图视图参数：可由鼠标滚轮缩放、拖拽平移
    property real pixelsPerMeter: 4.0     // 缩放（1m 对应像素数）
    property real viewPanX: 0.0           // 视图水平偏移（像素，正值右移）
    property real viewPanY: 0.0           // 视图垂直偏移（像素，正值下移）
    // 缩放上下限，防止过大/过小
    readonly property real ppmMin: 0.5
    readonly property real ppmMax: 20.0

    // 场地坐标 -> 屏幕坐标（考虑中心 + 平移 + 缩放）
    function w2sX(wx: real, cw: real): real { return cw / 2 + page.viewPanX + wx * page.pixelsPerMeter }
    function w2sY(wy: real, ch: real): real { return ch / 2 + page.viewPanY - wy * page.pixelsPerMeter }
    // 屏幕坐标 -> 场地坐标（供缩放围绕鼠标点计算）
    function s2wX(sx: real, cw: real): real { return (sx - cw / 2 - page.viewPanX) / page.pixelsPerMeter }
    function s2wY(sy: real, ch: real): real { return -(sy - ch / 2 - page.viewPanY) / page.pixelsPerMeter }

    // 重置视图到默认
    function resetView() {
        page.pixelsPerMeter = 4.0
        page.viewPanX = 0.0
        page.viewPanY = 0.0
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacing

        // ---- 顶部：状态流程条（横跨全宽）----
        PanelFrame {
            Layout.fillWidth: true
            Layout.margins: Theme.spacing
            Layout.preferredHeight: 120
            title: "测试流程"
            titleEn: "state machine"
            liveText: page.stm.testState === 4 ? "运行中"
                    : (page.stm.testState === 3 ? "已预备"
                    : (page.stm.result.length > 0 ? page.stm.result : ""))
            liveColor: page.stm.testState === 7 ? Theme.colorEmergency
                    : (page.stm.testState === 6 ? Theme.colorError
                    : (page.stm.testState === 4 ? Theme.colorNormal : Theme.colorAccent))

            StateFlow {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignHCenter
                currentState: page.stm.testState
            }
        }

        // ---- 异常状态横幅 ----
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacing
            Layout.rightMargin: Theme.spacing
            Layout.preferredHeight: 32
            visible: page.stm.testState === 6 || page.stm.testState === 7
            color: page.stm.testState === 7 ? Theme.colorEmergency : Theme.colorError
            radius: Theme.radius
            Text {
                anchors.centerIn: parent
                text: page.stm.testState === 7
                      ? "⚠ 急停已激活：%1（请复位后继续）".arg(page.ctrl.safetyManager.emergencySource)
                      : "⚠ 故障状态：%1（请检查并复位）".arg(page.stm.result)
                color: "white"
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
                font.bold: true
            }
        }

        // ---- 主体：左地图（主导）+ 右操作栏（紧凑）----
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: Theme.spacing
            Layout.rightMargin: Theme.spacing
            Layout.bottomMargin: Theme.spacing
            spacing: Theme.spacing

            // 左：地图（占绝大部分宽度）
            PanelFrame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: "场地地图"
                titleEn: "live map"
                liveText: "10Hz"
                liveColor: Theme.colorNormal

                Canvas {
                    id: mapCanvas
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Connections {
                        target: page.stm
                        function onStateChanged() { mapCanvas.requestPaint() }
                    }
                    // 视图参数变化时重绘
                    Connections {
                        target: page
                        function onPixelsPerMeterChanged() { mapCanvas.requestPaint() }
                        function onViewPanXChanged() { mapCanvas.requestPaint() }
                        function onViewPanYChanged() { mapCanvas.requestPaint() }
                    }
                    Component.onCompleted: requestPaint()
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()

                    // 鼠标交互：滚轮缩放（围绕鼠标点）+ 拖拽平移
                    MouseArea {
                        id: mapMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        property bool dragging: false
                        property real lastX: 0
                        property real lastY: 0
                        // 光标随状态变化：默认手型，拖拽时闭合手
                        cursorShape: mapMouse.dragging ? Qt.ClosedHandCursor : Qt.OpenHandCursor

                        onWheel: (wheel) => {
                            // 围绕鼠标点缩放：保持鼠标处场地坐标不变
                            const wxBefore = page.s2wX(wheel.x, mapCanvas.width)
                            const wyBefore = page.s2wY(wheel.y, mapCanvas.height)
                            const factor = wheel.angleDelta.y > 0 ? 1.15 : 1/1.15
                            page.pixelsPerMeter = Math.max(page.ppmMin,
                                                    Math.min(page.ppmMax, page.pixelsPerMeter * factor))
                            const wxAfter = page.s2wX(wheel.x, mapCanvas.width)
                            const wyAfter = page.s2wY(wheel.y, mapCanvas.height)
                            // 补偿偏移，让鼠标处场地坐标保持不变
                            page.viewPanX += (wxAfter - wxBefore) * page.pixelsPerMeter
                            page.viewPanY -= (wyAfter - wyBefore) * page.pixelsPerMeter
                        }
                        onPressed: (mouse) => {
                            mapMouse.dragging = true
                            mapMouse.lastX = mouse.x
                            mapMouse.lastY = mouse.y
                        }
                        onReleased: mapMouse.dragging = false
                        onPositionChanged: (mouse) => {
                            if (!mapMouse.dragging) return
                            page.viewPanX += mouse.x - mapMouse.lastX
                            page.viewPanY += mouse.y - mapMouse.lastY
                            mapMouse.lastX = mouse.x
                            mapMouse.lastY = mouse.y
                        }
                    }

                    onPaint: {
                        const ctx = getContext("2d");
                        ctx.reset();
                        const ppm = page.pixelsPerMeter;
                        const W = mapCanvas.width;
                        const H = mapCanvas.height;
                        const wx = (mx) => page.w2sX(mx, W);
                        const wy = (my) => page.w2sY(my, H);

                        // ---- 第 0 层：背景网格（跟随平移与缩放），间距 5m ----
                        const gridM = 5;
                        const gridPx = gridM * ppm;
                        if (gridPx > 4) {
                            ctx.strokeStyle = Theme.colorDivider; ctx.lineWidth = 1;
                            const ox = page.w2sX(0, W);
                            const oy = page.w2sY(0, H);
                            for (let gx = ox % gridPx; gx < W; gx += gridPx) {
                                ctx.beginPath(); ctx.moveTo(gx, 0); ctx.lineTo(gx, H); ctx.stroke();
                            }
                            for (let gy = oy % gridPx; gy < H; gy += gridPx) {
                                ctx.beginPath(); ctx.moveTo(0, gy); ctx.lineTo(W, gy); ctx.stroke();
                            }
                        }
                        // 坐标轴（过场地原点的十字线）
                        ctx.strokeStyle = Theme.colorBorder; ctx.lineWidth = 2;
                        const axisY = page.w2sY(0, H);
                        const axisX = page.w2sX(0, W);
                        ctx.beginPath(); ctx.moveTo(0, axisY); ctx.lineTo(W, axisY); ctx.stroke();
                        ctx.beginPath(); ctx.moveTo(axisX, 0); ctx.lineTo(axisX, H); ctx.stroke();
                        // 安全边界（±60m 矩形虚线）
                        ctx.strokeStyle = Theme.colorError; ctx.lineWidth = 2; ctx.setLineDash([8, 6]);
                        ctx.strokeRect(page.w2sX(-60, W), page.w2sY(60, H), 120 * ppm, 120 * ppm);
                        ctx.setLineDash([]);
                        // 起点区域（绿色圆，场地坐标 -50,0）
                        ctx.fillStyle = Theme.colorNormal; ctx.globalAlpha = 0.25;
                        ctx.beginPath(); ctx.arc(page.w2sX(-50, W), page.w2sY(0, H), 16, 0, 2 * Math.PI); ctx.fill();
                        ctx.globalAlpha = 1.0;

                        // ---- 第 1 层：工况预设航点路径（"应该如何走"，淡色虚线 + 空心圆点）----
                        // 这层让操作员看清工况定义的目标物运动规划
                        const wps = page.stm.targetWaypoints;
                        if (wps && wps.length >= 2) {
                            ctx.strokeStyle = Theme.colorWarning; ctx.lineWidth = 1.5;
                            ctx.setLineDash([4, 4]); ctx.globalAlpha = 0.5;
                            ctx.beginPath();
                            ctx.moveTo(wx(wps[0].x), wy(wps[0].y));
                            for (let i = 1; i < wps.length; ++i) ctx.lineTo(wx(wps[i].x), wy(wps[i].y));
                            ctx.stroke();
                            ctx.setLineDash([]);
                            // 航点空心圆
                            ctx.globalAlpha = 0.7;
                            for (let i = 0; i < wps.length; ++i) {
                                ctx.strokeStyle = Theme.colorWarning; ctx.lineWidth = 1.5;
                                ctx.beginPath(); ctx.arc(wx(wps[i].x), wy(wps[i].y), 4, 0, 2 * Math.PI); ctx.stroke();
                            }
                            ctx.globalAlpha = 1.0;
                        }

                        // ---- 第 2 层：运行累积轨迹历史（"实际怎么走的"，实线）----
                        // 主车轨迹（蓝色实线），目标物轨迹（黄色实线）
                        const drawTrail = (trail, color, width) => {
                            if (!trail || trail.length < 2) return;
                            ctx.strokeStyle = color; ctx.lineWidth = width;
                            ctx.beginPath();
                            ctx.moveTo(wx(trail[0].x), wy(trail[0].y));
                            for (let i = 1; i < trail.length; ++i) ctx.lineTo(wx(trail[i].x), wy(trail[i].y));
                            ctx.stroke();
                        };
                        drawTrail(page.stm.targetTrail, Theme.colorWarning, 2);
                        drawTrail(page.stm.egoTrail, Theme.colorAccent, 2.5);

                        // ---- 第 3 层：当前瞬时图标（最顶层）----
                        // 主车（三角形）
                        const ex = page.w2sX(page.stm.egoX, W);
                        const ey = page.w2sY(page.stm.egoY, H);
                        ctx.fillStyle = Theme.colorAccent;
                        ctx.beginPath(); ctx.moveTo(ex, ey - 8); ctx.lineTo(ex - 6, ey + 6); ctx.lineTo(ex + 6, ey + 6); ctx.closePath(); ctx.fill();
                        // 目标物（方块，碰撞风险时变红）
                        const tx = page.w2sX(page.stm.tgtX, W);
                        const ty = page.w2sY(page.stm.tgtY, H);
                        ctx.fillStyle = page.stm.collisionRisk ? Theme.colorEmergency : Theme.colorWarning;
                        ctx.fillRect(tx - 7, ty - 7, 14, 14);
                        // 主车-目标物连线 + 距离
                        ctx.strokeStyle = Theme.colorTextMuted; ctx.lineWidth = 1;
                        ctx.beginPath(); ctx.moveTo(ex, ey); ctx.lineTo(tx, ty); ctx.stroke();
                        ctx.fillStyle = Theme.colorText; ctx.font = "12px sans-serif";
                        ctx.fillText(page.stm.distance.toFixed(1) + " m", (ex + tx) / 2 + 6, (ey + ty) / 2 - 6);
                    }
                }

                // 地图右上角：缩放比例 + 重置视图按钮（浮层）
                ColumnLayout {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.spacingSm
                    spacing: Theme.spacingTiny
                    Rectangle {
                        color: Theme.colorPanel
                        radius: Theme.radius
                        border.color: Theme.colorBorder; border.width: 1
                        Layout.preferredWidth: zoomText.implicitWidth + 16
                        Layout.preferredHeight: 24
                        Text {
                            id: zoomText
                            anchors.centerIn: parent
                            text: "1m = %1px".arg(page.pixelsPerMeter.toFixed(1))
                            color: Theme.colorTextDim
                            font.family: Theme.fontNumeric; font.pixelSize: Theme.fontTiny
                        }
                    }
                    Button {
                        id: btnResetView
                        text: "重置视图"
                        Layout.preferredHeight: 26
                        contentItem: Text { text: btnResetView.text; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontTiny; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: btnResetView.down ? Theme.colorHover : Theme.colorPanel; radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
                        onClicked: page.resetView()
                    }
                }

                // 地图左下角：轨迹图例（说明各层绘制含义）
                Rectangle {
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.margins: Theme.spacingSm
                    color: Theme.colorPanel
                    radius: Theme.radius
                    border.color: Theme.colorBorder; border.width: 1
                    width: legendCol.implicitWidth + 16
                    height: legendCol.implicitHeight + 12
                    ColumnLayout {
                        id: legendCol
                        anchors.centerIn: parent
                        spacing: 4
                        // 主车轨迹
                        RowLayout {
                            spacing: 6
                            Rectangle { Layout.preferredWidth: 18; Layout.preferredHeight: 3; color: Theme.colorAccent }
                            Text { text: "主车轨迹"; color: Theme.colorTextMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontTiny }
                        }
                        // 目标物轨迹
                        RowLayout {
                            spacing: 6
                            Rectangle { Layout.preferredWidth: 18; Layout.preferredHeight: 3; color: Theme.colorWarning }
                            Text { text: "目标物轨迹"; color: Theme.colorTextMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontTiny }
                        }
                        // 工况预设航点
                        RowLayout {
                            spacing: 6
                            Canvas {
                                Layout.preferredWidth: 18; Layout.preferredHeight: 6
                                onPaint: {
                                    const c = getContext("2d"); c.reset();
                                    c.strokeStyle = Theme.colorWarning; c.lineWidth = 1.5; c.setLineDash([3, 3]);
                                    c.beginPath(); c.moveTo(0, 3); c.lineTo(18, 3); c.stroke();
                                }
                            }
                            Text { text: "工况航点路径"; color: Theme.colorTextMuted; font.family: Theme.fontFamily; font.pixelSize: Theme.fontTiny }
                        }
                    }
                }
            }

            // 右：紧凑操作栏（固定窄宽度，不使用 fillHeight 膨胀）
            PanelFrame {
                Layout.preferredWidth: 200
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignTop
                title: "工况与操作"
                titleEn: "controls"

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.spacing

                    // 小节标签
                    Text {
                        text: "工况选择"
                        color: Theme.colorTextDim
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontTiny
                        font.bold: true
                        Layout.fillWidth: true
                    }
                    // 工况下拉 + 加载按钮 同一栏
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacingSm

                        ComboBox {
                            id: scenarioCombo
                            Layout.fillWidth: true
                            model: page.ctrl.scenarioNames()
                            contentItem: Text { text: scenarioCombo.currentText; color: Theme.colorText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; leftPadding: 8; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: Theme.colorHover; radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
                        }
                        Button {
                            id: btnLoad
                            text: "加载"
                            Layout.preferredWidth: 56
                            Layout.preferredHeight: 34
                            enabled: page.ctrl.operationalReady
                            contentItem: Text { text: btnLoad.text; color: "white"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: btnLoad.down ? Theme.colorAccentHover : (btnLoad.enabled ? Theme.colorAccent : Theme.colorDisable); radius: Theme.radius }
                            onClicked: page.ctrl.scenarioEngine.loadScenario(scenarioCombo.currentText)
                        }
                    }
                    Text {
                        text: "已加载：" + page.stm.scenarioName
                        color: Theme.colorTextMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontTiny
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    // 分隔线
                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.colorBorder }

                    Text {
                        text: "操作"
                        color: Theme.colorTextDim
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontTiny
                        font.bold: true
                        Layout.fillWidth: true
                    }

                    Button {
                        id: btnCheck; text: "检查设备"
                        Layout.fillWidth: true; Layout.preferredHeight: 36
                        enabled: page.ctrl.operationalReady && page.stm.testState === 0
                        contentItem: Text { text: btnCheck.text; color: Theme.colorText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: btnCheck.down ? Theme.colorAccentHover : (btnCheck.enabled ? Theme.colorHover : Theme.colorDisable); radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
                        onClicked: page.ctrl.scenarioEngine.checkDevices()
                    }
                    Button {
                        id: btnArm; text: "预备"
                        Layout.fillWidth: true; Layout.preferredHeight: 36
                        enabled: page.ctrl.operationalReady && page.stm.testState === 2
                        contentItem: Text { text: btnArm.text; color: Theme.colorText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: btnArm.down ? Theme.colorAccentHover : (btnArm.enabled ? Theme.colorHover : Theme.colorDisable); radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
                        onClicked: page.ctrl.scenarioEngine.arm()
                    }
                    Button {
                        id: btnStartStop
                        text: page.stm.testState === 4 ? "■ 停止" : "▶ 开始"
                        Layout.fillWidth: true; Layout.preferredHeight: 40
                        // Armed 态可开始，Running 态可停止
                        enabled: page.stm.testState === 3 || page.stm.testState === 4
                        contentItem: Text {
                            text: btnStartStop.text
                            color: page.stm.testState === 4 ? Theme.colorWarning : Theme.colorNormal
                            font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; font.bold: true
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: btnStartStop.down
                                 ? (page.stm.testState === 4 ? Qt.darker(Theme.colorWarning, 1.2) : Qt.darker(Theme.colorNormal, 1.2))
                                 : (btnStartStop.enabled ? Theme.colorHover : Theme.colorDisable)
                            radius: Theme.radius
                            border.color: page.stm.testState === 4 ? Theme.colorWarning : Theme.colorNormal
                            border.width: 2
                        }
                        onClicked: {
                            if (page.stm.testState === 4) page.ctrl.scenarioEngine.stop()
                            else page.ctrl.scenarioEngine.start()
                        }
                    }

                    // 弹性占位
                    Item { Layout.fillHeight: true; Layout.fillWidth: true }

                    // ---- NCAP 评估结果卡（测试结束时显现）----
                    // verdictCode: 0=通过 1=未通过 2=待评定
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 88
                        visible: page.stm.verdictText.length > 0
                        radius: Theme.radius
                        // 颜色：通过绿、未通过红、待评定灰
                        color: Qt.rgba(page.verdictColor.r, page.verdictColor.g, page.verdictColor.b, 0.12)
                        border.color: page.verdictColor
                        border.width: 2

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.spacingSm
                            spacing: 2

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spacingSm
                                // 裁决徽标
                                Rectangle {
                                    Layout.preferredWidth: 56
                                    Layout.preferredHeight: 22
                                    radius: 11
                                    color: page.verdictColor
                                    Text {
                                        anchors.centerIn: parent
                                        text: page.stm.verdictText
                                        color: "white"
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontTiny
                                        font.bold: true
                                    }
                                }
                                // 评分
                                Text {
                                    text: "评分 %1%".arg(page.stm.scorePercent.toFixed(0))
                                    color: page.verdictColor
                                    font.family: Theme.fontNumeric
                                    font.pixelSize: Theme.fontMedium
                                    font.bold: true
                                    Layout.fillWidth: true
                                }
                            }
                            // 详情
                            Text {
                                Layout.fillWidth: true
                                text: page.stm.verdictDetail
                                color: Theme.colorTextMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontTiny
                                wrapMode: Text.WordWrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }
    }
}
