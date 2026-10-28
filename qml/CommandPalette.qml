import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GitNaga

// Every command in one fuzzy-filtered list. Commands are plain objects
// {label, hint, shortcut, enabled, run}; `sources` contributes them: each
// Action found among an object's `data` becomes one, so a feature that adds an
// Action to the window is in the palette without touching this file. Typing
// also offers every branch to check out, every tag and any commit to go to.
Popup {
    id: palette
    required property var repository
    required property var navigation
    // Objects whose `data` holds Actions (the window, the navigation layer).
    property var sources: []
    // Extra commands that are not Actions.
    property var commands: []

    property var entries: []
    property var shown: []

    parent: Overlay.overlay
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: 64
    width: 600
    height: Math.min(440, 52 + Math.max(1, shown.length) * 34 + 8)
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    Overlay.modal: Rectangle { color: "#66000000" }

    background: Rectangle {
        color: "#121826"
        border.color: "#2f3a50"
        radius: 6
    }

    function openPalette(initial) {
        entries = staticEntries()
        input.text = initial || ""
        refilter()
        open()
        input.forceActiveFocus()
    }

    function staticEntries() {
        const list = []
        for (const source of sources) {
            // A window keeps its declared objects on its content item.
            for (const item of (source.contentItem ? source.contentItem.data : source.data)) {
                if (!item || typeof item.trigger !== "function" || item.shortcut === undefined || !item.text)
                    continue
                const action = item
                list.push({ label: action.text.replace(/&/g, ""), hint: "", enabled: action.enabled,
                            shortcut: action.shortcut ? action.shortcut.toString() : "", run: () => action.trigger() })
            }
        }
        return list.concat(commands)
    }

    function dynamicEntries(needle) {
        const list = []
        const oid = navigation.resolveCommit(needle)
        if (oid.length > 0)
            list.push({ label: qsTr("Go to commit %1").arg(oid.substring(0, 10)), hint: qsTr("Commit"),
                        run: () => repository.selectOid(oid) })
        for (const ref of repository.references) {
            if (ref.kind === "local" && !ref.isHead)
                list.push({ label: qsTr("Checkout %1").arg(ref.name), hint: qsTr("Branch"),
                            run: () => repository.checkoutBranch(ref.name) })
            else if (ref.kind === "remote")
                list.push({ label: qsTr("Checkout %1").arg(ref.name), hint: qsTr("Remote branch"),
                            run: () => repository.checkoutRemoteBranch(ref.name) })
            else if (ref.kind === "tag")
                list.push({ label: qsTr("Go to %1").arg(ref.name), hint: qsTr("Tag"), run: () => repository.selectOid(ref.oid) })
        }
        return list
    }

    function refilter() {
        const needle = input.text.trim()
        const pool = needle.length > 0 ? entries.concat(dynamicEntries(needle)) : entries
        const order = navigation.rankCommands(pool.map(entry => entry.label), needle)
        const result = order.slice(0, 200).map(index => pool[index])
        // Searching the history for the words is always the last resort.
        if (needle.length > 0)
            result.push({ label: qsTr("Search commits for “%1”").arg(needle), hint: qsTr("Search"),
                          run: () => { navigation.search.query = needle; navigation.search.runNow() } })
        shown = result
        list.currentIndex = 0
    }

    function runCurrent() {
        const entry = shown[list.currentIndex]
        if (!entry || entry.enabled === false)
            return
        close()
        entry.run()
    }

    contentItem: ColumnLayout {
        spacing: 0
        TextField {
            id: input
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            leftPadding: 16
            rightPadding: 16
            placeholderText: qsTr("Type a command, a branch, a tag or a commit…")
            placeholderTextColor: "#5b6577"
            color: NagaTheme.text
            font.pixelSize: 15
            background: Rectangle {
                color: "transparent"
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#26304a" }
            }
            onTextEdited: palette.refilter()
            Keys.onUpPressed: list.currentIndex = Math.max(0, list.currentIndex - 1)
            Keys.onDownPressed: list.currentIndex = Math.min(list.count - 1, list.currentIndex + 1)
            Keys.onReturnPressed: palette.runCurrent()
            Keys.onEnterPressed: palette.runCurrent()
            Keys.onEscapePressed: palette.close()
        }
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 4
            Layout.bottomMargin: 4
            clip: true
            model: palette.shown
            boundsBehavior: Flickable.StopAtBounds
            onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
            ScrollBar.vertical: ScrollBar {}
            delegate: Item {
                id: entryRow
                required property int index
                required property var modelData
                readonly property bool current: index === list.currentIndex
                readonly property bool usable: modelData.enabled !== false
                width: list.width
                height: 34
                Rectangle {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    radius: 4
                    color: entryRow.current ? NagaTheme.accentTint(0.16) : entryHover.hovered ? NagaTheme.hoverFill : "transparent"
                }
                HoverHandler { id: entryHover }
                TapHandler {
                    onTapped: {
                        list.currentIndex = entryRow.index
                        palette.runCurrent()
                    }
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 18
                    spacing: 10
                    Text {
                        Layout.fillWidth: true
                        text: entryRow.modelData.label
                        color: !entryRow.usable ? "#5b6478" : entryRow.current ? "#ffffff" : "#dde3ee"
                        elide: Text.ElideRight
                        font.pixelSize: 13
                    }
                    Text {
                        visible: text.length > 0
                        text: entryRow.modelData.hint || ""
                        color: "#6f7a8f"
                        font.pixelSize: 11
                    }
                    Rectangle {
                        visible: shortcutText.text.length > 0
                        implicitWidth: shortcutText.implicitWidth + 12
                        implicitHeight: 18
                        radius: 3
                        color: "#1b2333"
                        border.color: "#2c3649"
                        Text {
                            id: shortcutText
                            anchors.centerIn: parent
                            text: entryRow.modelData.shortcut || ""
                            color: "#9aa4b8"
                            font.pixelSize: 10
                        }
                    }
                }
            }
            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                text: qsTr("Nothing matches")
                color: NagaTheme.muted
            }
        }
    }
}
