import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GitNaga

// The rebase editor's visible part: a header that says what is rewritten,
// the commit list, and a footer with autostash, the problem (if any) and the
// buttons.
Item {
    id: body
    required property var editor
    required property var repository
    required property var model

    readonly property alias list: stepList
    readonly property alias autostash: autostashBox.checked
    readonly property bool dirty: repository.uncommittedCount > 0
    readonly property string message: editor.error.length > 0 ? editor.error : editor.problem

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: NagaTheme.raised
            radius: 4
            Column {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3
                Text {
                    text: qsTr("Interactive rebase")
                    color: NagaTheme.text
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                }
                Text {
                    text: body.editor.loading ? qsTr("Reading commits…")
                        : body.model.count + (body.model.count === 1 ? qsTr(" commit") : qsTr(" commits"))
                          + (body.editor.base.length > 0 ? qsTr(" replayed onto ") + body.editor.base.substring(0, 8)
                                                          : qsTr(" rewritten from the root"))
                          + qsTr(" · oldest first")
                    color: NagaTheme.muted
                    font.pixelSize: 11
                }
            }
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Keys: p pick · r reword · e edit · s squash · f fixup · d drop · Alt+↑↓ move")
                color: NagaTheme.muted
                font.pixelSize: 11
            }
        }

        ListView {
            id: stepList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: body.model
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            moveDisplaced: Transition {
                NumberAnimation { properties: "y"; duration: 90 }
            }
            delegate: RebaseStepRow {
                editor: body.editor
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: NagaTheme.border
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            spacing: 10

            CheckBox {
                id: autostashBox
                visible: body.dirty
                text: qsTr("Autostash uncommitted changes")
                palette.windowText: NagaTheme.text
                font.pixelSize: 12
            }
            Text {
                Layout.fillWidth: true
                text: body.message
                color: body.editor.error.length > 0 ? NagaTheme.danger : NagaTheme.warning
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                font.pixelSize: 11
            }
            NagaButton {
                text: qsTr("Cancel")
                keepFocus: true
                enabled: !body.editor.running
                onClicked: body.editor.close()
            }
            NagaButton {
                text: body.editor.running ? qsTr("Rebasing…") : qsTr("Start rebase")
                active: true
                keepFocus: true
                enabled: !body.editor.loading && !body.editor.running && body.model.count > 0
                         && body.editor.problem.length === 0
                onClicked: body.editor.start()
            }
        }
    }
}
