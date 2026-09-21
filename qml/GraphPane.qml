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
    required property var colors

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

    color: colors.backgroundColor

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: colors.panel
            border.color: colors.border
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 8
                spacing: 8
                Text {
                    text: qsTr("Commit graph")
                    color: colors.text
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.4
                }
                Text {
                    text: repository.commits.count + qsTr(" commits")
                    color: colors.muted
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
                    color: colors.muted
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
                onCommitClicked: (row) => repository.selectCommit(row)
                onContextRequested: (row, oid, pos) => {
                    var local = pane.mapFromItem(null, pos)
                    if (row < 0)
                        commitMenu.showBlank(local)
                    else
                        commitMenu.show(row, oid, local)
                }
            }

            ListView {
                id: labels
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                x: Math.min(parent.width - 150, graph.laneWidth + 16)
                width: parent.width - x
                model: repository.commits
                interactive: false
                enabled: false
                clip: true
                reuseItems: true
                boundsBehavior: Flickable.StopAtBounds
                contentY: graph.contentY

                delegate: Item {
                    id: rowItem
                    required property int index
                    required property string subject
                    required property string author
                    required property string shortOid
                    required property string relativeDate
                    required property var refs
                    width: labels.width
                    height: graph.effectiveRowHeight
                    readonly property bool current: index === repository.selectedRow
                    // Below ~26 px per row a two-line layout cannot fit without
                    // colliding with its neighbours, so collapse to one line.
                    readonly property bool compact: graph.effectiveRowHeight < 26

                    Text {
                        visible: rowItem.compact
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        text: rowItem.subject
                        color: rowItem.current ? "#ffffff" : "#b9c2d2"
                        elide: Text.ElideRight
                        font.pixelSize: 10
                        font.weight: rowItem.current ? Font.DemiBold : Font.Normal
                    }

                    Column {
                        visible: !rowItem.compact
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 3

                        Row {
                            width: parent.width
                            spacing: 8
                            Text {
                                width: Math.max(60, parent.width - chipRow.width - 8)
                                text: rowItem.subject
                                color: rowItem.current ? "#ffffff" : "#dbe1ec"
                                elide: Text.ElideRight
                                font.pixelSize: 11
                                font.weight: rowItem.current ? Font.DemiBold : Font.Normal
                            }
                            Row {
                                id: chipRow
                                spacing: 4
                                Repeater {
                                    model: rowItem.refs.slice(0, 3)
                                    delegate: Rectangle {
                                        required property string modelData
                                        height: 14
                                        width: chipText.implicitWidth + 12
                                        color: modelData.indexOf("# ") === 0 ? "#3a2f17"
                                             : modelData.indexOf("⇄ ") === 0 ? "#262d3d" : "#1c2140"
                                        border.color: modelData.indexOf("# ") === 0 ? "#7a5f1f"
                                                    : modelData.indexOf("⇄ ") === 0 ? "#3a4560" : "#5b4bb8"
                                        Text {
                                            id: chipText
                                            anchors.centerIn: parent
                                            text: parent.modelData
                                            color: "#e6e2ff"
                                            font.pixelSize: 9
                                        }
                                    }
                                }
                            }
                        }
                        Row {
                            spacing: 8
                            Text { text: rowItem.shortOid; color: "#a78bfa"; font.family: "monospace"; font.pixelSize: 9 }
                            Text { text: rowItem.author; color: "#78839a"; font.pixelSize: 9 }
                            Text { text: "·"; color: "#78839a" }
                            Text { text: rowItem.relativeDate; color: "#78839a"; font.pixelSize: 9 }
                        }
                    }
                }
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
