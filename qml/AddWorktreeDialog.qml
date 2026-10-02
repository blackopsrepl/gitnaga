import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GitNaga

Dialog {
    id: dialog
    required property var repository

    modal: true
    focus: true
    width: 470
    padding: 0
    anchors.centerIn: parent
    closePolicy: Popup.CloseOnEscape
    background: Rectangle {
        color: NagaTheme.panel
        border.color: NagaTheme.border
    }

    function openForRepository() {
        modeBox.currentIndex = 0
        branchField.text = ""
        startPointField.text = ""
        pathField.text = repository.repositoryPath + "-worktree"
        open()
        pathField.forceActiveFocus()
    }

    contentItem: ColumnLayout {
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            color: NagaTheme.raised
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 8
                Label {
                    text: qsTr("Add Worktree")
                    color: NagaTheme.text
                    font.pixelSize: 13
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

        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 14
            spacing: 8

            Label { text: qsTr("Destination path"); color: NagaTheme.muted; font.pixelSize: 11 }
            TextField {
                id: pathField
                Layout.fillWidth: true
                selectByMouse: true
                placeholderText: qsTr("/path/to/worktree")
            }

            Label { text: qsTr("Checkout mode"); color: NagaTheme.muted; font.pixelSize: 11 }
            ComboBox {
                id: modeBox
                Layout.fillWidth: true
                model: [qsTr("Create a new branch"), qsTr("Use an existing branch"), qsTr("Detached HEAD")]
            }

            Label {
                visible: modeBox.currentIndex < 2
                text: modeBox.currentIndex === 0 ? qsTr("New branch name") : qsTr("Existing local branch")
                color: NagaTheme.muted
                font.pixelSize: 11
            }
            TextField {
                id: branchField
                visible: modeBox.currentIndex < 2
                Layout.fillWidth: true
                selectByMouse: true
                placeholderText: modeBox.currentIndex === 0 ? qsTr("feature/my-change") : qsTr("feature/ready-to-check-out")
            }

            Label {
                visible: modeBox.currentIndex !== 1
                text: qsTr("Start point (optional; defaults to HEAD)")
                color: NagaTheme.muted
                font.pixelSize: 11
            }
            TextField {
                id: startPointField
                visible: modeBox.currentIndex !== 1
                Layout.fillWidth: true
                selectByMouse: true
                placeholderText: qsTr("Branch, tag, or commit")
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: NagaTheme.raised
            RowLayout {
                anchors.fill: parent
                anchors.rightMargin: 12
                spacing: 8
                Item { Layout.fillWidth: true }
                NagaButton {
                    text: qsTr("Cancel")
                    keepFocus: true
                    onClicked: dialog.close()
                }
                NagaButton {
                    text: qsTr("Add Worktree")
                    keepFocus: true
                    active: true
                    enabled: pathField.text.trim().length > 0
                             && (modeBox.currentIndex === 2 || branchField.text.trim().length > 0)
                    onClicked: {
                        const mode = ["new", "existing", "detached"][modeBox.currentIndex]
                        const startPoint = mode === "existing" ? "" : startPointField.text.trim()
                        repository.addWorktree(pathField.text.trim(), branchField.text.trim(), mode, startPoint)
                        dialog.close()
                    }
                }
            }
        }
    }
}
