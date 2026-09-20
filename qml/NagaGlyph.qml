import QtQuick

Canvas {
    id: canvas
    property color bodyColor: "#34d399"
    property color hoodColor: "#0f766e"
    property color eyeColor: "#ffe08a"

    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()
        var w = width
        var h = height
        if (w <= 0 || h <= 0)
            return

        var body = ctx.createLinearGradient(w * 0.1, h, w * 0.9, 0)
        body.addColorStop(0, "#0f766e")
        body.addColorStop(0.5, bodyColor)
        body.addColorStop(1, "#a7f3d0")
        ctx.strokeStyle = body
        ctx.lineCap = "round"
        ctx.lineJoin = "round"

        ctx.lineWidth = Math.max(2, w * 0.13)
        ctx.beginPath()
        ctx.moveTo(w * 0.16, h * 0.9)
        ctx.bezierCurveTo(w * 0.95, h * 0.78, w * 0.02, h * 0.5, w * 0.62, h * 0.36)
        ctx.stroke()

        ctx.lineWidth = Math.max(1.5, w * 0.09)
        ctx.beginPath()
        ctx.moveTo(w * 0.62, h * 0.36)
        ctx.bezierCurveTo(w * 0.9, h * 0.3, w * 0.72, h * 0.16, w * 0.5, h * 0.14)
        ctx.stroke()

        var hood = ctx.createRadialGradient(w * 0.5, h * 0.16, 1, w * 0.5, h * 0.16, w * 0.34)
        hood.addColorStop(0, bodyColor)
        hood.addColorStop(1, hoodColor)
        ctx.fillStyle = hood
        ctx.beginPath()
        ctx.moveTo(w * 0.24, h * 0.17)
        ctx.bezierCurveTo(w * 0.24, h * 0.0, w * 0.76, h * 0.0, w * 0.76, h * 0.17)
        ctx.bezierCurveTo(w * 0.76, h * 0.31, w * 0.24, h * 0.31, w * 0.24, h * 0.17)
        ctx.fill()

        ctx.fillStyle = eyeColor
        ctx.beginPath()
        ctx.arc(w * 0.42, h * 0.155, Math.max(1, w * 0.035), 0, Math.PI * 2)
        ctx.fill()
        ctx.beginPath()
        ctx.arc(w * 0.58, h * 0.155, Math.max(1, w * 0.035), 0, Math.PI * 2)
        ctx.fill()
    }
}
