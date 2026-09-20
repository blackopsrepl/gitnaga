import QtQuick
import QtQuick.Controls

Dialog {
    id: dlg
    property string message: ""
    signal confirmed()

    modal: true
    closePolicy: Popup.CloseOnEscape
    width: 400
    anchors.centerIn: parent
    padding: 16
    standardButtons: Dialog.Ok | Dialog.Cancel
    onAccepted: dlg.confirmed()

    contentItem: Label {
        text: dlg.message
        wrapMode: Text.WordWrap
        color: "#e8ebf2"
        width: dlg.width - 32
    }
}
