import QtQuick
import QtQuick.Layouts
import GitNaga
import "DiffFormat.js" as DiffFormat

// The card shown instead of lines for a file a line diff cannot describe:
// a binary blob, a submodule pointer, an LFS pointer, or a change with no
// lines at all (mode only, or whitespace hidden by the option).
Item {
    id: card
    required property var model
    readonly property var special: model.special
    readonly property var file: model.file
    readonly property string kind: special.kind || ""

    function bytes(size) {
        return DiffFormat.bytes(size) || qsTr("none")
    }
    function shortOid(oid) {
        if (!oid)
            return ""
        const colon = oid.indexOf(":")
        const hash = oid.substring(colon + 1)
        return oid.substring(0, colon + 1) + hash.substring(0, 12) + "…" + hash.substring(hash.length - 6)
    }
    readonly property string title: {
        switch (kind) {
        case "binary": return file.added ? qsTr("Binary file added") : file.deleted ? qsTr("Binary file deleted")
                                         : qsTr("Binary file changed")
        case "submodule": return qsTr("Submodule %1").arg(file.path)
        case "lfs": return qsTr("Git LFS object")
        default: return qsTr("No content changes")
        }
    }
    readonly property string summary: {
        switch (kind) {
        case "binary":
            if (file.added) return bytes(special.newSize)
            if (file.deleted) return bytes(special.oldSize)
            return bytes(special.oldSize) + "  →  " + bytes(special.newSize)
        case "submodule":
            if (!special.from) return qsTr("Added at %1").arg(special.to.substring(0, 7))
            if (!special.to) return qsTr("Removed (was %1)").arg(special.from.substring(0, 7))
            return special.from.substring(0, 7) + "  →  " + special.to.substring(0, 7)
        case "lfs":
            return qsTr("The pointer file is tracked in Git; the content lives in LFS storage.")
        default:
            return model.ignoreWhitespace ? qsTr("Only whitespace changed, and Ignore whitespace is on.")
                 : (file.oldMode && file.newMode && file.oldMode !== file.newMode) ? qsTr("Only the file mode changed.")
                 : qsTr("Git reports no line changes for this file.")
        }
    }

    Rectangle {
        id: panel
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 28
        width: Math.min(parent.width - 40, 560)
        height: content.implicitHeight + 36
        radius: 8
        color: NagaTheme.raised
        border.color: NagaTheme.border

        ColumnLayout {
            id: content
            anchors.fill: parent
            anchors.margins: 18
            spacing: 10

            Text {
                id: titleText
                Layout.fillWidth: true
                text: card.title
                color: NagaTheme.text
                font.pixelSize: 15
                font.weight: Font.DemiBold
                elide: Text.ElideMiddle
            }
            Text {
                Layout.fillWidth: true
                text: card.summary
                color: card.kind === "lfs" || card.kind === "empty" ? NagaTheme.muted : NagaTheme.syntaxPlain
                font.family: card.kind === "binary" || card.kind === "submodule" ? "monospace" : titleText.font.family
                font.pixelSize: card.kind === "binary" || card.kind === "submodule" ? 13 : 12
                wrapMode: Text.WordWrap
            }
            NagaButton {
                visible: card.kind === "empty" && card.model.ignoreWhitespace
                text: qsTr("Show whitespace changes")
                onClicked: card.model.ignoreWhitespace = false
            }

            // LFS: the object before and after.
            Repeater {
                model: card.kind !== "lfs" ? [] : [
                    { label: qsTr("Before"), oid: card.special.lfsOldOid, size: card.special.lfsOldSize, colour: NagaTheme.diffRemovedInk },
                    { label: qsTr("After"), oid: card.special.lfsNewOid, size: card.special.lfsNewSize, colour: NagaTheme.diffAddedInk }
                ].filter(side => side.oid !== undefined)
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 10
                    Text { text: modelData.label; color: modelData.colour; font.pixelSize: 11; Layout.preferredWidth: 46 }
                    Text {
                        Layout.fillWidth: true
                        text: card.shortOid(modelData.oid)
                        color: NagaTheme.syntaxPlain
                        font.family: "monospace"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Text { text: card.bytes(modelData.size); color: NagaTheme.muted; font.family: "monospace"; font.pixelSize: 12 }
                }
            }

            // Submodule: the commits the pointer moved across.
            Text {
                visible: card.kind === "submodule" && card.special.dirty
                text: qsTr("The submodule has uncommitted changes of its own.")
                color: NagaTheme.diffNotice
                font.pixelSize: 12
            }
            Text {
                visible: card.kind === "submodule" && (card.special.commits || []).length > 0
                text: qsTr("%n commit(s)", "", (card.special.commits || []).length)
                color: NagaTheme.muted
                font.pixelSize: 11
                font.weight: Font.DemiBold
                topPadding: 4
            }
            Repeater {
                model: card.kind === "submodule" ? (card.special.commits || []) : []
                delegate: RowLayout {
                    required property string modelData
                    Layout.fillWidth: true
                    spacing: 10
                    Text {
                        text: modelData.substring(0, modelData.indexOf(" "))
                        color: NagaTheme.accent
                        font.family: "monospace"
                        font.pixelSize: 12
                    }
                    Text {
                        Layout.fillWidth: true
                        text: modelData.substring(modelData.indexOf(" ") + 1)
                        color: NagaTheme.text
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }
            }
            Text {
                visible: card.kind === "submodule" && !card.special.checkedOut
                Layout.fillWidth: true
                text: qsTr("The submodule is not checked out, so its commit subjects are not available.")
                color: NagaTheme.muted
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }
    }
}
