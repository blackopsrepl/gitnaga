import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GitNaga

Rectangle {
    id: pane
    required property var repository
    signal closeRequested()

    color: NagaTheme.panel

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: NagaTheme.panel
            border.color: NagaTheme.border
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 6
                Text {
                    text: qsTr("Review")
                    color: NagaTheme.text
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.4
                }
                Item { Layout.fillWidth: true }
                NagaIconButton {
                    text: "✕"
                    tooltip: qsTr("Close review")
                    onClicked: pane.closeRequested()
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: repository.selectedOid.length > 0 ? Math.min(150, details.implicitHeight + 20) : 62
            ColumnLayout {
                id: details
                anchors.fill: parent
                anchors.margins: 10
                spacing: 5
                Text {
                    text: repository.selectedOid.length > 0 ? repository.selectedSubject : qsTr("Select a commit to review")
                    color: repository.selectedOid.length > 0 ? NagaTheme.text : NagaTheme.muted
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Text {
                    visible: repository.selectedOid.length > 0
                    text: repository.selectedAuthor + "  ·  " + repository.selectedDate
                    color: NagaTheme.muted
                    font.pixelSize: 12
                }
                Text {
                    visible: repository.selectedBody.length > 0
                    text: repository.selectedBody
                    color: "#b5bbca"
                    wrapMode: Text.WordWrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Text {
                    visible: repository.selectedOid.length > 0
                    text: repository.selectedOid
                    color: "#34d399"
                    font.family: "monospace"
                    font.pixelSize: 11
                }
            }
        }
        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: NagaTheme.border }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            Rectangle {
                SplitView.preferredWidth: 200
                SplitView.minimumWidth: 150
                color: NagaTheme.panel
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0
                    Text {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 28
                        leftPadding: 10
                        verticalAlignment: Text.AlignVCenter
                        text: qsTr("Changed files")
                        color: NagaTheme.text
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }
                    ListView {
                        id: files
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: repository.changedFiles
                        currentIndex: count > 0 ? 0 : -1
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar {}
                        // Custom row rather than ItemDelegate: the control
                        // default painted its own highlight, which was the one
                        // surface whose hover and selection did not follow the
                        // shared interaction language.
                        delegate: Item {
                            id: fileRow
                            required property int index
                            required property string status
                            required property string fileName
                            required property string directory
                            width: files.width
                            height: 34
                            readonly property bool selected: index === files.currentIndex

                            Rectangle {
                                anchors.fill: parent
                                color: fileRow.selected ? NagaTheme.accentTint(0.16)
                                     : fileHover.hovered ? NagaTheme.hoverFill
                                     : "transparent"
                            }
                            Rectangle {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 2
                                color: NagaTheme.accent
                                visible: fileRow.selected
                            }
                            HoverHandler { id: fileHover }
                            TapHandler {
                                onTapped: {
                                    files.currentIndex = fileRow.index
                                    repository.selectFile(fileRow.index)
                                }
                            }
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 8
                                Text {
                                    text: fileRow.status
                                    color: fileRow.status === "A" ? "#4ade80" : fileRow.status === "D" ? "#ff7d8b" : "#fbbf24"
                                    font.bold: true
                                    font.pixelSize: 12
                                }
                                ColumnLayout {
                                    spacing: 1
                                    Layout.fillWidth: true
                                    Text { text: fileRow.fileName; color: NagaTheme.text; elide: Text.ElideMiddle; Layout.fillWidth: true; font.pixelSize: 12 }
                                    Text { visible: fileRow.directory.length > 0; text: fileRow.directory; color: NagaTheme.muted; elide: Text.ElideMiddle; Layout.fillWidth: true; font.pixelSize: 9 }
                                }
                            }
                        }
                    }
                }
            }

            ListView {
                id: diff
                SplitView.fillWidth: true
                SplitView.minimumWidth: 240
                model: repository.diffLines
                clip: true
                reuseItems: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                ScrollBar.horizontal: ScrollBar {}
                delegate: Rectangle {
                    id: diffRow
                    required property int kind
                    required property string text
                    required property var oldLine
                    required property var newLine
                    width: Math.max(diff.width, lineRow.implicitWidth + 24)
                    height: 22
                    color: kind === 1 ? "#12291f" : kind === 2 ? "#331a20" : kind === 4 ? "#182b3f" : "transparent"
                    Row {
                        id: lineRow
                        height: parent.height
                        Text { width: 40; text: oldLine || ""; color: "#5b6577"; horizontalAlignment: Text.AlignRight; rightPadding: 8; font.family: "monospace"; font.pixelSize: 11 }
                        Text { width: 40; text: newLine || ""; color: "#5b6577"; horizontalAlignment: Text.AlignRight; rightPadding: 8; font.family: "monospace"; font.pixelSize: 11 }
                        Rectangle { width: 1; height: parent.height; color: NagaTheme.border }
                        Text { leftPadding: 10; text: diffRow.text; color: kind === 1 ? "#8fe6b8" : kind === 2 ? "#ffadb6" : kind === 4 ? "#9fc2ff" : kind === 3 ? "#77839a" : "#cbd0dc"; font.family: "monospace"; font.pixelSize: 12 }
                    }
                }
            }
        }
    }
}
