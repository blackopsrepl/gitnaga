import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GitNaga

// The whole window of `gitnaga --askpass`: one prompt from git or ssh, one
// answer. Secrets (passwords, passphrases) are masked; questions such as a
// username or an unknown host key are typed in the clear.
ApplicationWindow {
    id: window
    required property string prompt
    property string answer: ""
    property bool accepted: false
    readonly property bool secret: /assword|passphrase|PIN/i.test(prompt)

    width: 460
    height: column.implicitHeight + 32
    minimumWidth: 360
    visible: true
    flags: Qt.Dialog
    title: qsTr("GitNaga · Authentication")
    color: NagaTheme.raised
    palette.window: NagaTheme.raised
    palette.windowText: NagaTheme.text
    palette.base: NagaTheme.panel
    palette.text: NagaTheme.text
    palette.button: NagaTheme.raised
    palette.buttonText: NagaTheme.text
    palette.highlight: NagaTheme.accent
    palette.placeholderText: NagaTheme.muted

    function finish(ok) {
        window.answer = ok ? field.text : ""
        window.accepted = ok
        window.visible = false
    }

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        Label {
            text: window.secret ? qsTr("Git needs a secret") : qsTr("Git has a question")
            color: NagaTheme.text
            font.pixelSize: 13
            font.weight: Font.DemiBold
        }
        Label {
            Layout.fillWidth: true
            text: window.prompt.trim()
            color: NagaTheme.muted
            font.pixelSize: 12
            wrapMode: Text.WrapAnywhere
        }
        TextField {
            id: field
            Layout.fillWidth: true
            focus: true
            echoMode: window.secret ? TextInput.Password : TextInput.Normal
            selectByMouse: true
            font.pixelSize: 12
            onAccepted: window.finish(true)
            Keys.onEscapePressed: window.finish(false)
            Component.onCompleted: forceActiveFocus()
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Item { Layout.fillWidth: true }
            NagaButton {
                text: qsTr("Cancel")
                keepFocus: true
                onClicked: window.finish(false)
            }
            NagaButton {
                text: qsTr("OK")
                active: true
                keepFocus: true
                onClicked: window.finish(true)
            }
        }
    }
}
