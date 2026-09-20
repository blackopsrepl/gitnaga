import QtQuick
import QtQuick.Controls

Button {
    id: control
    implicitHeight: 32
    leftPadding: 14
    rightPadding: 14

    contentItem: Label {
        text: control.text
        color: control.enabled ? "#e8ebf2" : "#626a79"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: 12
        font.weight: Font.Medium
    }
    background: Rectangle {
        color: !control.enabled ? "#151922"
             : control.down ? "#30394a"
             : control.hovered ? "#272e3c"
             : "#1d222d"
        border.color: control.activeFocus ? "#78a9ff" : "#303746"
    }
}
