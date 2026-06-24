// =============================================================================
// WaypointEditor.qml
// 功能：航点表格编辑器。四列（序号/X/Y/速度），行尾删除按钮，
//       底部"添加航点"与"保存"按钮。编辑中维护本地副本，保存时发 pathChanged。
// 输入：waypoints（QVariantList of {x,y,speed}）。
// 输出：pathChanged(var newPath) 信号。
// =============================================================================
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import ADASMaster

pragma ComponentBehavior: Bound

Item {
    id: root
    property var waypoints: []        // QVariantList of {x,y,speed}
    signal pathChanged(var newPath)

    // 本地可编辑副本
    property var editing: []

    function syncFromInput() {
        const copy = [];
        for (let i = 0; i < root.waypoints.length; ++i) {
            copy.push({ x: root.waypoints[i].x, y: root.waypoints[i].y, speed: root.waypoints[i].speed });
        }
        root.editing = copy;
    }
    Component.onCompleted: syncFromInput()
    onWaypointsChanged: syncFromInput()

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacingSm

        // 表头
        RowLayout {
            Layout.fillWidth: true
            spacing: 1
            Rectangle { Layout.preferredWidth: 28; Layout.preferredHeight: 24; color: Theme.colorTitleBar; Text { anchors.centerIn: parent; text: "#"; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall } }
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 24; color: Theme.colorTitleBar; Text { anchors.centerIn: parent; text: "X (m)"; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall } }
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 24; color: Theme.colorTitleBar; Text { anchors.centerIn: parent; text: "Y (m)"; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall } }
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 24; color: Theme.colorTitleBar; Text { anchors.centerIn: parent; text: "速度 (m/s)"; color: Theme.colorTextDim; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall } }
            Rectangle { Layout.preferredWidth: 36; Layout.preferredHeight: 24; color: Theme.colorTitleBar }
        }

        // 行列表
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.editing.length
            spacing: 1
            delegate: RowLayout {
                id: row
                required property int index
                spacing: 1

                Rectangle {
                    Layout.preferredWidth: 28; Layout.fillHeight: true
                    color: Theme.colorPanelAlt
                    Text { anchors.centerIn: parent; text: String(row.index + 1); color: Theme.colorTextDim; font.family: Theme.fontNumeric; font.pixelSize: Theme.fontSmall }
                }
                TextField {
                    Layout.fillWidth: true
                    text: root.editing[row.index].x.toFixed(1)
                    font.family: Theme.fontNumeric; font.pixelSize: Theme.fontSmall; color: Theme.colorText
                    background: Rectangle { color: Theme.colorPanelAlt; border.color: Theme.colorBorder; border.width: 1 }
                    onTextEdited: { root.editing[row.index].x = parseFloat(text) || 0; }
                }
                TextField {
                    Layout.fillWidth: true
                    text: root.editing[row.index].y.toFixed(1)
                    font.family: Theme.fontNumeric; font.pixelSize: Theme.fontSmall; color: Theme.colorText
                    background: Rectangle { color: Theme.colorPanelAlt; border.color: Theme.colorBorder; border.width: 1 }
                    onTextEdited: { root.editing[row.index].y = parseFloat(text) || 0; }
                }
                TextField {
                    Layout.fillWidth: true
                    text: root.editing[row.index].speed.toFixed(1)
                    font.family: Theme.fontNumeric; font.pixelSize: Theme.fontSmall; color: Theme.colorText
                    background: Rectangle { color: Theme.colorPanelAlt; border.color: Theme.colorBorder; border.width: 1 }
                    onTextEdited: { root.editing[row.index].speed = parseFloat(text) || 0; }
                }
                Button {
                    id: btnDelete
                    Layout.preferredWidth: 36
                    text: "×"
                    contentItem: Text { text: "×"; color: Theme.colorEmergency; font.bold: true; font.pixelSize: Theme.fontMedium; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: btnDelete.down ? Theme.colorHover : Theme.colorPanelAlt }
                    onClicked: {
                        const copy = root.editing.slice();
                        copy.splice(row.index, 1);
                        root.editing = copy;
                    }
                }
            }
        }

        Text {
            visible: root.editing.length === 0
            text: "暂无航点，点击下方添加"
            color: Theme.colorTextDim
            font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall
            Layout.alignment: Qt.AlignCenter
        }

        // 底部按钮
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSm
            Button {
                id: btnAdd
                text: "+ 添加航点"
                Layout.preferredHeight: 32
                contentItem: Text { text: "+ 添加航点"; color: Theme.colorText; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: btnAdd.down ? Theme.colorAccentHover : Theme.colorHover; radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
                onClicked: {
                    const copy = root.editing.slice();
                    const last = copy.length > 0 ? copy[copy.length - 1] : { x: 0, y: 0, speed: 0 };
                    copy.push({ x: last.x, y: last.y, speed: last.speed });
                    root.editing = copy;
                }
            }
            Item { Layout.fillWidth: true }
            Button {
                id: btnSave
                text: "保存"
                Layout.preferredHeight: 32
                contentItem: Text { text: "保存"; color: Theme.colorNormal; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSmall; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: btnSave.down ? Qt.darker(Theme.colorNormal, 1.2) : Theme.colorHover; radius: Theme.radius; border.color: Theme.colorNormal; border.width: 2 }
                onClicked: root.pathChanged(root.editing)
            }
        }
    }
}
