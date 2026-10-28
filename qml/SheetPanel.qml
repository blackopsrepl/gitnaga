import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GitNaga

// A panel that slides over the review side of the window and leaves the graph
// in view, so whatever it selects in the graph stays visible. File history
// and blame use it; Escape or the close button dismisses it.
Popup {
    id: sheet
    property string title: ""
    property string subtitle: ""
    // Extra header controls, placed between the title and the close button.
    property alias tools: toolRow.data
    default property alias content: body.data

    parent: Overlay.overlay
    x: parent ? parent.width - width : 0
    y: 40
    width: parent ? Math.max(660, Math.round(parent.width * 0.5)) : 660
    height: parent ? parent.height - 40 - 26 : 600
    padding: 0
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape

    background: Rectangle {
        color: NagaTheme.panel
        border.color: NagaTheme.border
        // A soft edge so the sheet reads as above the graph, not part of it.
        Rectangle {
            anchors.right: parent.left
            width: 10
            height: parent.height
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 1.0; color: "#80000000" }
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: 0
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            color: NagaTheme.raised
            border.color: NagaTheme.border
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 6
                spacing: 10
                Label {
                    text: sheet.title
                    color: NagaTheme.text
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }
                Label {
                    Layout.fillWidth: true
                    text: sheet.subtitle
                    color: NagaTheme.muted
                    font.pixelSize: 12
                    elide: Text.ElideMiddle
                }
                RowLayout {
                    id: toolRow
                    spacing: 6
                }
                NagaIconButton {
                    text: "✕"
                    tooltip: qsTr("Close (Escape)")
                    onClicked: sheet.close()
                }
            }
        }
        Item {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
