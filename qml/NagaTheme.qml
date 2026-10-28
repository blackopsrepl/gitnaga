pragma Singleton

import QtQuick

// Single source of truth for every colour in the interface. Panes receive it
// through their `colors` property, controls read it directly, so a token can
// only be changed in one place.
//
// Interaction states follow one rule, which is what stops hover effects from
// fighting each other:
//
//   * hover is a **neutral lift** — `hoverFill` behind the element and
//     `hoverContent` on its text, icon, or check mark; it never introduces a
//     hue;
//   * pressed is the same lift, stronger;
//   * identity (branch colour) and selection/active (accent) keep their hue,
//     and hover is layered on top of them rather than replacing them;
//   * keyboard focus uses `focusStroke`, and controls that do not need a
//     focus ring opt out so a mouse click cannot leave one behind.
QtObject {
    // Surfaces and text.
    readonly property color background: "#080b12"
    readonly property color panel: "#0e1420"
    readonly property color raised: "#141b2a"
    readonly property color alternate: "#1a2233"
    readonly property color border: "#222c40"
    readonly property color text: "#e8ebf2"
    readonly property color muted: "#8590a3"
    readonly property color accent: "#34d399"
    // Something half done that needs the person (a stopped merge), and
    // something that went wrong or cannot be taken back.
    readonly property color warning: "#e6b45a"
    readonly property color danger: "#ff7d8b"
    // The two sides of a conflict, so ours and theirs read the same everywhere.
    readonly property color ours: "#9fc2ff"
    readonly property color theirs: "#d7a6ff"
    readonly property color code: "#cbd0dc"

    // Interaction states.
    readonly property color hoverFill: "#1f2836"
    readonly property color pressedFill: "#2b3446"
    readonly property color hoverContent: "#ffffff"
    readonly property color focusStroke: "#34d399"
    // Content on accent-tinted (active) surfaces, and the same content one
    // step brighter while the pointer is on it.
    readonly property color accentContent: "#7cf0bd"
    readonly property color accentContentHover: "#a7f8d3"

    // Accent tints for selected/active surfaces, so no component has to invent
    // its own green.
    function accentTint(strength) {
        return Qt.rgba(accent.r, accent.g, accent.b, strength)
    }
    function warningTint(strength) {
        return Qt.rgba(warning.r, warning.g, warning.b, strength)
    }
    function tint(base, strength) {
        return Qt.rgba(base.r, base.g, base.b, strength)
    }
}
