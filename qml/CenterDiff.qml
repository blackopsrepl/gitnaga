import QtQuick
import QtQuick.Layouts
import GitNaga

// A file's diff where the graph was, the way it reads best: the full width
// of the window's middle, while the review pane keeps the message and the
// file list beside it. Back (or Escape) returns to the graph.
Rectangle {
    id: center
    required property var repository
    property string side: ""
    signal closeRequested()

    color: NagaTheme.panel

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: NagaTheme.panel
            border.color: NagaTheme.border
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 10
                spacing: 10
                NagaButton {
                    text: qsTr("← Graph")
                    implicitHeight: 24
                    onClicked: center.closeRequested()
                }
                Text {
                    Layout.fillWidth: true
                    text: center.repository.selectedOid === center.repository.workInProgressOid
                          ? qsTr("Uncommitted changes") : (center.repository.selectedCommit.subject || "")
                    elide: Text.ElideRight
                    color: NagaTheme.muted
                    font.pixelSize: 12
                }
                Text {
                    text: qsTr("Esc")
                    color: NagaTheme.muted
                    font.pixelSize: 11
                    font.family: "monospace"
                }
            }
        }

        StageableDiff {
            Layout.fillWidth: true
            Layout.fillHeight: true
            repository: center.repository
            side: center.side
        }
    }
}
