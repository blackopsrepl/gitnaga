import QtQuick
import QtQuick.Layouts
import GitNaga

// The actions a hunk header offers: stage or discard from the unstaged
// diff, unstage from the staged one.
Rectangle {
    id: actions
    property string side: ""
    signal requested(string action)

    // Painted in the header row's own colour (set by the row), so the
    // buttons sit on a solid strip instead of over the hunk text.
    implicitWidth: buttons.implicitWidth + 12
    implicitHeight: buttons.implicitHeight + 4
    color: "transparent"

    RowLayout {
        id: buttons
        anchors.centerIn: parent
        spacing: 4
        NagaButton {
            visible: actions.side === "unstaged"
            text: qsTr("Discard hunk")
            implicitHeight: 18
            leftPadding: 6
            rightPadding: 6
            onClicked: actions.requested("discard")
        }
        NagaButton {
            visible: actions.side === "unstaged"
            text: qsTr("Stage hunk")
            active: true
            implicitHeight: 18
            leftPadding: 6
            rightPadding: 6
            onClicked: actions.requested("stage")
        }
        NagaButton {
            visible: actions.side === "staged"
            text: qsTr("Unstage hunk")
            implicitHeight: 18
            leftPadding: 6
            rightPadding: 6
            onClicked: actions.requested("unstage")
        }
    }
}
