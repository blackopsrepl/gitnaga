import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: pane
    required property var repository
    required property var colors
    signal closeRequested()

    property var target: null

    color: colors.panel

    readonly property var sections: [
        { kind: "local", label: qsTr("Branches") },
        { kind: "remote", label: qsTr("Remotes") },
        { kind: "tag", label: qsTr("Tags") }
    ]

    function filtered(kind) {
        var out = []
        var refs = pane.repository.references
        var needle = filterField.text.toLowerCase()
        for (var i = 0; i < refs.length; ++i) {
            var reference = refs[i]
            if (reference.kind !== kind)
                continue
            if (needle.length > 0 && reference.name.toLowerCase().indexOf(needle) < 0)
                continue
            out.push(reference)
        }
        return out
    }

    function countOf(kind) {
        return filtered(kind).length
    }

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
                anchors.rightMargin: 6
                Text {
                    text: qsTr("References")
                    color: colors.text
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.4
                }
                Item { Layout.fillWidth: true }
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
            color: colors.panel
            TextField {
                id: filterField
                anchors.fill: parent
                anchors.margins: 4
                placeholderText: qsTr("Filter references")
                font.pixelSize: 11
                selectByMouse: true
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
                        width: sectionsColumn.width

                        Rectangle {
                            width: parent.width
                            height: 24
                            color: colors.panel
                            Row {
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 6
                                Text {
                                    text: sectionColumn.modelData.label
                                    color: colors.muted
                                    font.pixelSize: 10
                                    font.weight: Font.Bold
                                    font.letterSpacing: 0.8
                                }
                                Text {
                                    text: pane.countOf(sectionColumn.modelData.kind)
                                    color: colors.muted
                                    font.pixelSize: 10
                                }
                            }
                        }

                        Repeater {
                            model: pane.filtered(sectionColumn.modelData.kind)
                            delegate: Rectangle {
                                id: refRow
                                required property var modelData
                                width: sectionColumn.width
                                height: 26
                                color: refHover.hovered ? colors.alternate : "transparent"

                                HoverHandler { id: refHover }
                                TapHandler {
                                    onTapped: pane.repository.selectOid(refRow.modelData.oid)
                                }
                                TapHandler {
                                    acceptedButtons: Qt.RightButton
                                    onTapped: pane.openMenu(refRow.modelData)
                                }

                                Rectangle {
                                    width: 8
                                    height: 8
                                    anchors.left: parent.left
                                    anchors.leftMargin: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: refRow.modelData.color
                                }
                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 28
                                    anchors.right: parent.right
                                    anchors.rightMargin: 64
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: refRow.modelData.name
                                    color: colors.text
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                                Text {
                                    anchors.right: parent.right
                                    anchors.rightMargin: 10
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: refRow.modelData.shortOid
                                    color: colors.muted
                                    font.family: "monospace"
                                    font.pixelSize: 10
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Menu {
        id: refMenu
        background: Rectangle { color: "#12161f"; border.color: "#2b3242" }

        MenuItem {
            visible: pane.target !== null && pane.target.kind === "local"
            text: qsTr("Checkout ") + (pane.target ? pane.target.name : "")
            onTriggered: pane.repository.checkoutBranch(pane.target.name)
        }
        MenuSeparator { visible: pane.target !== null && pane.target.kind === "local" }
        MenuItem {
            visible: pane.target !== null && pane.target.kind === "local"
            text: qsTr("Delete branch ") + (pane.target ? pane.target.name : "") + qsTr("…")
            onTriggered: confirm.ask(qsTr("Force delete branch ") + pane.target.name + qsTr("?"),
                                     function() { pane.repository.deleteBranch(pane.target.name) })
        }
        MenuSeparator { visible: pane.target !== null && pane.target.kind === "local" }
        MenuItem {
            text: qsTr("Show commit ") + (pane.target ? pane.target.shortOid : "")
            onTriggered: pane.repository.selectOid(pane.target.oid)
        }
        MenuItem {
            text: qsTr("Checkout commit")
            onTriggered: pane.repository.checkoutCommit(pane.target.oid)
        }
        MenuItem {
            text: qsTr("Copy name")
            onTriggered: pane.repository.copyToClipboard(pane.target.name)
        }
    }

    function openMenu(reference) {
        if (!reference)
            return
        target = reference
        refMenu.popup()
    }

    ConfirmDialog {
        id: confirm
        property var onConfirm: null
        onConfirmed: if (onConfirm) onConfirm()
        function ask(message, callback) {
            confirm.message = message
            confirm.onConfirm = callback
            confirm.open()
        }
    }
}
