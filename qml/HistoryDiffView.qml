import QtQuick
import QtQuick.Controls
import GitNaga

// A read-only unified diff: the same colours and gutters as the review pane,
// for panels that show a diff of their own.
ListView {
    id: diff
    clip: true
    reuseItems: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar {}
    ScrollBar.horizontal: ScrollBar {}
    // The rows grow wider than the view; the list scrolls sideways with them.
    contentWidth: Math.max(width, widest)
    flickableDirection: Flickable.AutoFlickIfNeeded
    property real widest: width
    onCountChanged: if (count === 0) widest = width

    delegate: Rectangle {
        id: diffRow
        required property int kind
        required property string text
        required property var oldLine
        required property var newLine
        width: Math.max(diff.width, lineRow.implicitWidth + 24)
        height: 20
        color: kind === 1 ? "#12291f" : kind === 2 ? "#331a20" : kind === 4 ? "#182b3f" : "transparent"
        Component.onCompleted: diff.widest = Math.max(diff.widest, width)
        Row {
            id: lineRow
            height: parent.height
            Text { width: 40; height: parent.height; verticalAlignment: Text.AlignVCenter; text: diffRow.oldLine || ""; color: "#5b6577"; horizontalAlignment: Text.AlignRight; rightPadding: 8; font.family: "monospace"; font.pixelSize: 11 }
            Text { width: 40; height: parent.height; verticalAlignment: Text.AlignVCenter; text: diffRow.newLine || ""; color: "#5b6577"; horizontalAlignment: Text.AlignRight; rightPadding: 8; font.family: "monospace"; font.pixelSize: 11 }
            Rectangle { width: 1; height: parent.height; color: NagaTheme.border }
            Text {
                height: parent.height
                verticalAlignment: Text.AlignVCenter
                leftPadding: 10
                text: diffRow.text
                color: diffRow.kind === 1 ? "#8fe6b8" : diffRow.kind === 2 ? "#ffadb6" : diffRow.kind === 4 ? "#9fc2ff"
                     : diffRow.kind === 3 ? "#77839a" : "#cbd0dc"
                font.family: "monospace"
                font.pixelSize: 12
            }
        }
    }
}
