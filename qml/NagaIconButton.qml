import QtQuick
import QtQuick.Controls

Button {
    id: control
    property string tooltip: ""
    property string iconName: ""
    implicitWidth: 26
    implicitHeight: 24
    leftPadding: 0
    rightPadding: 0
    focusPolicy: Qt.NoFocus

    contentItem: Loader {
        sourceComponent: control.iconName.length > 0 ? iconItem : textItem
    }

    Component {
        id: textItem
        Text {
            text: control.text
            color: !control.enabled ? "#4c5566" : control.hovered ? "#f2f6ff" : "#c3cad8"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 13
            font.weight: Font.DemiBold
        }
    }
    Component {
        id: iconItem
        Item {
            NagaIcon {
                anchors.centerIn: parent
                name: control.iconName
                color: !control.enabled ? "#4c5566" : control.down ? "#f2f6ff" : control.hovered ? "#eef2fb" : "#b9c2d2"
            }
        }
    }
    background: Rectangle {
        color: control.down ? "#2b3446" : control.hovered ? "#212a38" : "transparent"
        border.color: control.activeFocus ? "#34d399" : "transparent"
    }
    ToolTip.visible: hovered && tooltip.length > 0
    ToolTip.text: tooltip
}
