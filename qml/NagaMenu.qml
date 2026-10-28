import QtQuick
import QtQuick.Controls

Menu {
    id: control
    padding: 4
    // Basic's Menu takes its width from the background's implicit width, and
    // this background has none, so without this every menu is a sliver.
    implicitWidth: {
        var widest = 200
        for (var i = 0; i < control.count; ++i) {
            var item = control.itemAt(i)
            if (item && item.visible)
                widest = Math.max(widest, item.implicitWidth)
        }
        return widest + leftPadding + rightPadding
    }

    background: Rectangle {
        color: "#12161f"
        border.color: "#2b3242"
    }
}
