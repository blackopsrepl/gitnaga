import QtQuick
import GitNaga

// Visual feedback for dragging a branch chip across the graph: the row under
// the pointer lights up and a ghost chip follows the pointer, saying where it
// would land. It takes no input itself; the chip that started the drag feeds
// it positions and asks it where the drop landed.
Item {
    id: layer
    required property var repository
    required property var graph
    required property var labels

    property bool active: false
    property var reference: null
    property color branchColor: NagaTheme.accent
    property point pointer: Qt.point(0, 0)
    property int targetRow: -1
    property var targetChip: null

    signal dropped(var reference, int row, var chip, point position)

    readonly property string targetLabel: {
        if (targetChip)
            return targetChip.name
        var info = targetRow >= 0 ? repository.commits.at(targetRow) : null
        if (!info)
            return ""
        // Name the row by its branch when it has one, as the drop menu does.
        var refs = info.refs || []
        for (var i = 0; i < refs.length; ++i) {
            if (refs[i].kind === "local" && refs[i].name !== reference.name)
                return refs[i].name
        }
        return info.shortOid
    }

    function rowAt(y) {
        var height = graph.effectiveRowHeight
        if (height <= 0)
            return -1
        var row = Math.floor((y + graph.contentY) / height)
        if (row < 0 || row >= repository.commits.count)
            return -1
        var info = repository.commits.at(row)
        return info.oid === repository.workInProgressOid ? -1 : row
    }

    function begin(reference, color) {
        layer.reference = reference
        layer.branchColor = color
        active = true
    }

    function update(position) {
        pointer = position
        targetRow = rowAt(position.y)
        var chip = targetRow >= 0 ? labels.chipAt(labels.mapFromItem(layer, position.x, position.y)) : null
        // Only another local branch is something to merge or rebase with.
        targetChip = chip && chip.kind === "local" && chip.name !== reference.name ? chip : null
    }

    function finish() {
        var row = targetRow
        var chip = targetChip
        var ref = reference
        active = false
        targetRow = -1
        targetChip = null
        if (row >= 0)
            dropped(ref, row, chip, pointer)
    }

    Rectangle {
        visible: layer.active && layer.targetRow >= 0
        x: 0
        width: layer.width
        y: layer.targetRow * layer.graph.effectiveRowHeight - layer.graph.contentY
        height: layer.graph.effectiveRowHeight
        color: NagaTheme.accentTint(0.10)
        border.color: NagaTheme.accentTint(0.75)
        border.width: 1
    }

    Rectangle {
        id: ghost
        visible: layer.active
        x: Math.min(layer.width - width - 4, layer.pointer.x + 14)
        y: Math.max(2, layer.pointer.y - height - 6)
        width: ghostColumn.implicitWidth + 16
        height: ghostColumn.implicitHeight + 8
        radius: 4
        // Opaque, so the row outline it crosses never shows through the text.
        color: NagaTheme.raised
        border.color: layer.branchColor
        border.width: 1.5
        Rectangle {
            anchors.fill: parent
            anchors.margins: 1.5
            radius: 3
            color: Qt.rgba(layer.branchColor.r, layer.branchColor.g, layer.branchColor.b, 0.25)
        }
        Column {
            id: ghostColumn
            anchors.centerIn: parent
            spacing: 1
            Text {
                text: layer.reference ? layer.reference.name : ""
                color: NagaTheme.hoverContent
                font.pixelSize: 11
                font.weight: Font.DemiBold
            }
            Text {
                visible: layer.targetRow >= 0
                text: qsTr("drop on ") + layer.targetLabel
                color: NagaTheme.accentContent
                font.pixelSize: 10
            }
        }
    }
}
