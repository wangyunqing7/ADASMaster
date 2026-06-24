// =============================================================================
// StateFlow.qml
// 功能：测试状态机流程条。6 步（空闲→检查中→就绪→预备→运行中→完成），
//       三态着色：已完成(绿勾)/当前(蓝)/未到(灰)；故障(6)橙、急停(7)红。
//       结构：6 个等宽列横排，圆圈水平居中、文字在下方，连线水平连接圆圈中线。
// 输入：currentState(int，TestState 枚举值)。
// 输出：只读流程可视化。
// =============================================================================
import QtQuick
import ADASMaster

Item {
    id: root
    property int currentState: 0   // 由调用方绑定 stateManager.testState

    readonly property var steps: [
        { idx: 0, name: "空闲" }, { idx: 1, name: "检查中" },
        { idx: 2, name: "就绪" }, { idx: 3, name: "预备" },
        { idx: 4, name: "运行中" }, { idx: 5, name: "完成" }
    ]

    readonly property color overrideColor: {
        if (currentState === 6) return Theme.colorError;
        if (currentState === 7) return Theme.colorEmergency;
        return Theme.colorAccent;
    }

    // 固定尺寸，避免 Row/子项循环依赖导致高度塌缩
    readonly property real cellWidth: 120
    readonly property real circleSize: 42
    readonly property real rowHeight: 76   // 圆圈42 + 间距 + 文字

    implicitHeight: rowHeight

    // 横排容器：固定高度，整体居中
    Row {
        id: flow
        anchors.centerIn: parent
        width: root.cellWidth * root.steps.length
        height: root.rowHeight
        spacing: 0

        Repeater {
            model: root.steps.length

            // 单个步骤列：固定宽高，内部用绝对坐标定位圆圈/文字/连线
            Item {
                id: stepCell
                required property int index
                readonly property var step: root.steps[stepCell.index]
                width: root.cellWidth
                height: root.rowHeight

                // 圆圈（水平居中，距顶 4px）
                Rectangle {
                    id: circle
                    x: (root.cellWidth - root.circleSize) / 2
                    y: 4
                    width: root.circleSize; height: root.circleSize; radius: root.circleSize / 2
                    color: {
                        if (root.currentState === 6) return Theme.colorError;
                        if (root.currentState === 7) return Theme.colorEmergency;
                        if (stepCell.step.idx < root.currentState) return Theme.colorBorder;
                        if (stepCell.step.idx === root.currentState) return Theme.colorAccent;
                        return Theme.colorPanelAlt;
                    }
                    border.width: stepCell.step.idx === root.currentState ? 3 : 2
                    border.color: {
                        if (root.currentState >= 6) return Qt.lighter(circle.color, 1.3);
                        if (stepCell.step.idx === root.currentState) return "#5a9fd4";
                        return Theme.colorBorder;
                    }

                    Text {
                        anchors.centerIn: parent
                        text: {
                            if (root.currentState >= 6) return "!";
                            if (stepCell.step.idx < root.currentState) return "✓";
                            return (stepCell.step.idx + 1);
                        }
                        color: {
                            if (root.currentState >= 6) return "white";
                            if (stepCell.step.idx < root.currentState) return Theme.colorNormal;
                            return stepCell.step.idx === root.currentState ? "white" : Theme.colorTextMuted;
                        }
                        font.family: Theme.fontNumeric
                        font.pixelSize: 16
                        font.bold: true
                    }
                }

                // 步骤名称（圆圈正下方，水平居中）
                Text {
                    x: (root.cellWidth - width) / 2
                    y: 4 + root.circleSize + Theme.spacingSm
                    width: Math.min(root.cellWidth - 8, paintedWidth)
                    text: stepCell.step.name
                    color: {
                        if (root.currentState >= 6) return root.overrideColor;
                        if (stepCell.step.idx === root.currentState) return Theme.colorText;
                        return Theme.colorTextDim;
                    }
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall
                    font.bold: stepCell.step.idx === root.currentState
                }

                // 连线：水平连接本列圆圈右边缘到下一列圆圈左边缘
                // 圆圈中心 y = 4 + circleSize/2，连线 y 取圆圈中线
                Rectangle {
                    visible: stepCell.index < root.steps.length - 1
                    x: (root.cellWidth + root.circleSize) / 2   // 本列圆圈右边缘
                    y: 4 + root.circleSize / 2 - 1              // 圆圈垂直中线（减1居中2px线）
                    width: root.cellWidth - root.circleSize     // 到下一列圆圈左边缘的距离
                    height: 2
                    color: Theme.colorBorder
                }
            }
        }
    }
}
