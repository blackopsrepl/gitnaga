import QtQuick
import QtQuick.Controls
import GitNaga

// One reference in the sidebar: its colour, its name, how far it is from its
// upstream, and whether the graph currently hides it.
Rectangle {
    id: row
    required property var reference
    required property var repository
    signal menuRequested(var reference)

    readonly property bool checkedOut: reference.isHead === true
    readonly property bool hiddenInGraph: reference.hidden === true
    width: parent ? parent.width : 200
    height: 26
    color: hover.hovered ? NagaTheme.hoverFill : "transparent"

    HoverHandler { id: hover }
    TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: if (row.reference.oid) row.repository.selectOid(row.reference.oid)
        onDoubleTapped: {
            if (row.reference.kind === "local" && !row.checkedOut)
                row.repository.checkoutBranch(row.reference.name)
            else if (row.reference.kind === "remote")
                row.repository.checkoutRemoteBranch(row.reference.name)
        }
    }
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: row.menuRequested(row.reference)
    }

    Rectangle {
        width: 8
        height: 8
        radius: row.reference.kind === "tag" ? 4 : 1
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        color: row.reference.color || "#5b6478"
        opacity: row.hiddenInGraph ? 0.35 : 1.0
    }
    Text {
        id: nameText
        anchors.left: parent.left
        anchors.leftMargin: 28
        anchors.right: trackText.left
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        text: (row.checkedOut ? "⎇ " : "") + row.reference.name
        color: row.checkedOut ? NagaTheme.accent : row.hiddenInGraph ? NagaTheme.muted : NagaTheme.text
        font.pixelSize: 12
        font.weight: row.checkedOut ? Font.DemiBold : Font.Normal
        font.strikeout: row.hiddenInGraph
        elide: Text.ElideMiddle
    }
    // Distance from the upstream, the number a person checks before pushing
    // or pulling. A gone upstream is said, not hidden.
    Text {
        id: trackText
        anchors.right: eyeButton.visible ? eyeButton.left : oidText.left
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        visible: text.length > 0
        text: {
            if (row.reference.upstreamGone)
                return qsTr("gone")
            var parts = []
            if (row.reference.ahead > 0)
                parts.push("↑" + row.reference.ahead)
            if (row.reference.behind > 0)
                parts.push("↓" + row.reference.behind)
            if (row.reference.solo)
                parts.push(qsTr("solo"))
            return parts.join(" ")
        }
        color: row.reference.upstreamGone ? "#ff8f9c" : "#e6b45a"
        font.pixelSize: 10
        font.weight: Font.DemiBold
    }
    // Hide/show in the graph, one click from the row. It shows on hover, and
    // stays while the ref is hidden so the state can be read at a glance.
    Item {
        id: eyeButton
        readonly property bool available: row.reference.kind !== "stash" && (row.reference.oid || "").length > 0
        visible: available && (hover.hovered || row.hiddenInGraph)
        anchors.right: oidText.left
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        width: 18
        height: 18
        Rectangle {
            anchors.fill: parent
            radius: 3
            color: eyeHover.hovered ? NagaTheme.pressedFill : "transparent"
        }
        NagaIcon {
            anchors.centerIn: parent
            width: 14
            height: 14
            name: row.hiddenInGraph ? "eye-off" : "eye"
            color: eyeHover.hovered ? NagaTheme.hoverContent : NagaTheme.muted
        }
        HoverHandler { id: eyeHover; cursorShape: Qt.PointingHandCursor }
        TapHandler {
            onTapped: row.repository.setRefHidden(row.reference.fullName, !row.hiddenInGraph)
        }
        ToolTip.visible: eyeHover.hovered
        ToolTip.delay: 400
        ToolTip.text: row.hiddenInGraph ? qsTr("Show in graph") : qsTr("Hide in graph")
    }
    Text {
        id: oidText
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        text: row.reference.shortOid || ""
        color: NagaTheme.muted
        font.family: "monospace"
        font.pixelSize: 10
    }
    ToolTip.visible: hover.hovered && (row.reference.upstream || "").length > 0
    ToolTip.delay: 600
    ToolTip.text: qsTr("Tracks ") + (row.reference.upstream || "")
}
