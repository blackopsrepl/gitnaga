import QtQuick
import QtQuick.Controls
import GitNaga

// The footer's undo affordance: names the operation it would take back, or
// says plainly that the last one cannot be taken back.
Item {
    id: holder
    required property var repository
    property alias action: button.action

    visible: repository.undoStack.label.length > 0
    implicitWidth: visible ? button.implicitWidth : 0
    implicitHeight: 20
    HoverHandler { id: hover }
    ToolTip.visible: hover.hovered
    ToolTip.text: repository.undoStack.hint
    ToolTip.delay: 300

    NagaButton {
        id: button
        anchors.fill: parent
        implicitHeight: 20
        leftPadding: 8
        rightPadding: 8
    }
}
