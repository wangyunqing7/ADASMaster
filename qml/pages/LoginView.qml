// =============================================================================
// LoginView.qml
// 功能：启动准入登录界面。居中卡片：用户名 + 密码，校验通过后进入主界面。
// 输入：appController.authManager（login/logout/isLoggedIn/loginError/...）。
// 输出：认证成功后 authManager.isLoggedIn=true，由 Main.qml 切换到主界面。
// =============================================================================
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import ADASMaster

Item {
    id: root

    readonly property var auth: appController.authManager // qmllint disable unqualified

    // 全屏深色背景
    Rectangle {
        anchors.fill: parent
        color: Theme.colorBg
    }

    // 居中登录卡片
    Rectangle {
        id: card
        anchors.centerIn: parent
        width: 600
        height: 500
        color: Theme.colorPanel
        radius: Theme.radiusLg
        border.color: Theme.colorBorder
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.spacingLg
            spacing: Theme.spacing

            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: "ADAS 场地测试主控系统"
                color: Theme.colorText
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontLarge
                font.bold: true
            }
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: "用户登录"
                color: Theme.colorTextDim
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
            }
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.colorDivider }

            // 用户名
            TextField {
                id: userField
                Layout.fillWidth: true
                placeholderText: "用户名"
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontNormal
                color: Theme.colorText
                background: Rectangle { color: Theme.colorHover; radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
                onAccepted: passField.forceActiveFocus()
            }
            // 密码
            TextField {
                id: passField
                Layout.fillWidth: true
                placeholderText: "密码"
                echoMode: TextInput.Password
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontNormal
                color: Theme.colorText
                background: Rectangle { color: Theme.colorHover; radius: Theme.radius; border.color: Theme.colorBorder; border.width: 1 }
                onAccepted: loginButton.clicked()
            }

            // 错误提示（仅失败时显示）
            Text {
                Layout.fillWidth: true
                visible: root.auth.loginError.length > 0
                text: "⚠ " + root.auth.loginError
                color: Theme.colorEmergency
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSmall
            }

            Button {
                id: loginButton
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                text: "登 录"
                contentItem: Text { text: loginButton.text; color: "white"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontNormal; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { color: loginButton.down ? Theme.colorAccentHover : Theme.colorAccent; radius: Theme.radius }
                onClicked: root.auth.login(userField.text, passField.text)
            }

            Item { Layout.fillHeight: true }

            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: "初始账号：admin / admin123"
                color: Theme.colorTextMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontTiny
            }
        }
    }
}
