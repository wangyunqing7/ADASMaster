// =============================================================================
// 功能：MPC 软件调试页；输入仿真/控制参数，输出轨迹、误差、转角和导出操作。
// 所有后端操作经 appController.mpcSimulation，不访问真实设备。
// =============================================================================
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import ADASMaster

Item {
    id: page
    objectName: "mpcPage"
    readonly property var sim: appController.mpcSimulation // qmllint disable unqualified
    readonly property var m: page.sim.metrics
    readonly property color statusColor: page.sim.safetyBlocked ? Theme.colorEmergency
        : (page.sim.phase === 5 ? Theme.colorError : (page.sim.phase === 2 ? Theme.colorWarning : Theme.colorNormal))
    function parameters(): var {
        return {scenario: scenario.currentValue, speedKph: page.number(speed.text), initialOffsetM: page.number(offset.text),
            durationS: page.number(duration.text), radiusM: page.number(radius.text), laneLengthM: page.number(laneLength.text),
            laneWidthM: page.number(laneWidth.text), playbackRate: playback.currentValue,
            steeringRatio: page.number(ratio.text), actuatorLagMs: page.number(lag.text), sampleTimeMs: period.currentValue,
            horizon: page.number(horizon.text), lateralWeight: page.number(qy.text), headingWeight: page.number(qpsi.text),
            rateWeight: page.number(rd.text), maxLateralAcceleration: page.number(ay.text), maxRobotRateDegS: page.number(rate.text)}
    }
    function number(text: string): real { return text.trim().length ? Number(text) : NaN }

    FileDialog {
        id: exportDialog
        objectName: "mpcExportDialog"
        title: "导出 MPC 仿真结果"
        fileMode: FileDialog.SaveFile
        nameFilters: ["CSV 文件 (*.csv)"]
        defaultSuffix: "csv"
        selectedFile: page.sim.defaultExportUrl
        onAccepted: page.sim.exportCsv(exportDialog.selectedFile)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing
        spacing: Theme.spacing

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            spacing: Theme.spacing
            ColumnLayout {
                spacing: 3
                Text { text: "MPC 横向控制调试"; color: Theme.colorText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontLarge; font.bold: true }
                Text { text: "软件仿真 · 质心轨迹跟踪 · 方向盘角度控制"; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall }
            }
            Item { Layout.fillWidth: true }
            StatusLamp { text: page.sim.safetyBlocked ? "系统急停" : page.sim.statusText; level: page.sim.safetyBlocked ? 3 : (page.sim.phase === 5 ? 2 : (page.sim.phase === 2 ? 1 : 0)) }
            MpcButton {
                objectName: "mpcStartButton"
                text: "开始仿真"; primary: true
                enabled: !page.sim.busy && !page.sim.safetyBlocked
                onClicked: page.sim.start(page.parameters())
            }
            MpcButton { objectName: "mpcPauseButton"; text: page.sim.phase === 2 ? "继续" : "暂停"; enabled: page.sim.busy; onClicked: page.sim.pauseResume() }
            MpcButton { objectName: "mpcStopButton"; text: "停止"; enabled: page.sim.busy; onClicked: page.sim.stop() }
            MpcButton { objectName: "mpcExportButton"; text: "导出 CSV"; enabled: page.sim.sampleCount > 0; onClicked: exportDialog.open() }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 84
            color: Theme.colorBorder
            radius: Theme.radius
            RowLayout {
                anchors.fill: parent
                anchors.margins: 1
                spacing: 1
                MetricCell { Layout.fillWidth: true; Layout.fillHeight: true; label: "当前横向误差"; value: Number(page.m.errorM || 0).toFixed(3); unit: "m"; valueColor: Theme.colorNormal }
                MetricCell { Layout.fillWidth: true; Layout.fillHeight: true; label: "横向误差 RMS"; value: Number(page.m.rmsM || 0).toFixed(3); unit: "m" }
                MetricCell { Layout.fillWidth: true; Layout.fillHeight: true; label: "方向盘目标角"; value: Number(page.m.targetDeg || 0).toFixed(1); unit: "°"; valueColor: Theme.colorInfo }
                MetricCell { Layout.fillWidth: true; Layout.fillHeight: true; label: "方向盘实际角"; value: Number(page.m.actualDeg || 0).toFixed(1); unit: "°"; valueColor: Theme.colorNormal }
                MetricCell { Layout.fillWidth: true; Layout.fillHeight: true; label: "本轮 MPC 耗时"; value: Number(page.m.solveMs || 0).toFixed(2); unit: "ms" }
                MetricCell { Layout.fillWidth: true; Layout.fillHeight: true; label: "仿真时间"; value: Number(page.m.timeS || 0).toFixed(1); unit: "s" }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacing

            PanelFrame {
                Layout.preferredWidth: 270
                Layout.fillHeight: true
                title: "仿真与控制参数"
                titleEn: "parameters"
                contentPadding: Theme.spacingSm

                ScrollView {
                    id: parameterScroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: availableWidth
                    contentHeight: parameterColumn.implicitHeight
                    ScrollBar.vertical.policy: ScrollBar.AlwaysOn
                    ColumnLayout {
                        id: parameterColumn
                        width: Math.max(0, parameterScroll.availableWidth - 12)
                        spacing: Theme.spacingSm

                        GroupLabel { text: "仿真场景" }
                        MpcComboBox {
                            id: scenario
                            objectName: "mpcScenarioInput"
                            Layout.fillWidth: true
                            enabled: !page.sim.busy
                            textRole: "name"; valueRole: "value"; currentIndex: 2
                            model: [{name: "直线跟踪", value: "straight"}, {name: "恒半径圆弧", value: "circle"}, {name: "平滑变道", value: "lane-change"}]
                        }
                        ParameterRow { id: speed; inputName: "mpcSpeedInput"; label: "车速 / km/h"; text: "43.2" }
                        ParameterRow { id: offset; label: "初始偏差 / m"; text: "0.4" }
                        ParameterRow { id: duration; inputName: "mpcDurationInput"; label: "仿真时长 / s"; text: "15" }
                        ParameterRow { id: radius; visible: scenario.currentIndex === 1; label: "圆弧半径 / m"; text: "80" }
                        ParameterRow { id: laneLength; visible: scenario.currentIndex === 2; label: "变道长度 / m"; text: "60" }
                        ParameterRow { id: laneWidth; visible: scenario.currentIndex === 2; label: "变道宽度 / m"; text: "3.5" }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: "播放速度"; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall }
                            MpcComboBox {
                                id: playback; objectName: "mpcPlaybackInput"
                                Layout.preferredWidth: 95; enabled: !page.sim.busy
                                textRole: "name"; valueRole: "value"
                                model: [{name: "1×", value: 1}, {name: "2×", value: 2}, {name: "5×", value: 5}]
                            }
                        }

                        GroupLabel { text: "车辆与执行器" }
                        ParameterRow { id: ratio; label: "转向传动比"; text: "16" }
                        ParameterRow { id: lag; label: "执行器滞后 / ms"; text: "80" }

                        GroupLabel { text: "MPC 参数" }
                        RowLayout {
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: "控制周期"; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall }
                            MpcComboBox {
                                id: period
                                Layout.preferredWidth: 137; enabled: !page.sim.busy
                                textRole: "name"; valueRole: "value"; currentIndex: 1
                                model: [{name: "20 ms · 50 Hz", value: 20}, {name: "50 ms · 20 Hz", value: 50}, {name: "100 ms · 10 Hz", value: 100}]
                            }
                        }
                        ParameterRow { id: horizon; label: "预测步数 N"; text: "30" }
                        ParameterRow { id: qy; label: "横向误差权重 Qy"; text: "40" }
                        ParameterRow { id: qpsi; label: "航向误差权重 Qψ"; text: "12" }
                        ParameterRow { id: rd; label: "转角变化权重 Rd"; text: "20" }

                        GroupLabel { text: "约束限值" }
                        ParameterRow { id: ay; label: "横向加速度 / m/s²"; text: "3.0" }
                        ParameterRow { id: rate; label: "方向盘角速度 / °/s"; text: "600" }
                        MpcButton { Layout.fillWidth: true; text: "清空结果 / 更新预览"; enabled: !page.sim.busy; onClicked: page.sim.preview(page.parameters()) }
                        Text {
                            Layout.fillWidth: true
                            text: "参数在下一次开始仿真时生效。\n20 ms 配置请使用 Release 构建。"
                            wrapMode: Text.WordWrap
                            color: Theme.colorTextMuted
                            font.family: Theme.fontFamily; font.pixelSize: Theme.fontTiny
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.spacing
                PanelFrame {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 350
                    title: "参考与实际轨迹"
                    titleEn: "trajectory"
                    accent: true
                    liveText: page.sim.busy ? "闭环仿真" : "质心坐标 / m"
                    liveColor: page.statusColor
                    RowLayout {
                        Layout.fillWidth: true
                        Legend { text: "参考轨迹"; lineColor: Theme.colorInfo }
                        Legend { text: "实际轨迹"; lineColor: Theme.colorNormal }
                        Item { Layout.fillWidth: true }
                        CheckBox { id: follow; text: "跟随车辆"; checked: true }
                        MpcButton { text: "−"; Layout.preferredWidth: 32; onClicked: trajectory.spanM = Math.min(200, trajectory.spanM * 1.4) }
                        MpcButton { text: "+"; Layout.preferredWidth: 32; onClicked: trajectory.spanM = Math.max(15, trajectory.spanM / 1.4) }
                    }
                    MpcTrajectoryView {
                        id: trajectory
                        objectName: "mpcTrajectoryView"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        reference: page.sim.referencePath
                        samples: page.sim.chartSamples
                        pose: page.m
                        followVehicle: follow.checked
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 225
                    spacing: Theme.spacing
                    PanelFrame {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        title: "横向误差"; titleEn: "lateral error"
                        liveText: "峰值 " + Number(page.m.peakM || 0).toFixed(3) + " m"
                        liveColor: Theme.colorTextDim
                        MpcLineChart { Layout.fillWidth: true; Layout.fillHeight: true; samples: page.sim.chartSamples; minimumRange: 0.05 }
                    }
                    PanelFrame {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        title: "方向盘角度"; titleEn: "steering"
                        RowLayout {
                            Layout.fillWidth: true
                            Legend { text: "目标"; lineColor: Theme.colorInfo }
                            Legend { text: "实际"; lineColor: Theme.colorNormal }
                            Item { Layout.fillWidth: true }
                        }
                        MpcLineChart {
                            Layout.fillWidth: true; Layout.fillHeight: true
                            samples: page.sim.chartSamples; keys: ["targetDeg", "actualDeg"]
                            colors: [Theme.colorInfo, Theme.colorNormal]; unit: "°"; minimumRange: 10
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 25
            Text {
                Layout.fillWidth: true
                text: page.sim.message.length ? page.sim.message : "软件仿真 · 非线性车辆与角度伺服 · " + page.sim.sampleCount + " 个样本"
                elide: Text.ElideMiddle
                color: page.sim.phase === 5 || page.sim.safetyBlocked ? Theme.colorError : Theme.colorTextDim
                font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall
            }
            Text { text: "平均 / 最大求解 " + Number(page.m.meanSolveMs || 0).toFixed(2) + " / " + Number(page.m.maxSolveMs || 0).toFixed(2) + " ms"; color: Theme.colorTextDim; font.family: Theme.fontNumeric; font.pixelSize: Theme.fontTiny }
        }
        ProgressBar { Layout.fillWidth: true; Layout.preferredHeight: 5; value: Number(page.m.progress || 0) }
    }

    component MpcButton : Button {
        id: button
        property bool primary: false
        Layout.preferredHeight: 34
        Layout.preferredWidth: Math.max(62, implicitWidth)
        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall
        contentItem: Text { text: button.text; color: button.enabled ? Theme.colorText : Theme.colorTextMuted; font: button.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: Theme.radius; color: !button.enabled ? Theme.colorPanelAlt : (button.primary ? (button.down ? Theme.colorAccentHover : Theme.colorAccent) : (button.down ? Theme.colorHover : Theme.colorTitleBar)); border.color: button.primary && button.enabled ? Theme.colorAccent : Theme.colorBorder }
    }
    component MpcComboBox : ComboBox {
        id: combo
        implicitHeight: 34
        leftPadding: 9; rightPadding: 22
        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall
        contentItem: Text { text: combo.displayText; font: combo.font; color: combo.enabled ? Theme.colorText : Theme.colorTextMuted; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
        indicator: Text { text: "▾"; x: combo.width - width - 7; y: (combo.height - height) / 2; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall }
        background: Rectangle { color: Theme.colorPanelAlt; radius: Theme.radius; border.color: combo.activeFocus ? Theme.colorAccent : Theme.colorBorder }
    }
    component GroupLabel : Text {
        Layout.fillWidth: true
        Layout.topMargin: Theme.spacingSm
        color: Theme.colorText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; font.bold: true
    }
    component Legend : RowLayout {
        id: legend
        property string text: ""
        property color lineColor: Theme.colorNormal
        spacing: 5
        Rectangle { Layout.preferredWidth: 16; Layout.preferredHeight: 2; color: legend.lineColor }
        Text { text: legend.text; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontTiny }
    }
    component ParameterRow : RowLayout {
        id: row
        property string label: ""
        property string inputName: ""
        property alias text: field.text
        Layout.fillWidth: true
        Text { Layout.fillWidth: true; text: row.label; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall }
        TextField {
            id: field
            objectName: row.inputName
            Layout.preferredWidth: 86
            Layout.preferredHeight: 30
            enabled: !page.sim.busy
            selectByMouse: true
            horizontalAlignment: TextInput.AlignRight
            font.family: Theme.fontNumeric; font.pixelSize: Theme.fontSmall
            color: acceptableInput ? Theme.colorText : Theme.colorError
            validator: DoubleValidator { bottom: -100000; top: 100000; decimals: 4; notation: DoubleValidator.StandardNotation; locale: "en_US" }
            background: Rectangle { color: Theme.colorPanelAlt; radius: Theme.radius; border.color: field.activeFocus ? Theme.colorAccent : Theme.colorBorder }
        }
    }
}
