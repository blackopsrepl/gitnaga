import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: pane
    required property var repository
    required property var colors
    color: colors.panel
    border.color: colors.border

    readonly property real rowHeight: 48

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Label { text: qsTr("History"); color: colors.text; font.pixelSize: 12; font.weight: Font.DemiBold }
            Item { Layout.fillWidth: true }
            Label { text: history.count + qsTr(" commits"); color: colors.muted; font.pixelSize: 12 }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: colors.border }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: history
                anchors.fill: parent
                model: repository.commits
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                currentIndex: -1
                reuseItems: true
                ScrollBar.vertical: ScrollBar {}

                delegate: Item {
                    required property int index
                    required property string subject
                    required property string author
                    required property string shortOid
                    required property string relativeDate
                    required property var refs
                    width: history.width
                    height: pane.rowHeight

                    Rectangle {
                        anchors.fill: parent
                        color: history.currentIndex === index ? "#202838" : mouse.containsMouse ? "#191e28" : "transparent"
                    }
                    Column {
                        anchors.left: parent.left
                        anchors.leftMargin: 150
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 5
                        Row {
                            width: parent.width
                            spacing: 8
                            Label {
                                width: Math.max(80, parent.width - refRow.width - 8)
                                text: subject
                                color: colors.text
                                elide: Text.ElideRight
                                font.pixelSize: 12
                                font.weight: Font.Medium
                            }
                            Row {
                                id: refRow
                                spacing: 4
                                Repeater {
                                    model: refs.slice(0, 2)
                                    delegate: Rectangle {
                                        required property string modelData
                                        height: 18
                                        width: refText.implicitWidth + 12
                                        color: "#243149"
                                        border.color: "#3b5279"
                                        Label { id: refText; anchors.centerIn: parent; text: modelData; color: "#b6ccfa"; font.pixelSize: 10 }
                                    }
                                }
                            }
                        }
                        Row {
                            spacing: 8
                            Label { text: shortOid; color: colors.accent; font.family: "monospace"; font.pixelSize: 11 }
                            Label { text: author; color: colors.muted; font.pixelSize: 11 }
                            Label { text: "·"; color: colors.muted }
                            Label { text: relativeDate; color: colors.muted; font.pixelSize: 11 }
                        }
                    }
                    HoverHandler { id: mouse }
                    TapHandler {
                        onTapped: {
                            history.currentIndex = index
                            repository.selectCommit(index)
                        }
                    }
                }
            }

            CommitGraph {
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                width: 150
                model: repository.commits
                contentY: history.contentY
                rowHeight: pane.rowHeight
                selectedRow: history.currentIndex
            }
        }
    }
}
