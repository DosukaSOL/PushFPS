// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// PushFPS for Linux and macOS: the Windows build's new design around the preview's pages. A title bar,
// the navigation and a status strip (the Windows build's own files, qml/Veyra) stand around four pages:
// home, Professional mode, the picture alone (min) and settings.
//
// There is one picture item. It is put into the page being shown, at the place that page keeps for it
// (`videoViewport`); a page without one (settings) leaves it hidden. So going from page to page never
// stops what plays: only Disconnect does.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Shapes
import QtQuick.Window
import PushFPS
import "PushShell.js" as PushShell
import "PushCrop.js" as Crop

Window {
    id: root
    width: 1280
    height: 800
    minimumWidth: 720
    minimumHeight: 520
    visible: true
    title: "PushFPS"
    color: Theme.bgOuter
    flags: Qt.Window | Qt.FramelessWindowHint | (app.alwaysOnTop ? Qt.WindowStaysOnTopHint : 0)

    // The interface choices (Settings → Appearance), under the Windows build's names.
    readonly property var prefs: veyra.preferences || ({})
    Binding { target: Theme; property: "design"; value: "new" }
    Binding { target: Theme; property: "mode"; value: root.prefs.pushTheme || "default" }
    Binding { target: Theme; property: "accentName"; value: root.prefs.accent || "orange" }
    Binding { target: Theme; property: "reduced"; value: root.prefs.reducedMotion === true }

    // ---- pages ----
    readonly property var startPages: ["home", "pro", "min", "set"]
    property string page: startPages.indexOf(app.startPage) >= 0 ? app.startPage : "home"
    // Where the picture alone goes back to.
    property string pageBefore: "home"
    function go(p) {
        if (p === page) return
        if (p === "min" && page !== "min") pageBefore = page
        if (p !== "min" && fullscreen) showNormal()
        page = p
    }
    function pageName(p) {
        switch (p) {
        case "home": return qsTr("首页")
        case "pro": return qsTr("专业模式")
        case "min": return qsTr("极简")
        case "set": return qsTr("设置")
        }
        return ""
    }

    readonly property bool fullscreen: visibility === Window.FullScreen
    readonly property bool maximized: visibility === Window.Maximized
    function toggleMaximized() { visibility = maximized ? Window.Windowed : Window.Maximized }
    // Full screen is the picture alone, as in the Windows build.
    function toggleFullscreen() {
        if (fullscreen) { showNormal(); return }
        go("min")
        showFullScreen()
    }
    function leavePicture() {
        if (fullscreen) showNormal()
        page = pageBefore === "min" ? "home" : pageBefore
    }

    // A source connected from a page without a picture (the home page in one column) shows the picture alone.
    Connections {
        target: app
        function onRunningChanged() {
            if (app.running && root.page === "home" && !homePage.threePane) root.go("min")
        }
    }

    // ---- the shell ----
    readonly property bool shellShown: !fullscreen && page !== "min"
    readonly property bool navForcedSlim: PushShell.navForcedSlim(width)
    readonly property bool navCollapsed: prefs.pushNavCollapsed === true
    readonly property bool navSlim: PushShell.navSlim(width, prefs)
    readonly property bool sideCollapsed: PushShell.sideCollapsed(prefs)
    readonly property bool sideOffered: PushShell.sideOffered(page, homePage.threePane)
    readonly property real shellLeft: shellShown ? (navSlim ? Theme.shellNavSlim : Theme.shellNav) : 0
    readonly property real shellTop: shellShown ? Theme.shellHeader : 0
    readonly property real shellBottom: shellShown ? Theme.shellStatus : 0

    // The window's outline (frameless, as in the Windows build).
    Rectangle {
        anchors.fill: parent
        visible: !root.fullscreen && !root.maximized
        color: "transparent"
        border.width: 1
        border.color: Theme.stroke2
        z: 120
    }

    Item {
        id: pages
        anchors.fill: parent
        anchors.leftMargin: root.shellLeft + (root.shellShown ? 0 : 0)
        anchors.topMargin: root.shellTop
        anchors.bottomMargin: root.shellBottom
        anchors.rightMargin: root.shellShown ? 1 : 0
        HomePage {
            id: homePage
            anchors.fill: parent
            visible: root.page === "home"
            sideCollapsed: root.sideCollapsed
            onRequestFullscreen: root.toggleFullscreen()
            onRequestScreenshot: root.screenshot()
            onRequestCheckSetup: checkSetup.open()
        }
        ProPage {
            id: proPage
            anchors.fill: parent
            visible: root.page === "pro"
            sideCollapsed: root.sideCollapsed
            onRequestFullscreen: root.toggleFullscreen()
            onRequestScreenshot: root.screenshot()
            onRequestCheckSetup: checkSetup.open()
            onRequestSources: root.go("home")
        }
        MinimalPage {
            id: minPage
            anchors.fill: parent
            visible: root.page === "min"
            fullscreen: root.fullscreen
            onRequestBack: root.leavePicture()
            onRequestFullscreen: root.toggleFullscreen()
            onRequestScreenshot: root.screenshot()
        }
        SettingsPage {
            id: setPage
            anchors.fill: parent
            visible: root.page === "set"
            onRequestCheckSetup: checkSetup.open()
        }
    }
    // Check my setup (CheckSetupDialog.qml), opened from the home page, Professional mode and Settings.
    CheckSetupDialog { id: checkSetup; parent: Overlay.overlay }
    CropDialog { id: cropDialog; parent: Overlay.overlay }
    // A dialog asked for by a part that cannot reach this window (app.openDialog).
    Connections {
        target: app
        function onDialogRequested(key) {
            if (key === "crop") cropDialog.open()
            else if (key === "diagnose") checkSetup.open()
        }
    }
    // Crop and resize (PushCrop.js): for a capture card's picture, and the test pattern that stands in for
    // one (the Windows build crops a console picture, its Frame Input test pattern included, never a file).
    readonly property var crop: Crop.read(root.prefs)
    readonly property bool cropShown: root.crop.on && !Crop.neutral(root.crop)
                                      && (app.sourceKind === "camera" || app.sourceKind === "test")

    // ---- the one picture ----
    readonly property Item viewport: page === "home" ? homePage.videoViewport
                                   : page === "pro" ? proPage.videoViewport
                                   : page === "min" ? minPage.videoViewport : null
    Item { id: pictureHolder; visible: false }
    FrameView {
        id: picture
        objectName: "player-view"
        parent: root.viewport || pictureHolder
        anchors.fill: parent
        z: 0
        running: root.viewport !== null && app.running
        brightness: app.brightness
        contrast: app.contrast
        saturation: app.saturation
        gamma: app.gamma
        fill: app.fillMode
        keep: root.cropShown ? Qt.rect(root.crop.l / 1000, root.crop.t / 1000, (1000 - root.crop.l - root.crop.r) / 1000,
                                       (1000 - root.crop.t - root.crop.b) / 1000) : Qt.rect(0, 0, 1, 1)
        zoom: root.cropShown ? root.crop.zoom / 100 : 1
        shift: root.cropShown ? Qt.point(root.crop.x / 1000, root.crop.y / 1000) : Qt.point(0, 0)
        // HUD protection boxes (HudDialog.qml, game presets): the older real frame inside them in generated frames.
        protect: app.hudRegions
        protectFeather: app.hudFeather
        // Upscaling with NVIDIA Image Scaling (NisNode.h); the Windows build's names.
        upscale: app.srEnabled && app.videoSrQuality === 6
        upscaleTarget: app.srTargetIndex
        sharpness: (root.prefs.nisSharpness === undefined ? 50 : Number(root.prefs.nisSharpness)) / 100
        // The live comparison (Professional mode → Display), as the Windows build's.
        compare: app.compareMode
        compareSplit: app.compareSplit
        compareSwap: root.prefs.pushCompareSwap === true
        // The split's line follows the left button anywhere on the picture, as on Windows; only while
        // the split is shown, so every other press on the picture is what it was.
        MouseArea {
            objectName: "player-compare-drag"
            anchors.fill: parent
            enabled: app.compareMode === 2 && app.running
            acceptedButtons: Qt.LeftButton
            cursorShape: enabled ? Qt.SplitHCursor : Qt.ArrowCursor
            function move(x) { if (width > 0) app.compareSplit = Math.max(0, Math.min(1, x / width)) }
            onPressed: mouse => move(mouse.x)
            onPositionChanged: mouse => { if (pressed) move(mouse.x) }
        }
        // The latency readout over the picture, as the Windows build's (PushLatencyBadge.qml): dragged
        // anywhere on it, in Professional mode and on the picture alone (not on the home page, as on Windows).
        PushLatencyBadge {
            z: 2
            areaWidth: picture.width
            areaHeight: picture.height
            visible: wanted && stats.running === true && picture.width > 200 && (root.page === "pro" || root.page === "min")
        }
    }
    // What upscaling does, for the picture panel's hint (the picture is drawn on another thread).
    Binding { target: app; property: "upscaleState"; value: picture.upscaleState }
    Binding { target: app; property: "upscaleSizes"; value: picture.upscaleSizes }
    function screenshot() {
        if (!app.running || root.viewport === null) return
        const path = app.newScreenshotPath()
        picture.grabToImage(function(result) { if (result.saveToFile(path)) savedNote.show() })
    }

    PushTitleBar {
        id: shellBar
        visible: root.shellShown
        x: 1; y: 1
        width: parent.width - 2
        height: Theme.shellHeader - 1
        pageName: root.pageName(root.page)
        maximized: root.maximized
        onRequestMinimize: root.showMinimized()
        onRequestMaximize: root.toggleMaximized()
        onRequestClose: root.close()
        onRequestMove: root.startSystemMove()
        navCollapsed: root.navCollapsed
        navToggleShown: !root.navForcedSlim
        sideCollapsed: root.sideCollapsed
        sideToggleShown: root.sideOffered
        onRequestToggleNav: veyra.setPreference("pushNavCollapsed", !root.navCollapsed)
        onRequestToggleSide: veyra.setPreference("pushSideCollapsed", !root.sideCollapsed)
    }
    PushSidebar {
        id: shellNav
        visible: root.shellShown
        x: 1; y: Theme.shellHeader
        width: root.shellLeft - 1
        height: parent.height - Theme.shellHeader - 1
        slim: root.navSlim
        currentPage: root.page
        entries: [
            { id: "home", icon: "home",     label: qsTr("首页") },
            { id: "pro",  icon: "sliders",  label: qsTr("专业模式") },
            { id: "min",  icon: "tv",       label: qsTr("极简") },
            { id: "set",  icon: "settings", label: qsTr("设置") }
        ]
        onRequestPage: p => root.go(p)
        onRequestAppearance: { root.go("set"); setPage.showAppearance() }
    }
    PushStatusStrip {
        id: shellStatus
        visible: root.shellShown
        x: root.shellLeft; y: parent.height - Theme.shellStatus
        width: parent.width - root.shellLeft - 1
        height: Theme.shellStatus - 1
    }

    // "Screenshot saved": a press opens the folder.
    Rectangle {
        id: savedNote
        objectName: "screenshot-saved"
        function show() { opacity = 1; noteTimer.restart() }
        z: 110
        opacity: 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.d(200) } }
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.shellBottom + 24
        anchors.horizontalCenter: parent.horizontalCenter
        radius: 8
        color: Qt.rgba(0, 0, 0, 0.75)
        implicitWidth: note.implicitWidth + 24
        implicitHeight: note.implicitHeight + 12
        Text {
            id: note
            anchors.centerIn: parent
            color: "white"
            font.family: Theme.fontUi
            font.pixelSize: 13
            text: qsTr("截图已保存")
        }
        TapHandler { onTapped: app.openScreenshotFolder() }
        HoverHandler { cursorShape: Qt.PointingHandCursor }
        Timer { id: noteTimer; interval: 2500; onTriggered: savedNote.opacity = 0 }
    }

    // Resizing the frameless window from its edges and corners.
    Repeater {
        model: [
            { e: Qt.LeftEdge, c: Qt.SizeHorCursor }, { e: Qt.RightEdge, c: Qt.SizeHorCursor },
            { e: Qt.TopEdge, c: Qt.SizeVerCursor }, { e: Qt.BottomEdge, c: Qt.SizeVerCursor },
            { e: Qt.LeftEdge | Qt.TopEdge, c: Qt.SizeFDiagCursor }, { e: Qt.RightEdge | Qt.BottomEdge, c: Qt.SizeFDiagCursor },
            { e: Qt.RightEdge | Qt.TopEdge, c: Qt.SizeBDiagCursor }, { e: Qt.LeftEdge | Qt.BottomEdge, c: Qt.SizeBDiagCursor }
        ]
        delegate: MouseArea {
            required property var modelData
            readonly property bool l: (modelData.e & Qt.LeftEdge) !== 0
            readonly property bool r: (modelData.e & Qt.RightEdge) !== 0
            readonly property bool t: (modelData.e & Qt.TopEdge) !== 0
            readonly property bool b: (modelData.e & Qt.BottomEdge) !== 0
            readonly property bool corner: (l || r) && (t || b)
            z: 130
            enabled: !root.fullscreen && !root.maximized
            visible: enabled
            width: corner ? 12 : (l || r) ? 6 : root.width - 24
            height: corner ? 12 : (t || b) ? 6 : root.height - 24
            x: l ? 0 : r ? root.width - width : 12
            y: t ? 0 : b ? root.height - height : 12
            cursorShape: modelData.c
            onPressed: root.startSystemResize(modelData.e)
        }
    }

    Shortcut { sequences: ["F11", "Ctrl+F"]; onActivated: root.toggleFullscreen() }
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (root.fullscreen) root.showNormal()
            else if (root.page === "min") root.leavePicture()
        }
    }
    // The latency readout: F3 as the Windows build's default key, and Tab on the picture alone as before.
    function toggleLatency() { veyra.setPreference("pushLatencyOverlay", (veyra.preferences || {}).pushLatencyOverlay !== true) }
    Shortcut { objectName: "pushfps-key-latency"; sequence: "F3"; autoRepeat: false; onActivated: root.toggleLatency() }
    Shortcut { sequence: "Tab"; enabled: root.page === "min"; onActivated: root.toggleLatency() }
}
