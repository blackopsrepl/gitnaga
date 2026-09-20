import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    required property var repository
    width: 1280
    height: 800
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    title: qsTr("GitNaga")
    color: "#111318"

    palette.window: "#111318"
    palette.windowText: "#e8eaf0"
    palette.base: "#171a21"
    palette.text: "#e8eaf0"
    palette.button: "#232733"
    palette.buttonText: "#e8eaf0"
    palette.highlight: "#6f8cff"
    palette.highlightedText: "#ffffff"

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 12

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("GitNaga")
            font.pixelSize: 34
            font.weight: Font.DemiBold
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: qsTr("Native Git, clearly seen")
            color: "#9299aa"
            font.pixelSize: 15
        }
    }
}
