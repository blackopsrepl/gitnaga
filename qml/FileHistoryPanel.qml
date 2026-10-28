import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GitNaga

// Every commit that touched one file, renames included, with that commit's
// diff of the file beside it. Selecting a commit here does not move the graph;
// "Show in graph" (or a double-click) does.
SheetPanel {
    id: panel
    required property var repository
    required property var navigation
    readonly property var history: navigation ? navigation.fileHistory : null
    readonly property string selectedOid: history.selectedRow >= 0 ? history.oidAt(history.selectedRow) : ""

    title: qsTr("File history")
    subtitle: history.path + (history.count > 0 ? "  ·  " + (history.count === 1 ? qsTr("1 commit") : qsTr("%1 commits").arg(history.count)) : "")
    onOpened: commits.forceActiveFocus()

    function showInGraph(row) {
        const oid = history.oidAt(row)
        if (oid.length > 0)
            repository.selectOid(oid)
    }

    tools: [
        NagaButton {
            text: qsTr("Blame")
            implicitHeight: 26
            enabled: panel.selectedOid.length > 0
            onClicked: panel.navigation.showBlame(panel.history.path, panel.selectedOid)
        },
        NagaButton {
            text: qsTr("Show in graph")
            implicitHeight: 26
            enabled: panel.selectedOid.length > 0
            onClicked: panel.showInGraph(panel.history.selectedRow)
        }
    ]

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        ListView {
            id: commits
            SplitView.preferredWidth: 300
            SplitView.minimumWidth: 220
            model: panel.history
            currentIndex: panel.history.selectedRow
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            Keys.onUpPressed: panel.history.select(Math.max(0, currentIndex - 1))
            Keys.onDownPressed: panel.history.select(Math.min(count - 1, currentIndex + 1))
            Keys.onReturnPressed: panel.showInGraph(currentIndex)

            delegate: Item {
                id: entry
                required property int index
                required property string shortOid
                required property string subject
                required property string author
                required property string date
                required property string status
                required property string path
                required property string oldPath
                readonly property bool current: index === commits.currentIndex
                width: commits.width
                height: oldPath.length > 0 ? 62 : 46

                Rectangle {
                    anchors.fill: parent
                    color: entry.current ? NagaTheme.accentTint(0.16) : rowHover.hovered ? NagaTheme.hoverFill : "transparent"
                }
                Rectangle { width: 2; height: parent.height; color: NagaTheme.accent; visible: entry.current }
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#161d2b" }
                HoverHandler { id: rowHover }
                TapHandler {
                    onTapped: panel.history.select(entry.index)
                    onDoubleTapped: panel.showInGraph(entry.index)
                }
                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    anchors.topMargin: 6
                    anchors.bottomMargin: 6
                    spacing: 2
                    RowLayout {
                        spacing: 6
                        Text {
                            text: entry.status
                            color: entry.status === "A" ? "#4ade80" : entry.status === "D" ? "#ff7d8b"
                                 : entry.status === "R" ? "#9fc2ff" : "#fbbf24"
                            font.bold: true
                            font.pixelSize: 11
                        }
                        Text {
                            Layout.fillWidth: true
                            text: entry.subject
                            color: entry.current ? "#ffffff" : NagaTheme.text
                            elide: Text.ElideRight
                            font.pixelSize: 12
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        textFormat: Text.StyledText
                        text: "<font color=\"#a78bfa\">" + entry.shortOid + "</font>&nbsp;&nbsp;"
                              + entry.author.replace(/&/g, "&amp;").replace(/</g, "&lt;") + "&nbsp;·&nbsp;" + entry.date
                        color: NagaTheme.muted
                        elide: Text.ElideRight
                        font.pixelSize: 10
                    }
                    Text {
                        visible: entry.oldPath.length > 0
                        Layout.fillWidth: true
                        text: qsTr("renamed from %1").arg(entry.oldPath)
                        color: "#9fc2ff"
                        elide: Text.ElideMiddle
                        font.pixelSize: 10
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                width: parent.width - 32
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                visible: commits.count === 0
                text: panel.history.loading ? qsTr("Reading history…")
                    : panel.history.error.length > 0 ? panel.history.error
                    : qsTr("No commit touches this file yet.")
                color: panel.history.error.length > 0 ? "#ff9aa5" : NagaTheme.muted
                font.pixelSize: 12
            }
        }

        DiffView {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 260
            model: panel.history.diffLines
        }
    }
}
