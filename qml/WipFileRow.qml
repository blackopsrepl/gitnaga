import QtQuick
import QtQuick.Layouts
import GitNaga

// One file of a work-in-progress list. Its actions show while the pointer
// is on the row, and stay on the row being reviewed.
Item {
    id: row
    required property int index
    required property string status
    required property string fileName
    required property string directory
    required property string path
    required property bool untracked
    required property bool conflicted
    property string side: "unstaged"
    property bool current: false
    signal chosen()
    signal action(string name)
    signal menuRequested(point scenePosition)

    height: 34
    readonly property bool showActions: hover.hovered || row.current

    Rectangle {
        anchors.fill: parent
        color: row.current ? NagaTheme.accentTint(0.16) : hover.hovered ? NagaTheme.hoverFill : "transparent"
    }
    Rectangle {
        width: 2
        height: parent.height
        color: NagaTheme.accent
        visible: row.current
    }
    HoverHandler { id: hover }
    TapHandler {
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onTapped: (point, button) => {
            row.chosen()
            if (button === Qt.RightButton)
                row.menuRequested(point.scenePosition)
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 6
        spacing: 8
        Text {
            text: row.untracked ? "?" : row.status
            Layout.preferredWidth: 10
            horizontalAlignment: Text.AlignHCenter
            color: row.conflicted ? "#ff9b5c" : row.untracked || row.status === "A" ? "#4ade80"
                 : row.status === "D" ? "#ff7d8b" : row.status === "R" ? "#9fc2ff" : "#fbbf24"
            font.bold: true
            font.pixelSize: 12
        }
        ColumnLayout {
            spacing: 1
            Layout.fillWidth: true
            Text { text: row.fileName; color: NagaTheme.text; elide: Text.ElideMiddle; Layout.fillWidth: true; font.pixelSize: 12 }
            Text { visible: row.directory.length > 0; text: row.directory; color: NagaTheme.muted; elide: Text.ElideMiddle; Layout.fillWidth: true; font.pixelSize: 9 }
        }
        NagaButton {
            visible: row.showActions && row.side === "unstaged" && !row.conflicted
            text: row.untracked ? qsTr("Delete") : qsTr("Discard")
            implicitHeight: 22
            leftPadding: 6
            rightPadding: 6
            onClicked: row.action("discard")
        }
        NagaButton {
            visible: row.showActions
            text: row.side === "staged" ? qsTr("Unstage") : row.conflicted ? qsTr("Mark resolved") : qsTr("Stage")
            active: row.side === "unstaged"
            implicitHeight: 22
            leftPadding: 6
            rightPadding: 6
            onClicked: row.action(row.side === "staged" ? "unstage" : "stage")
        }
    }
}
