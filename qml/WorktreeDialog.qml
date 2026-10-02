import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GitNaga
Dialog {
    id: dialog
    required property var repository
    modal: true
    focus: true
    width: Math.min(940, parent ? parent.width - 48 : 900)
    height: Math.min(650, parent ? parent.height - 48 : 600)
    padding: 0
    anchors.centerIn: parent
    closePolicy: Popup.CloseOnEscape
    background: Rectangle {
        color: NagaTheme.panel
        border.color: NagaTheme.border
    }
    function openManager() {
        forceRemoval.checked = false
        lockReason.text = ""
        repository.refresh()
        open()
    }
    function askRemove(entry) {
        confirmation.action = "remove"
        confirmation.targetPath = entry.path
        confirmation.message = forceRemoval.checked
            ? qsTr("Force-remove %1 and discard its uncommitted changes?").arg(entry.path)
            : qsTr("Remove %1? Git will refuse if it contains uncommitted changes.").arg(entry.path)
        confirmation.open()
    }
    function askPrune() {
        confirmation.action = "prune"
        confirmation.targetPath = ""
        confirmation.message = qsTr("Prune stale worktree metadata. Existing worktree files are not deleted.")
        confirmation.open()
    }
    function askRepair() {
        confirmation.action = "repair"
        confirmation.targetPath = ""
        confirmation.message = qsTr("Repair Git's administrative links for registered worktrees?")
        confirmation.open()
    }
    contentItem: ColumnLayout {
        spacing: 0
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            color: NagaTheme.raised
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 8
                Label {
                    text: qsTr("Worktrees")
                    color: NagaTheme.text
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }
                Label {
                    text: dialog.repository.worktrees.length
                    color: NagaTheme.muted
                    font.pixelSize: 11
                }
                Item { Layout.fillWidth: true }
                NagaButton {
                    text: qsTr("Add")
                    keepFocus: true
                    active: true
                    enabled: !dialog.repository.busy
                    onClicked: addDialog.openForRepository()
                }
                NagaButton {
                    text: qsTr("Prune")
                    keepFocus: true
                    enabled: !dialog.repository.busy
                    onClicked: dialog.askPrune()
                }
                NagaButton {
                    text: qsTr("Repair")
                    keepFocus: true
                    enabled: !dialog.repository.busy
                    onClicked: dialog.askRepair()
                }
                NagaIconButton {
                    text: "✕"
                    tooltip: qsTr("Close worktrees")
                    onClicked: dialog.close()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.topMargin: 8
            Layout.bottomMargin: 6
            Label {
                text: qsTr("Lock reason")
                color: NagaTheme.muted
                font.pixelSize: 11
            }
            TextField {
                id: lockReason
                Layout.fillWidth: true
                placeholderText: qsTr("Optional; prevents automatic pruning")
                selectByMouse: true
                font.pixelSize: 11
            }
            CheckBox {
                id: forceRemoval
                text: qsTr("Force removal")
                palette.text: NagaTheme.text
                font.pixelSize: 11
                enabled: !dialog.repository.busy
            }
        }

        ListView {
            id: worktreeList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: dialog.repository.worktrees
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                id: row
                required property var modelData
                width: worktreeList.width
                height: 84
                color: modelData.isCurrent ? NagaTheme.accentTint(0.12)
                                           : rowHover.hovered ? NagaTheme.hoverFill : "transparent"
                border.color: modelData.isCurrent ? NagaTheme.accentTint(0.35) : "transparent"
                HoverHandler { id: rowHover }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    anchors.topMargin: 6
                    anchors.bottomMargin: 5
                    spacing: 2
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: row.modelData.path
                            color: row.modelData.isCurrent ? NagaTheme.accent : NagaTheme.text
                            font.family: "monospace"
                            font.pixelSize: 11
                            elide: Text.ElideMiddle
                        }
                        Label {
                            text: row.modelData.isCurrent ? qsTr("OPEN")
                                  : row.modelData.isMain ? qsTr("MAIN")
                                  : row.modelData.isBare ? qsTr("BARE") : ""
                            color: NagaTheme.accent
                            font.pixelSize: 9
                            font.weight: Font.Bold
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: row.modelData.isBare ? qsTr("Bare repository")
                                  : row.modelData.isDetached ? qsTr("Detached at ") + row.modelData.head.substring(0, 8)
                                  : qsTr("Branch: ") + row.modelData.branch
                            color: NagaTheme.muted
                            font.pixelSize: 10
                            elide: Text.ElideMiddle
                        }
                        Label {
                            visible: row.modelData.isLocked || row.modelData.isPrunable
                            text: row.modelData.isLocked ? qsTr("LOCKED") : qsTr("PRUNABLE")
                            color: row.modelData.isLocked ? "#e6b45a" : "#ff7d8b"
                            font.pixelSize: 9
                            font.weight: Font.Bold
                        }
                        NagaButton {
                            text: row.modelData.isCurrent ? qsTr("Current") : qsTr("Open")
                            keepFocus: true
                            enabled: !dialog.repository.busy && !row.modelData.isCurrent && !row.modelData.isPrunable
                            onClicked: {
                                dialog.repository.openWorktree(row.modelData.path)
                                dialog.close()
                            }
                        }
                        NagaButton {
                            visible: !row.modelData.isMain && !row.modelData.isBare && !row.modelData.isPrunable
                            text: row.modelData.isLocked ? qsTr("Unlock") : qsTr("Lock")
                            keepFocus: true
                            enabled: !dialog.repository.busy
                            onClicked: row.modelData.isLocked
                                ? dialog.repository.unlockWorktree(row.modelData.path)
                                : dialog.repository.lockWorktree(row.modelData.path, lockReason.text.trim())
                        }
                        NagaButton {
                            visible: !row.modelData.isMain && !row.modelData.isBare && !row.modelData.isCurrent && !row.modelData.isPrunable
                            text: qsTr("Move")
                            keepFocus: true
                            enabled: !dialog.repository.busy
                            onClicked: moveDialog.openFor(row.modelData.path)
                        }
                        NagaButton {
                            visible: !row.modelData.isMain && !row.modelData.isBare && !row.modelData.isCurrent && !row.modelData.isPrunable
                            text: qsTr("Remove")
                            keepFocus: true
                            enabled: !dialog.repository.busy
                            onClicked: dialog.askRemove(row.modelData)
                        }
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: worktreeList.count === 0
                text: qsTr("No worktrees found")
                color: NagaTheme.muted
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: NagaTheme.raised
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Removal is safe by default; force removal discards local changes.")
                    color: NagaTheme.muted
                    font.pixelSize: 10
                }
                NagaButton {
                    text: qsTr("Done")
                    keepFocus: true
                    onClicked: dialog.close()
                }
            }
        }
    }

    AddWorktreeDialog {
        id: addDialog
        repository: dialog.repository
    }

    Dialog {
        id: moveDialog
        property string sourcePath: ""
        modal: true
        focus: true
        width: 500
        anchors.centerIn: parent
        closePolicy: Popup.CloseOnEscape
        standardButtons: Dialog.Ok | Dialog.Cancel
        title: qsTr("Move Worktree")
        onAccepted: if (destination.text.trim().length > 0)
            dialog.repository.moveWorktree(sourcePath, destination.text.trim())

        function openFor(path) {
            sourcePath = path
            destination.text = path + "-moved"
            open()
            destination.forceActiveFocus()
        }
        contentItem: ColumnLayout {
            Label { text: moveDialog.sourcePath; color: NagaTheme.muted; elide: Text.ElideMiddle }
            TextField {
                id: destination
                Layout.fillWidth: true
                selectByMouse: true
                placeholderText: qsTr("New path")
            }
        }
    }

    ConfirmDialog {
        id: confirmation
        property string action: ""
        property string targetPath: ""
        onConfirmed: {
            if (action === "remove")
                dialog.repository.removeWorktree(targetPath, forceRemoval.checked)
            else if (action === "prune")
                dialog.repository.pruneWorktrees()
            else if (action === "repair")
                dialog.repository.repairWorktrees()
        }
    }
}
