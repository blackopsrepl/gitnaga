import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GitNaga

// Folder picker and project switcher. The search field at the top filters the
// remembered projects (fuzzy subsequence match) and accepts a typed path; the
// directory browser below is for picking something that is not remembered yet.
Dialog {
    id: dialog
    required property var repository

    property string currentPath: ""
    property var entries: []
    readonly property bool currentIsRepository: repository.looksLikeRepository(currentPath)
    // Remembered projects that match the search, best first.
    readonly property var recentsModel: repository.fuzzyMatchProjects(searchField.text)
    signal acceptedPath(string path)

    modal: true
    focus: true
    width: 640
    height: 540
    padding: 0
    anchors.centerIn: parent
    closePolicy: Popup.CloseOnEscape
    background: Rectangle {
        color: "#0e1420"
        border.color: "#2b3242"
    }

    function openAt(path) {
        currentPath = path && path.length > 0 ? path : repository.homeDirectory()
        searchField.text = currentPath
        refresh()
        open()
    }
    function refresh() {
        entries = repository.directories(currentPath)
    }
    function navigateTo(path) {
        if (path && path.length > 0) {
            currentPath = path
            refresh()
        }
    }
    function goUp() {
        const slash = currentPath.lastIndexOf("/")
        navigateTo(slash > 0 ? currentPath.substring(0, slash) : "/")
    }
    function accept(path) {
        acceptedPath(path)
        close()
    }

    contentItem: ColumnLayout {
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: "#141b2a"
            border.color: "#222c40"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 6
                Text {
                    text: qsTr("Open Git Repository")
                    color: "#e8ebf2"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }
                Item { Layout.fillWidth: true }
                NagaIconButton {
                    text: "✕"
                    tooltip: qsTr("Close")
                    onClicked: dialog.close()
                }
            }
        }

        TextField {
            id: searchField
            Layout.fillWidth: true
            Layout.margins: 8
            placeholderText: qsTr("Search remembered projects, or type a path…")
            font.pixelSize: 12
            selectByMouse: true
            onTextChanged: dialog.navigateTo(text)
            onAccepted: {
                const matches = repository.fuzzyMatchProjects(text)
                if (matches.length > 0 && repository.looksLikeRepository(matches[0].path))
                    dialog.accept(matches[0].path)
                else if (dialog.currentIsRepository)
                    dialog.accept(dialog.currentPath)
                else
                    dialog.navigateTo(text)
            }
        }

        // Remembered projects that match the search, best first.
        ListView {
            id: recentsList
            model: dialog.recentsModel
            readonly property int matchCount: dialog.recentsModel.length
            visible: matchCount > 0
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(matchCount * 30, 4 * 30)
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            delegate: ItemDelegate {
                id: recentRow
                required property int index
                required property var modelData
                width: recentsList.width
                height: 30
                onClicked: dialog.accept(modelData.path)
                background: Rectangle {
                    color: recentRow.hovered ? NagaTheme.hoverFill : "transparent"
                }
                contentItem: RowLayout {
                    spacing: 8
                    Text {
                        text: "◆"
                        color: NagaTheme.accent
                        font.pixelSize: 10
                    }
                    Text {
                        Layout.fillWidth: true
                        text: recentRow.modelData.path
                        color: "#dbe1ec"
                        font.pixelSize: 12
                        elide: Text.ElideMiddle
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            Layout.margins: 8
            color: NagaTheme.border
            visible: recentsList.visible
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 8
            spacing: 6
            NagaButton {
                text: qsTr("Up")
                implicitHeight: 26
                keepFocus: true
                onClicked: dialog.goUp()
            }
            TextField {
                id: pathField
                Layout.fillWidth: true
                text: dialog.currentPath
                selectByMouse: true
                font.pixelSize: 12
                onAccepted: dialog.navigateTo(text)
            }
            NagaButton {
                text: qsTr("Go")
                implicitHeight: 26
                keepFocus: true
                onClicked: dialog.navigateTo(pathField.text)
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            color: "#0b1120"
            border.color: "#222c40"

            ListView {
                id: listing
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                model: dialog.entries
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                delegate: Item {
                    id: entry
                    required property var modelData
                    width: listing.width
                    height: 26

                    Rectangle {
                        anchors.fill: parent
                        color: entryHover.hovered ? NagaTheme.hoverFill : "transparent"
                    }
                    HoverHandler { id: entryHover }
                    TapHandler {
                        onTapped: dialog.navigateTo(entry.modelData.path)
                    }
                    Row {
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 8
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "▸"
                            visible: entry.modelData.repository
                            color: NagaTheme.accent
                            font.pixelSize: 11
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: entry.modelData.name
                            color: "#dbe1ec"
                            font.pixelSize: 12
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            Layout.topMargin: 8
            color: "#141b2a"
            border.color: "#222c40"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8
                Text {
                    Layout.fillWidth: true
                    text: dialog.currentIsRepository ? qsTr("Git repository") : qsTr("Not a Git repository")
                    color: dialog.currentIsRepository ? NagaTheme.accent : "#7c8698"
                    font.pixelSize: 11
                    elide: Text.ElideMiddle
                }
                NagaButton {
                    text: qsTr("Open Repository")
                    enabled: dialog.currentIsRepository
                    implicitHeight: 26
                    keepFocus: true
                    onClicked: {
                        dialog.accept(dialog.currentPath)
                    }
                }
                NagaButton {
                    text: qsTr("Cancel")
                    implicitHeight: 26
                    keepFocus: true
                    onClicked: dialog.close()
                }
            }
        }
    }
}
