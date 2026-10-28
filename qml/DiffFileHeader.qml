import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GitNaga

// The diff's file header: what changed (path, rename, mode, +/− counts) and
// how to look at it (unified or split, wrap, whitespace, context).
Rectangle {
    id: header
    required property var model
    property bool sideBySide: false
    property bool wrap: false
    property bool showLineOptions: true
    signal sideBySideToggled(bool on)
    signal wrapToggled(bool on)
    signal ignoreWhitespaceToggled(bool on)
    signal contextChosen(int lines)

    readonly property var file: model.file
    readonly property string path: file.path || ""
    readonly property string fileName: path.substring(path.lastIndexOf("/") + 1)
    readonly property string directory: path.lastIndexOf("/") >= 0 ? path.substring(0, path.lastIndexOf("/") + 1) : ""
    readonly property string status: file.added ? qsTr("Added") : file.deleted ? qsTr("Deleted")
                                   : file.renamed ? qsTr("Renamed") : file.copied ? qsTr("Copied") : qsTr("Modified")
    readonly property color statusColour: file.added ? NagaTheme.diffAddedInk : file.deleted ? NagaTheme.diffRemovedInk
                                        : file.renamed || file.copied ? NagaTheme.syntaxFunction : NagaTheme.diffNotice
    readonly property bool modeChanged: (file.oldMode || "").length > 0 && (file.newMode || "").length > 0
                                        && file.oldMode !== file.newMode
    readonly property var details: {
        const parts = []
        if ((file.renamed || file.copied) && (file.oldPath || "").length > 0)
            parts.push((file.copied ? qsTr("Copied from ") : qsTr("Renamed from ")) + file.oldPath
                       + (file.similarity >= 0 ? qsTr("  ·  %1% similar").arg(file.similarity) : ""))
        if (modeChanged)
            parts.push(qsTr("Mode %1 → %2").arg(file.oldMode).arg(file.newMode)
                       + (file.newMode === "100755" ? qsTr(" (executable)") : ""))
        return parts
    }

    visible: model.loaded
    height: visible ? column.implicitHeight + 14 : 0
    color: NagaTheme.panel
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: NagaTheme.border }

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 10
        anchors.topMargin: 8
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Rectangle {
                implicitWidth: statusLabel.implicitWidth + 12
                implicitHeight: 18
                radius: 3
                color: Qt.rgba(header.statusColour.r, header.statusColour.g, header.statusColour.b, 0.14)
                border.color: Qt.rgba(header.statusColour.r, header.statusColour.g, header.statusColour.b, 0.45)
                Text {
                    id: statusLabel
                    anchors.centerIn: parent
                    text: header.status
                    color: header.statusColour
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.3
                }
            }
            // The name always reads in full; the directory gives way first.
            Text {
                Layout.preferredWidth: implicitWidth
                Layout.minimumWidth: Math.min(implicitWidth, 90)
                textFormat: Text.PlainText
                text: header.fileName
                elide: Text.ElideRight
                color: NagaTheme.text
                font.pixelSize: 13
                font.weight: Font.DemiBold
                ToolTip.visible: pathHover.hovered
                ToolTip.text: header.path
                HoverHandler { id: pathHover }
            }
            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 0
                visible: header.directory.length > 0
                textFormat: Text.PlainText
                text: header.directory
                elide: Text.ElideLeft
                color: NagaTheme.muted
                font.pixelSize: 11
            }
            Text {
                visible: (header.file.additions || 0) > 0 || (header.file.deletions || 0) > 0
                text: "+" + (header.file.additions || 0)
                color: NagaTheme.diffAddedInk
                font.family: "monospace"
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }
            Text {
                visible: (header.file.additions || 0) > 0 || (header.file.deletions || 0) > 0
                text: "−" + (header.file.deletions || 0)
                color: NagaTheme.diffRemovedInk
                font.family: "monospace"
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }
            NagaButton {
                id: copyButton
                property bool copied: false
                implicitHeight: 22
                leftPadding: 8
                rightPadding: 8
                text: copied ? qsTr("Copied") : qsTr("Copy path")
                active: copied
                onClicked: {
                    header.model.copyPath()
                    copied = true
                    copiedTimer.restart()
                }
                Timer { id: copiedTimer; interval: 1400; onTriggered: copyButton.copied = false }
            }
        }

        Repeater {
            model: header.details
            delegate: Text {
                required property string modelData
                Layout.fillWidth: true
                text: modelData
                elide: Text.ElideMiddle
                color: NagaTheme.muted
                font.pixelSize: 11
            }
        }

        // Groups wrap onto a second line rather than clip in a narrow pane.
        Flow {
            Layout.fillWidth: true
            visible: header.showLineOptions
            spacing: 12
            Row {
                spacing: 4
                DiffChip { text: qsTr("Unified"); active: !header.sideBySide; onClicked: header.sideBySideToggled(false) }
                DiffChip { text: qsTr("Split"); active: header.sideBySide; onClicked: header.sideBySideToggled(true) }
            }
            Row {
                spacing: 4
                DiffChip { text: qsTr("Wrap"); active: header.wrap; onClicked: header.wrapToggled(!header.wrap) }
                DiffChip {
                    text: qsTr("Ignore whitespace")
                    active: header.model.ignoreWhitespace
                    onClicked: header.ignoreWhitespaceToggled(!header.model.ignoreWhitespace)
                }
            }
            Row {
                spacing: 4
                Text {
                    height: parent.height
                    verticalAlignment: Text.AlignVCenter
                    text: qsTr("Context")
                    color: NagaTheme.muted
                    font.pixelSize: 11
                    rightPadding: 2
                }
                Repeater {
                    model: [{ label: "3", lines: 3 }, { label: "10", lines: 10 }, { label: qsTr("File"), lines: -1 }]
                    delegate: DiffChip {
                        required property var modelData
                        text: modelData.label
                        active: header.model.contextLines === modelData.lines
                        onClicked: header.contextChosen(modelData.lines)
                    }
                }
            }
        }
    }
}
