import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

MenuItem {
    id: control
    implicitHeight: 28
    implicitWidth: Math.max(240, labelText.implicitWidth + shortcutLabel.implicitWidth + 72)

    // The default indicator collides with the label; draw the state instead.
    indicator: null

    // Show the action's shortcut on the right, or nothing when it has none.
    readonly property string shortcutText: {
        if (!control.action || !control.action.shortcut)
            return ""
        return control.action.shortcut.toString()
    }

    contentItem: RowLayout {
        spacing: 8

        Text {
            Layout.leftMargin: 4
            Layout.preferredWidth: 14
            text: control.checkable && control.checked ? "✓" : ""
            color: "#34d399"
            font.pixelSize: 12
        }
        Text {
            id: labelText
            text: control.text.replace(/&/g, "")
            color: control.enabled ? "#dde3ee" : "#5b6478"
            font.pixelSize: 12
        }
        Item { Layout.fillWidth: true }
        Text {
            id: shortcutLabel
            Layout.rightMargin: 4
            text: control.shortcutText
            color: "#7c8698"
            font.pixelSize: 12
        }
    }

    background: Rectangle {
        color: control.highlighted ? "#1e2634" : "transparent"
    }
}
