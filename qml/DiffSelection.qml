import QtQuick

// Which change rows of the shown diff are picked for a line action. A press
// starts a selection, shift extends it from the anchor, control toggles one
// row, and a drag extends it as the pointer moves. Only '+' and '-' rows can
// be picked; context in a dragged range is skipped.
QtObject {
    id: selection
    required property var model
    property var rows: []
    property int anchor: -1
    readonly property int count: rows.length

    function kindAt(row) {
        // KindRole is the model's first role (Qt.UserRole + 1).
        return model.data(model.index(row, 0), 257)
    }
    function isChange(row) {
        var kind = kindAt(row)
        return kind === 1 || kind === 2
    }
    function contains(row) {
        return rows.indexOf(row) >= 0
    }
    function clear() {
        rows = []
        anchor = -1
    }
    function range(from, to) {
        var picked = []
        for (var row = Math.min(from, to); row <= Math.max(from, to); ++row)
            if (isChange(row))
                picked.push(row)
        return picked
    }
    function press(row, modifiers) {
        if (!isChange(row))
            return
        if ((modifiers & Qt.ShiftModifier) && anchor >= 0) {
            rows = range(anchor, row)
        } else if (modifiers & Qt.ControlModifier) {
            var next = rows.slice()
            var at = next.indexOf(row)
            if (at >= 0)
                next.splice(at, 1)
            else
                next.push(row)
            rows = next.sort(function(a, b) { return a - b })
            anchor = row
        } else {
            anchor = row
            rows = [row]
        }
    }
    function extend(row) {
        if (anchor >= 0 && row >= 0)
            rows = range(anchor, row)
    }
}
