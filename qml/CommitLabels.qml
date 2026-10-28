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
    // Where a dragged branch chip reports its position (BranchDragLayer).
    property var dragLayer: null

    // The reference chip at a point in this view's coordinates, if any.
    function chipAt(point) {
        var row = labels.itemAt(point.x, point.y + labels.contentY)
        // The delegate is an inline type the linter cannot see through itemAt().
        return row ? row.chipAt(labels.mapToItem(row, point.x, point.y)) : null // qmllint disable missing-property
    }

    // While a search is active its hits are marked and the rest recede.
    property var search: null

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
        // The uncommitted-work row renders through the same path as a commit;
        // same path and only its identity differs.
        readonly property bool snapshot: oid.length > 0 && oid === repository.workInProgressOid
        readonly property string title: snapshot
            ? qsTr("Uncommitted changes") + "  ·  " + repository.uncommittedCount
              + (repository.uncommittedCount === 1 ? qsTr(" file") : qsTr(" files"))
            : subject
        // Below ~26 px per row a two-line layout cannot fit without colliding
        // with its neighbours, so collapse to one line.
        readonly property bool compact: graph.effectiveRowHeight < 26
        readonly property bool searching: labels.search !== null && labels.search.active
        readonly property bool matched: searching && labels.search.revision >= 0 && labels.search.isMatch(index)
        opacity: searching && !matched && !snapshot ? 0.3 : 1

        Rectangle {
            visible: rowItem.matched
            anchors.fill: parent
            anchors.topMargin: 1
            anchors.bottomMargin: 1
            anchors.rightMargin: 10
            radius: 3
            color: NagaTheme.searchHitTint(rowItem.current ? 0.2 : 0.11)
            Rectangle {
                width: 2
                height: parent.height
                color: NagaTheme.searchHit
            }
        }

        function chipAt(point) {
            for (var i = 0; i < chipRepeater.count; ++i) {
                var chip = chipRepeater.itemAt(i)
                if (chip && chip.visible && chip.contains(chip.mapFromItem(rowItem, point.x, point.y)))
                    return chip.modelData // qmllint disable missing-property
            }
            return null
        }

        Text {
            visible: rowItem.compact
            anchors.left: parent.left
            anchors.leftMargin: rowItem.matched ? 6 : 0
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            text: rowItem.title
            color: rowItem.snapshot ? "#e6b45a" : rowItem.current ? "#ffffff" : "#b9c2d2"
            elide: Text.ElideRight
            font.pixelSize: 10
            font.weight: rowItem.current ? Font.DemiBold : Font.Normal
        }

        Column {
            visible: !rowItem.compact
            anchors.left: parent.left
            anchors.leftMargin: rowItem.matched ? 6 : 0
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 3

            Row {
                width: parent.width
                spacing: 8
                Text {
                    width: Math.max(60, parent.width - chipRow.width - 8)
                    text: rowItem.title
                    color: rowItem.snapshot ? "#e6b45a" : rowItem.current ? "#ffffff" : "#dbe1ec"
                    elide: Text.ElideRight
                    font.pixelSize: 11
                    font.weight: rowItem.current ? Font.DemiBold : Font.Normal
                }
                Row {
                    id: chipRow
                    spacing: 4
                    Repeater {
                        id: chipRepeater
                        model: rowItem.refs.slice(0, 3)
                        delegate: Rectangle {
                            id: chip
                            required property var modelData
                            readonly property bool isBranch: modelData.kind === "local"
                            readonly property bool isTag: modelData.kind === "tag"
                            // The chip wears its branch's colour, so the badge,
                            // its node, and its line agree; tags stay neutral.
                            readonly property color branchColor: isTag ? "#9aa3b5" : rowItem.color
                            height: 14
                            width: chipText.implicitWidth + 12
                            radius: isTag ? 7 : 3
                            // Hover raises the fill and the text; the border
                            // keeps the branch colour, so hover never introduces
                            // a hue of its own.
                            color: Qt.rgba(branchColor.r, branchColor.g, branchColor.b,
                                           chipMouse.containsMouse ? 0.38 : (modelData.kind === "remote" ? 0.08 : 0.22))
                            border.color: Qt.rgba(branchColor.r, branchColor.g, branchColor.b, modelData.isHead ? 1.0 : 0.7)
                            border.width: modelData.isHead ? 1.5 : 1
                            Behavior on color {
                                ColorAnimation { duration: 90 }
                            }
                            MouseArea {
                                id: chipMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                // A local branch chip can be dragged onto a commit
                                // or another branch; nothing may steal that drag.
                                preventStealing: chip.isBranch
                                cursorShape: dragging ? Qt.ClosedHandCursor : chip.isBranch ? Qt.OpenHandCursor : Qt.ArrowCursor
                                property bool dragging: false
                                property point pressPoint
                                onPressed: (mouse) => { pressPoint = Qt.point(mouse.x, mouse.y); dragging = false }
                                onPositionChanged: (mouse) => {
                                    if (!pressed || !chip.isBranch || !labels.dragLayer)
                                        return
                                    var layer = labels.dragLayer
                                    if (!dragging && Math.abs(mouse.x - pressPoint.x) + Math.abs(mouse.y - pressPoint.y) > 6) {
                                        dragging = true
                                        layer.begin(chip.modelData, chip.branchColor)
                                    }
                                    if (dragging)
                                        layer.update(chipMouse.mapToItem(layer, mouse.x, mouse.y))
                                }
                                onReleased: if (dragging) labels.dragLayer.finish()
                                onCanceled: if (dragging) { dragging = false; labels.dragLayer.finish() }
                                onClicked: if (!dragging) repository.selectCommit(rowItem.index)
                                onDoubleClicked: {
                                    if (!chip.isBranch || chip.modelData.isHead)
                                        return
                                    repository.checkoutBranch(chip.modelData.name)
                                }
                            }
                            Accessible.role: Accessible.PushButton
                            Accessible.name: chip.modelData.name
                            Accessible.onPressAction: repository.selectCommit(rowItem.index)
                            Text {
                                id: chipText
                                anchors.centerIn: parent
                                text: chip.modelData.name
                                font.bold: chip.modelData.isHead
                                color: chipMouse.containsMouse ? NagaTheme.hoverContent : "#f2f4fa"
                                font.pixelSize: 9
                            }
                        }
                    }
                }
            }
            Text {
                width: parent.width
                visible: !rowItem.snapshot
                textFormat: Text.StyledText
                text: "<font color=\"#a78bfa\">" + rowItem.shortOid + "</font>&nbsp;&nbsp;"
                      + rowItem.author.replace(/&/g, "&amp;").replace(/</g, "&lt;")
                      + "&nbsp;·&nbsp;" + rowItem.relativeDate
                color: "#78839a"
                elide: Text.ElideRight
                font.pixelSize: 9
            }
        }
    }
}
