import QtQuick
import GitNaga

// Right-click actions on one sidebar reference. Each kind gets the actions
// that make sense for it; the rest stay out of the menu instead of greyed.
NagaMenu {
    id: menu
    required property var repository
    property var confirm
    property var upstreamDialog
    property var target: null

    readonly property string kind: target ? target.kind : ""
    readonly property string name: target ? target.name : ""
    readonly property bool isLocal: kind === "local"
    readonly property bool isRemote: kind === "remote"
    readonly property bool isTag: kind === "tag"
    readonly property bool isStash: kind === "stash"
    readonly property bool inGraph: target !== null && !isStash && (target.oid || "").length > 0

    NagaMenuItem {
        visible: menu.isLocal && !(menu.target && menu.target.isHead)
        height: visible ? implicitHeight : 0
        text: qsTr("Checkout ") + menu.name
        onTriggered: menu.repository.checkoutBranch(menu.name)
    }
    NagaMenuItem {
        visible: menu.isRemote
        height: visible ? implicitHeight : 0
        text: qsTr("Checkout as local branch")
        onTriggered: menu.repository.checkoutRemoteBranch(menu.name)
    }
    NagaMenuItem {
        visible: menu.isLocal && menu.repository.currentBranch.length > 0 && !(menu.target && menu.target.isHead)
        height: visible ? implicitHeight : 0
        text: qsTr("Merge ") + menu.name + qsTr(" into ") + menu.repository.currentBranch
        onTriggered: menu.repository.mergeCommit(menu.target.oid, menu.name)
    }
    NagaMenuItem {
        visible: menu.isLocal
        height: visible ? implicitHeight : 0
        text: qsTr("Set upstream…")
        onTriggered: menu.upstreamDialog.openFor(menu.target)
    }
    NagaMenuItem {
        visible: menu.isLocal && (menu.target.upstream || "").length > 0
        height: visible ? implicitHeight : 0
        text: qsTr("Unset upstream ") + (menu.target ? menu.target.upstream : "")
        onTriggered: menu.repository.sync.unsetUpstream(menu.name)
    }
    NagaMenuItem {
        visible: menu.isTag
        height: visible ? implicitHeight : 0
        text: qsTr("Push tag ") + menu.name
        onTriggered: menu.repository.sync.pushTag(menu.name)
    }
    NagaMenuSeparator { visible: menu.isLocal || menu.isRemote || menu.isTag; height: visible ? implicitHeight : 0 }
    NagaMenuItem {
        visible: menu.inGraph
        height: visible ? implicitHeight : 0
        text: menu.target && menu.target.hidden ? qsTr("Show in graph") : qsTr("Hide in graph")
        onTriggered: menu.repository.setRefHidden(menu.target.fullName, !menu.target.hidden)
    }
    NagaMenuItem {
        visible: menu.inGraph
        height: visible ? implicitHeight : 0
        text: menu.target && menu.target.solo ? qsTr("Show all branches") : qsTr("Solo in graph")
        onTriggered: menu.repository.setSoloRef(menu.target.fullName)
    }
    NagaMenuSeparator { visible: menu.inGraph; height: visible ? implicitHeight : 0 }
    NagaMenuItem {
        visible: menu.isLocal
        height: visible ? implicitHeight : 0
        text: qsTr("Delete branch ") + menu.name + qsTr("…")
        onTriggered: menu.confirm.ask(qsTr("Force delete branch ") + menu.name + qsTr("?"),
                                      function() { menu.repository.deleteBranch(menu.name) })
    }
    NagaMenuItem {
        visible: menu.isRemote
        height: visible ? implicitHeight : 0
        // The remote branch is not this repository's to lose, but it is not
        // recoverable from here either, so it is confirmed like any other
        // destructive action.
        text: qsTr("Delete remote branch ") + menu.name + qsTr("…")
        onTriggered: menu.confirm.ask(qsTr("Delete branch ") + menu.name
                                      + qsTr(" on its remote? This cannot be undone from here."),
                                      function() { menu.repository.deleteRemoteBranch(menu.name) })
    }
    NagaMenuItem {
        visible: menu.isTag
        height: visible ? implicitHeight : 0
        text: qsTr("Delete tag ") + menu.name + qsTr("…")
        onTriggered: menu.confirm.ask(qsTr("Delete tag ") + menu.name + qsTr("?"),
                                      function() { menu.repository.deleteTag(menu.name) })
    }
    NagaMenuItem {
        visible: menu.isStash
        height: visible ? implicitHeight : 0
        text: qsTr("Drop ") + (menu.target ? menu.target.selector : "") + qsTr("…")
        onTriggered: menu.confirm.ask(qsTr("Drop ") + menu.target.selector + qsTr("? Undo (Ctrl+Z) can bring it back."),
                                      function() { menu.repository.dropStash(menu.target.selector, menu.target.message) })
    }
    NagaMenuSeparator { visible: menu.isLocal || menu.isRemote || menu.isTag || menu.isStash; height: visible ? implicitHeight : 0 }
    NagaMenuItem {
        text: qsTr("Show commit ") + (menu.target ? menu.target.shortOid : "")
        onTriggered: menu.repository.selectOid(menu.target.oid)
    }
    NagaMenuItem {
        visible: !menu.isStash
        height: visible ? implicitHeight : 0
        text: qsTr("Checkout commit")
        onTriggered: menu.repository.checkoutCommit(menu.target.oid)
    }
    NagaMenuItem {
        text: menu.isStash ? qsTr("Copy hash") : qsTr("Copy name")
        onTriggered: menu.repository.copyToClipboard(menu.isStash ? menu.target.oid : menu.name)
    }
}
