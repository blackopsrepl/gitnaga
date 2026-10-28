import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GitNaga

// Edit one commit's message in place: the same rebase the editor runs, with
// every other commit picked as it is.
Dialog {
    id: dialog
    required property var repository
    property string oid: ""
    property bool loading: false
    property string error: ""

    parent: Overlay.overlay
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    width: 560
    modal: true
    padding: 16
    closePolicy: Popup.CloseOnEscape
    title: qsTr("Edit message of ") + oid.substring(0, 8)
    standardButtons: Dialog.Ok | Dialog.Cancel

    function openFor(target) {
        oid = target
        loading = true
        error = ""
        messageArea.text = ""
        open()
        repository.history.requestRebasePlan(target)
    }

    onAccepted: if (messageArea.text.trim().length > 0) repository.history.rewordCommit(oid, messageArea.text)

    Connections {
        target: dialog.repository.history
        function onRebasePlanReady(plan) {
            if (!dialog.loading || plan.oid !== dialog.oid)
                return
            dialog.loading = false
            if (!plan.ok) {
                dialog.error = plan.error
                return
            }
            messageArea.text = plan.steps[0].original.replace(/\s+$/, "")
            messageArea.forceActiveFocus()
        }
    }

    contentItem: ColumnLayout {
        spacing: 8
        Text {
            Layout.fillWidth: true
            text: dialog.error.length > 0 ? dialog.error
                : qsTr("The commits above it are replayed unchanged; nothing else is touched.")
            color: dialog.error.length > 0 ? NagaTheme.danger : NagaTheme.muted
            wrapMode: Text.WordWrap
            font.pixelSize: 11
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 180
            TextArea {
                id: messageArea
                enabled: !dialog.loading && dialog.error.length === 0
                placeholderText: dialog.loading ? qsTr("Reading the message…") : ""
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                font.pixelSize: 12
                color: NagaTheme.text
                background: Rectangle {
                    color: NagaTheme.background
                    border.color: messageArea.activeFocus ? NagaTheme.focusStroke : NagaTheme.border
                }
            }
        }
    }
}
