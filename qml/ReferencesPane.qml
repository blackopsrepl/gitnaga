import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GitNaga

Rectangle {
    id: pane
    required property var repository
    signal closeRequested()

    property var target: null

    color: NagaTheme.panel

    // Internal refs (tool checkpoints, backups, a remote's HEAD) only appear
    // when asked for, in a section of their own.
    readonly property var sections: {
        var list = [
            { kind: "local", label: qsTr("Branches") },
            { kind: "remote", label: qsTr("Remotes") },
            { kind: "tag", label: qsTr("Tags") },
            { kind: "stash", label: qsTr("Stashes") }
        ]
        if (repository.showInternalRefs)
            list.push({ kind: "other", label: qsTr("Internal") })
        return list
    }

    function filtered(kind) {
        var needle = filterField.text.toLowerCase()
        var out = []
        if (kind === "stash") {
            var stashes = pane.repository.stashes
            for (var s = 0; s < stashes.length; ++s) {
                var label = stashes[s].selector + "  " + stashes[s].message
                if (needle.length > 0 && label.toLowerCase().indexOf(needle) < 0)
                    continue
                out.push({ kind: "stash", name: label, selector: stashes[s].selector, message: stashes[s].message, oid: stashes[s].oid,
                           shortOid: stashes[s].shortOid, color: "#7c8698" })
            }
            return out
        }
        var refs = pane.repository.references
        for (var i = 0; i < refs.length; ++i) {
            var reference = refs[i]
            var matchesKind = kind === "other" ? (reference.kind === "other" || reference.internal) : (reference.kind === kind && !reference.internal)
            if (!matchesKind)
                continue
            if (needle.length > 0 && reference.name.toLowerCase().indexOf(needle) < 0)
                continue
            out.push(reference)
        }
        return out
    }

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
                anchors.rightMargin: 6
                Text {
                    text: qsTr("References")
                    color: NagaTheme.text
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.4
                }
                Item { Layout.fillWidth: true }
                NagaButton {
                    text: qsTr("Internal")
                    active: pane.repository.showInternalRefs
                    implicitHeight: 22
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Show refs that tools keep for themselves")
                    onClicked: pane.repository.showInternalRefs = !pane.repository.showInternalRefs
                }
                NagaIconButton {
                    text: "✕"
                    tooltip: qsTr("Close references")
                    onClicked: pane.closeRequested()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            color: NagaTheme.panel
            TextField {
                id: filterField
                anchors.fill: parent
                anchors.margins: 4
                placeholderText: qsTr("Filter references")
                font.pixelSize: 11
                selectByMouse: true
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 26
            visible: pane.repository.soloRef.length > 0
            color: NagaTheme.accentTint(0.12)
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Graph shows only ") + pane.repository.soloRef.replace(/^refs\/(heads|remotes|tags)\//, "")
                color: NagaTheme.accentContent
                font.pixelSize: 11
                elide: Text.ElideMiddle
                width: parent.width - 80
            }
            NagaButton {
                anchors.right: parent.right
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                implicitHeight: 20
                text: qsTr("Show all")
                onClicked: pane.repository.setSoloRef(pane.repository.soloRef)
            }
        }

        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentHeight: sectionsColumn.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            Column {
                id: sectionsColumn
                width: parent.width
                Repeater {
                    model: pane.sections
                    delegate: Column {
                        id: sectionColumn
                        required property var modelData
                        readonly property var entries: pane.filtered(modelData.kind)
                        property bool collapsed: modelData.kind === "tag" && entries.length > 12
                        width: sectionsColumn.width
                        visible: entries.length > 0 || modelData.kind === "local"

                        Rectangle {
                            width: parent.width
                            height: 24
                            color: sectionHover.hovered ? NagaTheme.hoverFill : NagaTheme.panel
                            HoverHandler { id: sectionHover }
                            TapHandler { onTapped: sectionColumn.collapsed = !sectionColumn.collapsed }
                            Row {
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 6
                                Text {
                                    text: (sectionColumn.collapsed ? "▸ " : "▾ ") + sectionColumn.modelData.label
                                    color: NagaTheme.muted
                                    font.pixelSize: 10
                                    font.weight: Font.Bold
                                    font.letterSpacing: 0.8
                                }
                                Text {
                                    text: sectionColumn.entries.length
                                    color: NagaTheme.muted
                                    font.pixelSize: 10
                                }
                            }
                        }

                        Repeater {
                            model: sectionColumn.collapsed ? [] : sectionColumn.entries
                            delegate: RefRow {
                                required property var modelData
                                reference: modelData
                                repository: pane.repository
                                width: sectionColumn.width
                                onMenuRequested: (reference) => pane.openMenu(reference)
                            }
                        }
                    }
                }
            }
        }

        // Hidden refs are easy to forget about; say how many there are and
        // offer them all back in one click. It sits below the list so hiding a
        // ref never shifts the rows under the pointer.
        Rectangle {
            readonly property int count: {
                var refs = pane.repository.references
                var total = 0
                for (var i = 0; i < refs.length; ++i) {
                    if (refs[i].hidden)
                        ++total
                }
                return total
            }
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            visible: count > 0
            color: NagaTheme.raised
            border.color: NagaTheme.border
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Hidden from the graph")
                color: NagaTheme.muted
                font.pixelSize: 11
            }
            NagaButton {
                anchors.right: parent.right
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                implicitHeight: 20
                text: qsTr("Show all hidden (") + parent.count + ")"
                onClicked: pane.repository.showAllRefs()
            }
        }
    }

    ReferenceMenu {
        id: refMenu
        repository: pane.repository
        confirm: confirm
        upstreamDialog: upstreamDialog
    }

    UpstreamDialog {
        id: upstreamDialog
        repository: pane.repository
    }

    function openMenu(reference) {
        if (!reference)
            return
        refMenu.target = reference
        refMenu.popup()
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
