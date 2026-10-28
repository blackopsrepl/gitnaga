import QtQuick
import GitNaga

// One row of the unified diff: both line numbers, the +/− marker and the
// painted line. Clicking the gutter of an added or removed line selects it
// (shift extends), for a parent that acts on lines.
Item {
    id: row
    required property var diffView
    required property int index
    required property int kind
    required property var oldLine
    required property var newLine
    required property int hunk
    required property string content
    required property var lineStyle

    readonly property bool isHunk: kind === 4
    readonly property bool added: kind === 1
    readonly property bool removed: kind === 2
    readonly property bool isNote: kind === 0 && !oldLine && !newLine
    readonly property bool selected: (added || removed) && diffView.isSelected(index)
    readonly property real gutter: diffView.gutterWidth
    readonly property real pitch: diffView.textPalette.lineHeight

    width: ListView.view ? ListView.view.width : 0
    height: isHunk ? 30 : diffView.wrap ? Math.max(line.textHeight, pitch) : pitch

    Loader {
        anchors.fill: parent
        active: row.isHunk
        sourceComponent: DiffHunkBar {
            diffView: row.diffView
            hunk: row.hunk
            content: row.content
            indent: 2 * row.gutter + 8
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: !row.isHunk
        color: row.added ? NagaTheme.diffAddedFill : row.removed ? NagaTheme.diffRemovedFill : "transparent"
    }
    Rectangle {
        width: 2 * row.gutter
        height: parent.height
        visible: !row.isHunk
        color: row.added ? NagaTheme.diffAddedGutter : row.removed ? NagaTheme.diffRemovedGutter : NagaTheme.diffSurface
        Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: NagaTheme.border }
        Text {
            width: row.gutter - 6
            height: parent.height
            text: row.oldLine || ""
            color: row.removed ? NagaTheme.diffRemovedInk : NagaTheme.diffLineNumber
            opacity: row.removed ? 0.75 : 1
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
            font.family: "monospace"
            font.pixelSize: 11
        }
        Text {
            x: row.gutter
            width: row.gutter - 8
            height: parent.height
            text: row.newLine || ""
            color: row.added ? NagaTheme.diffAddedInk : NagaTheme.diffLineNumber
            opacity: row.added ? 0.75 : 1
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
            font.family: "monospace"
            font.pixelSize: 11
        }
        MouseArea {
            anchors.fill: parent
            enabled: row.diffView.selectable && (row.added || row.removed)
            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: mouse => row.diffView.toggleLine(row.index, mouse.modifiers & Qt.ShiftModifier)
        }
    }
    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 2 * row.gutter
        visible: row.selected
        color: NagaTheme.accentTint(0.16)
        Rectangle { width: 2; height: parent.height; color: NagaTheme.accent }
    }

    Text {
        x: 2 * row.gutter + 7
        height: row.pitch
        visible: row.added || row.removed
        text: row.added ? "+" : "−"
        color: row.added ? NagaTheme.diffAddedInk : NagaTheme.diffRemovedInk
        verticalAlignment: Text.AlignVCenter
        font.family: "monospace"
        font.pixelSize: 12
    }
    DiffLineText {
        id: line
        x: 2 * row.gutter + row.diffView.textPadding
        width: row.diffView.textWidth
        height: row.height
        visible: !row.isHunk
        textPalette: row.diffView.textPalette
        text: row.content
        lineStyle: row.lineStyle
        added: row.added
        wrap: row.diffView.wrap
        offset: row.diffView.offset
        colour: row.isNote ? NagaTheme.muted : "transparent"
    }
}
