import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GitNaga

// Asks which remote to publish to when there are several and none is origin.
// `tag` is empty when a branch is being published.
Dialog {
    id: dlg
    required property var sync
    property string tag: ""
    property var remotes: []

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    closePolicy: Popup.CloseOnEscape
    width: 380
    padding: 16
    title: dlg.tag.length > 0 ? qsTr("Push tag %1 to…").arg(dlg.tag) : qsTr("Publish branch to…")
    standardButtons: Dialog.Ok | Dialog.Cancel

    function choose(tag, remotes) {
        dlg.tag = tag
        dlg.remotes = remotes
        picker.currentIndex = 0
        open()
    }

    onAccepted: {
        var remote = picker.currentText
        if (remote.length === 0)
            return
        if (dlg.tag.length > 0)
            dlg.sync.pushTag(dlg.tag, remote)
        else
            dlg.sync.pushTo(remote)
    }

    contentItem: ColumnLayout {
        spacing: 8
        Label {
            Layout.fillWidth: true
            text: qsTr("This repository has several remotes. Choose where it goes:")
            color: NagaTheme.text
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
        ComboBox {
            id: picker
            Layout.fillWidth: true
            model: dlg.remotes
        }
    }
}
