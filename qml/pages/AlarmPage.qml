// =============================================================================
// AlarmPage.qml
// 功能：报警中心页面。三段式：统计概览（6 指标）+ 筛选栏（等级/来源/未确认/搜索）
//       + 增强列表（来源 pill、确认/已恢复三态徽标）+ 空状态 + 清空（仅内存）/导出 CSV。
// 数据源：appController.alarmManager.alarmListModel（全量与统计）/ filterModel（筛选代理）
// 输入：用户筛选条件、确认/清空/导出操作。
// 输出：可视化报警管理与导出文件。
// =============================================================================
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import ADASMaster

// Bound 模式：delegate / 动态 Component 作为独立作用域，必须用 id 显式限定
// 访问外部对象与祖先属性，消除 qmllint 的 unqualified 警告（Qt6 现代 QML 写法）。
pragma ComponentBehavior: Bound

Item {
    id: root

    // 来源枚举(0..5) -> 中文，与后端 CSV 导出一致
    function sourceText(s) {
        const names = ["系统", "安全", "设备", "RTK", "工况", "通信"];
        return (s >= 0 && s < names.length) ? names[s] : "系统";
    }

    PanelFrame {
        id: frame
        anchors.fill: parent
        // anchors.margins: Theme.spacing
        anchors.bottomMargin: 0
        title: "报警中心"
        titleEn: "alarms"

        // 标题栏右侧实时指示：未确认数 / 全部正常
        // liveText/liveColor 是 PanelFrame 自身绑定，直接访问自身 property lm，无需 id 限定。
        liveText: lm.unackedCount > 0 ? "%1 条未确认".arg(lm.unackedCount) : "全部正常"
        liveColor: lm.unackedCount > 0 ? Theme.colorEmergency : Theme.colorNormal

        // 别名，简化访问。appController 由 main.cpp setContextProperty 注入，
        // 静态分析工具无法识别其类型，故 am 定义处行内抑制；lm/fm 经 am 派生，不触达 appController。
        readonly property var am: appController.alarmManager // qmllint disable unqualified
        readonly property var lm: frame.am.alarmListModel
        readonly property var fm: frame.am.filterModel

        // ---------- [1] 统计行：6 个 MetricCell（外层 1px 网格形成分隔线）----------
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: Theme.colorBorder
            GridLayout {
                anchors.fill: parent
                anchors.margins: 1
                columns: 6
                rowSpacing: 1
                columnSpacing: 1
                MetricCell { Layout.fillWidth: true; Layout.fillHeight: true; label: "总数";   value: frame.lm.count;            valueColor: Theme.colorText }
                MetricCell { Layout.fillWidth: true; Layout.fillHeight: true; label: "未确认"; value: frame.lm.unackedCount;     valueColor: frame.lm.unackedCount > 0 ? Theme.colorWarning : Theme.colorTextDim }
                MetricCell { Layout.fillWidth: true; Layout.fillHeight: true; label: "信息";   value: frame.lm.infoCount;        valueColor: Theme.colorInfo }
                MetricCell { Layout.fillWidth: true; Layout.fillHeight: true; label: "警告";   value: frame.lm.warnCount;        valueColor: Theme.colorWarning }
                MetricCell { Layout.fillWidth: true; Layout.fillHeight: true; label: "错误";   value: frame.lm.errorCount;       valueColor: Theme.colorError }
                MetricCell { Layout.fillWidth: true; Layout.fillHeight: true; label: "急停";   value: frame.lm.emergencyCount;   valueColor: Theme.colorEmergency }
            }
        }

        // ---------- [2] 筛选栏 ----------
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing

            // 等级筛选 Tab 组（单选：全部/信息/警告/错误/急停）
            RowLayout {
                spacing: 2
                Repeater {
                    model: [
                        { label: "全部", level: -1, c: Theme.colorTextDim },
                        { label: "信息", level: 0,  c: Theme.colorInfo },
                        { label: "警告", level: 1,  c: Theme.colorWarning },
                        { label: "错误", level: 2,  c: Theme.colorError },
                        { label: "急停", level: 3,  c: Theme.colorEmergency }
                    ]
                    delegate: Button {
                        id: lvlBtn
                        required property var modelData
                        readonly property bool active: frame.fm.activeLevel === lvlBtn.modelData.level
                        text: lvlBtn.modelData.label
                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 52
                        checkable: false
                        contentItem: Text {
                            text: lvlBtn.text
                            color: lvlBtn.active ? "white" : lvlBtn.modelData.c
                            font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall
                            font.bold: lvlBtn.active
                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: lvlBtn.active ? lvlBtn.modelData.c
                                  : (lvlBtn.down ? Theme.colorHover : Theme.colorPanelAlt)
                            radius: Theme.radius
                            border.color: lvlBtn.active ? lvlBtn.modelData.c : Theme.colorBorder
                            border.width: 1
                        }
                        onClicked: frame.fm.activeLevel = lvlBtn.modelData.level
                    }
                }
            }

            // 来源下拉
            ComboBox {
                id: sourceCombo
                Layout.preferredHeight: 30
                Layout.preferredWidth: 116
                model: ["全部来源", "系统", "安全", "设备", "RTK", "工况", "通信"]
                onActivated: function(index) { frame.fm.activeSource = index - 1 }
            }

            // 只看未确认（可切换）
            Button {
                id: unackedBtn
                checkable: true
                checked: frame.fm.onlyUnacked
                Layout.preferredHeight: 30
                Layout.preferredWidth: 104
                contentItem: Text {
                    text: "只看未确认"
                    color: unackedBtn.checked ? "white" : Theme.colorTextDim
                    font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall
                    font.bold: unackedBtn.checked
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: unackedBtn.checked ? Theme.colorWarning
                          : (unackedBtn.down ? Theme.colorHover : Theme.colorPanelAlt)
                    radius: Theme.radius
                    border.color: unackedBtn.checked ? Theme.colorWarning : Theme.colorBorder
                    border.width: 1
                }
                onClicked: frame.fm.onlyUnacked = unackedBtn.checked
            }

            // 搜索框
            TextField {
                id: searchField
                Layout.preferredHeight: 30
                Layout.preferredWidth: 190
                placeholderText: "🔍 搜索标题 / 内容"
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
                color: Theme.colorText
                onTextChanged: frame.fm.keyword = text
                background: Rectangle {
                    color: Theme.colorPanelAlt
                    radius: Theme.radius
                    border.color: searchField.activeFocus ? Theme.colorAccent : Theme.colorBorder
                    border.width: 1
                }
            }

            Item { Layout.fillWidth: true }

            // 显示 当前可见 / 总数
            Text {
                text: "显示 %1 / %2 条".arg(list.count).arg(frame.lm.count)
                color: Theme.colorTextMuted
                font.family: Theme.fontNumeric
                font.pixelSize: Theme.fontSmall
                Layout.preferredWidth: 150
            }

            // 操作按钮
            Button {
                id: ackAllBtn
                text: "全部确认"
                Layout.preferredHeight: 30
                contentItem: Text { text: ackAllBtn.text; color: Theme.colorText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: ackAllBtn.down ? Theme.colorAccentHover : Theme.colorHover; radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
                onClicked: frame.am.acknowledgeAll()
            }
            Button {
                id: clearBtn
                text: "清空"
                Layout.preferredHeight: 30
                contentItem: Text { text: clearBtn.text; color: Theme.colorText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: clearBtn.down ? Theme.colorAccentHover : Theme.colorHover; radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
                onClicked: clearConfirmDialog.open()
            }
            Button {
                id: exportBtn
                text: "导出 CSV"
                Layout.preferredHeight: 30
                contentItem: Text { text: exportBtn.text; color: Theme.colorText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: exportBtn.down ? Theme.colorAccentHover : Theme.colorAccent; radius: Theme.radius; border.color: Theme.colorAccent; border.width: 1 }
                onClicked: exportDialog.open()
            }
        }

        // ---------- [3] 列表 ----------
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ListView {
                id: list
                width: parent.width
                model: frame.fm
                spacing: 1

                delegate: Rectangle {
                    id: alarmRow
                    required property int index
                    required property int alarmLevel
                    required property int alarmSource
                    required property string alarmTitle
                    required property string alarmMessage
                    required property var alarmTime
                    required property bool alarmAcked
                    required property bool alarmRecovered

                    width: list.width
                    height: 52
                    color: alarmRow.index % 2 === 0 ? Theme.colorPanelAlt : Theme.colorBg

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.spacing; anchors.rightMargin: Theme.spacing
                        spacing: Theme.spacing

                        // 等级色条
                        Rectangle { Layout.preferredWidth: 4; Layout.preferredHeight: 28; color: Theme.levelColor(alarmRow.alarmLevel); radius: 2 }

                        Text {
                            text: alarmRow.alarmLevel === 3 ? "急停" : alarmRow.alarmLevel === 2 ? "错误" : alarmRow.alarmLevel === 1 ? "警告" : "信息"
                            color: Theme.levelColor(alarmRow.alarmLevel); font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; font.bold: true
                            Layout.preferredWidth: 44
                        }
                        Text {
                            text: alarmRow.alarmTitle
                            color: Theme.colorText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; font.bold: true
                            Layout.preferredWidth: 150
                            elide: Text.ElideRight
                        }
                        Text {
                            text: alarmRow.alarmMessage
                            color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        // 来源 pill
                        Rectangle {
                            Layout.preferredWidth: 50; Layout.preferredHeight: 20
                            color: Theme.colorPanelAlt; radius: 10
                            border.color: Theme.colorBorder; border.width: 1
                            Text { anchors.centerIn: parent; text: root.sourceText(alarmRow.alarmSource); color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontTiny }
                        }
                        Text {
                            text: {
                                const d = new Date(alarmRow.alarmTime);
                                return Qt.formatDateTime(d, "MM-dd HH:mm:ss");
                            }
                            color: Theme.colorTextMuted; font.family: Theme.fontNumeric; font.pixelSize: Theme.fontSmall
                            Layout.preferredWidth: 120
                        }
                        // 三态徽标：已恢复(绿) / 未确认(黄) / 已确认(不显示)
                        Rectangle {
                            visible: alarmRow.alarmRecovered
                            color: Theme.colorNormal; radius: 3
                            Layout.preferredWidth: 50; Layout.preferredHeight: 20
                            Text { anchors.centerIn: parent; text: "已恢复"; color: "black"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontTiny; font.bold: true }
                        }
                        Rectangle {
                            visible: !alarmRow.alarmAcked && !alarmRow.alarmRecovered
                            color: Theme.colorWarning; radius: 3
                            Layout.preferredWidth: 50; Layout.preferredHeight: 20
                            Text { anchors.centerIn: parent; text: "未确认"; color: "black"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontTiny; font.bold: true }
                        }
                        Button {
                            id: ackBtn
                            text: "确认"
                            visible: !alarmRow.alarmAcked
                            Layout.preferredHeight: 26
                            contentItem: Text { text: ackBtn.text; color: Theme.colorText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontTiny; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { color: ackBtn.down ? Theme.colorAccentHover : Theme.colorHover; radius: 3; border.color: Theme.colorBorder; border.width: 1 }
                            // 代理行号映射回源模型行号，筛选下也能确认正确的行
                            onClicked: frame.am.acknowledge(frame.fm.mapRowToSource(alarmRow.index))
                        }
                    }
                }

                // 空状态
                Text {
                    anchors.centerIn: parent
                    visible: list.count === 0
                    text: frame.lm.count === 0 ? "暂无报警记录" : "当前筛选下无匹配记录"
                    color: Theme.colorTextMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMedium
                }
            }
        }
    }

    // ---------- 清空确认对话框 ----------
    Dialog {
        id: clearConfirmDialog
        modal: true
        title: "确认清空"
        standardButtons: Dialog.Ok | Dialog.Cancel
        background: Rectangle { color: Theme.colorPanel; radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
        contentItem: Text {
            text: "将清空当前列表（%1 条）。\n数据库历史记录会保留。".arg(frame.lm.count)
            color: Theme.colorText
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal
            wrapMode: Text.WordWrap
        }
        onAccepted: frame.am.clear()
    }

    // ---------- 导出文件对话框 ----------
    FileDialog {
        id: exportDialog
        title: "导出报警记录为 CSV"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "csv"
        nameFilters: ["CSV 文件 (*.csv)"]
        onAccepted: frame.am.exportCsv(currentFile)
    }
}
