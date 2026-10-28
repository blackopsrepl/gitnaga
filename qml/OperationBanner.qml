import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GitNaga

// Shown above the graph while git has an operation half done: what is being
// applied onto what, how far along, the files still in conflict, and the
// ways forward (continue, skip, abort) so nobody is left reading git's error.
Rectangle {
    id: banner
    required property var repository
    readonly property var state: repository.inProgress
    readonly property string kind: state.kind
    readonly property bool bisecting: kind === "bisect"
    readonly property int conflicts: state.conflictCount

    visible: kind.length > 0
    implicitHeight: visible ? content.implicitHeight + 16 : 0
    color: NagaTheme.warningTint(0.10)
    border.color: NagaTheme.warningTint(0.45)

    function verb() {
        switch (kind) {
        case "merge": return qsTr("MERGING")
        case "rebase": return qsTr("REBASING")
        case "cherry-pick": return qsTr("CHERRY-PICK")
        case "revert": return qsTr("REVERTING")
        case "am": return qsTr("APPLYING")
        case "bisect": return qsTr("BISECTING")
        }
        return kind.toUpperCase()
    }

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8
        anchors.leftMargin: 10
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Rectangle {
                implicitWidth: badgeText.implicitWidth + 12
                implicitHeight: 18
                radius: 3
                color: NagaTheme.warningTint(0.22)
                border.color: NagaTheme.warningTint(0.7)
                Text {
                    id: badgeText
                    anchors.centerIn: parent
                    text: banner.verb()
                    color: NagaTheme.warning
                    font.pixelSize: 10
                    font.weight: Font.Bold
                    font.letterSpacing: 0.6
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    Layout.fillWidth: true
                    text: banner.state.title.length > 0 ? banner.state.title : banner.kind
                    color: NagaTheme.text
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: banner.state.detail
                    color: NagaTheme.muted
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }
            Item {
                visible: !banner.bisecting
                implicitWidth: continueButton.implicitWidth
                implicitHeight: continueButton.implicitHeight
                HoverHandler { id: continueHover }
                NagaButton {
                    id: continueButton
                    text: qsTr("Continue")
                    active: enabled
                    implicitHeight: 26
                    enabled: banner.conflicts === 0 && !banner.repository.busy
                    onClicked: banner.repository.operation.continueOperation()
                }
                ToolTip.visible: continueHover.hovered && banner.conflicts > 0
                ToolTip.text: banner.conflicts === 1 ? qsTr("Resolve the conflicted file first")
                                                     : qsTr("Resolve the %1 conflicted files first").arg(banner.conflicts)
            }
            NagaButton {
                visible: banner.state.canSkip
                text: qsTr("Skip")
                implicitHeight: 26
                enabled: !banner.repository.busy
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Drop the commit being applied and go on with the next")
                onClicked: confirm.ask(qsTr("Skip the commit being applied? Its changes are left out of the result."),
                                       function() { banner.repository.operation.skipOperation() })
            }
            Repeater {
                model: banner.bisecting ? [["good", qsTr("Good")], ["bad", qsTr("Bad")], ["skip", qsTr("Skip")]] : []
                delegate: NagaButton {
                    required property var modelData
                    text: modelData[1]
                    implicitHeight: 26
                    enabled: !banner.repository.busy
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Mark the checked-out commit %1").arg(modelData[0])
                    onClicked: banner.repository.operation.bisect(modelData[0])
                }
            }
            NagaButton {
                text: banner.bisecting ? qsTr("Reset") : qsTr("Abort")
                implicitHeight: 26
                enabled: !banner.repository.busy
                onClicked: confirm.ask(banner.bisecting
                                       ? qsTr("End the bisect and return to where it started?")
                                       : qsTr("Abort the %1 and put everything back the way it was before it started?").arg(banner.kind),
                                       function() { banner.repository.operation.abortOperation() })
            }
        }

        Text {
            visible: !banner.bisecting
            Layout.fillWidth: true
            text: banner.conflicts > 0
                  ? (banner.conflicts === 1 ? qsTr("1 file in conflict") : qsTr("%1 files in conflict").arg(banner.conflicts))
                    + (banner.state.oursName.length > 0
                       ? qsTr("  ·  ours is %1, theirs is %2").arg(banner.state.oursName).arg(banner.state.theirsName) : "")
                  : qsTr("No conflicts left. Continue to finish the %1.").arg(banner.kind)
            color: banner.conflicts > 0 ? NagaTheme.warning : NagaTheme.accent
            font.pixelSize: 11
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        ListView {
            id: files
            visible: banner.conflicts > 0
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(count, 5) * 30
            clip: true
            interactive: count > 5
            model: banner.state.conflicts
            delegate: ConflictFileRow {
                required property string modelData
                width: files.width
                path: modelData
                repository: banner.repository
                onResolveRequested: (path) => conflictView.openFor(path)
            }
        }
    }

    ConflictDialog {
        id: conflictView
        repository: banner.repository
    }

    ConfirmDialog {
        id: confirm
        parent: Overlay.overlay
        property var callback: null
        onConfirmed: {
            var run = callback
            if (typeof run === "function")
                run()
        }
        function ask(message, run) {
            confirm.message = message
            confirm.callback = run
            confirm.open()
        }
    }
}
