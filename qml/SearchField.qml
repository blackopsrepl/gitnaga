import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GitNaga

// The graph's search field. Plain words match subject, author and sha;
// `path:`, `content:` and `author:` narrow the question. Enter walks the
// hits down the graph, Shift+Enter walks back up, Escape ends the search.
Rectangle {
    id: field
    required property var search
    signal finished()
    // Typing or holding a query: the header may make room for the field.
    readonly property bool engaged: input.activeFocus || input.text.length > 0

    function focusInput() {
        input.forceActiveFocus()
        input.selectAll()
    }
    function step(backwards) {
        // Enter straight after typing asks the pending question first; its
        // answer already lands on the first hit.
        if (search.runNow())
            return
        if (backwards)
            search.previous()
        else
            search.next()
    }

    implicitHeight: 24
    radius: 4
    color: NagaTheme.background
    border.color: input.activeFocus ? NagaTheme.focusStroke : NagaTheme.border

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 7
        anchors.rightMargin: 2
        spacing: 4

        NagaIcon {
            name: "search"
            color: input.activeFocus ? NagaTheme.text : NagaTheme.muted
            Layout.preferredWidth: 13
            Layout.preferredHeight: 13
        }
        TextField {
            id: input
            Layout.fillWidth: true
            Layout.fillHeight: true
            leftPadding: 2
            rightPadding: 2
            topPadding: 0
            bottomPadding: 0
            verticalAlignment: TextInput.AlignVCenter
            background: null
            color: NagaTheme.text
            placeholderText: qsTr("Search  (path: content: author:)")
            placeholderTextColor: "#5b6577"
            selectByMouse: true
            font.pixelSize: 12
            onTextEdited: field.search.query = text
            Keys.onReturnPressed: (event) => field.step(event.modifiers & Qt.ShiftModifier)
            Keys.onEnterPressed: (event) => field.step(event.modifiers & Qt.ShiftModifier)
            Keys.onEscapePressed: {
                field.search.clear()
                field.finished()
            }
        }
        Text {
            visible: text.length > 0
            text: field.search.running ? qsTr("searching…")
                : !field.search.active ? ""
                : field.search.error.length > 0 ? qsTr("error")
                : field.search.count === 0 ? qsTr("no matches")
                : field.search.position > 0 ? qsTr("%1 of %2").arg(field.search.position).arg(field.search.count)
                : qsTr("%1 matches").arg(field.search.count)
            color: field.search.active && field.search.count === 0 && !field.search.running ? "#ff9aa5" : NagaTheme.muted
            font.pixelSize: 11
            ToolTip.visible: errorHover.hovered && field.search.error.length > 0
            ToolTip.text: field.search.error
            HoverHandler { id: errorHover }
        }
        NagaIconButton {
            visible: field.search.active
            text: "↑"
            implicitWidth: 20
            implicitHeight: 20
            tooltip: qsTr("Previous match (Shift+Enter)")
            enabled: field.search.count > 0
            onClicked: field.search.previous()
        }
        NagaIconButton {
            visible: field.search.active
            text: "↓"
            implicitWidth: 20
            implicitHeight: 20
            tooltip: qsTr("Next match (Enter)")
            enabled: field.search.count > 0
            onClicked: field.search.next()
        }
        NagaIconButton {
            visible: input.text.length > 0
            text: "✕"
            implicitWidth: 20
            implicitHeight: 20
            tooltip: qsTr("Clear search (Escape)")
            onClicked: {
                field.search.clear()
                field.finished()
            }
        }
    }

    // The query can also be cleared from elsewhere (Escape in the graph).
    Connections {
        target: field.search
        function onQueryChanged() {
            if (input.text !== field.search.query)
                input.text = field.search.query
        }
    }
}
