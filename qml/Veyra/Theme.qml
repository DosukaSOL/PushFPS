// Design tokens, transcribed from the approved prototype's app.css.
//
// Every classic value here is copied from the prototype rather than chosen. That
// is the point of this file: the interface has to match the approved design, so
// the numbers live in one place that can be diffed against app.css.
//
// 设置 → 外观 → 界面配色 adds a second palette, 日落 (dusk, the default since
// 2026-11-14): the same depths tinted indigo-violet, a sunset accent that fills
// switches, sliders, primary buttons and seek bars as a peach -> pink gradient
// (VAccentFill.qml), and a twilight backdrop (tools/qt_probe/make-backdrop-dusk.py).
// Its values are our own, chosen after sunset skies. "classic" keeps every value
// of the prototype exactly.
pragma Singleton
import QtQuick

QtObject {
    // PushFPS appearance (设置 → 外观, preference pushTheme; Main.qml binds it):
    // "default" the design's dark grey, "black" near-black surfaces (OLED), "light"
    // light surfaces with dark text. Every surface, text and accent below follows it;
    // ink(a) replaces the design's white-at-alpha overlays so tracks, fills and
    // hover states stay visible on light surfaces too. Contrast: t1/t2/t3 on card2
    // are at least 4.5:1 in every mode.
    property string mode: "default"
    readonly property bool light: mode === "light"
    readonly property bool black: mode === "black"
    // Three more dark backgrounds (after 4.1; "give people more options for what the interface looks like"):
    // the same depths as the dark grey, tinted navy, aubergine or deep green. With the accents below that is
    // six backgrounds by seven accents. Every pair is checked for contrast (tst_push_appearance.qml).
    readonly property bool tinted: mode === "midnight" || mode === "plum" || mode === "forest"
    // High contrast (4.1, for people who need it): black surfaces, white text at every level, and
    // outlines strong enough to show where a card or a control ends without relying on a shade of grey.
    readonly property bool contrast: mode === "contrast"
    readonly property var surfaces: ({
        "default":  { card: "#131317", card2: "#19191E", card3: "#24242A", popover: "#1B1B20", dialog: "#141418", bgOuter: "#070709", bgMid: "#0C0C10", bgInner: "#17171C" },
        "black":    { card: "#08080A", card2: "#0D0D10", card3: "#17171B", popover: "#101013", dialog: "#0A0A0C", bgOuter: "#000000", bgMid: "#010102", bgInner: "#050506" },
        "light":    { card: "#FFFFFF", card2: "#F3F5F0", card3: "#E4E7E0", popover: "#FFFFFF", dialog: "#FFFFFF", bgOuter: "#DFE3DA", bgMid: "#E9ECE5", bgInner: "#F4F6F1" },
        "midnight": { card: "#0F1522", card2: "#141B2B", card3: "#1F2942", popover: "#161E30", dialog: "#101726", bgOuter: "#05080F", bgMid: "#090E19", bgInner: "#121A2B" },
        "plum":     { card: "#1B1224", card2: "#22172D", card3: "#322341", popover: "#241930", dialog: "#1C1326", bgOuter: "#09050D", bgMid: "#110A18", bgInner: "#1F152A" },
        "forest":   { card: "#0F1613", card2: "#141D19", card3: "#1F2B25", popover: "#16201B", dialog: "#101814", bgOuter: "#050907", bgMid: "#090F0C", bgInner: "#131C17" },
        "contrast": { card: "#000000", card2: "#000000", card3: "#1C1C1C", popover: "#000000", dialog: "#000000", bgOuter: "#000000", bgMid: "#000000", bgInner: "#000000" }
    })
    // PushFPS redesign (after 4.2.1). `design` is "new" (what PushFPS shows unless Legacy is chosen) or "legacy", the interface every
    // version up to 4.2.1 had (preference pushDesign; the palette button's Appearance panel switches it).
    // The new design has its own graphite and its own light surfaces; every other theme (black, the
    // tinted ones, high contrast) is one table for both. In legacy nothing below differs from 4.2.1:
    // each token that has a new value is written as `modernTone ? new : what it was`.
    // The value here is "legacy": the main window (Main.qml) binds it to the preference, where no choice
    // means the new design. So a component used without the shell (every QML test) is drawn as it always
    // was unless the test asks for the new design.
    property var shot: ({})
    property string design: "legacy"
    readonly property bool modern: design !== "legacy"
    readonly property var surfacesModern: ({
        "default": { card: "#141A21", card2: "#1C242E", card3: "#28323F", popover: "#1A222B", dialog: "#141A21", bgOuter: "#0B0F14", bgMid: "#0B0F14", bgInner: "#0E131A" },
        "light":   { card: "#FFFFFF", card2: "#EDF1F5", card3: "#DDE4EB", popover: "#FFFFFF", dialog: "#FFFFFF", bgOuter: "#F5F7FA", bgMid: "#F5F7FA", bgInner: "#F5F7FA" }
    })
    // The surfaces of a theme as this design draws it (the pickers show a tile of each).
    function surfaceOf(id) { return (modern && surfacesModern[id]) || surfaces[id] || surfaces["default"] }
    readonly property var surface: surfaceOf(mode)
    // Whether this theme has the new design's own colours (graphite and light).
    readonly property bool modernTone: modern && surfacesModern[mode] !== undefined
    function ink(a) { return light ? Qt.rgba(0.07, 0.08, 0.06, Math.min(1, a * 1.15)) : contrast ? Qt.rgba(1, 1, 1, Math.min(1, a * 1.8)) : Qt.rgba(1, 1, 1, a) }
    // Text and icons drawn over the video picture or a dark chip stay white in every mode.
    readonly property color onVideo: "#FFFFFF"

    // --- surfaces (app.css .vy) -------------------------------------------
    readonly property color card: surface.card
    readonly property color card2: surface.card2
    readonly property color card3: surface.card3
    readonly property color popover: surface.popover
    readonly property color dialog: surface.dialog
    readonly property color stroke: modernTone ? (light ? "#D7DEE6" : "#252E39") : light ? Qt.rgba(0, 0, 0, 0.10) : contrast ? Qt.rgba(1, 1, 1, 0.55) : Qt.rgba(1, 1, 1, black ? 0.08 : 0.07)
    readonly property color stroke2: modernTone ? (light ? "#C2CBD6" : "#344050") : light ? Qt.rgba(0, 0, 0, 0.18) : contrast ? Qt.rgba(1, 1, 1, 0.85) : Qt.rgba(1, 1, 1, black ? 0.14 : 0.13)
    // The app background: a radial gradient in the prototype, approximated here
    // with the three stops it interpolates between.
    readonly property color bgOuter: surface.bgOuter
    readonly property color bgMid: surface.bgMid
    readonly property color bgInner: surface.bgInner
    // Veyra 2.1.9 palette API, kept so upstream components work unchanged. PushFPS has
    // its own appearance modes (above) instead of Veyra's dusk palette, so it is always
    // "classic" here: shade() returns the colour as written, no accent gradient.
    readonly property string paletteName: "classic"
    readonly property bool dusk: false
    readonly property color videoBlack: "#000000"
    // Dark neutrals some components still spell out (the cinema pill, the node
    // canvas, the dock): exactly that colour in the classic palette, the same
    // depth tinted indigo-violet in the dusk one.
    function shade(r, g, b, a) {
        const alpha = a === undefined ? 1 : a
        return dusk ? Qt.rgba(Math.min(255, r + 2) / 255, Math.max(0, g - 1) / 255, Math.min(255, b + 14) / 255, alpha)
                    : Qt.rgba(r / 255, g / 255, b / 255, alpha)
    }

    // --- text -------------------------------------------------------------
    readonly property color t1: modernTone ? (light ? "#17202B" : "#F4F7FA") : light ? "#15171A" : contrast ? "#FFFFFF" : "#F3F3F5"
    readonly property color t2: modernTone ? (light ? "#485566" : "#B7C1CD") : light ? Qt.rgba(0.08, 0.09, 0.10, 0.74) : contrast ? Qt.rgba(1, 1, 1, 0.92) : Qt.rgba(0.953, 0.953, 0.961, 0.62)
    // PushFPS: secondary text a little brighter than the design's .38, so hints
    // stay readable on the dark cards (contrast about 4.5:1 on card2).
    readonly property color t3: modernTone ? (light ? "#5A6676" : "#93A0AF") : light ? Qt.rgba(0.08, 0.09, 0.10, 0.62) : contrast ? Qt.rgba(1, 1, 1, 0.82) : Qt.rgba(0.953, 0.953, 0.961, 0.48)
    // Row labels: between t1 and t2, so a label reads before its hint.
    readonly property color tLabel: modernTone ? (light ? "#1F2A36" : "#E6EBF1") : light ? Qt.rgba(0.08, 0.09, 0.10, 0.90) : contrast ? "#FFFFFF" : Qt.rgba(0.953, 0.953, 0.961, 0.88)
    readonly property real fsHint: 11.5

    // --- accent and state -------------------------------------------------
    // 设置 → 强调色: PushFPS green (default) or the design's .accent-white variant.
    // The saved preference value stays "orange" (Veyra's name for the default) so
    // existing settings files keep working; PushFPS draws it in its brand green.
    property string accentName: "orange"
    readonly property bool accentWhite: accentName === "white"
    // Light mode: a deeper green (and near-black for the white variant), since the
    // brand green is too light for text and icons on white.
    // Five more accents (after 4.1). Each has a bright form for dark backgrounds, a deeper one for the light
    // background, and the colour of text drawn on it.
    readonly property var accents: ({
        "orange": { dark: "#89F52A", light: "#2F7A06", inkDark: "#0D1A04", inkLight: "#FFFFFF" },
        "white":  { dark: "#F3F3F5", light: "#15171A", inkDark: "#0B0B0E", inkLight: "#FFFFFF" },
        "ice":    { dark: "#5CE1E6", light: "#0B6F7B", inkDark: "#04191B", inkLight: "#FFFFFF" },
        "blue":   { dark: "#6EA8FF", light: "#1E5FD6", inkDark: "#071631", inkLight: "#FFFFFF" },
        "violet": { dark: "#B79BFF", light: "#5B3FC4", inkDark: "#150B33", inkLight: "#FFFFFF" },
        "rose":   { dark: "#FF86B8", light: "#B4246B", inkDark: "#33081B", inkLight: "#FFFFFF" },
        "amber":  { dark: "#FFC857", light: "#8A5A00", inkDark: "#2A1B00", inkLight: "#FFFFFF" }
    })
    readonly property var accentPair: accents[accentName] || accents["orange"]
    readonly property color accent: light ? accentPair.light : accentPair.dark
    // The logo's green, also with the white accent (the PushFPS wordmark's "FPS").
    readonly property color brandGreen: light ? "#2F7A06" : "#89F52A"
    readonly property color accentInk: light ? accentPair.inkLight : accentPair.inkDark
    readonly property color accentSoft: accentWhite ? ink(0.1) : Qt.rgba(accent.r, accent.g, accent.b, light ? 0.12 : 0.14)
    readonly property color accentGlow: accentWhite ? ink(0.35) : Qt.rgba(accent.r, accent.g, accent.b, light ? 0.30 : 0.35)
    // 设置 → 背景渐变强度: 0 none, 1 light (the design default), 2 medium.
    property int backdropLevel: 1
    readonly property color ok: modernTone ? (light ? "#147D49" : "#52D98B") : light ? "#17824A" : "#3DDC84"
    readonly property color okGlow: Qt.rgba(0.239, 0.863, 0.518, 0.16)
    readonly property color warn: modernTone ? (light ? "#946100" : "#F0BD55") : light ? "#9A6200" : "#F5C84B"
    // Veyra 2.1.9 accent-gradient API (VAccentFill.qml): a plain fill in PushFPS.
    readonly property bool accentGradient: false
    readonly property color accentFrom: accent
    readonly property color accentTo: accent
    readonly property color accentFar: accent
    readonly property string backdropImage: "backdrop" + (backdropLevel === 2 ? "-strong" : "") + ".png"
    // PushFPS logo (the chevrons, scripts/brand/make_icon.py) in every mode.
    readonly property string logoImage: "logo.png"
    readonly property color warnGlow: Qt.rgba(0.961, 0.784, 0.294, 0.16)
    readonly property color err: modernTone ? (light ? "#B62F35" : "#FF7373") : light ? "#C62828" : "#FF5D5D"
    readonly property color errGlow: Qt.rgba(1, 0.365, 0.365, 0.16)
    readonly property color exp: light ? "#5B3FC4" : "#B79BFF"
    readonly property color expSoft: Qt.rgba(0.718, 0.608, 1.0, 0.14)

    // --- geometry (--r-win / --r-card / --r-ctl) --------------------------
    // The window radius is 8px, small, because Windows' own default is small.
    // The video surface is NEVER rounded: .video in the prototype carries
    // border-radius:0!important because rounding crops the picture.
    readonly property int rWindow: 8
    readonly property int rCard: modern ? 12 : 14
    readonly property int rCtl: modern ? 8 : 10
    // The new design's shell (Main.qml): the navigation's width with labels and as icons, the title
    // bar and the status strip.
    readonly property int shellNav: 208
    readonly property int shellNavSlim: 60
    readonly property int shellHeader: 46
    readonly property int shellStatus: 34

    // --- motion (--spring / --spring-soft / --out) ------------------------
    // easing.bezierCurve takes whole cubic segments, six numbers each
    // [c1x, c1y, c2x, c2y, endX, endY], the last one ending at (1, 1). Any other
    // length is rejected and the animation silently runs linear (G0.5/G0.6 caught
    // the old four-number curves doing exactly that).
    // --spring and --spring-soft are CSS linear() functions: straight lines between
    // stops. A cubic with its control points at 1/3 and 2/3 of a line is that line,
    // so each stop becomes one segment and the curve matches the design exactly,
    // overshoot included. Stops without a position are spread evenly, as in CSS.
    // At most 10 segments: Qt 6.8.3 corrupts the heap when a BezierSpline curve of
    // 11+ segments is destroyed (reproduced in C++ with no QML, G1.1). --spring has
    // 12, so its 1.035@28.4% and .998@38.5% stops are dropped; the remaining line
    // stays within 0.007 of the design curve (veyra_qml_easing_tests checks 0.01).
    // Reduced motion (the design's .vy.reduced: every transition and animation at
    // 0s). Main.qml binds it to the saved setting or the --reduced-motion switch;
    // every duration in the app goes through d(), so one flag covers all of them.
    // Endless animations also stop on it: a zero-length loop would spin.
    property bool reduced: false
    function d(ms) { return reduced ? 0 : ms }

    readonly property int durFast: 150
    readonly property int durNormal: 240
    readonly property int durSlow: 450
    function linearEasing(stops) {
        var out = []
        for (var i = 1; i < stops.length; ++i) {
            var a = stops[i - 1], b = stops[i]
            out.push(a[0] + (b[0] - a[0]) / 3, a[1] + (b[1] - a[1]) / 3,
                     a[0] + (b[0] - a[0]) * 2 / 3, a[1] + (b[1] - a[1]) * 2 / 3,
                     b[0], b[1])
        }
        return out
    }
    // linear(0,.009,.035 2.1%,.141 4.4%,.723 12.9%,.938 16.7%,1.017 20.2%,1.043 24%,
    //        1.035 28.4%,.998 38.5%,.99 44.1%,1.001 60.7%,1)
    readonly property var spring: linearEasing([
        [0, 0], [0.0105, 0.009], [0.021, 0.035], [0.044, 0.141], [0.129, 0.723],
        [0.167, 0.938], [0.202, 1.017], [0.24, 1.043],
        [0.441, 0.99], [0.607, 1.001], [1, 1]])
    // linear(0,.02,.08 3.2%,.3 7.5%,.84 17%,1.01 25%,1.025 31%,1.004 45%,1)
    readonly property var springSoft: linearEasing([
        [0, 0], [0.016, 0.02], [0.032, 0.08], [0.075, 0.3], [0.17, 0.84],
        [0.25, 1.01], [0.31, 1.025], [0.45, 1.004], [1, 1]])
    // cubic-bezier(.2,.8,.2,1)
    readonly property var easeOut: [0.2, 0.8, 0.2, 1.0, 1.0, 1.0]

    // --- type (--f-ui / --f-mono and the .h1/.h2/.h3 scale) ---------------
    // --f-ui is "Geist", "Noto Sans SC", ...: Geist is not on this machine (D4), so
    // the design's own second choice, which is also what Edge rendered the reference
    // with. Noto Sans SC ships as a variable font; Qt 6.8.3 maps font.weight 600 to a
    // thin instance of it (500 and 700 are fine), so every DemiBold also sets the wght
    // axis directly (G1.3, tmp/.../min/Font.qml).
    // Per interface language (Qt.uiLanguage, set by the bridge): Japanese kana and kanji
    // and Traditional characters read best in their own Windows UI fonts.
    // Korean uses Windows' Hangul UI font instead of Simplified Chinese glyph fallback.
    readonly property string fontUi: Qt.uiLanguage === "ja" ? "Yu Gothic UI"
                                   : Qt.uiLanguage === "ko" ? "Malgun Gothic"
                                   : Qt.uiLanguage === "zh-TW" ? "Microsoft JhengHei UI"
                                   : Qt.uiLanguage === "ar" ? "Segoe UI" : "Noto Sans SC"
    readonly property var axesDemiBold: ({ "wght": 600 })
    // 500: the design's .seek .peek time strip is `font: 500 11px/1 var(--f-mono)`.
    // Same reason as DemiBold - 600 lands on a thin instance, so the axis is set
    // explicitly rather than left to font.weight.
    readonly property var axesMedium: ({ "wght": 500 })
    readonly property string fontMono: "Consolas"
    readonly property real fsEyebrow: 11
    readonly property real fsSmall: 11.5
    readonly property real fsBody: 12.5
    readonly property real fsH3: 15
    readonly property real fsH2: 18
    readonly property real fsH1: 26

    // --- component metrics from the CSS -----------------------------------
    readonly property int ctlHeight: 32      // .btn / .pill
    readonly property int rowMinHeight: 40   // .row (PushFPS: 38 + room to breathe)
    readonly property int switchWidth: 34    // .sw
    readonly property int switchHeight: 20
    readonly property int switchKnob: 14
    readonly property int sliderKnob: 13
    readonly property int tagHeight: 20
    readonly property int dialogWidth: 640
    readonly property int popoverMinWidth: 220
}
