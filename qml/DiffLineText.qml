// The DiffText item is a C++ type whose QQuickPaintedItem prototype the
// linter cannot resolve, so its Item properties read as missing. This
// wrapper is the one place that touches it; rows lay out the wrapper.
// qmllint disable missing-property
import QtQuick
import GitNaga

Item {
    id: wrapper
    property var textPalette
    property string text
    property var lineStyle
    property bool added: false
    property bool wrap: false
    property real offset: 0
    property color colour: "transparent"
    readonly property real textHeight: painted.implicitHeight

    DiffText {
        id: painted
        width: wrapper.width
        height: wrapper.wrap ? implicitHeight : wrapper.height
        textPalette: wrapper.textPalette
        text: wrapper.text
        lineStyle: wrapper.lineStyle
        added: wrapper.added
        wrap: wrapper.wrap
        offset: wrapper.offset
        colour: wrapper.colour
    }
}
