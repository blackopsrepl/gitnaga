import QtQuick
import QtQuick.Controls
import GitNaga

// Interactive rebase: the commits from one commit up to HEAD, oldest first.
// Drag a row by its handle to reorder; p r e s f d (or the chip) pick what
// happens to it. Start hands the arrangement to the controller, which runs it
// without ever opening an editor.
Popup {
    id: editor
    required property var repository

    property string pendingOid: ""
    property string base: ""
    property bool loading: false
    property bool running: false
    property string error: ""
    // Bumped on every structural change, so bindings that read the model
    // through functions re-evaluate.
    property int revision: 0
    readonly property var actions: ["pick", "reword", "edit", "squash", "fixup", "drop"]
    readonly property var keys: ({ p: "pick", r: "reword", e: "edit", s: "squash", f: "fixup", d: "drop" })
    readonly property string problem: revision >= 0 ? validation() : ""

    parent: Overlay.overlay
    x: Math.round((parent.width - width) / 2)
    // A fixed top: rows grow downwards as messages open, never jumping up
    // under the pointer.
    y: Math.round(parent.height * 0.1)
    width: Math.min(880, parent.width - 80)
    // Fits the commits it lists, up to most of the window.
    height: Math.min(parent.height - y - 40, 56 + 58 + Math.max(200, list ? list.contentHeight : 0))
    modal: true
    focus: true
    padding: 0
    closePolicy: running ? Popup.NoAutoClose : Popup.CloseOnEscape

    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.45) }

    background: Rectangle {
        color: NagaTheme.panel
        border.color: NagaTheme.border
        radius: 4
    }

    ListModel { id: stepsModel }

    function openFor(oid) {
        pendingOid = oid
        base = ""
        error = repository.operationInProgress.length > 0
            ? qsTr("A ") + repository.operationInProgress + qsTr(" is in progress; finish or abort it first.") : ""
        loading = error.length === 0
        running = false
        stepsModel.clear()
        open()
        if (loading)
            repository.requestRebasePlan(oid)
    }

    function step(i) { return stepsModel.get(i) }
    function folds(action) { return action === "squash" || action === "fixup" }
    function actionColor(action) {
        switch (action) {
        case "reword": return NagaTheme.info
        case "edit": return NagaTheme.warning
        case "squash": case "fixup": return NagaTheme.violet
        case "drop": return NagaTheme.danger
        default: return NagaTheme.accent
        }
    }
    function effectText(action) {
        switch (action) {
        case "reword": return qsTr("new message")
        case "edit": return qsTr("stops to amend")
        case "squash": return qsTr("folds in, messages combined")
        case "fixup": return qsTr("folds in, message discarded")
        case "drop": return qsTr("removed")
        default: return qsTr("kept")
        }
    }
    function groupHead(i) {
        var j = i - 1
        while (j >= 0 && (folds(step(j).action) || step(j).action === "drop"))
            --j
        return j
    }
    function isGroupTail(i) {
        if (i < 0 || i >= stepsModel.count || step(i).action !== "squash")
            return false
        for (var j = i + 1; j < stepsModel.count; ++j) {
            var action = step(j).action
            if (action === "squash")
                return false
            if (!folds(action) && action !== "drop")
                break
        }
        return true
    }
    function headMessage(h) {
        var head = step(h)
        return head.action === "reword" && head.message.trim().length > 0 ? head.message : head.original
    }
    function messageFor(i) {
        var row = step(i)
        if (row.action === "reword" || row.edited)
            return row.message
        var h = groupHead(i)
        if (h < 0)
            return row.original
        var messages = [headMessage(h)]
        for (var j = h + 1; j <= i; ++j) {
            if (step(j).action === "squash")
                messages.push(step(j).original)
        }
        return repository.combineMessages(messages).replace(/\s+$/, "")
    }
    function setMessage(i, text) {
        stepsModel.setProperty(i, "message", text)
        stepsModel.setProperty(i, "edited", true)
    }
    function setAction(i, action) {
        if (i < 0 || i >= stepsModel.count)
            return
        if (action === "reword" && step(i).message.length === 0)
            stepsModel.setProperty(i, "message", step(i).original.replace(/\s+$/, ""))
        stepsModel.setProperty(i, "action", action)
        ++revision
    }
    function moveStep(from, to) {
        if (from === to || to < 0 || to >= stepsModel.count)
            return
        stepsModel.move(from, to, 1)
        ++revision
    }
    function dragRowTo(from, contentY) {
        moveStep(from, list.indexAt(20, contentY))
    }
    function focusRow(i) {
        var item = list.itemAtIndex(i)
        if (item) {
            list.positionViewAtIndex(i, ListView.Contain)
            item.forceActiveFocus()
        }
    }
    function handleKey(row: Item, i: int, event: KeyEvent) {
        // Read the key, not the text: a key with no text (synthesised, or
        // under an input method) still means its letter.
        var letter = event.key >= Qt.Key_A && event.key <= Qt.Key_Z
            ? String.fromCharCode(event.key).toLowerCase() : event.text
        var action = keys[letter]
        if (action && !(event.modifiers & (Qt.ControlModifier | Qt.AltModifier))) {
            setAction(i, action)
        } else if ((event.modifiers & Qt.AltModifier) && (event.key === Qt.Key_Up || event.key === Qt.Key_Down)) {
            var to = i + (event.key === Qt.Key_Up ? -1 : 1)
            moveStep(i, to)
            focusRow(Math.max(0, Math.min(stepsModel.count - 1, to)))
        } else if (event.key === Qt.Key_Up || event.key === Qt.Key_Down) {
            focusRow(Math.max(0, Math.min(stepsModel.count - 1, i + (event.key === Qt.Key_Up ? -1 : 1))))
        } else {
            return
        }
        event.accepted = true
    }
    function validation() {
        var kept = false
        for (var i = 0; i < stepsModel.count; ++i) {
            var action = step(i).action
            if (action === "drop")
                continue
            if (!kept && folds(action))
                return step(i).shortOid + qsTr(" has nothing above it to fold into; pick or reword the first commit.")
            kept = true
        }
        return kept || stepsModel.count === 0 ? "" : qsTr("Every commit is dropped; keep at least one.")
    }
    function start() {
        var steps = []
        for (var i = 0; i < stepsModel.count; ++i) {
            var row = step(i)
            var message = row.action === "reword" || (isGroupTail(i) && row.edited) ? row.message : ""
            steps.push({ oid: row.oid, action: row.action, subject: row.subject, original: row.original, message: message })
        }
        error = ""
        running = true
        repository.startInteractiveRebase(base, steps, body.autostash)
    }
    function openActionMenu(i: int, anchor: Item) {
        actionMenu.row = i
        var point = anchor.mapToItem(editor.contentItem, 0, anchor.height + 2)
        actionMenu.x = point.x
        actionMenu.y = point.y
        actionMenu.open()
    }

    Connections {
        target: editor.repository
        function onRebasePlanReady(plan) {
            if (plan.oid !== editor.pendingOid || !editor.loading)
                return
            editor.loading = false
            if (!plan.ok) {
                editor.error = plan.error
                return
            }
            editor.base = plan.base
            for (var i = 0; i < plan.steps.length; ++i) {
                var s = plan.steps[i]
                stepsModel.append({ oid: s.oid, shortOid: s.shortOid, subject: s.subject, original: s.original,
                                    action: "pick", message: "", edited: false })
            }
            ++editor.revision
            Qt.callLater(editor.focusRow, 0)
        }
        function onOperationFinished(ok, message) {
            if (!editor.running)
                return
            editor.running = false
            if (ok)
                editor.close()
            else
                editor.error = message
        }
        // A stop (a conflict or an edit) belongs to the in-progress banner.
        function onStatusChanged() {
            if (editor.opened && !editor.loading && editor.repository.operationInProgress === "rebase")
                editor.close()
        }
    }

    contentItem: RebaseEditorBody {
        id: body
        editor: editor
        repository: editor.repository
        model: stepsModel
    }

    RebaseActionMenu {
        id: actionMenu
        editor: editor
    }

    readonly property ListView list: body.list
}
