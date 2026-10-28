import QtQuick
import GitNaga

// Right-click actions on one sidebar reference. Each kind gets the actions
// that make sense for it; the rest stay out of the menu instead of greyed.
NagaMenu {
    id: menu
    required property var repository
    property var confirm
    property var target: null
    // Qt sizes the menu from the items visible when it was first laid out;
    // a stash hides nearly all of them, which collapsed it to a sliver.
    width: Math.max(implicitWidth, 260)

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
    NagaMenuSeparator { visible: menu.isLocal || menu.isRemote; height: visible ? implicitHeight : 0 }
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
    NagaMenuSeparator { visible: menu.isLocal || menu.isRemote || menu.isTag; height: visible ? implicitHeight : 0 }
    NagaMenuItem {
        visible: menu.isStash
        height: visible ? implicitHeight : 0
        text: qsTr("Apply stash")
        onTriggered: menu.repository.stashAction("apply", menu.target.selector, menu.target.oid)
    }
    NagaMenuItem {
        visible: menu.isStash
        height: visible ? implicitHeight : 0
        text: qsTr("Pop stash")
        onTriggered: menu.repository.stashAction("pop", menu.target.selector, menu.target.oid)
    }
    NagaMenuItem {
        visible: menu.isStash
        height: visible ? implicitHeight : 0
        text: qsTr("Drop stash…")
        onTriggered: menu.confirm.ask(qsTr("Drop ") + menu.target.selector + qsTr("? Its changes are lost."),
                                      function() { menu.repository.stashAction("drop", menu.target.selector, menu.target.oid) })
    }
    NagaMenuSeparator { visible: menu.isStash; height: visible ? implicitHeight : 0 }
    NagaMenuItem {
        text: (menu.isStash ? qsTr("Show stash ") : qsTr("Show commit ")) + (menu.target ? menu.target.shortOid : "")
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
