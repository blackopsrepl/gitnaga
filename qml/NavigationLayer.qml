import QtQuick
import QtQuick.Controls
import GitNaga

// Search, file history, blame and the command palette, with the actions and
// shortcuts that reach them. The window declares one of these and forwards
// the review's file menu requests to it.
Item {
    id: layer
    required property var repository
    required property var navigation
    required property var window
    required property var graphPane

    // The review list's selected file, at the reviewed commit (or the worktree).
    readonly property bool fileSelected: repository.selectedFile >= 0 && repository.selectedOid.length > 0

    function showFileMenu(path: string, scenePosition: point) {
        fileMenu.path = path
        fileMenu.popup(layer.mapFromItem(null, scenePosition.x, scenePosition.y))
    }
    function openPalette(initial) {
        palette.openPalette(initial)
    }

    Action { id: searchAction; text: qsTr("Search commits"); shortcut: "Ctrl+F"; enabled: layer.repository.repositoryPath.length > 0; onTriggered: { layer.window.showGraph(); layer.graphPane.focusSearch() } }
    Action { id: nextMatchAction; text: qsTr("Next search match"); shortcut: "F3"; enabled: layer.navigation.search.count > 0; onTriggered: layer.navigation.search.next() }
    Action { id: previousMatchAction; text: qsTr("Previous search match"); shortcut: "Shift+F3"; enabled: layer.navigation.search.count > 0; onTriggered: layer.navigation.search.previous() }
    Action { id: clearSearchAction; text: qsTr("Clear search"); enabled: layer.navigation.search.active; onTriggered: layer.navigation.search.clear() }
    Action { id: paletteAction; text: qsTr("Command palette"); shortcut: "Ctrl+K"; onTriggered: layer.openPalette("") }
    Action { id: fileHistoryAction; text: qsTr("File history"); shortcut: "Ctrl+Shift+H"; enabled: layer.fileSelected; onTriggered: layer.navigation.showFileHistory(layer.navigation.selectedPath(), layer.repository.selectedOid) }
    Action { id: blameAction; text: qsTr("Blame"); shortcut: "Ctrl+Shift+B"; enabled: layer.fileSelected; onTriggered: layer.navigation.showBlame(layer.navigation.selectedPath(), layer.repository.selectedOid) }
    Action { id: headAction; text: qsTr("Go to HEAD"); enabled: layer.repository.headOid.length > 0; onTriggered: layer.repository.selectOid(layer.repository.headOid) }
    Action { id: graphFocusAction; text: qsTr("Focus commit graph"); shortcut: "Ctrl+G"; onTriggered: { layer.window.showGraph(); layer.graphPane.focusGraph() } }
    // VS Code's palette chord, for hands that already know it.
    Shortcut { sequence: "Ctrl+Shift+P"; onActivated: layer.openPalette("") }

    CommandPalette {
        id: palette
        repository: layer.repository
        navigation: layer.navigation
        sources: [layer.window, layer]
    }

    FileHistoryPanel {
        id: fileHistoryPanel
        repository: layer.repository
        navigation: layer.navigation
    }

    BlamePanel {
        id: blamePanel
        repository: layer.repository
        navigation: layer.navigation
    }

    Connections {
        target: layer.navigation
        // One sheet at a time: blame opened from the history replaces it.
        function onFileHistoryRequested() {
            blamePanel.close()
            fileHistoryPanel.open()
        }
        function onBlameRequested() {
            fileHistoryPanel.close()
            blamePanel.open()
        }
    }

    NagaMenu {
        id: fileMenu
        property string path: ""
        // The menu sits in a zero-size layer, so it cannot size itself from it.
        width: 240
        NagaMenuItem {
            text: qsTr("File history")
            onTriggered: layer.navigation.showFileHistory(fileMenu.path, layer.repository.selectedOid)
        }
        NagaMenuItem {
            text: qsTr("Blame")
            onTriggered: layer.navigation.showBlame(fileMenu.path, layer.repository.selectedOid)
        }
        NagaMenuSeparator {}
        NagaMenuItem {
            text: qsTr("Copy path")
            onTriggered: layer.repository.copyToClipboard(fileMenu.path)
        }
    }
}
