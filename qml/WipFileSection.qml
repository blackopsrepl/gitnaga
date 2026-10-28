import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GitNaga

// One side of the work in progress: a titled list with its bulk actions.
// An empty side takes no room at all.
ColumnLayout {
    id: section
    required property var repository
    required property var files
    property string side: "unstaged"
    property string title: ""
    // Row of the reviewed file within this list, or -1.
    property int currentRow: -1
    signal fileAction(int row, string action)
    signal bulkAction(string action)
    signal fileMenuRequested(string path, point scenePosition)
    signal fileChosen()

    visible: files.count > 0
    spacing: 0

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 30
        color: NagaTheme.raised
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 6
            spacing: 6
            Text {
                text: section.title
                color: NagaTheme.text
                font.pixelSize: 11
                font.weight: Font.DemiBold
            }
            Text {
                text: section.files.count
                color: NagaTheme.muted
                font.pixelSize: 11
            }
            Item { Layout.fillWidth: true }
            NagaButton {
                visible: section.side === "unstaged"
                text: qsTr("Discard all")
                implicitHeight: 22
                leftPadding: 6
                rightPadding: 6
                onClicked: section.bulkAction("discard")
            }
            NagaButton {
                text: section.side === "staged" ? qsTr("Unstage all") : qsTr("Stage all")
                active: section.side === "unstaged"
                implicitHeight: 22
                leftPadding: 6
                rightPadding: 6
                onClicked: section.bulkAction(section.side === "staged" ? "unstage" : "stage")
            }
        }
    }

    ListView {
        id: list
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: contentHeight
        Layout.maximumHeight: contentHeight
        Layout.minimumHeight: Math.min(contentHeight, 102)
        model: section.files
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        delegate: WipFileRow {
            width: list.width
            side: section.side
            current: index === section.currentRow
            onChosen: {
                section.repository.selectWorktreeFile(section.side, index)
                section.fileChosen()
            }
            onAction: (name) => section.fileAction(index, name)
            onMenuRequested: (position) => section.fileMenuRequested(path, position)
        }
    }
}
