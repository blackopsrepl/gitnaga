import QtQuick
import QtQuick.Controls
import GitNaga

// One commit in the rebase editor: a drag handle, what happens to it, and the
// message it will carry when that message is being rewritten.
Rectangle {
    id: row
    required property int index
    required property string oid
    required property string shortOid
    required property string subject
    required property string original
    required property string action
    required property string message
    required property bool edited
    required property var editor

    readonly property int revision: editor.revision
    readonly property bool folded: action === "squash" || action === "fixup"
    readonly property bool tail: revision >= 0 && editor.isGroupTail(index)
    readonly property bool rewording: action === "reword"
    readonly property bool showsMessage: rewording || tail
    readonly property color actionColor: editor.actionColor(action)

    width: ListView.view ? ListView.view.width : 600
    height: 40 + (showsMessage ? 112 : 0)
    color: row.activeFocus ? NagaTheme.accentTint(0.10) : rowHover.hovered ? NagaTheme.hoverFill : "transparent"
    activeFocusOnTab: true

    HoverHandler { id: rowHover }
    TapHandler { onTapped: row.forceActiveFocus() }
    Keys.onPressed: (event) => editor.handleKey(row, row.index, event)

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: NagaTheme.border
    }

    // The handle: three bars, and the only place a drag starts, so a click
    // anywhere else still just focuses the row.
    Item {
        id: handle
        x: 8
        y: 0
        width: 22
        height: 40
        Column {
            anchors.centerIn: parent
            spacing: 3
            Repeater {
                model: 3
                Rectangle {
                    width: 12
                    height: 2
                    radius: 1
                    color: handleArea.pressed || handleArea.containsMouse ? NagaTheme.hoverContent : NagaTheme.muted
                }
            }
        }
        MouseArea {
            id: handleArea
            anchors.fill: parent
            hoverEnabled: true
            preventStealing: true
            cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
            onPressed: row.forceActiveFocus()
            onPositionChanged: (mouse) => {
                if (pressed)
                    row.editor.dragRowTo(row.index, handleArea.mapToItem(row.ListView.view.contentItem, mouse.x, mouse.y).y)
            }
        }
    }

    // Folded commits sit indented under the commit they fold into.
    Rectangle {
        visible: row.folded
        x: 40
        y: 0
        width: 2
        height: 20
        color: row.actionColor
        opacity: 0.6
    }
    Rectangle {
        visible: row.folded
        x: 40
        y: 19
        width: 10
        height: 2
        color: row.actionColor
        opacity: 0.6
    }

    Rectangle {
        id: actionChip
        x: row.folded ? 56 : 40
        y: 9
        width: 78
        height: 22
        radius: 3
        color: Qt.rgba(row.actionColor.r, row.actionColor.g, row.actionColor.b, chipHover.hovered ? 0.30 : 0.16)
        border.color: Qt.rgba(row.actionColor.r, row.actionColor.g, row.actionColor.b, 0.8)
        Text {
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: row.action
            color: chipHover.hovered ? NagaTheme.hoverContent : row.actionColor
            font.pixelSize: 11
            font.weight: Font.DemiBold
        }
        Text {
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            text: "▾"
            color: row.actionColor
            font.pixelSize: 10
        }
        HoverHandler { id: chipHover; cursorShape: Qt.PointingHandCursor }
        TapHandler {
            onTapped: {
                row.forceActiveFocus()
                row.editor.openActionMenu(row.index, actionChip)
            }
        }
    }

    Text {
        id: oidText
        anchors.left: actionChip.right
        anchors.leftMargin: 12
        y: 12
        text: row.shortOid
        color: NagaTheme.violet
        font.family: "monospace"
        font.pixelSize: 11
    }
    Text {
        anchors.left: oidText.right
        anchors.leftMargin: 12
        anchors.right: effectText.left
        anchors.rightMargin: 12
        y: 11
        text: row.subject
        elide: Text.ElideRight
        color: row.action === "drop" ? NagaTheme.muted : NagaTheme.text
        font.strikeout: row.action === "drop"
        font.pixelSize: 12
    }
    Text {
        id: effectText
        anchors.right: parent.right
        anchors.rightMargin: 14
        y: 12
        text: row.editor.effectText(row.action)
        color: NagaTheme.muted
        font.pixelSize: 11
    }

    Column {
        visible: row.showsMessage
        x: actionChip.x
        y: 40
        width: row.width - x - 14
        spacing: 4
        Text {
            text: row.rewording ? qsTr("New message") : qsTr("Combined message")
            color: NagaTheme.muted
            font.pixelSize: 10
            font.weight: Font.DemiBold
            font.letterSpacing: 0.6
        }
        ScrollView {
            width: parent.width
            height: 86
            TextArea {
                id: messageArea
                text: row.revision >= 0 && row.showsMessage ? row.editor.messageFor(row.index) : ""
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                font.pixelSize: 12
                color: NagaTheme.text
                background: Rectangle {
                    color: NagaTheme.background
                    border.color: messageArea.activeFocus ? NagaTheme.focusStroke : NagaTheme.border
                }
                onTextChanged: if (activeFocus) row.editor.setMessage(row.index, text)
            }
        }
    }
}
