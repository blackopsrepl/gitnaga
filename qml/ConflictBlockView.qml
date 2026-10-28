import QtQuick
import QtQuick.Layouts
import GitNaga

// One conflict in the conflict view: both sides next to each other and the
// four ways to settle it. The chosen side stays bright, the other dims.
Rectangle {
    id: view
    property var block: ({})
    property int number: 0
    property int total: 0
    property string choice: ""
    property string oursName: ""
    property string theirsName: ""
    signal chosen(string choice)

    readonly property bool takesOurs: choice === "ours" || choice === "ours-theirs" || choice === "theirs-ours"
    readonly property bool takesTheirs: choice === "theirs" || choice === "ours-theirs" || choice === "theirs-ours"

    implicitHeight: column.implicitHeight + 16
    radius: 4
    color: NagaTheme.panel
    border.color: choice.length > 0 ? NagaTheme.accentTint(0.55) : NagaTheme.warningTint(0.5)

    component Side: Rectangle {
        id: side
        property string title: ""
        property string body: ""
        property color tone: NagaTheme.ours
        property bool taken: false
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        implicitHeight: sideColumn.implicitHeight + 10
        radius: 3
        color: NagaTheme.tint(tone, taken ? 0.10 : 0.04)
        border.color: NagaTheme.tint(tone, taken ? 0.7 : 0.25)
        opacity: view.choice.length > 0 && !taken ? 0.45 : 1
        ColumnLayout {
            id: sideColumn
            anchors.fill: parent
            anchors.margins: 5
            spacing: 4
            Text {
                Layout.fillWidth: true
                text: side.title
                color: side.tone
                font.pixelSize: 11
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                text: side.body.length > 0 ? side.body.replace(/\r?\n$/, "") : qsTr("(nothing on this side)")
                color: side.body.length > 0 ? NagaTheme.code : NagaTheme.muted
                font.family: "monospace"
                font.pixelSize: 12
                font.italic: side.body.length === 0
                textFormat: Text.PlainText
                wrapMode: Text.WrapAnywhere
            }
        }
    }

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Text {
                text: qsTr("Conflict %1 of %2").arg(view.number).arg(view.total)
                color: NagaTheme.text
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }
            Text {
                text: view.choice.length > 0 ? qsTr("resolved") : qsTr("needs a choice")
                color: view.choice.length > 0 ? NagaTheme.accent : NagaTheme.warning
                font.pixelSize: 11
            }
            Item { Layout.fillWidth: true }
            Repeater {
                model: [["ours", qsTr("Ours")], ["theirs", qsTr("Theirs")],
                        ["ours-theirs", qsTr("Both, ours first")], ["theirs-ours", qsTr("Both, theirs first")]]
                delegate: NagaButton {
                    required property var modelData
                    text: modelData[1]
                    implicitHeight: 24
                    leftPadding: 8
                    rightPadding: 8
                    active: view.choice === modelData[0]
                    onClicked: view.chosen(view.choice === modelData[0] ? "" : modelData[0])
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Side {
                title: qsTr("Ours") + (view.oursName.length > 0 ? " · " + view.oursName : "")
                       + (view.block.oursLabel && view.block.oursLabel !== view.oursName ? "  (" + view.block.oursLabel + ")" : "")
                body: view.block.ours || ""
                tone: NagaTheme.ours
                taken: view.takesOurs
            }
            Side {
                title: qsTr("Theirs") + (view.theirsName.length > 0 ? " · " + view.theirsName : "")
                       + (view.block.theirsLabel && view.block.theirsLabel !== view.theirsName ? "  (" + view.block.theirsLabel + ")" : "")
                body: view.block.theirs || ""
                tone: NagaTheme.theirs
                taken: view.takesTheirs
            }
        }

        Side {
            visible: view.block.hasBase === true
            Layout.fillWidth: true
            title: qsTr("Common ancestor") + (view.block.baseLabel ? "  (" + view.block.baseLabel + ")" : "")
            body: view.block.base || ""
            tone: NagaTheme.muted
            taken: true
            opacity: 0.8
        }
    }
}
