import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GitNaga

// Picks the remote branch a local branch tracks. The remote branch of the
// same name, when there is one, is offered first.
Dialog {
    id: dlg
    required property var repository
    property string branch: ""
    property var choices: []

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    closePolicy: Popup.CloseOnEscape
    width: 400
    padding: 16
    title: qsTr("Set upstream of %1").arg(dlg.branch)
    standardButtons: Dialog.Ok | Dialog.Cancel

    function openFor(reference) {
        var names = []
        var refs = dlg.repository.references
        for (var i = 0; i < refs.length; ++i) {
            if (refs[i].kind === "remote" && !refs[i].internal)
                names.push(refs[i].name)
        }
        dlg.branch = reference.name
        dlg.choices = names
        var current = reference.upstream || ""
        var index = names.indexOf(current)
        for (var j = 0; index < 0 && j < names.length; ++j) {
            if (names[j].substring(names[j].indexOf("/") + 1) === reference.name)
                index = j
        }
        picker.currentIndex = Math.max(0, index)
        open()
    }

    onAccepted: if (picker.currentText.length > 0) dlg.repository.sync.setUpstream(dlg.branch, picker.currentText)

    contentItem: ColumnLayout {
        spacing: 8
        Label {
            Layout.fillWidth: true
            text: dlg.choices.length > 0
                  ? qsTr("Pull and push of %1 will use:").arg(dlg.branch)
                  : qsTr("No remote branches yet. Fetch, or push %1 to publish it.").arg(dlg.branch)
            color: NagaTheme.text
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
        ComboBox {
            id: picker
            Layout.fillWidth: true
            visible: dlg.choices.length > 0
            model: dlg.choices
        }
    }
}
