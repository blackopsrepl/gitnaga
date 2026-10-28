import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GitNaga

// The built-in conflict view: every conflict block of one file with ours and
// theirs side by side. Saving writes the chosen text and stages the file.
Dialog {
    id: dialog
    required property var repository
    property string path: ""
    property var document: ({})
    property var choices: []
    readonly property int total: document.conflicts || 0
    readonly property int resolved: {
        var count = 0
        for (var i = 0; i < choices.length; ++i)
            count += choices[i].length > 0 ? 1 : 0
        return count
    }

    parent: Overlay.overlay
    modal: true
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 80 : 1100, 1200)
    height: parent ? parent.height - 90 : 760
    padding: 0
    closePolicy: Popup.CloseOnEscape
    background: Rectangle { color: NagaTheme.raised; border.color: NagaTheme.border; radius: 6 }
    Overlay.modal: Rectangle { color: NagaTheme.tint(NagaTheme.background, 0.72) }

    function openFor(file) {
        path = file
        document = repository.operation.conflictDocument(file)
        var empty = []
        for (var i = 0; i < (document.conflicts || 0); ++i)
            empty.push("")
        choices = empty
        open()
    }
    function choose(index, choice) {
        var next = choices.slice()
        next[index] = choice
        choices = next
    }
    function chooseAll(choice) {
        var next = []
        for (var i = 0; i < total; ++i)
            next.push(choice)
        choices = next
    }
    // Plain text between conflicts is context, not a decision; a few lines of
    // it on each side are enough to place the block.
    function context(text, leading) {
        var lines = text.replace(/\r?\n$/, "").split("\n")
        if (lines.length <= 4)
            return lines.join("\n")
        var more = qsTr("… %1 more lines").arg(lines.length - 3)
        return leading ? more + "\n" + lines.slice(-3).join("\n") : lines.slice(0, 3).join("\n") + "\n" + more
    }

    contentItem: ColumnLayout {
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: NagaTheme.panel
            border.color: NagaTheme.border
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 10
                spacing: 8
                Text {
                    text: dialog.path
                    color: NagaTheme.text
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    elide: Text.ElideMiddle
                    Layout.maximumWidth: dialog.width * 0.45
                }
                Text {
                    visible: dialog.total > 0
                    text: qsTr("%1 of %2 conflicts resolved").arg(dialog.resolved).arg(dialog.total)
                    color: dialog.resolved === dialog.total ? NagaTheme.accent : NagaTheme.warning
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }
                Item { Layout.fillWidth: true }
                NagaButton { text: qsTr("All ours"); visible: dialog.total > 1; implicitHeight: 26; onClicked: dialog.chooseAll("ours") }
                NagaButton { text: qsTr("All theirs"); visible: dialog.total > 1; implicitHeight: 26; onClicked: dialog.chooseAll("theirs") }
            }
        }

        Text {
            visible: (dialog.document.error || "").length > 0 || (dialog.total === 0 && !dialog.document.error)
            Layout.fillWidth: true
            Layout.margins: 20
            text: dialog.document.error
                  || qsTr("No conflict markers left in this file. Mark it resolved from the banner once it looks right.")
            color: NagaTheme.muted
            wrapMode: Text.WordWrap
        }

        ListView {
            id: blocks
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 12
            clip: true
            spacing: 8
            model: dialog.document.blocks || []
            ScrollBar.vertical: ScrollBar {}
            delegate: Loader {
                id: entry
                required property var modelData
                required property int index
                width: blocks.width - 12
                sourceComponent: modelData.conflict ? conflictBlock : textBlock
                Component {
                    id: conflictBlock
                    ConflictBlockView {
                        block: entry.modelData
                        number: entry.modelData.index + 1
                        total: dialog.total
                        choice: dialog.choices[entry.modelData.index] || ""
                        oursName: dialog.repository.inProgress.oursName
                        theirsName: dialog.repository.inProgress.theirsName
                        onChosen: (choice) => dialog.choose(entry.modelData.index, choice)
                    }
                }
                Component {
                    id: textBlock
                    Text {
                        leftPadding: 10
                        text: dialog.context(entry.modelData.text, entry.index === 0)
                        color: NagaTheme.muted
                        font.family: "monospace"
                        font.pixelSize: 12
                        textFormat: Text.PlainText
                        wrapMode: Text.WrapAnywhere
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            color: NagaTheme.panel
            border.color: NagaTheme.border
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 10
                spacing: 8
                Text {
                    Layout.fillWidth: true
                    text: dialog.resolved < dialog.total
                          ? qsTr("Choose a side for every conflict to save. Saving writes the file and stages it.")
                          : qsTr("Saving writes the file and stages it as resolved.")
                    color: NagaTheme.muted
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
                NagaButton { text: qsTr("Cancel"); keepFocus: true; onClicked: dialog.close() }
                NagaButton {
                    text: qsTr("Save resolution")
                    keepFocus: true
                    active: enabled
                    enabled: dialog.total > 0 && dialog.resolved === dialog.total && !dialog.repository.busy
                    onClicked: {
                        dialog.repository.operation.saveConflictResolution(dialog.path, dialog.choices)
                        dialog.close()
                    }
                }
            }
        }
    }
}
