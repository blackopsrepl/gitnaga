import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: root
    required property var repository

    width: 1440
    height: 860
    minimumWidth: 960
    minimumHeight: 620
    visible: true
    title: repository.repositoryName.length > 0
           ? repository.repositoryName + " · GitNaga"
           : qsTr("GitNaga")
    color: "#0d0f14"

    readonly property color panel: "#141820"
    readonly property color raised: "#1b202b"
    readonly property color border: "#292f3d"
    readonly property color text: "#e8ebf2"
    readonly property color muted: "#8d95a7"
    readonly property color accent: "#78a9ff"

    palette.window: color
    palette.windowText: text
    palette.base: panel
    palette.text: text
    palette.button: raised
    palette.buttonText: text
    palette.highlight: accent
    palette.highlightedText: "#08101f"

    FolderDialog {
        id: repositoryDialog
        title: qsTr("Open a Git repository")
        onAccepted: repository.openRepository(selectedFolder)
    }

    header: ToolBar {
        height: 52
        background: Rectangle {
            color: root.panel
            border.color: root.border
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 12

            Label {
                text: "GITNAGA"
                color: root.accent
                font.pixelSize: 13
                font.weight: Font.Bold
                font.letterSpacing: 1.8
            }
            ToolSeparator {}
            NagaButton { text: qsTr("Open"); onClicked: repositoryDialog.open() }
            Label {
                text: repository.repositoryName || qsTr("No repository")
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }
            Rectangle {
                visible: repository.currentBranch.length > 0
                implicitWidth: branchLabel.implicitWidth + 16
                implicitHeight: 26
                color: "#202b3d"
                border.color: "#344768"
                Label {
                    id: branchLabel
                    anchors.centerIn: parent
                    text: "⎇  " + repository.currentBranch
                    color: "#a9c5ff"
                    font.pixelSize: 12
                }
            }
            Item { Layout.fillWidth: true }
            BusyIndicator { running: repository.loading; visible: running; implicitWidth: 28; implicitHeight: 28 }
            NagaButton {
                text: qsTr("Refresh")
                enabled: repository.repositoryPath.length > 0 && !repository.loading
                onClicked: repository.refresh()
            }
        }
    }

    EmptyState {
        anchors.fill: parent
        visible: repository.repositoryPath.length === 0
        errorMessage: repository.errorMessage
        onOpenRequested: repositoryDialog.open()
    }

    SplitView {
        anchors.fill: parent
        visible: repository.repositoryPath.length > 0
        orientation: Qt.Horizontal

        HistoryPane {
            SplitView.preferredWidth: root.width * 0.52
            SplitView.minimumWidth: 480
            repository: root.repository
            colors: root
        }
        InspectorPane {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 400
            repository: root.repository
            colors: root
        }
    }

    Rectangle {
        visible: repository.errorMessage.length > 0 && repository.repositoryPath.length > 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: errorText.implicitHeight + 20
        color: "#4b2028"
        border.color: "#8e4050"
        Label {
            id: errorText
            anchors.centerIn: parent
            width: parent.width - 32
            text: repository.errorMessage
            color: "#ffd6dc"
            elide: Text.ElideRight
        }
    }
}
