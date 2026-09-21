import QtQuick
import GitNaga

// The label overlay that rides the graph. It follows the graph's scroll offset
// and row height, draws one row per commit, and lets the reference badges take
// clicks for selection and branch checkout while wheel and drag still reach the
// graph underneath.
ListView {
    id: labels

    required property var repository
    required property var graph

    model: repository.commits
    // Interactive stays off so wheel and drag still reach the graph; enabling
    // the list lets the reference chips take clicks.
    interactive: false
    enabled: true
    clip: true
    reuseItems: true
    boundsBehavior: Flickable.StopAtBounds
    contentY: graph.contentY

    delegate: Item {
        id: rowItem
        required property int index
        required property string subject
        required property string author
        required property string shortOid
        required property string relativeDate
        required property string oid
        required property string color
        required property var refs
        width: labels.width
        height: graph.effectiveRowHeight
        readonly property bool current: index === repository.selectedRow
        // The uncommitted snapshot is a real commit, so it renders through the
        // same path and only its identity differs.
        readonly property bool snapshot: oid.length > 0 && oid === repository.workInProgressOid
        // Below ~26 px per row a two-line layout cannot fit without colliding
        // with its neighbours, so collapse to one line.
        readonly property bool compact: graph.effectiveRowHeight < 26

        Text {
            visible: rowItem.compact
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            text: rowItem.subject
            color: rowItem.snapshot ? "#e6b45a" : rowItem.current ? "#ffffff" : "#b9c2d2"
            elide: Text.ElideRight
            font.pixelSize: 10
            font.weight: rowItem.current ? Font.DemiBold : Font.Normal
        }

        Column {
            visible: !rowItem.compact
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 3

            Row {
                width: parent.width
                spacing: 8
                Text {
                    width: Math.max(60, parent.width - chipRow.width - 8)
                    text: rowItem.subject
                    color: rowItem.snapshot ? "#e6b45a" : rowItem.current ? "#ffffff" : "#dbe1ec"
                    elide: Text.ElideRight
                    font.pixelSize: 11
                    font.weight: rowItem.current ? Font.DemiBold : Font.Normal
                }
                Row {
                    id: chipRow
                    spacing: 4
                    Repeater {
                        model: rowItem.refs.slice(0, 3)
                        delegate: Rectangle {
                            id: chip
                            required property string modelData
                            // Local branches carry no prefix; remotes start with
                            // "⇄ ", tags with "# ".
                            readonly property bool isBranch: modelData.indexOf("# ") !== 0
                                                             && modelData.indexOf("⇄ ") !== 0
                            // The chip wears its branch's colour, so the badge,
                            // its node, and its line agree.
                            readonly property color branchColor: rowItem.color
                            height: 14
                            width: chipText.implicitWidth + 12
                            radius: 3
                            // Hover raises the fill and the text; the border
                            // keeps the branch colour, so hover never introduces
                            // a hue of its own.
                            color: Qt.rgba(branchColor.r, branchColor.g, branchColor.b,
                                           chipMouse.containsMouse ? 0.38 : 0.22)
                            border.color: Qt.rgba(branchColor.r, branchColor.g, branchColor.b, 0.7)
                            Behavior on color {
                                ColorAnimation { duration: 90 }
                            }
                            MouseArea {
                                id: chipMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: chip.isBranch ? Qt.PointingHandCursor : Qt.ArrowCursor
                                onClicked: repository.selectCommit(rowItem.index)
                                onDoubleClicked: {
                                    if (!chip.isBranch || chip.modelData === repository.currentBranch)
                                        return
                                    repository.checkoutBranch(chip.modelData)
                                }
                            }
                            Accessible.role: Accessible.PushButton
                            Accessible.name: chip.modelData
                            Accessible.onPressAction: repository.selectCommit(rowItem.index)
                            Text {
                                id: chipText
                                anchors.centerIn: parent
                                text: parent.modelData
                                color: chipMouse.containsMouse ? NagaTheme.hoverContent : "#f2f4fa"
                                font.pixelSize: 9
                            }
                        }
                    }
                }
            }
            Row {
                spacing: 8
                Text { text: rowItem.shortOid; color: "#a78bfa"; font.family: "monospace"; font.pixelSize: 9 }
                Text { text: rowItem.author; color: "#78839a"; font.pixelSize: 9 }
                Text { text: "·"; color: "#78839a" }
                Text { text: rowItem.relativeDate; color: "#78839a"; font.pixelSize: 9 }
            }
        }
    }
}
