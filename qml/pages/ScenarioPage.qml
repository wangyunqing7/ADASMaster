// =============================================================================
// ScenarioPage.qml
// 功能：工况管理页面（融合原数据回放页）。左侧工况列表 + 右上路径编辑
//       （航点表格 + 地图预览）+ 右下该工况历史回放记录。选中记录可轨迹回放。
// 数据源：appController.scenarioNames/scenarioWaypoints/runSamples、replayListModel
// =============================================================================
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import ADASMaster

pragma ComponentBehavior: Bound

Item {
    id: page
    // appController 由 main.cpp setContextProperty 注入，静态分析工具无法识别其类型，故此处行内抑制
    readonly property var ctrl: appController // qmllint disable unqualified

    property int selected: 0
    property var scenarioNames: page.ctrl.scenarioNames()
    property var scenarioTree: page.ctrl.scenarioTree()
    property var currentTargetWaypoints: []    // 平板（目标物）路径
    property var currentEgoWaypoints: []        // 主车路径
    property var currentTargetSegments: []
    property var currentEgoSegments: []
    // 当前编辑的目标："target"（平板）或 "ego"（主车），决定表格编辑哪条路径
    property string editingTarget: "target"
    property string currentReplayRunId: ""
    property var currentSamples: []
    property int replayIndex: 0
    property bool replayPlaying: false
    // 展开的组索引集合（用 JSON 数组模拟集合）
    property var expandedGroups: ({})
    // 右键菜单上下文：当前右键的组名
    property string contextGroupName: ""
    property string contextGroupId: ""
    property string contextSubgroupId: ""
    property string createNodeKind: "group"
    property string createGroupId: ""
    property string createSubgroupId: ""

    function openCreateNode(kind, groupId, subgroupId) {
        createNodeKind = kind
        createGroupId = groupId || ""
        createSubgroupId = subgroupId || ""
        newScenarioName.text = ""
        newScenarioError.text = ""
        newScenarioPopup.open()
    }

    function allSubgroups(groupId) {
        const output = []
        for (let i = 0; i < page.scenarioTree.length; ++i) {
            const group = page.scenarioTree[i]
            if (groupId && group.id !== groupId) continue
            for (let j = 0; j < group.subgroups.length; ++j) {
                output.push({ id: group.subgroups[j].id, name: group.name + " / " + group.subgroups[j].name, groupId: group.id })
            }
        }
        return output
    }

    // 当前编辑中的路径（根据 editingTarget 返回对应路径，供 WaypointEditor 绑定）
    readonly property var currentWaypoints: page.editingTarget === "ego" ? page.currentEgoWaypoints : page.currentTargetWaypoints
    readonly property var currentSegments: page.editingTarget === "ego" ? page.currentEgoSegments : page.currentTargetSegments

    // 选中工况变化时加载两条路径
    function loadSelectedWaypoints() {
        if (page.scenarioNames.length === 0) {
            page.currentTargetWaypoints = []
            page.currentEgoWaypoints = []
            return
        }
        const name = page.scenarioNames[page.selected]
        page.currentTargetWaypoints = page.ctrl.scenarioWaypoints(name)
        page.currentEgoWaypoints = page.ctrl.scenarioEgoWaypoints(name)
        page.currentTargetSegments = page.ctrl.scenarioTrajectory(name, "target")
        page.currentEgoSegments = page.ctrl.scenarioTrajectory(name, "ego")
        const targetPreview = page.ctrl.previewTrajectory(page.currentTargetSegments)
        const egoPreview = page.ctrl.previewTrajectory(page.currentEgoSegments)
        if (targetPreview.length > 0) page.currentTargetWaypoints = targetPreview
        if (egoPreview.length > 0) page.currentEgoWaypoints = egoPreview
        page.ctrl.replayListModel.filterByScenario(name)
        // 切换工况时清空回放
        page.currentReplayRunId = ""
        page.currentSamples = []
        page.replayIndex = 0
        page.replayPlaying = false
        replayTimer.stop()
        pathPreview.mode = "path"
    }

    // WaypointEditor 保存路径后，根据 editingTarget 写回对应路径
    function onPathSaved(newPath) {
        if (page.editingTarget === "ego") page.currentEgoWaypoints = newPath
        else page.currentTargetWaypoints = newPath
        pathPreview.egoWaypoints = page.currentEgoWaypoints
        pathPreview.targetWaypoints = page.currentTargetWaypoints
    }

    function onTrajectoryPreview(points) {
        if (page.editingTarget === "ego") page.currentEgoWaypoints = points
        else page.currentTargetWaypoints = points
    }

    function onTrajectorySaved(segments) {
        const name = page.scenarioNames[page.selected]
        if (!page.ctrl.saveScenarioTrajectory(name, page.editingTarget, segments)) return
        if (page.editingTarget === "ego") page.currentEgoSegments = page.ctrl.normalizeTrajectory(segments)
        else page.currentTargetSegments = page.ctrl.normalizeTrajectory(segments)
        page.onTrajectoryPreview(page.ctrl.previewTrajectory(segments))
    }

    function toggleGroup(g) {
        const copy = Object.assign({}, page.expandedGroups);
        if (copy[g]) delete copy[g]; else copy[g] = true;
        page.expandedGroups = copy;
    }

    // 选中某条记录后加载采样
    function loadRunSamples(runId: string) {
        page.currentReplayRunId = runId;
        page.currentSamples = page.ctrl.runSamples(runId);
        page.replayIndex = 0;
        page.replayPlaying = page.currentSamples.length > 0;
        pathPreview.mode = "replay";
        if (page.replayPlaying) replayTimer.start();
    }

    Component.onCompleted: loadSelectedWaypoints()

    Timer {
        id: replayTimer
        interval: 100   // 10Hz，与采样频率一致
        repeat: true
        onTriggered: {
            if (page.replayIndex < page.currentSamples.length - 1) {
                page.replayIndex++;
            } else {
                page.replayPlaying = false;
                replayTimer.stop();
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: Theme.spacing

        // 左：工况列表
        PanelFrame {
            Layout.fillHeight: true
            Layout.preferredWidth: 220
            title: "工况列表"
            titleEn: "scenarios"

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                Text { text: "工况结构"; color: Theme.colorTextMuted; font.pixelSize: Theme.fontTiny; Layout.fillWidth: true }
                Button {
                    id: createNodeButton
                    text: "+"
                    Layout.preferredWidth: 30; Layout.preferredHeight: 28
                    ToolTip.visible: hovered; ToolTip.text: "新建场景结构"
                    onClicked: nodeCreateMenu.popup()
                }
            }

            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true
                clip: true

                // 树形工况列表：组（一级）→ 子组（二级）→ 工况（三级）
                // 视觉层次：组=深色条+三角箭头+图标，子组=缩进+方块标记，工况=卡片样式+深度缩进
                Column {
                    id: treeColumn
                    width: parent.width
                    spacing: Theme.spacingSm

                    // 顶级标签
                    Text {
                        width: parent.width
                        leftPadding: Theme.spacingSm
                        bottomPadding: 2
                        text: "工况库"
                        color: Theme.colorTextMuted
                        font.family: Theme.fontFamily; font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: 1
                    }

                    Repeater {
                        model: page.scenarioTree

                        // ===== 一级：场景组 =====
                        Column {
                            id: groupNode
                            required property var modelData
                            required property int index
                            width: parent.width
                            spacing: 2

                            // 组标题条（深色背景，可点击展开/折叠）
                            Rectangle {
                                width: parent.width
                                height: 36
                                radius: Theme.radius
                                color: page.expandedGroups[groupNode.modelData.group] ? Theme.colorAccent : Theme.colorTitleBar
                                border.color: page.expandedGroups[groupNode.modelData.group] ? Theme.colorAccent : Theme.colorBorder
                                border.width: 1

                                MouseArea {
                                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                    onClicked: (mouse) => {
                                        if (mouse.button === Qt.RightButton) {
                                            page.contextGroupName = groupNode.modelData.group
                                            page.contextGroupId = groupNode.modelData.id
                                            page.contextSubgroupId = ""
                                            contextMenu.groupMenu.popup()
                                        } else {
                                            page.toggleGroup(groupNode.modelData.group)
                                        }
                                    }
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Theme.spacingSm; anchors.rightMargin: Theme.spacingSm
                                    spacing: Theme.spacingSm

                                    // 展开/折叠三角
                                    Text {
                                        text: page.expandedGroups[groupNode.modelData.group] ? "▼" : "▶"
                                        color: page.expandedGroups[groupNode.modelData.group] ? "white" : Theme.colorTextDim
                                        font.family: Theme.fontNumeric; font.pixelSize: 10
                                        Layout.preferredWidth: 12
                                    }
                                    // 组图标（文件夹样式）
                                    Text {
                                        text: "📁"
                                        color: "white"
                                        font.pixelSize: 12
                                        Layout.preferredWidth: 16
                                    }
                                    // 组名
                                    Text {
                                        text: groupNode.modelData.group
                                        color: page.expandedGroups[groupNode.modelData.group] ? "white" : Theme.colorText
                                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; font.bold: true
                                        Layout.fillWidth: true
                                    }
                                    // 工况计数徽章
                                    Rectangle {
                                        Layout.preferredWidth: countText.implicitWidth + 12
                                        Layout.preferredHeight: 18
                                        radius: 9
                                        color: page.expandedGroups[groupNode.modelData.group] ? Theme.colorTitleBar : Theme.colorHover
                                        border.color: Theme.colorBorder; border.width: 1
                                        Text {
                                            id: countText
                                            anchors.centerIn: parent
                                            text: {
                                                let n = 0;
                                                const subs = groupNode.modelData.subgroups;
                                                for (let i = 0; i < subs.length; ++i) n += subs[i].scenarios.length;
                                                return String(n);
                                            }
                                            color: page.expandedGroups[groupNode.modelData.group] ? Theme.colorTextDim : Theme.colorTextMuted
                                            font.family: Theme.fontNumeric; font.pixelSize: 10; font.bold: true
                                        }
                                    }
                                }
                            }

                            // ===== 二级：子组 + 三级工况（展开时显示）=====
                            Column {
                                width: parent.width
                                leftPadding: Theme.spacing          // 整体缩进
                                visible: page.expandedGroups[groupNode.modelData.group] === true
                                spacing: 2

                                Repeater {
                                    model: groupNode.modelData.subgroups

                                    Column {
                                        id: subNode
                                        required property var modelData
                                        width: parent.width - Theme.spacing
                                        spacing: 1

                                        // 子组标题（仅当 subgroup 非空时显示）
                                        Rectangle {
                                            visible: subNode.modelData.subgroup.length > 0
                                            width: parent.width
                                            height: 24
                                            color: "transparent"
                                            MouseArea {
                                                anchors.fill: parent
                                                acceptedButtons: Qt.RightButton
                                                onClicked: {
                                                    page.contextGroupId = groupNode.modelData.id
                                                    page.contextGroupName = groupNode.modelData.group
                                                    page.contextSubgroupId = subNode.modelData.id
                                                    contextMenu.subgroupMenu.popup()
                                                }
                                            }
                                            Text {
                                                anchors.left: parent.left; anchors.leftMargin: Theme.spacingSm
                                                anchors.verticalCenter: parent.verticalCenter
                                                text: "▣ " + subNode.modelData.subgroup
                                                color: Theme.colorTextDim
                                                font.family: Theme.fontFamily; font.pixelSize: Theme.fontTiny; font.bold: true
                                            }
                                        }

                                        // ===== 三级：工况项 =====
                                        Repeater {
                                            model: subNode.modelData.scenarioNodes

                                            Rectangle {
                                                id: scenarioItem
                                                required property var modelData
                                                width: parent.width
                                                height: 36
                                                radius: Theme.radius
                                                color: page.scenarioNames[page.selected] === scenarioItem.modelData.name ? Theme.colorAccent : Theme.colorPanelAlt
                                                border.color: page.scenarioNames[page.selected] === scenarioItem.modelData.name ? Theme.colorAccent : Theme.colorBorder
                                                border.width: 1

                                                MouseArea {
                                                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                                    onClicked: (mouse) => {
                                                        if (mouse.button === Qt.RightButton) {
                                                            page.contextGroupId = groupNode.modelData.id
                                                            page.contextGroupName = groupNode.modelData.group
                                                            page.contextSubgroupId = subNode.modelData.id
                                                            contextMenu.scenarioMenu.popup()
                                                            return
                                                        }
                                                        const idx = page.scenarioNames.indexOf(scenarioItem.modelData.name);
                                                        if (idx >= 0) { page.selected = idx; page.loadSelectedWaypoints(); }
                                                    }
                                                }

                                                RowLayout {
                                                    anchors.fill: parent
                                                    anchors.leftMargin: Theme.spacingLg; anchors.rightMargin: Theme.spacing
                                                    spacing: Theme.spacingSm

                                                    // 工况图标
                                                    Text {
                                                        text: "●"
                                                        color: page.scenarioNames[page.selected] === scenarioItem.modelData.name ? "white" : Theme.colorAccent
                                                        font.pixelSize: 8
                                                        Layout.preferredWidth: 10
                                                    }
                                                    // 工况名
                                                    Text {
                                                        text: scenarioItem.modelData.name
                                                        color: page.scenarioNames[page.selected] === scenarioItem.modelData.name ? "white" : Theme.colorText
                                                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; font.bold: true
                                                        Layout.fillWidth: true
                                                        elide: Text.ElideRight
                                                    }
                                                    // 航点数
                                                    Text {
                                                        text: page.ctrl.scenarioTrajectory(scenarioItem.modelData.name, "target").length + "段"
                                                        color: page.scenarioNames[page.selected] === scenarioItem.modelData.name ? "#cccccc" : Theme.colorTextMuted
                                                        font.family: Theme.fontNumeric; font.pixelSize: 10
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                }
            }
        }

        // 右：上下分区
        ColumnLayout {
            Layout.fillWidth: true; Layout.fillHeight: true
            spacing: Theme.spacing

            // 右上：路径编辑 + 地图预览
            PanelFrame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: parent.height * 0.55
                title: page.scenarioNames.length > 0 ? page.scenarioNames[page.selected] + " · 路径" + (page.currentReplayRunId.length > 0 ? "（回放中）" : "") : "路径"
                titleEn: page.currentReplayRunId.length > 0 ? "replay" : "waypoints"

                ColumnLayout {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    spacing: Theme.spacing

                    // 路径编辑目标切换：主车 / 平板
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Button {
                            id: tabEgo
                            text: "🚗 主车轨迹（%1 段）".arg(page.currentEgoSegments.length)
                            Layout.preferredHeight: 30
                            checkable: false
                            contentItem: Text {
                                text: tabEgo.text
                                color: page.editingTarget === "ego" ? "white" : Theme.colorTextDim
                                font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall
                                font.bold: page.editingTarget === "ego"
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: page.editingTarget === "ego" ? Theme.colorAccent : Theme.colorPanelAlt
                                radius: Theme.radius
                                border.color: page.editingTarget === "ego" ? Theme.colorAccent : Theme.colorBorder
                                border.width: 1
                            }
                            onClicked: page.editingTarget = "ego"
                        }
                        Button {
                            id: tabTarget
                            text: "📦 平板轨迹（%1 段）".arg(page.currentTargetSegments.length)
                            Layout.preferredHeight: 30
                            checkable: false
                            contentItem: Text {
                                text: tabTarget.text
                                color: page.editingTarget === "target" ? "white" : Theme.colorTextDim
                                font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall
                                font.bold: page.editingTarget === "target"
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: page.editingTarget === "target" ? Theme.colorAccent : Theme.colorPanelAlt
                                radius: Theme.radius
                                border.color: page.editingTarget === "target" ? Theme.colorAccent : Theme.colorBorder
                                border.width: 1
                            }
                            onClicked: page.editingTarget = "target"
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: "当前编辑：" + (page.editingTarget === "ego" ? "主车" : "平板")
                            color: Theme.colorTextMuted
                            font.family: Theme.fontFamily; font.pixelSize: Theme.fontTiny
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        spacing: Theme.spacing

                        TrajectorySegmentEditor {
                            Layout.fillWidth: true; Layout.fillHeight: true
                            segments: page.currentSegments
                            onPreviewRequested: (points) => page.onTrajectoryPreview(points)
                            onSaveRequested: (segments) => page.onTrajectorySaved(segments)
                        }

                        PathPreviewMap {
                            id: pathPreview
                            Layout.fillWidth: true; Layout.fillHeight: true
                            Layout.preferredWidth: parent.width * 0.5
                            mode: "path"
                            egoWaypoints: page.currentEgoWaypoints
                            targetWaypoints: page.currentTargetWaypoints
                            waypoints: page.currentWaypoints
                            activeTarget: page.editingTarget
                            samples: page.currentSamples
                            replayIndex: page.replayIndex
                        }
                    }

                    // 回放控件（仅回放模式显示）
                    RowLayout {
                        Layout.fillWidth: true
                        visible: page.currentReplayRunId.length > 0
                        spacing: Theme.spacingSm
                        Button {
                            id: btnPlay
                            text: page.replayPlaying ? "暂停" : "播放"
                            Layout.preferredHeight: 32
                            enabled: page.currentSamples.length > 0
                            contentItem: Text { text: btnPlay.text; color: Theme.colorNormal; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: btnPlay.down ? Qt.darker(Theme.colorNormal, 1.2) : Theme.colorHover; radius: Theme.radius; border.color: Theme.colorNormal; border.width: 1 }
                            onClicked: {
                                if (page.replayPlaying) { page.replayPlaying = false; replayTimer.stop(); }
                                else {
                                    if (page.replayIndex >= page.currentSamples.length - 1) page.replayIndex = 0;
                                    page.replayPlaying = true; replayTimer.start();
                                }
                            }
                        }
                        Text {
                            text: page.currentSamples.length > 0 ? "帧 %1 / %2".arg(page.replayIndex + 1).arg(page.currentSamples.length) : "无采样数据"
                            color: Theme.colorTextDim
                            font.family: Theme.fontNumeric; font.pixelSize: Theme.fontSmall
                        }
                        Item { Layout.fillWidth: true }
                        Button {
                            text: "退出回放"
                            Layout.preferredHeight: 32
                            contentItem: Text { text: "退出回放"; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: Theme.colorHover; radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
                            onClicked: {
                                page.currentReplayRunId = ""; page.currentSamples = [];
                                page.replayIndex = 0; page.replayPlaying = false;
                                replayTimer.stop(); pathPreview.mode = "path";
                            }
                        }
                    }
                }
            }

            // 右下：数据回放记录
            PanelFrame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: parent.height * 0.45
                title: "数据回放"
                titleEn: "replay"
                liveText: "共 %1 条".arg(page.ctrl.replayListModel.count)
                liveColor: Theme.colorInfo

                ColumnLayout {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    spacing: 1

                    // 表头
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 30
                        color: Theme.colorTitleBar
                        RowLayout {
                            anchors.fill: parent; anchors.leftMargin: Theme.spacing; anchors.rightMargin: Theme.spacing
                            spacing: 0
                            Text { text: "开始时间"; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; Layout.preferredWidth: 150 }
                            Text { text: "状态"; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; Layout.preferredWidth: 90 }
                            Text { text: "结果"; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; Layout.fillWidth: true }
                            Text { text: "操作员"; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; Layout.preferredWidth: 80 }
                        }
                    }

                    ListView {
                        id: recordList
                        Layout.fillWidth: true; Layout.fillHeight: true
                        clip: true
                        model: page.ctrl.replayListModel
                        spacing: 1
                        delegate: Rectangle {
                            id: recordRow
                            required property int index
                            required property string runId
                            required property string startedAt
                            required property string runStatus
                            required property string resultText
                            required property string operatorName

                            width: recordList.width; height: 36
                            color: page.currentReplayRunId === recordRow.runId ? Theme.colorAccent : (recordRow.index % 2 === 0 ? Theme.colorPanelAlt : Theme.colorBg)
                            border.color: Theme.colorBorder; border.width: page.currentReplayRunId === recordRow.runId ? 2 : 0
                            MouseArea {
                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onClicked: page.loadRunSamples(recordRow.runId)
                            }
                            RowLayout {
                                anchors.fill: parent; anchors.leftMargin: Theme.spacing; anchors.rightMargin: Theme.spacing
                                spacing: 0
                                Text { text: recordRow.startedAt; color: page.currentReplayRunId === recordRow.runId ? "white" : Theme.colorText; font.family: Theme.fontNumeric; font.pixelSize: Theme.fontSmall; Layout.preferredWidth: 150 }
                                Text {
                                    text: recordRow.runStatus
                                    color: page.currentReplayRunId === recordRow.runId ? "white" : (recordRow.runStatus === "EMERGENCY" || recordRow.runStatus === "FAULT" ? Theme.colorEmergency : (recordRow.runStatus === "RUNNING" ? Theme.colorWarning : Theme.colorNormal))
                                    font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; font.bold: true; Layout.preferredWidth: 90
                                }
                                Text { text: recordRow.resultText.length > 0 ? recordRow.resultText : "—"; color: page.currentReplayRunId === recordRow.runId ? "#cccccc" : Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; elide: Text.ElideRight; Layout.fillWidth: true }
                                Text { text: recordRow.operatorName; color: page.currentReplayRunId === recordRow.runId ? "#cccccc" : Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; Layout.preferredWidth: 80 }
                            }
                        }
                    }

                    Text {
                        visible: page.ctrl.replayListModel.count === 0
                        text: "该工况暂无测试记录"
                        color: Theme.colorTextDim
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall
                        Layout.alignment: Qt.AlignCenter
                    }
                }
            }
        }
    }

    Menu {
        id: nodeCreateMenu
        MenuItem { text: "新建场景组"; onTriggered: page.openCreateNode("group", "", "") }
        MenuItem { text: "新建场景子组"; onTriggered: page.openCreateNode("subgroup", "", "") }
        MenuItem { text: "新建场景"; onTriggered: page.openCreateNode("scenario", "", "") }
    }

    Menu {
        id: contextMenu
        property Menu groupMenu: groupMenuInst
        property Menu subgroupMenu: subgroupMenuInst
        property Menu scenarioMenu: scenarioMenuInst
        Menu {
            id: groupMenuInst
            MenuItem { text: "新建场景子组"; onTriggered: page.openCreateNode("subgroup", page.contextGroupId, "") }
            MenuItem { text: "新建场景"; onTriggered: page.openCreateNode("scenario", page.contextGroupId, "") }
            MenuItem { text: "展开/折叠"; onTriggered: page.toggleGroup(page.contextGroupName) }
        }
        Menu {
            id: subgroupMenuInst
            MenuItem { text: "新建场景"; onTriggered: page.openCreateNode("scenario", page.contextGroupId, page.contextSubgroupId) }
        }
        Menu {
            id: scenarioMenuInst
            MenuItem { text: "新建同级场景"; onTriggered: page.openCreateNode("scenario", page.contextGroupId, page.contextSubgroupId) }
        }
    }

    Popup {
        id: newScenarioPopup
        modal: true
        anchors.centerIn: Overlay.overlay
        width: 420; height: 330; padding: Theme.spacingLg
        property var subgroupOptions: page.allSubgroups(page.createGroupId)
        background: Rectangle { color: Theme.colorPanel; radius: Theme.radiusLg; border.color: Theme.colorBorder }

        ColumnLayout {
            anchors.fill: parent; spacing: Theme.spacing
            Text {
                text: page.createNodeKind === "group" ? "新建场景组" : (page.createNodeKind === "subgroup" ? "新建场景子组" : "新建场景")
                color: Theme.colorText; font.pixelSize: Theme.fontMedium; font.bold: true
            }
            Text { text: "名称 *"; color: Theme.colorTextDim; font.pixelSize: Theme.fontTiny }
            TextField {
                id: newScenarioName
                Layout.fillWidth: true; placeholderText: "输入名称"
                color: Theme.colorText
                background: Rectangle { color: Theme.colorHover; radius: Theme.radius; border.color: newScenarioName.activeFocus ? Theme.colorAccent : Theme.colorBorder }
            }
            Text { visible: parentGroupCombo.visible; text: "所属场景组"; color: Theme.colorTextDim; font.pixelSize: Theme.fontTiny }
            ComboBox {
                id: parentGroupCombo
                Layout.fillWidth: true
                visible: page.createNodeKind === "subgroup" && page.createGroupId.length === 0
                model: page.scenarioTree; textRole: "name"; valueRole: "id"
            }
            Text { visible: parentSubgroupCombo.visible; text: "所属场景子组"; color: Theme.colorTextDim; font.pixelSize: Theme.fontTiny }
            ComboBox {
                id: parentSubgroupCombo
                Layout.fillWidth: true
                visible: page.createNodeKind === "scenario" && page.createSubgroupId.length === 0
                model: newScenarioPopup.subgroupOptions; textRole: "name"; valueRole: "id"
            }
            Text { id: newScenarioError; text: ""; color: Theme.colorEmergency; font.pixelSize: Theme.fontSmall; visible: text.length > 0 }
            Item { Layout.fillHeight: true }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button { text: "取消"; onClicked: newScenarioPopup.close() }
                Button {
                    text: "创建"
                    onClicked: {
                        const name = newScenarioName.text.trim()
                        if (name.length === 0) { newScenarioError.text = "请填写名称"; return }
                        let parentId = ""
                        if (page.createNodeKind === "subgroup") parentId = page.createGroupId || parentGroupCombo.currentValue || ""
                        if (page.createNodeKind === "scenario") parentId = page.createSubgroupId || parentSubgroupCombo.currentValue || ""
                        if (page.createNodeKind !== "group" && parentId.length === 0) { newScenarioError.text = "请选择父级"; return }
                        const createdId = page.ctrl.createScenarioNode(page.createNodeKind, parentId, name)
                        if (createdId.length === 0) { newScenarioError.text = "创建失败：名称重复或父级无效"; return }
                        page.scenarioTree = page.ctrl.scenarioTree()
                        page.scenarioNames = page.ctrl.scenarioNames()
                        if (page.createGroupId.length > 0 && !page.expandedGroups[page.contextGroupName]) page.toggleGroup(page.contextGroupName)
                        newScenarioPopup.close()
                    }
                }
            }
        }
    }
}
