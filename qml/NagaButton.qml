import QtQuick
import QtQuick.Controls
import GitNaga

Button {
    id: control
    property bool active: false
    // Chrome buttons opt out of focus so a click cannot leave a focus ring
    // behind; dialog buttons keep it so the keyboard can still reach them.
    property bool keepFocus: false
    implicitHeight: 28
    leftPadding: 10
    rightPadding: 10
    focusPolicy: keepFocus ? Qt.StrongFocus : Qt.NoFocus

    contentItem: Label {
        text: control.text.replace(/&/g, "")
        color: !control.enabled ? "#626a79"
             : control.active ? (control.hovered ? NagaTheme.accentContentHover : NagaTheme.accentContent)
             : control.hovered ? NagaTheme.hoverContent
             : NagaTheme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: 12
        font.weight: Font.Medium
    }
    background: Rectangle {
        // Hover is layered on top of the active state rather than replacing it,
        // so a selected pane toggle still answers the pointer.
        color: !control.enabled ? "#151922"
             : control.active ? NagaTheme.accentTint(control.hovered ? 0.28 : 0.16)
             : control.down ? NagaTheme.pressedFill
             : control.hovered ? NagaTheme.hoverFill
             : "#1d222d"
        border.color: control.active ? NagaTheme.accentTint(0.75)
                    : control.activeFocus ? NagaTheme.focusStroke
                    : "#303746"
    }
}
