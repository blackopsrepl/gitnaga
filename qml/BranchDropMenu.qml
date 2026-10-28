import QtQuick
import GitNaga

// What a dropped branch chip can do where it landed: on a commit it merges,
// rebases, resets or branches; on another branch's chip it merges or rebases
// in either direction. Everything that moves a ref asks first, because a drop
// is easy to make by accident.
CommitMenu {
    id: menu

    function addResets(entries: var, name: string, target: string, shortOid: string) {
        var current = name === repository.currentBranch
        if (!current) {
            entries.push({ label: qsTr("Reset ") + name + qsTr(" to here…"), run: function() {
                menu.confirm.ask(qsTr("Move ") + name + qsTr(" to ") + shortOid + qsTr("? Commits only it reached become unreachable."),
                                 function() { repository.history.moveBranch(name, target) })
            } })
            return
        }
        var modes = [
            { mode: "soft", label: qsTr("Soft reset ") },
            { mode: "mixed", label: qsTr("Mixed reset ") },
            { mode: "hard", label: qsTr("Hard reset "), destructive: true }
        ]
        for (var i = 0; i < modes.length; ++i) {
            (function(entry) {
                entries.push({ label: entry.label + name + qsTr(" to here…"), destructive: entry.destructive === true,
                               run: function() {
                                   menu.confirm.ask(qsTr("Reset ") + name + qsTr(" to ") + shortOid + " (" + entry.mode + ")"
                                                    + (entry.mode === "hard" ? qsTr("? All working tree changes are discarded.") : "?"),
                                                    function() { repository.resetTo(target, entry.mode) })
                               } })
            })(modes[i])
        }
    }

    function addBranchPair(entries: var, source: string, target: string) {
        entries.push({ label: qsTr("Merge ") + target + qsTr(" into ") + source, run: function() { repository.history.mergeBranchInto(target, source) } })
        entries.push({ label: qsTr("Merge ") + source + qsTr(" into ") + target, run: function() { repository.history.mergeBranchInto(source, target) } })
        entries.push({ label: qsTr("Rebase ") + source + qsTr(" onto ") + target + qsTr("…"), destructive: true, run: function() {
            menu.confirm.ask(qsTr("Rebase ") + source + qsTr(" onto ") + target + qsTr("?"),
                             function() { repository.history.rebaseBranch(source, target) })
        } })
        entries.push({ label: qsTr("Rebase ") + target + qsTr(" onto ") + source + qsTr("…"), destructive: true, run: function() {
            menu.confirm.ask(qsTr("Rebase ") + target + qsTr(" onto ") + source + qsTr("?"),
                             function() { repository.history.rebaseBranch(target, source) })
        } })
    }

    function showDrop(reference: var, row: int, chip: var, localPoint: point) {
        var info = repository.commits.at(row)
        var name = reference.name
        var target = info.oid
        var shortOid = info.shortOid
        var branches = namesOfKind(info.refs, "local")
        var label = chip ? chip.name : branches.length > 0 && branches[0] !== name ? branches[0] : shortOid
        var entries = [{ label: name + qsTr(" dropped on ") + label, enabled: false }, { separator: true }]
        if (chip) {
            addBranchPair(entries, name, chip.name)
            entries.push({ separator: true })
        }
        var onOwnCommit = target === reference.oid
        var current = repository.currentBranch
        if (!onOwnCommit && current.length > 0 && name !== current && target === repository.headOid && !chip)
            entries.push({ label: qsTr("Merge ") + name + qsTr(" into ") + current, run: function() { repository.history.mergeBranchInto(name, current) } })
        if (!onOwnCommit && !chip) {
            var onto = label
            entries.push({ label: qsTr("Rebase ") + name + qsTr(" onto ") + onto + qsTr("…"), destructive: true, run: function() {
                menu.confirm.ask(qsTr("Rebase ") + name + qsTr(" onto ") + onto + qsTr("?"),
                                 function() { repository.history.rebaseBranch(name, onto === shortOid ? target : onto) })
            } })
        }
        if (!onOwnCommit)
            addResets(entries, name, target, shortOid)
        if (entries.length > 0 && !entries[entries.length - 1].separator)
            entries.push({ separator: true })
        entries.push({ label: qsTr("Create branch here…"), run: function() {
            menu.branchPrompt.pendingOid = target
            menu.branchPrompt.openWith("")
        } })
        actions = entries
        place(localPoint)
    }
}
