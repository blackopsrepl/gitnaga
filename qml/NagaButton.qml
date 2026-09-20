import QtQuick
import QtQuick.Controls

Button {
    id: control
    property bool active: false
    implicitHeight: 28
    leftPadding: 10
    rightPadding: 10

    contentItem: Label {
        text: control.text
        color: !control.enabled ? "#626a79" : control.active ? "#7cf0bd" : "#e8ebf2"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: 12
        font.weight: Font.Medium
    }
    background: Rectangle {
        color: !control.enabled ? "#151922"
             : control.active ? "#143024"
             : control.down ? "#30394a"
             : control.hovered ? "#272e3c"
             : "#1d222d"
        border.color: control.active ? "#2fbf87" : control.activeFocus ? "#34d399" : "#303746"
    }
}
