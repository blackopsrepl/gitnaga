import QtQuick
import QtQuick.Controls
import GitNaga

Popup {
    id: menu
    required property var repository
    property var branchPrompt
    property var tagPrompt
    property var confirm

    property var actions: []
    property string promptOid: ""

    width: 320
    height: actionColumn.implicitHeight + 8
    padding: 4
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        color: "#12161f"
        border.color: "#2b3242"
    }

    contentItem: Column {
        id: actionColumn
        spacing: 0

        Repeater {
            model: menu.actions
            delegate: Item {
                id: actionRow
                required property var modelData
                width: actionColumn.width
                height: modelData.separator ? 9 : 26

                Rectangle {
                    visible: actionRow.modelData.separator === true
                    width: parent.width
                    height: 1
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#242c3b"
                }
                Rectangle {
                    visible: actionRow.modelData.separator !== true
                    anchors.fill: parent
                    color: actionHover.hovered && actionRow.modelData.enabled !== false ? NagaTheme.hoverFill : "transparent"
                }
                Text {
                    visible: actionRow.modelData.separator !== true
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: actionRow.modelData.separator === true ? "" : (actionRow.modelData.label || "")
                    color: actionRow.modelData.destructive === true ? "#ff8f9c"
                         : actionRow.modelData.enabled === false ? "#5b6478" : "#dde3ee"
                    elide: Text.ElideRight
                    font.pixelSize: 12
                }
                HoverHandler { id: actionHover }
                TapHandler {
                    onTapped: {
                        if (actionRow.modelData.separator === true || actionRow.modelData.enabled === false)
                            return
                        menu.close()
                        actionRow.modelData.run()
                    }
                }
            }
        }
    }

    /**
     * @param {string} label
     * @param {function} run
     */
    function add(label, run) {
        actions.push({ label: label, run: run, separator: false })
    }
    function localBranches(refs) {
        var out = []
        if (!refs)
            return out
        for (var i = 0; i < refs.length; ++i) {
            if (refs[i].indexOf("⇄ ") !== 0 && refs[i].indexOf("# ") !== 0)
                out.push(refs[i])
        }
        return out
    }

    function place(localPoint) {
        x = Math.max(4, Math.min(localPoint.x, parent.width - width - 4))
        y = Math.max(4, Math.min(localPoint.y, parent.height - height - 4))
        open()
    }

    function showBlank(localPoint) {
        // Assign a whole new array: the Repeater bound to `actions` does not
        // observe in-place pushes, and an in-place mutation used to open an
        // empty sliver of a popup on the uncommitted-work row.
        actions = [{ label: qsTr("Refresh"), run: function() { repository.refresh() } }]
        place(localPoint)
    }

    /**
     * @param {number} row
     * @param {string} oid
     * @param {point} localPoint
     */
    function show(row, oid, localPoint) {
        var info = repository.commits.at(row)
        var refs = info.refs || []
        var target = oid
        var shortOid = info.shortOid || String(oid).substring(0, 8)
        var branch = repository.currentBranch
        var branches = localBranches(refs)
        var entries = []

        entries.push({ label: qsTr("Checkout ") + shortOid, run: function() { repository.checkoutCommit(target) } })
        for (var i = 0; i < branches.length; ++i) {
            (function(name) {
                entries.push({ label: qsTr("Checkout branch ") + name, run: function() { repository.checkoutBranch(name) } })
            })(branches[i])
        }

        entries.push({ separator: true })
        entries.push({ label: qsTr("Create branch here…"), run: function() {
            menu.promptOid = target
            menu.branchPrompt.pendingOid = target
            menu.branchPrompt.openWith("")
        } })
        entries.push({ label: qsTr("Create tag here…"), run: function() {
            menu.promptOid = target
            menu.tagPrompt.pendingOid = target
            menu.tagPrompt.openWith("")
        } })

        entries.push({ separator: true })
        entries.push({ label: qsTr("Cherry-pick ") + shortOid + qsTr(" onto ") + branch, run: function() { repository.cherryPick(target) } })
        entries.push({ label: qsTr("Merge ") + shortOid + qsTr(" into ") + branch, run: function() { repository.mergeCommit(target) } })
        entries.push({ label: qsTr("Revert ") + shortOid, run: function() { repository.revertCommit(target) } })

        entries.push({ separator: true })
        entries.push({ label: qsTr("Reset ") + branch + qsTr(" to ") + shortOid + qsTr(" (soft)"), run: function() { repository.resetTo(target, "soft") } })
        entries.push({ label: qsTr("Reset ") + branch + qsTr(" to ") + shortOid + qsTr(" (mixed)"), run: function() { repository.resetTo(target, "mixed") } })
        entries.push({ label: qsTr("Hard reset ") + branch + qsTr(" to ") + shortOid + qsTr("…"), destructive: true, run: function() {
            menu.confirm.ask(qsTr("Discard all working tree changes and move ") + branch + qsTr(" to ") + shortOid + qsTr("?"),
                             function() { repository.resetTo(target, "hard") })
        } })
        entries.push({ label: qsTr("Rebase ") + branch + qsTr(" onto ") + shortOid + qsTr("…"), destructive: true, run: function() {
            menu.confirm.ask(qsTr("Rebase ") + branch + qsTr(" onto ") + shortOid + qsTr("?"),
                             function() { repository.rebaseOnto(target) })
        } })

        if (branches.length > 0) {
            entries.push({ separator: true })
            for (var j = 0; j < branches.length; ++j) {
                (function(name) {
                    entries.push({ label: qsTr("Delete branch ") + name + qsTr("…"), destructive: true, run: function() {
                        menu.confirm.ask(qsTr("Force delete branch ") + name + qsTr("?"),
                                         function() { repository.deleteBranch(name) })
                    } })
                })(branches[j])
            }
        }

        entries.push({ separator: true })
        entries.push({ label: qsTr("Copy commit hash"), run: function() { repository.copyToClipboard(target) } })
        entries.push({ label: qsTr("Refresh"), run: function() { repository.refresh() } })

        actions = entries
        place(localPoint)
    }
}
