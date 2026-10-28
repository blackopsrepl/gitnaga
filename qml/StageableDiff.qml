import QtQuick
import QtQuick.Controls
import GitNaga

// The file diff. While a work-in-progress file is shown, staging sits on
// top of it: hunk headers carry their actions, change lines are picked by
// click, shift-click, control-click or drag, and the picked lines get a bar
// of line actions. Elsewhere it is a plain, read-only diff.
Item {
    id: view
    required property var repository
    // Which list the shown file came from ("unstaged"/"staged"), or empty
    // when the diff is not work in progress.
    property string side: ""
    // A conflicted file shows git's combined diff, which cannot be split.
    property bool combined: false
    readonly property bool staging: side.length > 0 && !combined

    DiffSelection {
        id: selection
        model: view.repository.diffLines
    }
    Connections {
        target: view.repository.diffLines
        function onModelReset() {
            selection.clear()
            // TextRole is Qt.UserRole + 2.
            var model = view.repository.diffLines
            var first = model.rowCount() > 0 ? model.data(model.index(0, 0), 258) : ""
            view.combined = first.startsWith("diff --cc")
        }
    }
    onSideChanged: selection.clear()

    function run(action, apply) {
        if (action === "discard") {
            confirm.apply = apply
            confirm.open()
        } else {
            apply(action)
        }
    }

    ListView {
        id: diff
        anchors.fill: parent
        model: view.repository.diffLines
        clip: true
        reuseItems: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        ScrollBar.horizontal: ScrollBar {}
        delegate: Rectangle {
            id: diffRow
            required property int index
            required property int kind
            required property string text
            required property var oldLine
            required property var newLine
            readonly property bool picked: view.staging && selection.contains(index)
            readonly property bool hunk: kind === 4 && text.startsWith("@@ -")
            width: Math.max(diff.width, lineRow.implicitWidth + 24)
            height: hunk && view.staging ? 26 : 22
            color: picked ? (kind === 1 ? "#1f5a3d" : "#6a2a36")
                 : kind === 1 ? "#12291f" : kind === 2 ? "#331a20" : kind === 4 ? "#182b3f" : "transparent"
            Rectangle {
                visible: diffRow.picked
                width: 3
                height: parent.height
                color: NagaTheme.accent
            }
            Row {
                id: lineRow
                height: parent.height
                Text { width: 40; height: parent.height; verticalAlignment: Text.AlignVCenter; text: diffRow.oldLine || ""; color: diffRow.picked ? "#c3cad8" : "#5b6577"; horizontalAlignment: Text.AlignRight; rightPadding: 8; font.family: "monospace"; font.pixelSize: 11 }
                Text { width: 40; height: parent.height; verticalAlignment: Text.AlignVCenter; text: diffRow.newLine || ""; color: diffRow.picked ? "#c3cad8" : "#5b6577"; horizontalAlignment: Text.AlignRight; rightPadding: 8; font.family: "monospace"; font.pixelSize: 11 }
                Rectangle { width: 1; height: parent.height; color: NagaTheme.border }
                Text { height: parent.height; verticalAlignment: Text.AlignVCenter; leftPadding: 10; text: diffRow.text; color: diffRow.kind === 1 ? "#8fe6b8" : diffRow.kind === 2 ? "#ffadb6" : diffRow.kind === 4 ? "#9fc2ff" : diffRow.kind === 3 ? "#77839a" : "#cbd0dc"; font.family: "monospace"; font.pixelSize: 12 }
            }
            MouseArea {
                anchors.fill: parent
                enabled: view.staging && (diffRow.kind === 1 || diffRow.kind === 2)
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                preventStealing: true
                onPressed: (mouse) => selection.press(diffRow.index, mouse.modifiers)
                onPositionChanged: (mouse) => {
                    var point = mapToItem(diff.contentItem, mouse.x, mouse.y)
                    selection.extend(diff.indexAt(diff.contentX + 1, point.y))
                }
            }
            // Pinned to the visible right edge, however far the line scrolls.
            HunkActions {
                visible: view.staging && diffRow.hunk
                side: view.side
                color: diffRow.color
                x: diff.contentX + diff.width - implicitWidth - 10
                anchors.verticalCenter: parent.verticalCenter
                onRequested: (action) => view.run(action, function(chosen) {
                    view.repository.applyHunk(diffRow.index, chosen)
                })
            }
        }
    }

    LineActionBar {
        visible: view.staging && selection.count > 0
        side: view.side
        count: selection.count
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 16
        onCleared: selection.clear()
        onRequested: (action) => view.run(action, function(chosen) {
            view.repository.applyLines(selection.rows, chosen)
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
