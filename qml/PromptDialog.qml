import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dlg
    property string label: qsTr("Name")
    property string hint: ""
    property string pendingOid: ""
    signal submitted(string value)

    modal: true
    closePolicy: Popup.CloseOnEscape
    width: 360
    anchors.centerIn: parent
    padding: 16
    standardButtons: Dialog.Ok | Dialog.Cancel
    onAccepted: if (field.text.trim().length > 0) dlg.submitted(field.text.trim())

    function openWith(initial) {
        field.text = initial !== undefined ? initial : ""
        open()
        field.forceActiveFocus()
    }

    contentItem: ColumnLayout {
        spacing: 8
        Label {
            text: dlg.label
            color: "#e8ebf2"
            font.pixelSize: 12
            font.weight: Font.DemiBold
        }
        TextField {
            id: field
            Layout.fillWidth: true
            placeholderText: dlg.hint
            selectByMouse: true
            onAccepted: if (field.text.trim().length > 0) {
                dlg.submitted(field.text.trim())
                dlg.accept()
            }
        }
    }
}
