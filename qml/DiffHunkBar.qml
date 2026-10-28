import QtQuick
import GitNaga

// A hunk header across the diff: the @@ range, the enclosing function git
// names, and any actions the parent put on hunks. Also shows notices
// (`hunk` -1), such as the truncation of a very large diff.
Rectangle {
    id: bar
    required property var diffView
    required property int hunk
    required property string content
    // Where the text column starts, so the header lines up with the code.
    property real indent: 0

    readonly property var parts: content.match(/^(@@ [^@]* @@)\s?(.*)$/)
    color: hunk < 0 ? Qt.rgba(NagaTheme.diffNotice.r, NagaTheme.diffNotice.g, NagaTheme.diffNotice.b, 0.08)
                    : NagaTheme.diffHunkFill

    Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: NagaTheme.border }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: NagaTheme.border; opacity: 0.6 }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: Math.max(10, bar.indent)
        anchors.right: actions.left
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: 10
        clip: true
        Text {
            text: bar.hunk < 0 ? bar.content : bar.parts ? bar.parts[1] : bar.content
            color: bar.hunk < 0 ? NagaTheme.diffNotice : NagaTheme.diffHunkInk
            font.family: "monospace"
            font.pixelSize: 11
        }
        Text {
            visible: bar.parts !== null && bar.parts[2].length > 0
            text: bar.parts ? bar.parts[2] : ""
            color: NagaTheme.muted
            font.family: "monospace"
            font.pixelSize: 11
            elide: Text.ElideRight
        }
    }

    Row {
        id: actions
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: 6
        visible: bar.hunk >= 0
        Repeater {
            model: bar.hunk >= 0 ? bar.diffView.hunkActions : []
            delegate: NagaButton {
                required property var modelData
                implicitHeight: 22
                leftPadding: 8
                rightPadding: 8
                text: modelData.label
                onClicked: bar.diffView.hunkActionRequested(bar.hunk, modelData.action)
            }
        }
    }
}
