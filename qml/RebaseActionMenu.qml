import QtQuick
import QtQuick.Controls
import GitNaga

// The action chooser a rebase row's chip opens: every action with its key.
Popup {
    id: menu
    required property var editor
    property int row: -1

    width: 190
    padding: 4
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        color: "#12161f"
        border.color: "#2b3242"
    }

    contentItem: Column {
        Repeater {
            model: menu.editor.actions
            delegate: Rectangle {
                id: entry
                required property string modelData
                width: menu.availableWidth
                height: 26
                color: entryHover.hovered ? NagaTheme.hoverFill : "transparent"
                Rectangle {
                    x: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: 8
                    height: 8
                    radius: 2
                    color: menu.editor.actionColor(entry.modelData)
                }
                Text {
                    x: 24
                    anchors.verticalCenter: parent.verticalCenter
                    text: entry.modelData
                    color: entryHover.hovered ? NagaTheme.hoverContent : NagaTheme.text
                    font.pixelSize: 12
                }
                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: entry.modelData.charAt(0)
                    color: NagaTheme.muted
                    font.pixelSize: 11
                    font.family: "monospace"
                }
                HoverHandler { id: entryHover }
                TapHandler {
                    onTapped: {
                        menu.editor.setAction(menu.row, entry.modelData)
                        menu.close()
                        menu.editor.focusRow(menu.row)
                    }
                }
            }
        }
    }
}
