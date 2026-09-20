import QtQuick
import QtQuick.Controls

Button {
    id: control
    property string tooltip: ""
    implicitWidth: 26
    implicitHeight: 24
    leftPadding: 0
    rightPadding: 0
    focusPolicy: Qt.NoFocus

    contentItem: Text {
        text: control.text
        color: !control.enabled ? "#4c5566" : control.hovered ? "#f2f6ff" : "#c3cad8"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: 13
        font.weight: Font.DemiBold
    }
    background: Rectangle {
        color: control.down ? "#2b3446" : control.hovered ? "#212a38" : "transparent"
        border.color: control.activeFocus ? "#34d399" : "transparent"
    }
    ToolTip.visible: hovered && tooltip.length > 0
    ToolTip.text: tooltip
}
