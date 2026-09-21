import QtQuick

// Slim overlay scrollbar for the commit graph. The thumb is a thin rounded
// pill that brightens and widens on hover, and disappears entirely when the
// content fits. Clicking the track jumps the thumb to that position; dragging
// it scrolls the graph through its own contentY property.
Item {
    id: bar

    required property var graph
    readonly property bool engaged: area.containsMouse || area.pressed
    readonly property real thumbHeight: Math.max(28, height * height / Math.max(1, graph.contentHeight))
    readonly property real maxTravel: height - thumbHeight
    readonly property real scrollable: graph.maxContentY

    visible: scrollable > 0.5
    width: 12
    Accessible.role: Accessible.ScrollBar
    Accessible.name: qsTr("Commit graph scrollbar")
    Accessible.description: qsTr("Click or drag to scroll the commit graph")

    Rectangle {
        id: thumb
        property real targetWidth: area.pressed ? 7 : area.containsMouse ? 6 : 4
        width: targetWidth
        radius: width / 2
        x: parent.width - width - 2
        height: bar.thumbHeight
        y: bar.scrollable > 0 ? (graph.contentY / bar.scrollable) * bar.maxTravel : 0
        color: area.pressed ? "#c9d1e0" : "#97a1b4"
        opacity: area.pressed ? 0.85 : area.containsMouse ? 0.55 : 0.28

        Behavior on width {
            NumberAnimation { duration: 90; easing.type: Easing.OutQuad }
        }
        Behavior on opacity {
            NumberAnimation { duration: 120; easing.type: Easing.OutQuad }
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton

        onPressed: (mouse) => jumpTo(mouse.y)
        onPositionChanged: (mouse) => {
            if (pressed)
                jumpTo(mouse.y)
        }
        function jumpTo(y) {
            const travel = Math.min(Math.max(y - bar.thumbHeight / 2, 0), bar.maxTravel)
            if (bar.maxTravel > 0)
                bar.graph.contentY = (travel / bar.maxTravel) * bar.scrollable
        }
    }
}
