import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: pane
    required property var repository
    required property var colors
    color: colors.backgroundColor

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: repository.selectedOid.length > 0 ? Math.min(150, details.implicitHeight + 20) : 68
            ColumnLayout {
                id: details
                anchors.fill: parent
                anchors.margins: 10
                spacing: 5
                Label {
                    text: repository.selectedOid.length > 0 ? repository.selectedSubject : qsTr("Select a commit")
                    color: colors.text
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                Label {
                    visible: repository.selectedOid.length > 0
                    text: repository.selectedAuthor + "  ·  " + repository.selectedDate
                    color: colors.muted
                    font.pixelSize: 12
                }
                Label {
                    visible: repository.selectedBody.length > 0
                    text: repository.selectedBody
                    color: "#b5bbca"
                    wrapMode: Text.WordWrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Label {
                    visible: repository.selectedOid.length > 0
                    text: repository.selectedOid
                    color: colors.accent
                    font.family: "monospace"
                    font.pixelSize: 11
                }
            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: colors.border }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            Rectangle {
                SplitView.preferredWidth: 220
                SplitView.minimumWidth: 160
                color: colors.panel
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0
                    Label {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        leftPadding: 8
                        verticalAlignment: Text.AlignVCenter
                        text: qsTr("Changed Files")
                        color: colors.text
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
                        ScrollBar.vertical: ScrollBar {}
                        delegate: ItemDelegate {
                            required property int index
                            required property string status
                            required property string fileName
                            required property string directory
                            width: files.width
                            highlighted: files.currentIndex === index
                            onClicked: { files.currentIndex = index; repository.selectFile(index) }
                            contentItem: RowLayout {
                                Label { text: status; color: status === "A" ? "#63d89b" : status === "D" ? "#ff7d8b" : "#f1c75b"; font.bold: true }
                                ColumnLayout {
                                    spacing: 1
                                    Layout.fillWidth: true
                                    Label { text: fileName; color: colors.text; elide: Text.ElideMiddle; Layout.fillWidth: true; font.pixelSize: 12 }
                                    Label { visible: directory.length > 0; text: directory; color: colors.muted; elide: Text.ElideMiddle; Layout.fillWidth: true; font.pixelSize: 9 }
                                }
                            }
                        }
                    }
                }
            }

            ListView {
                id: diff
                SplitView.fillWidth: true
                SplitView.minimumWidth: 260
                model: repository.diffLines
                clip: true
                reuseItems: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}
                ScrollBar.horizontal: ScrollBar {}
                delegate: Rectangle {
                    required property int kind
                    required property string text
                    required property var oldLine
                    required property var newLine
                    width: Math.max(diff.width, lineRow.implicitWidth + 24)
                    height: 22
                    color: kind === 1 ? "#132d25" : kind === 2 ? "#341b22" : kind === 4 ? "#1b2b45" : "transparent"
                    Row {
                        id: lineRow
                        height: parent.height
                        Label { width: 42; text: oldLine || ""; color: "#626b7d"; horizontalAlignment: Text.AlignRight; rightPadding: 8; font.family: "monospace"; font.pixelSize: 11 }
                        Label { width: 42; text: newLine || ""; color: "#626b7d"; horizontalAlignment: Text.AlignRight; rightPadding: 8; font.family: "monospace"; font.pixelSize: 11 }
                        Rectangle { width: 1; height: parent.height; color: colors.border }
                        Label { leftPadding: 10; text: parent.parent.text; color: kind === 1 ? "#9aebbf" : kind === 2 ? "#ffadb6" : kind === 4 ? "#9fc2ff" : kind === 3 ? "#77839a" : "#cbd0dc"; font.family: "monospace"; font.pixelSize: 12 }
                    }
                }
            }
        }
    }
}
