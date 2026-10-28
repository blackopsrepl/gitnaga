import QtQuick

// The graph's keyboard: Up/Down (and j/k) step through rows, PageUp/PageDown
// and Home/End jump, Left goes to the first parent and Right to the nearest
// child, Enter moves on to the review's files, Escape ends a search. Alt and
// Ctrl combinations are left to the window's actions.
QtObject {
    id: keys
    required property var repository
    required property var navigation
    required property var graph
    signal focusReviewRequested()

    function pageRows() {
        return Math.max(1, Math.floor(graph.height / graph.effectiveRowHeight) - 1)
    }

    function target(key, row) {
        const last = repository.commits.count - 1
        switch (key) {
        case Qt.Key_Up:
        case Qt.Key_K:
            return Math.max(0, row - 1)
        case Qt.Key_Down:
        case Qt.Key_J:
            return Math.min(last, row + 1)
        case Qt.Key_PageUp:
            return Math.max(0, row - pageRows())
        case Qt.Key_PageDown:
            return Math.min(last, Math.max(0, row) + pageRows())
        case Qt.Key_Home:
            return 0
        case Qt.Key_End:
            return last
        case Qt.Key_Left:
            return navigation.parentRow(row)
        case Qt.Key_Right:
            return navigation.childRow(row)
        }
        return -2
    }

    function handle(event) {
        if (event.modifiers & (Qt.AltModifier | Qt.ControlModifier | Qt.MetaModifier))
            return
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            event.accepted = true
            keys.focusReviewRequested()
            return
        }
        if (event.key === Qt.Key_Escape) {
            event.accepted = navigation.search.active
            navigation.search.clear()
            return
        }
        const row = target(event.key, repository.selectedRow)
        if (row === -2)
            return
        event.accepted = true
        // -1: no parent or child in the loaded graph; stay put.
        if (row >= 0 && row !== repository.selectedRow)
            repository.selectCommit(row)
    }
}
