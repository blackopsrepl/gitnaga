import QtQuick
import GitNaga

// The middle of the window: the commit graph, or the diff of a file a person
// picked, in its place. Another commit, a blame or history sheet (whose rows
// select commits in the graph), Back or Escape return to the graph.
Item {
    id: center
    required property var repository
    required property var navigation
    // Which list the shown file came from, for staging in the diff.
    property string side: ""
    property bool diffOpen: false
    readonly property alias graph: graphPane
    signal focusReviewRequested()

    property string diffOid: ""
    function openDiff() {
        diffOid = repository.selectedOid
        diffOpen = true
    }

    Connections {
        target: center.repository
        function onSelectionChanged() {
            if (center.repository.selectedOid !== center.diffOid)
                center.diffOpen = false
        }
    }
    Connections {
        target: center.navigation
        function onFileHistoryRequested() { center.diffOpen = false }
        function onBlameRequested() { center.diffOpen = false }
    }
    Shortcut {
        sequence: "Escape"
        enabled: center.diffOpen
        onActivated: center.diffOpen = false
    }

    GraphPane {
        id: graphPane
        anchors.fill: parent
        visible: !center.diffOpen
        repository: center.repository
        navigation: center.navigation
        onFocusReviewRequested: center.focusReviewRequested()
    }
    CenterDiff {
        anchors.fill: parent
        visible: center.diffOpen
        repository: center.repository
        side: center.side
        onCloseRequested: center.diffOpen = false
    }
}
