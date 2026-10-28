import QtQuick
import QtQuick.Layouts
import GitNaga

// Floating bar for the picked lines of a diff.
Rectangle {
    id: bar
    property string side: ""
    property int count: 0
    signal requested(string action)
    signal cleared()

    implicitWidth: row.implicitWidth + 16
    implicitHeight: row.implicitHeight + 12
    radius: 6
    color: NagaTheme.raised
    border.color: NagaTheme.accentTint(0.5)

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: 6
        Text {
            text: bar.count === 1 ? qsTr("1 line") : qsTr("%1 lines").arg(bar.count)
            color: NagaTheme.text
            font.pixelSize: 12
            rightPadding: 4
        }
        NagaButton {
            visible: bar.side === "unstaged"
            text: qsTr("Discard")
            implicitHeight: 24
            onClicked: bar.requested("discard")
        }
        NagaButton {
            visible: bar.side === "unstaged"
            text: qsTr("Stage")
            active: true
            implicitHeight: 24
            onClicked: bar.requested("stage")
        }
        NagaButton {
            visible: bar.side === "staged"
            text: qsTr("Unstage")
            active: true
            implicitHeight: 24
            onClicked: bar.requested("unstage")
        }
        NagaButton {
            text: qsTr("Clear")
            implicitHeight: 24
            onClicked: bar.cleared()
        }
    }
}
