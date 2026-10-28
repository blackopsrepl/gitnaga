import QtQuick
import GitNaga

// One side-by-side row: the old line on the left, the new line on the right,
// a blank (filler) side where one side has no counterpart. Rows were paired
// in C++, so this only lays them out.
Item {
    id: row
    required property var diffView
    required property int hunk
    required property bool spanning
    required property string text
    required property int leftRow
    required property int leftKind
    required property var leftLine
    required property string leftContent
    required property var leftStyle
    required property int rightRow
    required property int rightKind
    required property var rightLine
    required property string rightContent
    required property var rightStyle

    readonly property real gutter: diffView.gutterWidth
    readonly property real pitch: diffView.textPalette.lineHeight
    readonly property real half: (width - 1) / 2

    width: ListView.view ? ListView.view.width : 0
    height: spanning ? 30
          : diffView.wrap ? Math.max(left.textHeight, right.textHeight, pitch)
          : pitch

    Loader {
        anchors.fill: parent
        active: row.spanning
        sourceComponent: DiffHunkBar {
            diffView: row.diffView
            hunk: row.hunk
            content: row.text
            indent: row.gutter + 8
        }
    }

    component Side: Item {
        id: side
        property int sourceRow: -1
        property int kind: -1
        property var number
        property string content
        property var lineStyle
        readonly property bool added: kind === 1
        readonly property bool removed: kind === 2
        readonly property bool blank: kind < 0
        readonly property bool selected: (added || removed) && row.diffView.isSelected(sourceRow)
        readonly property real textHeight: text.textHeight
        width: row.half
        height: row.height
        visible: !row.spanning

        Rectangle {
            anchors.fill: parent
            color: side.blank ? NagaTheme.diffFiller : side.added ? NagaTheme.diffAddedFill
                 : side.removed ? NagaTheme.diffRemovedFill : "transparent"
        }
        Rectangle {
            width: row.gutter
            height: parent.height
            visible: !side.blank
            color: side.added ? NagaTheme.diffAddedGutter : side.removed ? NagaTheme.diffRemovedGutter
                                                                       : NagaTheme.diffSurface
            Text {
                width: parent.width - 8
                height: parent.height
                text: side.number || ""
                color: side.added ? NagaTheme.diffAddedInk : side.removed ? NagaTheme.diffRemovedInk
                                                                        : NagaTheme.diffLineNumber
                opacity: side.added || side.removed ? 0.75 : 1
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignVCenter
                font.family: "monospace"
                font.pixelSize: 11
            }
            MouseArea {
                anchors.fill: parent
                enabled: row.diffView.selectable && (side.added || side.removed)
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: mouse => row.diffView.toggleLine(side.sourceRow, mouse.modifiers & Qt.ShiftModifier)
            }
        }
        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: row.gutter
            visible: side.selected
            color: NagaTheme.accentTint(0.16)
            Rectangle { width: 2; height: parent.height; color: NagaTheme.accent }
        }
        Text {
            x: row.gutter + 6
            height: row.pitch
            visible: side.added || side.removed
            text: side.added ? "+" : "−"
            color: side.added ? NagaTheme.diffAddedInk : NagaTheme.diffRemovedInk
            verticalAlignment: Text.AlignVCenter
            font.family: "monospace"
            font.pixelSize: 12
        }
        DiffLineText {
            id: text
            x: row.gutter + row.diffView.textPadding
            width: row.diffView.textWidth
            height: row.height
            visible: !side.blank
            textPalette: row.diffView.textPalette
            text: side.content
            lineStyle: side.lineStyle
            added: side.added
            wrap: row.diffView.wrap
            offset: row.diffView.offset
            colour: side.kind === 0 && !side.number ? NagaTheme.muted : "transparent"
        }
    }

    Side {
        id: left
        sourceRow: row.leftRow
        kind: row.leftKind
        number: row.leftLine
        content: row.leftContent
        lineStyle: row.leftStyle
    }
    Rectangle {
        x: row.half
        width: 1
        height: parent.height
        visible: !row.spanning
        color: NagaTheme.border
    }
    Side {
        id: right
        x: row.half + 1
        sourceRow: row.rightRow
        kind: row.rightKind
        number: row.rightLine
        content: row.rightContent
        lineStyle: row.rightStyle
    }
}
