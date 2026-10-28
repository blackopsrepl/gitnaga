import QtQuick
import GitNaga

// The file diff. While a work-in-progress file is shown, staging sits on
// top of it: hunk headers carry their actions, change lines are picked in
// the gutter (shift extends), and the picked lines get a bar of line
// actions. Elsewhere it is the plain, read-only diff.
Item {
    id: view
    required property var repository
    // Which list the shown file came from ("unstaged"/"staged"), or empty
    // when the diff is not work in progress.
    property string side: ""
    // A conflicted file shows git's combined diff, and a whitespace-blind
    // diff does not match the file: neither can be staged in parts.
    readonly property bool staging: side.length > 0 && !repository.diffLines.combined
                                    && !repository.diffLines.ignoreWhitespace

    onSideChanged: diff.clearSelection()
    onStagingChanged: diff.clearSelection()

    function run(action, apply) {
        if (action === "discard") {
            confirm.apply = apply
            confirm.open()
        } else {
            apply(action)
        }
    }

    DiffView {
        id: diff
        anchors.fill: parent
        model: view.repository.diffLines
        selectable: view.staging
        hunkActions: !view.staging ? []
                   : view.side === "staged" ? [{ action: "unstage", label: qsTr("Unstage hunk") }]
                   : [{ action: "discard", label: qsTr("Discard hunk") }, { action: "stage", label: qsTr("Stage hunk") }]
        onHunkActionRequested: (hunk, action) => view.run(action, function(chosen) {
            view.repository.applyHunk(hunk, chosen)
        })
    }

    LineActionBar {
        visible: view.staging && diff.selectedLines.length > 0
        side: view.side
        count: diff.selectedLines.length
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        onCleared: diff.clearSelection()
        onRequested: (action) => view.run(action, function(chosen) {
            view.repository.applyLines(diff.selectedLines, chosen)
        })
    }

    ConfirmDialog {
        id: confirm
        property var apply: null
        message: qsTr("Discard the selected changes from the working tree? This cannot be undone.")
        onConfirmed: {
            var callback = apply
            if (typeof callback === "function")
                callback("discard")
        }
    }
}
