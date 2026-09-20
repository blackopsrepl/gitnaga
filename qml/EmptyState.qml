import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: empty
    required property string errorMessage
    signal openRequested()

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(420, parent.width - 48)
        spacing: 10

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("No repository open")
            font.pixelSize: 20
            font.weight: Font.DemiBold
        }
        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("Open a local Git working tree or bare repository.")
            color: palette.placeholderText
        }
        NagaButton {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 6
            text: qsTr("Open Repository…")
            onClicked: empty.openRequested()
        }
        Label {
            visible: empty.errorMessage.length > 0
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: empty.errorMessage
            color: "#d84f5f"
        }
    }
}
