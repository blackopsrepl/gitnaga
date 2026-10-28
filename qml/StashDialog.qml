import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Asks how to stash the work in progress: an optional message and whether
// untracked files go along.
Dialog {
    id: dlg
    signal submitted(string message, bool includeUntracked)

    title: qsTr("Stash changes")
    modal: true
    closePolicy: Popup.CloseOnEscape
    width: 380
    anchors.centerIn: parent
    padding: 16
    standardButtons: Dialog.Ok | Dialog.Cancel
    onAccepted: dlg.submitted(field.text.trim(), untracked.checked)

    function openFresh() {
        field.text = ""
        open()
        field.forceActiveFocus()
    }

    contentItem: ColumnLayout {
        spacing: 10
        TextField {
            id: field
            Layout.fillWidth: true
            placeholderText: qsTr("Message (optional)")
            selectByMouse: true
            onAccepted: dlg.accept()
        }
        CheckBox {
            id: untracked
            text: qsTr("Include untracked files")
            checked: true
        }
    }
}
