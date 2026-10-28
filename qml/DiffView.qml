import QtQuick
import QtQuick.Controls
import QtCore
import GitNaga

// The diff of one file: a header with the file facts and view options, then
// either the lines (unified or side by side) or a card for files a line diff
// cannot show. A parent can put actions on every hunk header (`hunkActions`,
// answered through `hunkActionRequested`) and read the rows a person picked
// in the gutter (`selectedLines`, unified row indexes; `model.rowInfo(row)`
// names each one).
Rectangle {
    id: view
    required property var model
    // [{ action: "stage", label: qsTr("Stage hunk") }, …]
    property var hunkActions: []
    property var selectedLines: []
    signal hunkActionRequested(int hunkIndex, string action)

    readonly property bool sideBySide: settings.sideBySide
    readonly property bool wrap: settings.wrap
    readonly property alias textPalette: diffPalette
    readonly property string specialKind: model.special.kind || ""
    readonly property real gutterWidth: Math.max(3, String(model.maxLineNumber).length) * diffPalette.charWidth + 16
    readonly property real textPadding: 22
    // Width of the text column(s), and of the longest line in them.
    readonly property real textWidth: sideBySide ? (list.width - 2 * gutterWidth - 1) / 2 - textPadding
                                                 : list.width - 2 * gutterWidth - textPadding
    readonly property real contentWidth: model.maxColumns * diffPalette.charWidth + 16
    property real offset: 0

    color: NagaTheme.diffSurface
    clip: true

    Settings {
        id: settings
        category: "DiffView"
        property bool sideBySide: false
        property bool wrap: false
        property bool ignoreWhitespace: false
        property int contextLines: 3
    }
    Component.onCompleted: {
        model.ignoreWhitespace = settings.ignoreWhitespace
        model.contextLines = settings.contextLines
    }

    DiffPalette {
        id: diffPalette
        plain: NagaTheme.syntaxPlain
        keyword: NagaTheme.syntaxKeyword
        type: NagaTheme.syntaxType
        functionCall: NagaTheme.syntaxFunction
        string: NagaTheme.syntaxString
        number: NagaTheme.syntaxNumber
        constant: NagaTheme.syntaxConstant
        comment: NagaTheme.syntaxComment
        meta: NagaTheme.syntaxMeta
        addedWord: NagaTheme.diffAddedWord
        removedWord: NagaTheme.diffRemovedWord
        pixelSize: 12
    }

    // Selection is a set keyed by row; the revision makes bindings re-read it.
    property var selection: ({})
    property int selectionRevision: 0
    property int selectionAnchor: -1
    function isSelected(row) { return selectionRevision >= 0 && selection[row] === true }
    function toggleLine(row, extend) {
        const next = Object.assign({}, selection)
        if (extend && selectionAnchor >= 0) {
            for (let r = Math.min(selectionAnchor, row); r <= Math.max(selectionAnchor, row); ++r) {
                const kind = model.rowInfo(r).kind
                if (kind === 1 || kind === 2)
                    next[r] = true
            }
        } else {
            if (next[row])
                delete next[row]
            else
                next[row] = true
            selectionAnchor = row
        }
        selection = next
        selectedLines = Object.keys(next).map(Number).sort((a, b) => a - b)
        selectionRevision++
    }
    function clearSelection() {
        selection = {}
        selectedLines = []
        selectionAnchor = -1
        selectionRevision++
    }
    Connections {
        target: view.model
        function onDocumentChanged() {
            view.clearSelection()
            view.offset = 0
            list.positionViewAtBeginning()
        }
    }

    DiffFileHeader {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        model: view.model
        sideBySide: settings.sideBySide
        wrap: settings.wrap
        showLineOptions: view.specialKind === ""
        onSideBySideToggled: on => settings.sideBySide = on
        onWrapToggled: on => settings.wrap = on
        onIgnoreWhitespaceToggled: on => {
            settings.ignoreWhitespace = on
            view.model.ignoreWhitespace = on
        }
        onContextChosen: lines => {
            settings.contextLines = lines
            view.model.contextLines = lines
        }
    }

    ListView {
        id: list
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: hbar.visible ? hbar.top : parent.bottom
        visible: view.specialKind === "" && view.model.loaded
        model: view.sideBySide ? view.model.split : view.model
        delegate: view.sideBySide ? splitRow : unifiedRow
        clip: true
        reuseItems: true
        cacheBuffer: 400
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        footer: Item { width: 1; height: 12 }

        WheelHandler {
            // Horizontal wheels and shift+wheel pan the text, not the list.
            acceptedModifiers: Qt.ShiftModifier
            orientation: Qt.Vertical
            enabled: !view.wrap
            onWheel: event => view.panBy(-event.angleDelta.y / 2)
        }
        WheelHandler {
            orientation: Qt.Horizontal
            enabled: !view.wrap
            onWheel: event => view.panBy(-event.angleDelta.x / 2)
        }
    }
    function panBy(delta) {
        offset = Math.max(0, Math.min(contentWidth - textWidth, offset + delta))
    }

    ScrollBar {
        id: hbar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: view.sideBySide ? 0 : 2 * view.gutterWidth
        orientation: Qt.Horizontal
        visible: list.visible && !view.wrap && view.contentWidth > view.textWidth
        size: Math.min(1, view.textWidth / Math.max(1, view.contentWidth))
        position: view.offset / Math.max(1, view.contentWidth)
        onPositionChanged: if (pressed) view.offset = position * view.contentWidth
    }

    Component {
        id: unifiedRow
        DiffUnifiedRow { diffView: view }
    }
    Component {
        id: splitRow
        DiffSplitRow { diffView: view }
    }

    Loader {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        active: view.model.loaded && view.specialKind !== ""
        sourceComponent: view.specialKind === "image" ? imageCard : specialCard
    }
    Component {
        id: specialCard
        DiffSpecialCard { model: view.model }
    }
    Component {
        id: imageCard
        DiffImageCompare { model: view.model }
    }

    Text {
        anchors.centerIn: parent
        visible: !view.model.loaded
        text: qsTr("Select a file to see its changes")
        color: NagaTheme.muted
        font.pixelSize: 13
    }
}
