import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GitNaga
import "DiffFormat.js" as DiffFormat

// An image diff: the old and the new image side by side on a checkerboard,
// each with its dimensions and size. Blobs load asynchronously through the
// diffblob image provider, so a large image never stalls the window.
Item {
    id: compare
    required property var model
    readonly property var special: model.special
    readonly property string checker: "image://diffblob/checker/" + String(NagaTheme.checkerDark).substring(1)
                                      + "/" + String(NagaTheme.checkerLight).substring(1)

    RowLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        Repeater {
            model: [
                { label: qsTr("Before"), source: compare.special.oldImage, size: compare.special.oldSize, ink: NagaTheme.diffRemovedInk },
                { label: qsTr("After"), source: compare.special.newImage, size: compare.special.newSize, ink: NagaTheme.diffAddedInk }
            ]
            delegate: ColumnLayout {
                id: side
                required property var modelData
                readonly property bool present: modelData.source.length > 0
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 1
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Rectangle { implicitWidth: 3; implicitHeight: 14; radius: 1; color: side.modelData.ink }
                    Text {
                        text: side.modelData.label
                        color: NagaTheme.text
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: !side.present ? ""
                              : (image.status === Image.Ready ? image.sourceSize.width + " × " + image.sourceSize.height + "   " : "")
                                + DiffFormat.bytes(side.modelData.size)
                        color: NagaTheme.muted
                        font.family: "monospace"
                        font.pixelSize: 11
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 6
                    color: NagaTheme.raised
                    border.color: NagaTheme.border
                    clip: true

                    Item {
                        id: stage
                        anchors.fill: parent
                        anchors.margins: 14
                        // Small images (icons, sprites) are scaled up with
                        // hard pixels so a one-pixel change is visible.
                        readonly property real natural: Math.max(1, image.sourceSize.width, image.sourceSize.height)
                        readonly property real scale: Math.min(width / Math.max(1, image.sourceSize.width),
                                                               height / Math.max(1, image.sourceSize.height),
                                                               natural <= 128 ? 8 : 1)
                        Image {
                            anchors.centerIn: parent
                            width: image.width
                            height: image.height
                            visible: image.status === Image.Ready
                            source: compare.checker
                            fillMode: Image.Tile
                        }
                        Image {
                            id: image
                            anchors.centerIn: parent
                            width: sourceSize.width * stage.scale
                            height: sourceSize.height * stage.scale
                            source: side.modelData.source
                            asynchronous: true
                            cache: false
                            smooth: stage.scale < 1
                            mipmap: stage.scale < 1
                        }
                        Rectangle {
                            anchors.centerIn: image
                            width: image.width + 2
                            height: image.height + 2
                            visible: image.status === Image.Ready
                            color: "transparent"
                            border.color: NagaTheme.border
                        }
                    }
                    BusyIndicator {
                        anchors.centerIn: parent
                        running: image.status === Image.Loading
                        visible: running
                    }
                    Text {
                        anchors.centerIn: parent
                        visible: !side.present || image.status === Image.Error
                        text: !side.present ? (side.modelData.label === qsTr("Before") ? qsTr("No image before this change")
                                                                                       : qsTr("Image deleted"))
                                            : qsTr("This image could not be decoded")
                        color: NagaTheme.muted
                        font.pixelSize: 12
                    }
                }
            }
        }
    }
}
