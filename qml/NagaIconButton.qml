import QtQuick
import QtQuick.Controls
import GitNaga

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
            color: !control.enabled ? "#4c5566" : control.hovered ? NagaTheme.hoverContent : "#c3cad8"
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
                color: !control.enabled ? "#4c5566" : control.hovered ? NagaTheme.hoverContent : "#b9c2d2"
            }
        }
    }
    background: Rectangle {
        color: control.down ? NagaTheme.pressedFill : control.hovered ? NagaTheme.hoverFill : "transparent"
        border.color: "transparent"
    }
    ToolTip.visible: hovered && tooltip.length > 0
    ToolTip.text: tooltip
}
