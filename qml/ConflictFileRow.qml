import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GitNaga

// One conflicted file in the operation banner and every way to settle it.
Rectangle {
    id: row
    required property var repository
    property string path: ""
    signal resolveRequested(string path)

    implicitHeight: 30
    color: hover.hovered ? NagaTheme.hoverFill : "transparent"
    radius: 3
    HoverHandler { id: hover }

    component SmallButton: NagaButton {
        implicitHeight: 22
        leftPadding: 7
        rightPadding: 7
        enabled: !row.repository.busy
        property string hint: ""
        ToolTip.visible: hovered && hint.length > 0
        ToolTip.text: hint
        ToolTip.delay: 400
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 2
        spacing: 4
        Text {
            text: "U"
            color: NagaTheme.danger
            font.pixelSize: 11
            font.weight: Font.Bold
            font.family: "monospace"
            Layout.preferredWidth: 12
        }
        Text {
            Layout.fillWidth: true
            text: row.path
            color: NagaTheme.text
            font.pixelSize: 12
            elide: Text.ElideMiddle
        }
        SmallButton {
            text: qsTr("Resolve…")
            active: true
            hint: qsTr("Pick ours or theirs block by block")
            onClicked: row.resolveRequested(row.path)
        }
        SmallButton {
            text: qsTr("Ours")
            hint: qsTr("Take %1's whole version of this file").arg(row.repository.inProgress.oursName)
            onClicked: row.repository.operation.resolveConflict(row.path, "ours")
        }
        SmallButton {
            text: qsTr("Theirs")
            hint: qsTr("Take %1's whole version of this file").arg(row.repository.inProgress.theirsName)
            onClicked: row.repository.operation.resolveConflict(row.path, "theirs")
        }
        SmallButton {
            text: qsTr("Mark resolved")
            hint: qsTr("Stage the file as it is in the working tree")
            onClicked: row.repository.operation.markResolved(row.path)
        }
        SmallButton {
            text: qsTr("Edit")
            hint: qsTr("Open in your editor")
            onClicked: row.repository.operation.openInEditor(row.path)
        }
        SmallButton {
            visible: row.repository.inProgress.mergeToolConfigured
            text: qsTr("Tool")
            hint: qsTr("Open in the configured merge tool")
            onClicked: row.repository.operation.openMergetool(row.path)
        }
    }
}
