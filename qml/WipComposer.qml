import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GitNaga

// Commit composer for the work-in-progress row. A commit takes the index
// as it stands; amend rewrites HEAD with it, starting from HEAD's message.
ColumnLayout {
    id: composer
    required property var repository
    property bool amend: false
    property bool published: false
    readonly property int summaryLimit: 72
    readonly property string summary: summaryField.text.trim()
    readonly property bool canCommit: summary.length > 0 && !repository.busy
                                      && (repository.stagedCount > 0 || amend)

    spacing: 6

    function commit() {
        if (!canCommit)
            return
        repository.commitWorktree(summary, descriptionField.text.trim(), amend)
        summaryField.text = ""
        descriptionField.text = ""
        amend = false
    }

    function commitOnControlEnter(event) {
        if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && (event.modifiers & Qt.ControlModifier)) {
            commit()
            event.accepted = true
        }
    }

    onAmendChanged: {
        if (amend) {
            repository.loadAmendMessage()
        } else {
            published = false
            summaryField.text = ""
            descriptionField.text = ""
        }
    }

    Connections {
        target: composer.repository
        function onAmendMessageLoaded(summary, description, published) {
            if (!composer.amend)
                return
            summaryField.text = summary
            descriptionField.text = description
            composer.published = published
        }
    }

    StashDialog {
        id: stashDialog
        onSubmitted: (message, includeUntracked) => composer.repository.stashWorktree(message, includeUntracked)
    }

    TextField {
        id: summaryField
        Layout.fillWidth: true
        Layout.topMargin: 8
        Layout.leftMargin: 8
        Layout.rightMargin: 8
        placeholderText: composer.amend ? qsTr("Amended summary") : qsTr("Summary")
        font.pixelSize: 12
        selectByMouse: true
        rightPadding: counter.implicitWidth + 14
        Keys.onPressed: (event) => composer.commitOnControlEnter(event)
        // A soft limit: the counter warns, the text is never cut.
        Text {
            id: counter
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: composer.summaryLimit - summaryField.length
            color: summaryField.length > composer.summaryLimit ? "#ff9b5c" : NagaTheme.muted
            font.pixelSize: 11
        }
    }
    TextArea {
        id: descriptionField
        Layout.fillWidth: true
        Layout.leftMargin: 8
        Layout.rightMargin: 8
        Layout.preferredHeight: Math.max(54, Math.min(120, implicitHeight))
        leftPadding: 8
        rightPadding: 8
        topPadding: 6
        bottomPadding: 6
        placeholderText: qsTr("Description")
        font.pixelSize: 12
        background: Rectangle {
            color: NagaTheme.background
            border.color: descriptionField.activeFocus ? NagaTheme.focusStroke : NagaTheme.border
            radius: 2
        }
        wrapMode: TextArea.Wrap
        Keys.onPressed: (event) => composer.commitOnControlEnter(event)
    }
    Text {
        visible: composer.amend && composer.published
        Layout.fillWidth: true
        Layout.leftMargin: 8
        Layout.rightMargin: 8
        text: qsTr("HEAD is already on %1: amending rewrites a pushed commit.").arg(composer.repository.upstream)
        color: "#e6b45a"
        wrapMode: Text.WordWrap
        font.pixelSize: 11
    }
    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: 8
        Layout.rightMargin: 8
        Layout.bottomMargin: 8
        spacing: 6
        NagaButton {
            text: qsTr("Amend")
            checkable: true
            checked: composer.amend
            active: composer.amend
            enabled: composer.repository.headOid.length > 0
            onToggled: composer.amend = checked
        }
        NagaButton {
            text: qsTr("Stash…")
            onClicked: stashDialog.openFresh()
        }
        Item { Layout.fillWidth: true }
        Text {
            text: composer.repository.stagedCount === 1 ? qsTr("1 file staged")
                                                        : qsTr("%1 files staged").arg(composer.repository.stagedCount)
            color: NagaTheme.muted
            font.pixelSize: 11
        }
        NagaButton {
            text: composer.amend ? qsTr("Amend commit") : qsTr("Commit")
            active: composer.canCommit
            enabled: composer.canCommit
            onClicked: composer.commit()
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Ctrl+Enter")
        }
    }
}
