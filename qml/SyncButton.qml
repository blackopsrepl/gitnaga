import QtQuick
import QtQuick.Controls
import GitNaga

// A toolbar action with an optional menu arrow beside it. The tooltip lives
// on the whole control so a disabled action still says why it is disabled.
Item {
    id: control
    property string text: ""
    property string tooltip: ""
    property bool actionEnabled: true
    property bool hasMenu: true
    signal clicked()
    signal menuRequested(Item anchor)

    implicitWidth: row.implicitWidth
    implicitHeight: 26

    Row {
        id: row
        spacing: -1
        NagaButton {
            text: control.text
            enabled: control.actionEnabled
            implicitHeight: control.implicitHeight
            onClicked: control.clicked()
        }
        NagaButton {
            id: arrow
            visible: control.hasMenu
            text: "▾"
            implicitHeight: control.implicitHeight
            leftPadding: 6
            rightPadding: 6
            onClicked: control.menuRequested(arrow)
        }
    }

    // Set by the owner while this button's menu is open: the tooltip would
    // otherwise sit on top of the menu it describes.
    property bool menuOpen: false

    HoverHandler { id: hover }
    ToolTip.visible: hover.hovered && !control.menuOpen && control.tooltip.length > 0
    ToolTip.delay: 500
    ToolTip.text: control.tooltip
}
