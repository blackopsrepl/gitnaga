import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: empty
    required property string errorMessage
    signal openRequested()

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(460, parent.width - 48)
        spacing: 14

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: 72
            height: 72
            color: "#1c2535"
            border.color: "#344768"
            Label { anchors.centerIn: parent; text: "⌁"; color: "#78a9ff"; font.pixelSize: 40 }
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Open a repository")
            color: "#eef1f7"
            font.pixelSize: 26
            font.weight: Font.DemiBold
        }
        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("Inspect branches, merges, commits and diffs through a graph built from Git itself.")
            color: "#8d95a7"
            font.pixelSize: 14
        }
        NagaButton {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 8
            text: qsTr("Choose repository")
            onClicked: empty.openRequested()
        }
        Label {
            visible: empty.errorMessage.length > 0
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: empty.errorMessage
            color: "#ff9aaa"
        }
    }
}
