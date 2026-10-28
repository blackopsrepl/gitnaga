import QtQuick
import QtQuick.Layouts
import GitNaga

// The work in progress as git holds it: what is not staged yet above what
// is. A file edited after staging appears in both.
Item {
    id: wip
    required property var repository
    signal fileMenuRequested(string path, point scenePosition)
    signal fileChosen()

    function rowIn(side) {
        if (repository.wipSide !== side)
            return -1
        return side === "staged" ? repository.selectedFile - repository.unstagedFiles.count
                                 : repository.selectedFile
    }

    function fileAction(side, row, action) {
        if (action === "stage")
            repository.stageFile(row)
        else if (action === "unstage")
            repository.unstageFile(row)
        else
            ask(row)
    }

    function ask(row) {
        var model = repository.unstagedFiles
        var index = model.index(row, 0)
        // PathRole and UntrackedRole: Qt.UserRole + 2 and + 6.
        var path = model.data(index, 258)
        var untracked = model.data(index, 262)
        confirm.message = untracked
            ? qsTr("Delete the untracked file %1? It is not in git, so this cannot be undone.").arg(path)
            : qsTr("Discard the changes to %1? This cannot be undone.").arg(path)
        confirm.apply = function() { wip.repository.discardFile(row) }
        confirm.open()
    }

    function bulkAction(action) {
        if (action === "stage") {
            repository.stageAll()
        } else if (action === "unstage") {
            repository.unstageAll()
        } else {
            confirm.message = qsTr("Discard all unstaged changes? Untracked files are deleted. This cannot be undone.")
            confirm.apply = function() { wip.repository.discardAll() }
            confirm.open()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        WipFileSection {
            Layout.fillWidth: true
            Layout.fillHeight: visible
            repository: wip.repository
            files: wip.repository.unstagedFiles
            side: "unstaged"
            title: qsTr("Unstaged")
            currentRow: wip.rowIn("unstaged")
            onFileAction: (row, action) => wip.fileAction("unstaged", row, action)
            onBulkAction: (action) => wip.bulkAction(action)
            onFileMenuRequested: (path, position) => wip.fileMenuRequested(path, position)
            onFileChosen: wip.fileChosen()
        }
        WipFileSection {
            Layout.fillWidth: true
            Layout.fillHeight: visible
            repository: wip.repository
            files: wip.repository.stagedFiles
            side: "staged"
            title: qsTr("Staged")
            currentRow: wip.rowIn("staged")
            onFileAction: (row, action) => wip.fileAction("staged", row, action)
            onBulkAction: (action) => wip.bulkAction(action)
            onFileMenuRequested: (path, position) => wip.fileMenuRequested(path, position)
            onFileChosen: wip.fileChosen()
        }
        Item { Layout.fillHeight: true }
    }

    ConfirmDialog {
        id: confirm
        property var apply: null
        onConfirmed: {
            var callback = apply
            if (typeof callback === "function")
                callback()
        }
    }
}
