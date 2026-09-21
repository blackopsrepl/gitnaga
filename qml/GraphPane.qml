// The CommitGraph item is a C++ type whose prototype the linter cannot resolve,
// so grouped property scopes on it (anchors) are reported as missing. That
// specific category is disabled for this file only.
// qmllint disable missing-property
import QtQuick
import QtQuick.Layouts
import GitNaga

Rectangle {
    id: pane
    required property var repository

    readonly property string headOid: {
        var branch = repository.currentBranch
        var count = repository.commits.count
        for (var i = 0; i < count; ++i) {
            var info = repository.commits.at(i)
            if (info.refs && info.refs.indexOf(branch) >= 0)
                return info.oid
        }
        return ""
    }

    function ensureVisible(row) {
        graph.ensureVisible(row)
    }
    function zoomIn() { graph.zoomIn() }
    function zoomOut() { graph.zoomOut() }
    function resetZoom() { graph.resetZoom() }

    color: NagaTheme.background

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: NagaTheme.panel
            border.color: NagaTheme.border
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 8
                spacing: 8
                Text {
                    text: qsTr("Commit graph")
                    color: NagaTheme.text
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.4
                }
                Text {
                    text: repository.commitCount + qsTr(" commits")
                    color: NagaTheme.muted
                    font.pixelSize: 11
                }
                Item { Layout.fillWidth: true }
                NagaIconButton {
                    text: "−"
                    tooltip: qsTr("Zoom out")
                    enabled: graph.zoom > graph.minimumZoom + 0.001
                    onClicked: graph.zoomOut()
                }
                Text {
                    text: Math.round(graph.zoom * 100) + "%"
                    color: NagaTheme.muted
                    font.pixelSize: 11
                    Layout.preferredWidth: 34
                    horizontalAlignment: Text.AlignHCenter
                }
                NagaIconButton {
                    text: "+"
                    tooltip: qsTr("Zoom in")
                    enabled: graph.zoom < graph.maximumZoom - 0.001
                    onClicked: graph.zoomIn()
                }
                NagaIconButton {
                    text: "⟲"
                    tooltip: qsTr("Reset zoom")
                    onClicked: graph.resetZoom()
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            CommitGraph {
                id: graph
                anchors.fill: parent
                model: repository.commits
                selectedRow: repository.selectedRow
                headOid: pane.headOid
                workInProgressOid: repository.workInProgressOid
                onCommitClicked: (row) => repository.selectCommit(row)
                onContextRequested: (row, oid, pos) => {
                    var local = pane.mapFromItem(null, pos)
                    var info = row >= 0 ? repository.commits.at(row) : null
                    if (row < 0 || (info && info.oid === repository.workInProgressOid))
                        commitMenu.showBlank(local)
                    else
                        commitMenu.show(row, oid, local)
                }
            }

            CommitLabels {
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                x: Math.min(parent.width - 150, graph.laneWidth + 16)
                width: parent.width - x
                repository: pane.repository
                graph: graph
            }

            GraphScrollBar {
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                graph: graph
            }

        }
    }

    Connections {
        target: pane.repository
        function onSelectionChanged() {
            if (graph.selectedRow >= 0)
                graph.ensureVisible(graph.selectedRow)
        }
    }

    CommitMenu {
        id: commitMenu
        repository: pane.repository
        branchPrompt: branchPrompt
        tagPrompt: tagPrompt
        confirm: confirm
    }
    PromptDialog {
        id: branchPrompt
        label: qsTr("New branch name")
        hint: qsTr("feature/my-branch")
        onSubmitted: (name) => repository.createBranch(name, branchPrompt.pendingOid)
    }
    PromptDialog {
        id: tagPrompt
        label: qsTr("New tag name")
        hint: qsTr("v1.0.0")
        onSubmitted: (name) => repository.createTag(name, tagPrompt.pendingOid)
    }
    ConfirmDialog {
        id: confirm
        property var onConfirm: null
        onConfirmed: {
            var callback = onConfirm
            if (typeof callback === "function")
                callback()
        }
        function ask(message, callback) {
            confirm.message = message
            confirm.onConfirm = callback
            confirm.open()
        }
    }
}
