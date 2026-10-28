import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GitNaga

// Fetch, Pull and Push in the toolbar, with the distance from the upstream on
// the buttons, and the dialogs a push can lead to.
RowLayout {
    id: bar
    required property var repository
    readonly property var sync: repository.sync
    readonly property bool hasBranch: repository.currentBranch.length > 0 && !repository.detached
    readonly property bool hasUpstream: repository.upstream.length > 0
    readonly property var pullModes: [
        { mode: "ff", label: qsTr("Fast-forward if possible") },
        { mode: "ff-only", label: qsTr("Fast-forward only") },
        { mode: "rebase", label: qsTr("Rebase") }
    ]
    spacing: 6
    enabled: repository.repositoryPath.length > 0

    function modeLabel(mode) {
        for (var i = 0; i < pullModes.length; ++i) {
            if (pullModes[i].mode === mode)
                return pullModes[i].label
        }
        return mode
    }

    SyncButton {
        id: fetchButton
        text: qsTr("Fetch")
        tooltip: qsTr("Fetch all remotes and prune deleted branches")
                 + (bar.sync.autoFetch ? qsTr(" (automatic every 10 minutes)") : "")
        onClicked: bar.sync.fetch()
        onMenuRequested: (anchor) => fetchMenu.popup(anchor, 0, anchor.height)
    }
    SyncButton {
        id: pullButton
        text: qsTr("Pull") + (bar.repository.behind > 0 ? " ↓" + bar.repository.behind : "")
        actionEnabled: bar.hasBranch && bar.hasUpstream
        tooltip: !bar.hasBranch ? qsTr("HEAD is detached: check out a branch to pull into")
               : !bar.hasUpstream ? qsTr("%1 tracks no upstream branch: set one from its menu").arg(bar.repository.currentBranch)
               : qsTr("Pull %1 · %2").arg(bar.repository.upstream).arg(bar.modeLabel(bar.sync.pullMode))
        onClicked: bar.sync.pull()
        onMenuRequested: (anchor) => pullMenu.popup(anchor, 0, anchor.height)
    }
    SyncButton {
        id: pushButton
        text: qsTr("Push") + (bar.repository.ahead > 0 ? " ↑" + bar.repository.ahead : "")
        hasMenu: false
        actionEnabled: bar.hasBranch
        tooltip: !bar.hasBranch ? qsTr("HEAD is detached: check out a branch to push")
               : bar.hasUpstream ? qsTr("Push %1 to %2").arg(bar.repository.currentBranch).arg(bar.repository.upstream)
               : qsTr("Publish %1 and track it").arg(bar.repository.currentBranch)
        onClicked: bar.sync.push()
    }

    NagaMenu {
        id: fetchMenu
        NagaMenuItem { text: qsTr("Fetch now"); onTriggered: bar.sync.fetch() }
        NagaMenuItem {
            text: qsTr("Fetch automatically every 10 minutes")
            checkable: true
            checked: bar.sync.autoFetch
            onTriggered: {
                bar.sync.autoFetch = !bar.sync.autoFetch
                checked = Qt.binding(function() { return bar.sync.autoFetch })
            }
        }
    }

    NagaMenu {
        id: pullMenu
        Repeater {
            model: bar.pullModes
            delegate: NagaMenuItem {
                required property var modelData
                text: modelData.label
                checkable: true
                checked: bar.sync.pullMode === modelData.mode
                enabled: pullButton.actionEnabled
                onTriggered: {
                    bar.sync.pullWith(modelData.mode)
                    checked = Qt.binding(function() { return bar.sync.pullMode === modelData.mode })
                }
            }
        }
    }

    ConfirmDialog {
        id: forceConfirm
        objectName: "forcePushConfirm"
        property string branch: ""
        property string remote: ""
        parent: Overlay.overlay
        title: qsTr("Push rejected")
        onConfirmed: bar.sync.forcePushWithLease(branch, remote)
        Component.onCompleted: standardButton(Dialog.Ok).text = qsTr("Force push with lease")
    }

    RemoteChoiceDialog {
        id: remoteChoice
        sync: bar.sync
    }

    Connections {
        target: bar.sync
        function onPushRejected(branch, remote, message) {
            forceConfirm.branch = branch
            forceConfirm.remote = remote
            forceConfirm.message = qsTr("%1/%2 has commits your %2 does not have. Replace it with your %2?\n\n"
                                        + "The lease refuses if %1 moved since your last fetch, so no work "
                                        + "you have not seen is lost.").arg(remote).arg(branch)
            forceConfirm.open()
        }
        function onRemoteChoiceNeeded(tag, remotes) { remoteChoice.choose(tag, remotes) }
    }
}
