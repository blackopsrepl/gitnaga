import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GitNaga

// Folder picker for opening a repository. Replaces the platform dialog so the
// surface matches the application instead of the system theme.
Dialog {
    id: dialog
    required property var repository

    property string currentPath: ""
    property var entries: []
    readonly property bool currentIsRepository: repository.looksLikeRepository(currentPath)

    signal acceptedPath(string path)

    modal: true
    focus: true
    width: 640
    height: 480
    padding: 0
    anchors.centerIn: parent
    closePolicy: Popup.CloseOnEscape
    background: Rectangle {
        color: "#0e1420"
        border.color: "#2b3242"
    }

    function openAt(path) {
        currentPath = path && path.length > 0 ? path : repository.homeDirectory()
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

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 8
            spacing: 6
            NagaButton {
                keepFocus: true
                text: qsTr("Up")
                implicitHeight: 26
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
                keepFocus: true
                text: qsTr("Go")
                implicitHeight: 26
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
                            color: "#34d399"
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
                    color: dialog.currentIsRepository ? "#34d399" : "#7c8698"
                    font.pixelSize: 11
                    elide: Text.ElideMiddle
                }
                NagaButton {
                    keepFocus: true
                    text: qsTr("Open Repository")
                    enabled: dialog.currentIsRepository
                    implicitHeight: 26
                    onClicked: {
                        dialog.acceptedPath(dialog.currentPath)
                        dialog.close()
                    }
                }
                NagaButton {
                    keepFocus: true
                    text: qsTr("Cancel")
                    implicitHeight: 26
                    onClicked: dialog.close()
                }
            }
        }
    }
}
