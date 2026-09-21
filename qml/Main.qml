import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GitNaga

ApplicationWindow {
    id: root
    required property var repository

    width: 1480
    height: 900
    minimumWidth: 980
    minimumHeight: 620
    visible: true
    title: repository.repositoryName.length > 0
           ? repository.repositoryName + " · GitNaga"
           : qsTr("GitNaga")

    property bool referencesVisible: true
    property bool reviewVisible: true
    property bool lastOperationOk: true

    // Every colour comes from the theme, including the interaction tokens, so
    // hover and selection cannot drift apart between surfaces.
    color: NagaTheme.background

    palette.window: NagaTheme.background
    palette.windowText: NagaTheme.text
    palette.base: NagaTheme.panel
    palette.alternateBase: NagaTheme.alternate
    palette.text: NagaTheme.text
    palette.button: NagaTheme.raised
    palette.buttonText: NagaTheme.text
    palette.highlight: NagaTheme.accent
    palette.highlightedText: "#04160f"
    palette.placeholderText: NagaTheme.muted

    onReferencesVisibleChanged: toggleReferencesAction.checked = referencesVisible
    onReviewVisibleChanged: toggleReviewAction.checked = reviewVisible

    function step(delta) {
        var row = repository.selectedRow + delta
        if (row >= 0 && row < repository.commits.count)
            repository.selectCommit(row)
    }

    Action { id: openAction; text: qsTr("&Open Repository…"); shortcut: "Ctrl+O"; onTriggered: repositoryDialog.openAt(root.repository.repositoryPath) }
    Action { id: refreshAction; text: qsTr("&Refresh"); shortcut: "Ctrl+R"; enabled: repository.repositoryPath.length > 0 && !repository.busy; onTriggered: repository.refresh() }
    Action { id: quitAction; text: qsTr("&Quit"); shortcut: "Ctrl+Q"; onTriggered: Qt.quit() }
    Action { id: toggleReferencesAction; text: qsTr("References sidebar"); checkable: true; checked: true; shortcut: "Ctrl+1"; onTriggered: referencesVisible = checked }
    Action { id: toggleReviewAction; text: qsTr("Review sidebar"); checkable: true; checked: true; shortcut: "Ctrl+2"; onTriggered: reviewVisible = checked }
    Action { id: zoomInAction; text: qsTr("Zoom graph in"); shortcut: "Ctrl+="; onTriggered: graphPane.zoomIn() }
    Action { id: zoomOutAction; text: qsTr("Zoom graph out"); shortcut: "Ctrl+-"; onTriggered: graphPane.zoomOut() }
    Action { id: resetZoomAction; text: qsTr("Reset graph zoom"); shortcut: "Ctrl+0"; onTriggered: graphPane.resetZoom() }
    Action { id: previousAction; text: qsTr("Previous commit"); shortcut: "Alt+Up"; enabled: repository.selectedRow > 0; onTriggered: root.step(-1) }
    Action { id: nextAction; text: qsTr("Next commit"); shortcut: "Alt+Down"; enabled: repository.selectedRow + 1 < repository.commits.count; onTriggered: root.step(1) }

    // The classic menu bar is gone on purpose: every command lives in the
    // toolbar, the context menus, or its keyboard shortcut. The actions below
    // stay declared at window scope so the shortcuts keep working.

    OpenRepositoryDialog {
        id: repositoryDialog
        repository: root.repository
        onAcceptedPath: (path) => root.repository.openRepositoryPath(path)
    }

    header: ToolBar {
        height: 40
        background: Rectangle { color: NagaTheme.raised; border.color: NagaTheme.border }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 6
            anchors.rightMargin: 6
            spacing: 6

            NagaIconButton {
                action: openAction
                iconName: "folder-open"
                tooltip: qsTr("Open repository… (Ctrl+O)")
                implicitHeight: 26
            }
            ToolSeparator {}
            Label {
                text: repository.repositoryName || qsTr("No repository open")
                font.weight: Font.DemiBold
                elide: Text.ElideMiddle
                Layout.maximumWidth: 240
            }
            Label {
                visible: repository.currentBranch.length > 0
                text: "⎇ " + repository.currentBranch
                color: NagaTheme.accent
                font.pixelSize: 12
            }
            Label {
                id: wipBadge
                visible: repository.workInProgressOid.length > 0
                text: qsTr("● Work in progress")
                color: "#e6b45a"
                font.pixelSize: 12
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: repository.selectCommit(0)
                }
            }
            Item { Layout.fillWidth: true }
            NagaButton {
                text: qsTr("References")
                active: root.referencesVisible
                implicitHeight: 26
                onClicked: root.referencesVisible = !root.referencesVisible
            }
            NagaButton {
                text: qsTr("Review")
                active: root.reviewVisible
                implicitHeight: 26
                onClicked: root.reviewVisible = !root.reviewVisible
            }
            BusyIndicator { running: repository.busy; visible: running; implicitWidth: 22; implicitHeight: 22 }
            NagaIconButton {
                action: refreshAction
                iconName: "refresh"
                tooltip: qsTr("Refresh (Ctrl+R)")
                implicitHeight: 26
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

        ReferencesPane {
            id: referencesPane
            visible: root.referencesVisible
            SplitView.preferredWidth: root.width * 0.24
            SplitView.minimumWidth: 200
            repository: root.repository
            onCloseRequested: root.referencesVisible = false
        }
        GraphPane {
            id: graphPane
            SplitView.fillWidth: true
            SplitView.minimumWidth: 380
            repository: root.repository
        }
        InspectorPane {
            id: reviewPane
            visible: root.reviewVisible
            SplitView.preferredWidth: root.width * 0.38
            SplitView.minimumWidth: 320
            repository: root.repository
            onCloseRequested: root.reviewVisible = false
        }
    }

    footer: Rectangle {
        height: 26
        color: NagaTheme.raised
        border.color: NagaTheme.border
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 10
            Label {
                text: repository.repositoryPath
                color: NagaTheme.muted
                font.pixelSize: 11
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }
            Label {
                visible: repository.operationMessage.length > 0
                text: repository.operationMessage
                color: root.lastOperationOk ? "#4ade80" : "#ff7d8b"
                font.pixelSize: 11
                elide: Text.ElideRight
            }
            Label {
                text: repository.commitCount + qsTr(" commits")
                      + (repository.workInProgressOid.length > 0 ? qsTr(" · work in progress") : "")
                color: NagaTheme.muted
                font.pixelSize: 11
            }
        }
    }

    Connections {
        target: root.repository
        function onOperationFinished(ok, message) {
            root.lastOperationOk = ok
        }
    }

    Rectangle {
        visible: repository.errorMessage.length > 0 && repository.repositoryPath.length > 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: errorText.implicitHeight + 14
        color: "#5c1f28"
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
