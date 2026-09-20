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
           ? repository.repositoryName + " — GitNaga"
           : qsTr("GitNaga")

    readonly property color backgroundColor: "#0d0f14"
    readonly property color panel: "#141820"
    readonly property color raised: "#1b202b"
    readonly property color alternate: "#202631"
    readonly property color border: "#292f3d"
    readonly property color text: "#e8ebf2"
    readonly property color muted: "#8d95a7"
    readonly property color accent: "#78a9ff"
    color: backgroundColor

    palette.window: backgroundColor
    palette.windowText: text
    palette.base: panel
    palette.alternateBase: alternate
    palette.text: text
    palette.button: raised
    palette.buttonText: text
    palette.highlight: accent
    palette.highlightedText: "#08101f"
    palette.placeholderText: muted

    Action { id: openAction; text: qsTr("&Open Repository…"); shortcut: StandardKey.Open; onTriggered: repositoryDialog.open() }
    Action { id: refreshAction; text: qsTr("&Refresh"); shortcut: StandardKey.Refresh; enabled: repository.repositoryPath.length > 0 && !repository.loading; onTriggered: repository.refresh() }
    Action { id: quitAction; text: qsTr("&Quit"); shortcut: StandardKey.Quit; onTriggered: Qt.quit() }
    Action { id: focusHistoryAction; text: qsTr("Focus History"); shortcut: "Ctrl+1"; onTriggered: historyPane.forceActiveFocus() }
    Action { id: focusInspectorAction; text: qsTr("Focus Inspector"); shortcut: "Ctrl+2"; onTriggered: inspectorPane.forceActiveFocus() }

    menuBar: MenuBar {
        Menu {
            title: qsTr("&File")
            MenuItem { action: openAction }
            MenuSeparator {}
            MenuItem { action: quitAction }
        }
        Menu {
            title: qsTr("&Repository")
            MenuItem { action: refreshAction }
        }
        Menu {
            title: qsTr("&View")
            MenuItem { action: focusHistoryAction }
            MenuItem { action: focusInspectorAction }
        }
    }

    FolderDialog {
        id: repositoryDialog
        title: qsTr("Open Git Repository")
        onAccepted: repository.openRepository(selectedFolder)
    }

    header: ToolBar {
        height: 38
        background: Rectangle { color: root.raised; border.color: root.border }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 6
            anchors.rightMargin: 6
            spacing: 6

            NagaButton { action: openAction; implicitHeight: 28 }
            ToolSeparator {}
            Label {
                text: repository.repositoryName || qsTr("No repository open")
                font.weight: Font.DemiBold
                elide: Text.ElideMiddle
                Layout.maximumWidth: 280
            }
            Label {
                visible: repository.currentBranch.length > 0
                text: "⎇ " + repository.currentBranch
                color: root.muted
                font.pixelSize: 12
            }
            Item { Layout.fillWidth: true }
            BusyIndicator { running: repository.loading; visible: running; implicitWidth: 22; implicitHeight: 22 }
            NagaButton { action: refreshAction; implicitHeight: 28 }
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
            id: historyPane
            SplitView.preferredWidth: root.width * 0.52
            SplitView.minimumWidth: 480
            repository: root.repository
            colors: root
        }
        InspectorPane {
            id: inspectorPane
            SplitView.fillWidth: true
            SplitView.minimumWidth: 400
            repository: root.repository
            colors: root
        }
    }

    footer: Rectangle {
        height: 24
        color: root.raised
        border.color: root.border
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            Label { text: repository.repositoryPath; color: root.muted; font.pixelSize: 11; elide: Text.ElideMiddle; Layout.fillWidth: true }
            Label { text: repository.commits.count + qsTr(" commits loaded"); color: root.muted; font.pixelSize: 11 }
        }
    }

    Rectangle {
        visible: repository.errorMessage.length > 0 && repository.repositoryPath.length > 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: errorText.implicitHeight + 14
        color: "#6b242c"
        border.color: "#a84a55"
        Label {
            id: errorText
            anchors.centerIn: parent
            width: parent.width - 24
            text: repository.errorMessage
            color: "#ffffff"
            elide: Text.ElideRight
        }
    }
}
