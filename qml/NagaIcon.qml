import QtQuick

// Small stroke-drawn vector icon painted on a Canvas, so the application
// carries no icon theme dependency and stays crisp at any DPI. The name picks
// one of the built-in paths and the colour tracks the palette it is given.
Canvas {
    id: canvas

    property string name: ""
    property color color: "#c3cad8"
    property real strokeWidth: 1.5

    width: 16
    height: 16
    antialiasing: true

    onNameChanged: requestPaint()
    onColorChanged: requestPaint()
    onStrokeWidthChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()
        ctx.clearRect(0, 0, width, height)
        ctx.strokeStyle = String(canvas.color)
        ctx.fillStyle = String(canvas.color)
        ctx.lineWidth = canvas.strokeWidth
        ctx.lineCap = "round"
        ctx.lineJoin = "round"
        if (name === "folder-open") {
            // Folder with a raised tab and an open front flap.
            ctx.beginPath()
            ctx.moveTo(1.5, 4)
            ctx.lineTo(1.5, 12.5)
            ctx.lineTo(10, 12.5)
            ctx.lineTo(14.5, 9)
            ctx.lineTo(6.5, 9)
            ctx.lineTo(5, 11)
            ctx.moveTo(1.5, 4)
            ctx.lineTo(1.5, 3)
            ctx.lineTo(5.5, 3)
            ctx.lineTo(7, 4.8)
            ctx.lineTo(13, 4.8)
            ctx.lineTo(13, 6.4)
            ctx.stroke()
        } else if (name === "refresh") {
            // Circular arrow: 300 degree arc plus a solid arrowhead.
            var cx = width / 2, cy = height / 2, r = width / 2 - strokeWidth
            ctx.beginPath()
            ctx.arc(cx, cy, r, -Math.PI / 2, Math.PI * 0.72)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(cx + r * 0.72, cy - r * 0.94)
            ctx.lineTo(cx + r * 1.06, cy - r * 0.30)
            ctx.lineTo(cx + r * 0.28, cy - r * 0.42)
            ctx.closePath()
            ctx.fill()
        } else if (name === "branch") {
            // Two dots joined by a rail through a third dot.
            ctx.beginPath()
            ctx.arc(4, 3.5, 1.9, 0, Math.PI * 2)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(4, 12.5, 1.9, 0, Math.PI * 2)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(11.5, 8, 1.9, 0, Math.PI * 2)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(4, 5.4)
            ctx.lineTo(4, 10.6)
            ctx.moveTo(5.9, 8)
            ctx.lineTo(9.6, 8)
            ctx.stroke()
        }
    }
}
