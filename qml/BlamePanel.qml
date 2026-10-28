import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GitNaga

// A file line by line with the commit that last changed each line. The
// gutter names a commit once per run of its lines; clicking it selects that
// commit in the graph, and the lines of the graph's selection stay marked.
SheetPanel {
    id: panel
    required property var repository
    required property var navigation
    readonly property var blame: navigation ? navigation.blame : null

    title: qsTr("Blame")
    subtitle: blame.path + "  ·  " + (blame.revision.length > 0 ? blame.revision.substring(0, 8) : qsTr("working copy"))
              + (blame.count > 0 ? "  ·  " + qsTr("%1 lines from %2 commits").arg(blame.count).arg(blame.commitCount) : "")
    onOpened: lines.forceActiveFocus()

    tools: [
        NagaButton {
            text: qsTr("File history")
            implicitHeight: 26
            onClicked: panel.navigation.showFileHistory(panel.blame.path, panel.blame.revision)
        }
    ]

    ListView {
        id: lines
        anchors.fill: parent
        model: panel.blame
        clip: true
        reuseItems: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        ScrollBar.horizontal: ScrollBar {}
        contentWidth: Math.max(width, widest)
        flickableDirection: Flickable.AutoFlickIfNeeded
        property real widest: width
        onCountChanged: if (count === 0) widest = width

        delegate: Item {
            id: row
            required property int line
            required property string text
            required property string oid
            required property string shortOid
            required property string author
            required property string age
            required property string summary
            required property bool groupStart
            required property int groupIndex
            required property bool uncommitted
            readonly property bool selectedCommit: oid.length > 0 && oid === panel.repository.selectedOid
            width: Math.max(lines.width, code.x + code.implicitWidth + 24)
            height: 19
            Component.onCompleted: lines.widest = Math.max(lines.widest, width)

            Rectangle {
                id: gutter
                width: 250
                height: parent.height
                color: row.selectedCommit ? NagaTheme.accentTint(gutterHover.hovered ? 0.26 : 0.18)
                     : gutterHover.hovered && row.oid.length > 0 ? NagaTheme.hoverFill
                     : row.groupIndex % 2 === 0 ? "#111827" : "#0c111b"
                Rectangle { visible: row.groupStart; width: parent.width; height: 1; color: "#232c3d" }
                Rectangle { visible: row.selectedCommit; width: 2; height: parent.height; color: NagaTheme.accent }
                RowLayout {
                    visible: row.groupStart
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 8
                    Text {
                        visible: !row.uncommitted
                        text: row.shortOid
                        color: "#a78bfa"
                        font.family: "monospace"
                        font.pixelSize: 11
                    }
                    Text {
                        Layout.fillWidth: true
                        text: row.author
                        color: row.uncommitted ? "#e6b45a" : "#c3cad8"
                        elide: Text.ElideRight
                        font.pixelSize: 11
                    }
                    Text {
                        text: row.age
                        color: row.uncommitted ? "#e6b45a" : NagaTheme.muted
                        font.pixelSize: 11
                    }
                }
                HoverHandler {
                    id: gutterHover
                    cursorShape: row.oid.length > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
                TapHandler {
                    onTapped: if (row.oid.length > 0) panel.repository.selectOid(row.oid)
                }
                ToolTip.visible: gutterHover.hovered && row.summary.length > 0
                ToolTip.delay: 400
                ToolTip.text: row.uncommitted ? qsTr("Not committed yet") : row.shortOid + "  " + row.summary
            }
            Rectangle {
                x: gutter.width
                width: parent.width - x
                height: parent.height
                color: row.selectedCommit ? NagaTheme.accentTint(0.07) : "transparent"
            }
            Text {
                id: number
                x: gutter.width
                width: 48
                height: parent.height
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignRight
                rightPadding: 8
                text: row.line
                color: "#5b6577"
                font.family: "monospace"
                font.pixelSize: 11
            }
            Rectangle { x: number.x + number.width; width: 1; height: parent.height; color: NagaTheme.border }
            Text {
                id: code
                x: number.x + number.width + 10
                height: parent.height
                verticalAlignment: Text.AlignVCenter
                text: row.text
                textFormat: Text.PlainText
                color: "#cbd0dc"
                font.family: "monospace"
                font.pixelSize: 12
            }
        }

        Label {
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 520)
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            visible: lines.count === 0
            text: panel.blame.loading ? qsTr("Reading blame…")
                : panel.blame.error.length > 0 ? panel.blame.error
                : qsTr("The file is empty.")
            color: panel.blame.error.length > 0 ? "#ff9aa5" : NagaTheme.muted
            font.pixelSize: 12
        }
    }
}
