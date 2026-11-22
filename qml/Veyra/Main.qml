// The application shell, rebuilt from the approved design.
//
// Structure comes from the prototype (index.html + app.css + board.js):
//   * a frameless window with the small 8px radius;
//   * a hidden top dock that drops in when the pointer reaches the top edge;
//   * one page visible at a time: home / min / pro / node / exp / set;
//   * in cinema mode the WINDOW ITSELF snaps to the film's aspect ratio so the
//     picture has no letterbox bars. The prototype's own technical note says the
//     window must resize to the film when a file opens (capture cards and games
//     are 16:9). The separate control window straddles its lower edge.
//
// The video is not in this scene graph. It stays the native D3D12 child window
// that apps/veyra-qml/main.cpp places over the item named "videoHost"; Qt hosts
// it and never composites it, which is what preserved present cost in S3.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "ExportLayout.js" as ExportLayout
import "PushShell.js" as PushShell

Window {
    id: root
    width: 1280
    height: 800
    // 极简 follows the film, and a portrait film needs a narrow window (the dock is
    // about 330px wide).
    minimumWidth: cinema ? 360 : 720
    minimumHeight: 260
    visible: !test.portableUpdateProbe
    // The software scene graph cannot draw the Shape background. Keep the native
    // window opaque as well, including while its first frame is being produced.
    color: typeof vySoftwareUi !== "undefined" && vySoftwareUi ? (cinema ? Theme.videoBlack : Theme.bgOuter) : "transparent"
    // 游戏模式 click-through (ui/GamePassthrough.h): Qt's own flags, so Qt keeps the
    // window's layered/transparent/no-activate styles in both interface renderers.
    flags: Qt.Window | Qt.FramelessWindowHint
           | (veyra.passthroughActive ? Qt.WindowTransparentForInput : 0)
           | (veyra.gameInputActive ? Qt.WindowDoesNotAcceptFocus : 0)
    title: veyra.appName + " " + veyra.version

    // Page ids match the prototype's PAGES keys so a design screenshot and an
    // app screenshot can be compared under the same name.
    property string page: "home"
    readonly property bool onboardingActive: onboarding.visible
    // The node page splitter: height of its video card (picture + 40px bar).
    property real nodeVideoHeight: 330
    // Every navigation goes through here: "专业" means the node page while the
    // node configuration is active, so the dock never drops into the list UI.
    function goPageTarget(p) { return p === "pro" && veyra.nodeMode === 1 ? "node" : p }
    function goPage(p) { page = goPageTarget(p) }
    // Test switches from the command line (main.cpp, G0.3); empty in normal use.
    readonly property var test: typeof vyTest !== "undefined" ? vyTest : ({})
    readonly property real testAspect: test.aspect !== undefined ? test.aspect : 0
    // PushFPS (screenshots only, --compare <0..1>): the live split comparison at that place once something plays.
    Timer {
        interval: 400; repeat: true
        running: root.test.compare !== undefined
        onTriggered: if (veyra.hasSource && veyra.compareMode !== 2) { veyra.compareMode = 2; veyra.setCompareSplitAt(Number(root.test.compare)) }
    }
    // PushFPS redesign (screenshots only, --tab home-preview with a source on the command line): back to the
    // home page once the picture plays, to photograph the new home page with its live preview.
    Timer {
        interval: 3000
        repeat: true
        running: root.test.tab === "home-preview"
        onTriggered: if (veyra.hasSource && root.page !== "home") root.goPage("home")
    }
    readonly property bool cinema: page === "min"
    // Maximized behaves like fullscreen for geometry: the screen owns the size,
    // so the cinema aspect rule must not fight the window manager.
    readonly property bool maximized: visibility === Window.Maximized
    // Use visibility directly: leaving fullscreen clears fullTarget before the
    // Maximized binding settles. The cinema animation must not resize a screen
    // sized window during that transition.
    readonly property bool screenSized: fullTarget || fullRestorePending || visibility === Window.FullScreen || visibility === Window.Maximized
    function toggleMaximized() {
        // The dock's restore button must also leave fullscreen, using the same
        // saved window state and cleanup as F11 / the transport button.
        if (fullscreen) { toggleFullscreen(); return }
        visibility = maximized ? Window.Windowed : Window.Maximized
        veyra.logUi("ui-window", "maximized=" + (visibility === Window.Maximized))
    }
    Binding { target: Theme; property: "reduced"; value: veyra.reducedMotion || root.test.reducedMotion === true }
    Binding { target: Theme; property: "accentName"; value: veyra.preferences.accent || "orange" }
    // PushFPS: Veyra 2.1.9's dusk palette is not used (PushFPS has its own appearance modes,
    // Theme.mode); Theme.paletteName is fixed to "classic".
    Binding { target: Theme; property: "backdropLevel"; value: veyra.preferences.backdrop !== undefined ? veyra.preferences.backdrop : 1 }
    // PushFPS appearance: default, black or light (设置 → 外观, or --theme for screenshots).
    Binding { target: Theme; property: "mode"; value: root.test.theme !== undefined ? root.test.theme : (veyra.preferences.pushTheme || "default") }
    // PushFPS redesign: the new design unless Legacy was chosen (the palette button's Appearance panel,
    // preference pushDesign; --pref pushDesign=legacy for screenshots).
    Binding { target: Theme; property: "design"; value: veyra.preferences.pushDesign === "legacy" ? "legacy" : "new" }
    // The new design's shell: a title bar, the named navigation and a status strip stand around the
    // pages, which keep their ids, their contents and their own picture rectangles (videoRect adds
    // where the pages stand). Not in cinema, where the picture is the window, and not in full screen:
    // there everything is as it always was, the dock included.
    readonly property bool shellShown: Theme.modern && !fullscreen && page !== "min" && test.portableUpdateProbe !== true
    // The navigation shows its icons alone in a narrow window, and (4.3.1) wherever the title bar's
    // button folded it. The button is not offered where the window's width has already decided.
    // (The rules are PushShell.js, which is tested without Qt: no test loads this window.)
    readonly property bool navForcedSlim: PushShell.navForcedSlim(width)
    readonly property bool navCollapsed: Theme.shot.pushNavCollapsed === true
    readonly property bool navSlim: PushShell.navSlim(width, Theme.shot)
    // (4.3.1) The settings side of a page: quick setup on the home page's three panes, the panel in
    // Professional mode. One choice for both, made with the title bar's other button.
    readonly property bool sideCollapsed: PushShell.sideCollapsed(Theme.shot)
    readonly property bool sideOffered: PushShell.sideOffered(page, homePage.threePane)
    // (4.3.1) A capture card connected from the new home page plays there: that page shows the
    // picture now. Not while full screen on connecting is on (Professional mode is the page that
    // goes full screen with its panel over the picture), and not where a user asked for Professional
    // mode. Legacy and a window too narrow for the three panes have no picture on the home page.
    readonly property bool staysHomeOnConnect: PushShell.staysHomeOnConnect(Theme.modern, homePage.threePane, veyra.preferences)
    readonly property real shellLeft: shellShown ? (navSlim ? Theme.shellNavSlim : Theme.shellNav) : 0
    readonly property real shellTop: shellShown ? Theme.shellHeader : 0
    readonly property real shellBottom: shellShown ? Theme.shellStatus : 0
    onShellShownChanged: Qt.callLater(videoHost.syncRect)
    onNavSlimChanged: Qt.callLater(videoHost.syncRect)
    function pageName(p) {
        switch (p) {
        case "home": return qsTr("首页")
        case "pro": return qsTr("专业模式")
        case "node": return qsTr("节点模式")
        case "live": return qsTr("直播模式")
        case "watch": return qsTr("视频播放模式")
        case "exp": return qsTr("导出")
        case "set": return qsTr("设置")
        default: return ""
        }
    }

    // Cinema geometry: the main window ends at the picture. The independent pill
    // can extend below it without leaving an opaque strip in this window.
    // 16:9 until a source reports its own shape: a PS5 / capture session has no
    // aspect while it connects, and 2.39 then left a thin black strip.
    property real filmAspect: 16 / 9
    readonly property bool gameAligned: fullscreen && veyra.gameMode && !veyra.gameScaledPreview
    readonly property real reportedAspect: veyra.previewAspect
    onReportedAspectChanged: if (reportedAspect < 0.2) fitToFilm(16 / 9)
    property string lastPage: "home"
    property real heightBeforeCinema: 0
    // The tallest picture that still fits on the screen with the pill (46px) under
    // it. A portrait video or a captured portrait window used to make the window
    // taller than the screen (field report 2026-10-01).
    // The screen under the window's top centre (Screen.desktopAvailable* is the whole
    // virtual desktop, wrong on multi-monitor setups).
    readonly property rect screenArea: veyra.screenAvailableAt(root.x + root.width / 2, root.y + 20)
    readonly property real maxPictureHeight: Math.max(200, root.screenArea.height - 46 - 16)
    readonly property real pictureHeight: Math.min(Math.round(width / filmAspect), maxPictureHeight)
    // The width a tall film took away, given back when a wider film fits again.
    property real widthBeforeNarrow: 0

    function fitToFilm(aspect) {
        if (aspect > 0.2 && aspect < 5.0) filmAspect = aspect
        // The page, not `cinema`: onPageChanged calls this before that binding updates.
        if (page !== "min" || screenSized) return
        // Narrow the window for a film too tall for the screen, keeping its centre.
        let w = widthBeforeNarrow > 0 ? widthBeforeNarrow : width
        if (Math.round(w / filmAspect) > maxPictureHeight)
            w = Math.max(minimumWidth, Math.round(maxPictureHeight * filmAspect))
        if (Math.abs(w - width) >= 1) {
            if (widthBeforeNarrow <= 0) widthBeforeNarrow = width
            x += Math.round((width - w) / 2)
            width = w
        }
        if (widthBeforeNarrow > 0 && w >= widthBeforeNarrow) widthBeforeNarrow = 0
        height = Math.round(pictureHeight)
        keepPillOnScreen()
    }
    // The pill hangs 46px below the picture: lift the window if that leaves the screen.
    function keepPillOnScreen() {
        const a = root.screenArea
        const bottom = a.y + a.height
        if (y + pictureHeight + 46 + 8 > bottom) y = Math.max(a.y, bottom - pictureHeight - 46 - 8)
    }
    // .vy.cine transition: height .7s var(--spring-soft)
    Behavior on height {
        enabled: root.cinema && !root.screenSized
        NumberAnimation { duration: Theme.d(700); easing.bezierCurve: Theme.springSoft }
    }
    onWidthChanged: {
        if (cinema && !screenSized) height = Math.round(pictureHeight)
        videoHost.syncRect()
    }
    // The window height animates towards the film's aspect, so the picture area has
    // to follow every step of that animation. Without this the host kept its
    // previous height and the picture sat inside black bars - exactly what cinema
    // mode exists to avoid.
    onHeightChanged: videoHost.syncRect()
    onMaximizedChanged: {
        if (visibility === Window.Windowed && cinema && !fullTarget && !fullRestorePending) height = Math.round(pictureHeight)
        videoHost.syncRect()
    }
    onPageChanged: {
        if (onboarding.visible) onboarding.close()
        if (page === "node" && veyra.nodeMode !== 1) {
            veyra.nodeMode = 1
            if (veyra.nodeMode !== 1) { page = "pro"; return }
        }
        // Tell the bridge where the user is, so a command like "open a file" can
        // behave differently from home than from the professional page.
        if (veyra.currentPage !== page) veyra.currentPage = page
        // A page the user picks while the Home panel is up ends it there: closing
        // the panel later does not snap back to the page it was opened from.
        if (quickPanel && !instantPage) {
            quickReturnPage = ""
            if (page !== "pro" && page !== "node") quickPanel = false
        }
        // Test the page itself: the cinema binding may not have caught up with
        // this change yet, and a stale value shrank the page after min instead.
        // Leaving 极简 gives the window back the height it had before (core.js
        // clears the cinema height on any other page).
        const wasCinema = lastPage === "min"
        lastPage = page
        if (!screenSized && wasCinema && page !== "min" && widthBeforeNarrow > 0) {
            // A tall film narrowed the window; the other pages get their width back.
            x -= Math.round((widthBeforeNarrow - width) / 2)
            width = widthBeforeNarrow
            widthBeforeNarrow = 0
        }
        if (!screenSized && page !== "min" && width < 720) { x -= Math.round((720 - width) / 2); width = 720 }
        if (screenSized) { /* the screen is the window */ }
        else if (page === "min") { if (!wasCinema) heightBeforeCinema = height; fitToFilm(filmAspect) }
        else if (wasCinema && heightBeforeCinema >= 400) height = heightBeforeCinema
        else if (height < 600) height = 800
        // core.js app.go: the old page sinks (.22s) and the new one shows 150 ms
        // later with its [data-in] items rising. Reduced motion shows it at once.
        const prev = shownPage
        if (prev === page) return
        leavingPage = Theme.reduced || prev === "" || instantPage ? "" : prev
        if (prev === "home" || prev === "set") videoHost.syncRect()
        if (leavingPage !== "") { leaveAnim.restart(); showTimer.restart() }
        else showPage(false)
    }
    // The page on screen (entering or settled) and the one sinking out.
    property string shownPage: "home"
    readonly property var pageItems: [homePage, minPage, proPage, nodePage, exportPage, settingsPage]
    property string leavingPage: ""
    function showPage(animated) {
        shownPage = page
        if (animated)
            // By list, not pages.children: hidden pages are out of the scene (VPage.home).
            for (const p of root.pageItems) if (p.pageId === page) p.enter()
        // The video rect follows the page, and it must be settled before the
        // engine opens anything: it samples the window's client size once. It moves
        // here, once, not at the click: the native window cannot fade with the pages.
        videoHost.syncRect()
    }
    Timer { id: showTimer; interval: 150; onTriggered: root.showPage(true) }
    // @keyframes sink { to { opacity: 0; transform: scale(.985) } }, .22s --out.
    property real leaveT: 0
    NumberAnimation {
        id: leaveAnim
        target: root; property: "leaveT"; from: 0; to: 1
        duration: 220; easing.bezierCurve: Theme.easeOut
        onFinished: root.leavingPage = ""
    }

    // Background: pure black in cinema mode (the picture is the window), the
    // three-stop gradient otherwise - the prototype's .vy / .vy.cine split.
    VBackdrop {
        id: backdrop
        anchors.fill: parent
        visible: !root.cinema
        strokeColor: root.cinema ? "transparent" : Theme.stroke
    }

    // The one host item for the native video window; main.cpp finds it by name.
    // Exactly one must exist in the tree: an earlier version had a copy in each
    // page and the engine bound its swapchain to a hidden, zero-sized one.
    Item {
        id: videoHost
        objectName: "videoHost"
        visible: false          // geometry proxy; the native window draws pixels
        function syncRect() {
            const r = root.videoRect()
            x = r.x; y = r.y; width = r.width; height = r.height
        }
    }

    // Where the picture sits, per page, computed from the design's own layout
    // numbers rather than read from a laid-out child. A child reports 0x0 on the
    // frame the user opens a file, and that once produced a 1x1 swapchain.
    function videoRect() {
        if (fullscreen && veyra.gameMode &&
            veyra.gameVideoRect.width > 0 && veyra.gameVideoRect.height > 0)
            return veyra.gameVideoRect
        // Fullscreen is a video-only viewport (AppShell): the picture is the screen
        // and the controls float above it in their own window (FullscreenBar).
        if ((fullscreen || (maximized && page === "min")) && page !== "home" && page !== "set")
            return { x: 0, y: 0, width: width, height: height }
        switch (page) {
        case "min":
            // .min .stage: full width, height = the picture height.
            return { x: 0, y: 0, width: width, height: Math.round(pictureHeight) }
        case "pro":
            // Share the page's responsive header, transport and status dimensions.
            // A second fixed-height formula let the HWND cover the playback bar.
            return onPages(proPage.videoViewport)
        case "node":
            return onPages(nodePage.videoViewport)
        case "exp":
            return onPages(ExportLayout.calculate(pages.width, pages.height, exportPage.compactTab).video)
        case "live":
            // PushFPS streaming mode: the preview card (StudioLayout.js).
            return onPages(studioPage.videoViewport)
        case "watch":
            // PushFPS video playback mode: the picture beside its controls.
            return onPages(watchPage.videoViewport)
        case "home":
            // PushFPS redesign: the new home page has the picture in its middle (legacy has none: 0 x 0).
            return Theme.modern ? onPages(homePage.videoViewport) : { x: 0, y: 0, width: 0, height: 0 }
        default:
            return { x: 0, y: 0, width: 0, height: 0 }
        }
    }
    // A page's own rectangle, in the window's coordinates: the pages stand inside the new design's
    // shell. Without the shell (legacy, cinema, full screen) the pages fill the window and this adds 0.
    function onPages(r) { return { x: r.x + pages.x, y: r.y + pages.y, width: r.width, height: r.height } }

    // Pages live for the whole session so switching never loses a half-edited
    // chain; only the visible page owns the video window.
    Item {
        id: pages
        anchors.fill: parent
        anchors.leftMargin: root.shellLeft
        anchors.topMargin: root.shellTop
        anchors.bottomMargin: root.shellBottom
        onXChanged: videoHost.syncRect()
        onYChanged: videoHost.syncRect()
        onWidthChanged: videoHost.syncRect()
        onHeightChanged: videoHost.syncRect()
        // In fullscreen the picture covers the page, and the native video window
        // passes the pointer through: a hidden page control must not take a click
        // meant for the picture.
        visible: !root.fullscreen || root.page === "home" || root.page === "set" || root.quickPanelShown
        // Each page is shown while current or while sinking out; the sinking one
        // takes no input (.page.leave { pointer-events: none }).
        HomePage {
            id: homePage; pageId: "home"; home: pages; topInset: !Theme.modern && dock.pinned && !homePage.sunset ? 40 : 0
            onRequestPage: p => root.goPage(p); onRequestOnboarding: root.startTour("home")
            // PushFPS redesign: the new home page has the picture in its middle.
            onVideoViewportChanged: if (root.page === "home" && !root.fullscreen) videoHost.syncRect()
            onRequestFullscreen: { root.goPage("min"); Qt.callLater(root.toggleFullscreen) }
            onRequestAppearance: dialogs.open("appearance")
        }
        MinimalPage {
            id: minPage
            pageId: "min"
            dragStripHeight: 0
            home: pages
            onRequestPage: p => root.goPage(p)
            onRequestAspect: aspect => { if (root.testAspect <= 0) root.fitToFilm(aspect) }
            onRequestFullscreen: root.toggleFullscreen()
        }
        ProPage {
            id: proPage
            pageId: "pro"
            // Also follows wrapping status text and the compact header/transport.
            onVideoViewportChanged: if (root.page === "pro" && !root.fullscreen) videoHost.syncRect()
            home: pages
            onboardingActive: root.onboardingActive
            overlay: root.quickPanelShown && root.page === "pro"
            onRequestFullscreen: root.toggleFullscreen()
            onRequestPage: p => root.goPage(p)
            onRequestDialog: key => dialogs.open(key)
        }
        NodePage {
            id: nodePage
            pageId: "node"
            dragStripHeight: 14
            onVideoViewportChanged: if (root.page === "node" && !root.fullscreen) videoHost.syncRect()
            home: pages
            videoHeight: root.nodeVideoHeight
            overlay: root.quickPanelShown && root.page === "node"
            onVideoHeightEdited: h => { root.nodeVideoHeight = h; videoHost.syncRect() }
            onRequestFullscreen: root.toggleFullscreen()
            onRequestPage: p => root.goPage(p)
            onRequestDialog: key => dialogs.open(key)
            onRequestInspector: (index, type) => {
                proPage.selectedEffect = index
                veyra.selectedNrLayer = index
                veyra.selectedColourLayer = index
                proPage.tab = type === "color" ? "color" : "quality"
                root.page = "pro"
            }
        }
        ExportPage { id: exportPage; pageId: "exp"; home: pages; onRequestPage: p => root.goPage(p) }
        WatchPage {
            id: watchPage
            pageId: "watch"
            home: pages
            onVideoViewportChanged: if (root.page === "watch" && !root.fullscreen) videoHost.syncRect()
            onRequestPage: p => root.goPage(p)
            onRequestSettings: (section, row) => {
                settingsPage.section = section
                root.goPage("set")
                Qt.callLater(() => settingsPage.scrollToObject(row))
            }
        }
        StudioPage {
            id: studioPage
            pageId: "live"
            home: pages
            onVideoViewportChanged: if (root.page === "live" && !root.fullscreen) videoHost.syncRect()
            onRequestPage: p => root.goPage(p)
            onRequestLayout: id => { dialogs.layoutScene = id; dialogs.open("layout") }
        }
        SettingsPage {
            id: settingsPage
            pageId: "set"
            home: pages
            onRequestPage: p => {
                if (p === "translation") dialogs.open(p)
                else root.goPage(p)
            }
        }
    }

    // PushFPS redesign: the shell. None of it lies over a page or over the picture.
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
        // The node page is the professional mode's node view.
        currentPage: root.page === "node" ? "pro" : root.page
        onRequestPage: p => root.goPage(p)
        onRequestAppearance: dialogs.open("appearance")
    }
    PushStatusStrip {
        id: shellStatus
        visible: root.shellShown
        x: root.shellLeft; y: parent.height - Theme.shellStatus
        width: parent.width - root.shellLeft - 1
        height: Theme.shellStatus - 1
    }

    // The five dialogs sit above the pages and below the dock's own tooltips.
    DialogHost {
        id: dialogs
        anchors.fill: parent
        // PushFPS redesign: the Appearance panel's way to the full settings.
        onRequestAppearanceSettings: { dialogs.close(); settingsPage.section = "look"; root.goPage("set") }
        onboardingActive: root.onboardingActive
        // The dialogs open the source through the bridge; show the picture.
        onStartPs5: if (root.page === "home") root.goPage("min")
        onStartMoonlight: if (root.page === "home") root.goPage("min")
        onStartXbox: if (root.page === "home") root.goPage("min")
        onStartScreen: {
            if (root.page === "home") root.goPage("min")
            if (veyra.screenOptions.passthrough === true) root.enterGameFullscreen()
        }
        onStartScreenCrop: {
            root.goPage("min")
            screenCrop.begin(veyra.screenOptions)
        }
    }

    // PushFPS's first-run tour: its steps and when it is offered (PushTour.qml). The tour's own
    // machinery (spotlight, card, keys) is the engine release's OnboardingGuide below.
    PushTour { id: pushTour }
    function startTour(source) {
        if (onboarding.visible) return
        dialogs.close()
        root.goPage("home")
        // (4.3.1) The tour points at quick setup: a hidden settings side is shown again for it.
        if (root.sideCollapsed) veyra.setPreference("pushSideCollapsed", false)
        onboarding.steps = pushTour.steps({ enhance: homePage.tourOffers("enhance") })
        veyra.logUi("push-tour", "start source=" + source + " steps=" + onboarding.steps.length)
        // Offered by itself only once; from then on it starts when asked for.
        if (veyra.preferences.pushTourSeen !== true) veyra.setPreference("pushTourSeen", true)
        onboarding.start()
    }
    // A new install: once the home page is there and nothing else is in the way (no intro still
    // playing, no dialog, nothing being opened), the tour starts by itself. Decided once a launch.
    Timer {
        id: tourOffer
        interval: 700; repeat: true
        running: Object.keys(root.test).length === 0
        onTriggered: {
            if (introLoader.active || root.page !== "home") return
            stop()
            if (dialogs.dialog !== "" || veyra.hasSource || veyra.openingSource) return
            if (pushTour.offeredAtStart(veyra.preferences, { captureUsed: veyra.hasCaptureSession === true, testRun: false }))
                root.startTour("first-run")
        }
    }
    // --tour <step>: the tour open at one of its steps, for screenshots.
    Timer {
        interval: 1600
        running: root.test.tour !== undefined
        onTriggered: { root.startTour("test"); onboarding.goTo(Math.max(0, Math.min(onboarding.steps.length - 1, Number(root.test.tour)))) }
    }

    OnboardingGuide {
        id: onboarding
        parent: root.contentItem
        z: 1100
        property var savedProView
        property real savedCaptureScroll
        property real savedHomeScroll
        property string savedFolded: ""
        targetFor: key => {
            // PushFPS's steps: a place on the home page, or a row of the capture dialog.
            if (pushTour.onHome(key)) return homePage.tourTarget(key)
            if (pushTour.inDialog(key)) return dialogs.captureSettings.onboardingTarget(key)
            return null
        }
        onAboutToShow: {
            savedProView = proPage.saveOnboardingView()
            savedCaptureScroll = dialogs.captureSettings.bodyViewport.contentY
            savedHomeScroll = homePage.sourceViewport.contentY
            savedFolded = String(veyra.preferences.pushQuickFolded || "")
        }
        onStepRequested: step => {
            // Only the displayed page changes. The player, remembered page and
            // native video window stay in the same state as on the home page.
            root.shownPage = "home"
            dialogs.dialog = pushTour.inDialog(step.target) ? "capture" : ""
            if (pushTour.onHome(step.target)) homePage.showTourStep(step.target)
        }
        onAboutToHide: {
            dialogs.close()
            dialogs.captureSettings.bodyViewport.contentY = savedCaptureScroll
            homePage.sourceViewport.contentY = savedHomeScroll
            root.shownPage = "home"
            proPage.restoreOnboardingView(savedProView)
            // A section the tour opened to point at one of its rows is folded again.
            if (String(veyra.preferences.pushQuickFolded || "") !== savedFolded) veyra.setPreference("pushQuickFolded", savedFolded)
            veyra.logUi("push-tour", "closed at step " + (stepIndex + 1) + " of " + steps.length)
        }
        onClosed: if (root.page === "home" && dialogs.dialog === "") homePage.onboardingEntry.forceActiveFocus()
    }

    // The dock floats above every page and retracts on its own.
    TopDock {
        id: dock
        // (PushFPS redesign: where the new design's shell stands, its navigation and title bar do the dock's work.)
        visible: !root.shellShown
        anchors.horizontalCenter: parent.horizontalCenter
        // The node page is the professional mode's node view.
        currentPage: root.page === "node" ? "pro" : root.page
        maximized: root.maximized || root.fullscreen
        pinned: root.test.dockPinned === true || veyra.preferences.dockPinned === true
        locked: root.fullLocked
        forcedTip: root.test.tip !== undefined ? root.test.tip : ""
        opened: pinned
        peekOnStart: Object.keys(root.test).length === 0
        // In fullscreen only the picture is on screen, so another page (专业 above all)
        // would change nothing visible: leave fullscreen first, as the pill's menu does.
        onRequestPage: p => {
            if (root.fullscreen && root.goPageTarget(p) !== root.page) root.toggleFullscreen()
            root.goPage(p)
        }
        onRequestMinimize: root.showMinimized()
        onRequestMaximize: root.toggleMaximized()
        onRequestClose: root.close()
        onRequestMove: if (!root.fullscreen) root.startSystemMove()
        pillToggleVisible: veyra.hasSource && (root.page === "min" || root.fullscreen)
        pillHidden: root.pillHidden
        onRequestTogglePill: root.setPillHidden(!root.pillHidden)
    }

    // Frameless window: the system resizes from 6px borders and 12px corners.
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
            objectName: "window-resize-" + modelData.e
            // Above the dialog layer (z 100): an open dialog must not stop the
            // window from being resized.
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

    // Drag and drop opens the file (AppShell WM_DROPFILES).
    DropArea {
        anchors.fill: parent
        onDropped: drop => {
            if (!drop.hasUrls || drop.urls.length === 0) return
            drop.accept(Qt.CopyAction)
            veyra.openDroppedUrls(drop.urls)
        }
    }

    // Keyboard, aligned with AppShell's message loop. Text fields keep their own
    // keys (Qt's shortcut override), so typing in a field never seeks. V (hold for
    // the original picture) needs its release as well; main.cpp handles it.
    readonly property bool fullscreen: visibility === Window.FullScreen
    // Set before the visibility changes: Windows resizes the window first, and the
    // cinema rule would otherwise snap that fullscreen size back to the film.
    property bool fullTarget: false
    property int fullRestoreVisibility: Window.Windowed
    // Windows may briefly report Windowed while restoring a maximized window.
    // Keep cinema sizing out of that native transition, without a polling timer.
    property bool fullRestorePending: false
    property bool fullLocked: false
    // Fullscreen quick-adjust panel (Home): the professional mode's own controls
    // over the picture - the list inspector (with the performance orbs) in list
    // mode, the node canvas along the bottom in node mode (veyra.nodeMode). From
    // 极简 the professional page becomes current underneath the fullscreen picture
    // while the panel is open, and 极简 returns when it closes.
    property bool quickPanel: false
    property string quickReturnPage: ""
    // A page change the user cannot see (under the fullscreen picture) skips the
    // sink animation, so the old page never flashes over the picture.
    property bool instantPage: false
    readonly property bool quickPanelShown: fullscreen && quickPanel && (page === "pro" || page === "node") && !fullLocked
    function switchPageInstantly(p) { instantPage = true; page = p; instantPage = false }
    function toggleQuickPanel() {
        if (quickPanel) { closeQuickPanel(); return }
        const target = veyra.nodeMode === 1 ? "node" : "pro"
        if (page !== target) {
            if (page !== "pro" && page !== "node") quickReturnPage = page
            switchPageInstantly(target)
        }
        quickPanel = true
    }
    function closeQuickPanel() {
        quickPanel = false
        if (quickReturnPage !== "") { const p = quickReturnPage; quickReturnPage = ""; switchPageInstantly(p) }
    }
    onQuickPanelShownChanged: {
        veyra.logUi("ui-fullscreen", "quick panel shown=" + quickPanelShown)
        veyra.setGamePanelOpen(quickPanelShown)
    }
    FullscreenTimingPanel {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        width: Math.min(330, Math.max(180, parent.width - 424))
        maximumHeight: Math.max(120, root.height - (root.page === "node" ? Math.min(root.height * 0.6, 460) : 90))
        visible: root.quickPanelShown && veyra.hasSource
        onCloseRequested: root.closeQuickPanel()
        z: 75
    }
    // 游戏模式 (屏幕捕获 → 游戏模式): fullscreen on the captured monitor (or the
    // monitor of the captured window); the bridge then makes the picture click-through
    // so the program underneath keeps the keyboard, mouse and gamepad.
    function enterGameFullscreen() {
        const i = veyra.gameModeScreenIndex()
        const screens = Qt.application.screens
        if (i >= 0 && i < screens.length) {
            const s = screens[i]
            const inside = x + width / 2 >= s.virtualX && x + width / 2 < s.virtualX + s.width &&
                           y + height / 2 >= s.virtualY && y + height / 2 < s.virtualY + s.height
            if (!inside) {
                if (fullscreen) toggleFullscreen()
                x = s.virtualX + Math.max(0, Math.round((s.width - width) / 2))
                y = s.virtualY + Math.max(0, Math.round((s.height - height) / 2))
            }
        }
        if (!fullscreen) toggleFullscreen()
        veyra.logUi("game-mode", "fullscreen for game mode on screen " + i)
    }
    Connections {
        target: veyra
        // Home: the adjust panel takes the input back / gives it to the program again.
        // Ctrl+Alt+Home: leave the fullscreen picture (and with it the click-through).
        function onGameHotkey(which) {
            if (which === 1) {
                root.closeQuickPanel()
                if (root.fullscreen) root.toggleFullscreen()
                return
            }
            if (!root.fullscreen) return
            root.fullLocked = false
            root.toggleQuickPanel()
        }
        function onGameModeStopped(reason) {
            if (root.fullscreen) root.toggleFullscreen()
            toast.show(reason, true)
        }
        function onGameModeChanged() {
            videoHost.syncRect()
            if (veyra.gameInputActive && playlistWindow.visible) playlistWindow.hide()
        }
    }
    onFullscreenChanged: {
        veyra.setPresentationFullscreen(fullscreen)
        fullTarget = fullscreen
        if (!fullscreen) {
            fullLocked = false
            closeQuickPanel()
            if (cinema && visibility === Window.Windowed && !fullRestorePending) height = Math.round(pictureHeight)
        }
        fullControls = true
        fullHide.restart()
        videoHost.syncRect()
    }
    function toggleFullscreen() {
        if (!fullscreen && veyra.fullscreenMemorySafe === false) {
            toast.show(qsTr("本次播放检测到全屏显存持续增长，请使用窗口播放；重新打开片源后可再次测试全屏。"), true)
            veyra.logUi("ui-fullscreen", "blocked after sustained video-memory growth")
            return
        }
        fullLocked = false
        if (!fullscreen) {
            fullRestoreVisibility = maximized ? Window.Maximized : Window.Windowed
            fullTarget = true
            visibility = Window.FullScreen
        } else {
            fullRestorePending = fullRestoreVisibility === Window.Maximized
            fullTarget = false
            visibility = fullRestoreVisibility
            Qt.callLater(function() { root.fullRestorePending = false })
        }
        veyra.logUi("ui-fullscreen", "enabled=" + fullscreen)
    }

    // G2.5 fullscreen controls. AppShell: any pointer movement shows the transport,
    // and 1.6 s without movement hides it (and the cursor) unless the pointer rests
    // on the bar or a menu is open; Ctrl+L locks it away. The bar is its own
    // top-level window (FullscreenBar.qml), because the native video window draws
    // above everything in this one.
    property bool fullControls: true
    // Also consumed by the native video HWND: a QML cursor alone cannot hide
    // the class cursor of the foreign child window under a stationary pointer.
    readonly property bool fullscreenCursorHidden: fullscreen && (!fullControls || fullLocked)
        && !quickPanelShown && dialogs.dialog === ""
    function pointerActivity() {
        if (!fullscreen || fullLocked || root.test.fullBar === "hidden") return
        if (root.test.fullDebug === true) veyra.logUi("ui-fullscreen-debug", "pointer activity")
        if (!fullControls) { fullControls = true; veyra.logUi("ui-fullscreen", "controls shown by pointer") }
        fullHide.restart()
    }
    Timer {
        id: fullHide
        interval: 1600
        onTriggered: {
            if (!root.fullscreen || !root.fullControls) return
            if (fullBar.hovered || fullBar.menuOpen || fullBar.interacting || root.test.fullBar === "shown") { restart(); return }
            root.fullControls = false
            veyra.logUi("ui-fullscreen", "controls hidden; video and subtitles only")
        }
    }
    // Every pointer move over this window (a HoverHandler sees them all, whatever
    // item is under the pointer; the video window passes them through). The cursor
    // hides with the controls, as in AppShell.
    HoverHandler {
        id: pointerWatch
        cursorShape: root.fullscreenCursorHidden ? Qt.BlankCursor : Qt.ArrowCursor
        // Qt re-sends hover while anything animates under a still pointer; only a
        // real change of position counts as movement.
        property point last: Qt.point(-1, -1)
        onPointChanged: {
            const p = point.scenePosition
            if (Math.abs(p.x - last.x) < 1 && Math.abs(p.y - last.y) < 1) return
            last = p
            root.pointerActivity()
            // Fullscreen: the top edge brings the dock down (极简 / 专业 switch in
            // place). Its own 12px hot zone sits under the native picture, where
            // hover is not always delivered, so the window-wide handler helps it.
            if (root.fullscreen && !root.fullLocked) {
                if (p.y <= 14) dock.open()
                else if (p.y > 90 && dock.opened && !dock.pinned) dock.opened = false
            }
        }
    }
    // Shared mask input follows the exact native host across list/node/cinema/fullscreen.
    ScreenCropEditor {
        id: screenCrop
        z: 94
        hostRect: Qt.rect(videoHost.x, videoHost.y, videoHost.width, videoHost.height)
        onCancelled: {
            veyra.stopPlayback()
            dialogs.open("screen")
        }
    }
    Connections {
        target: veyra
        function onMediaOpenRequested() { screenCrop.editing = false }
    }
    MouseArea {
        id: maskInput
        objectName: "mask-edit-input"
        x: videoHost.x; y: videoHost.y; width: videoHost.width; height: videoHost.height
        z: 80
        visible: veyra.hasSource && videoHost.width > 0 && videoHost.height > 0
                 && (veyra.protectionDrawShape.length > 0 || veyra.exclusionOverlayVisible)
        enabled: visible && !screenCrop.editing && !veyra.gameInputActive && dialogs.dialog === ""
        preventStealing: true
        cursorShape: veyra.protectionDrawShape.length > 0 ? Qt.CrossCursor : pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        property point start
        property bool drawing: false
        property bool moving: false
        onPressed: mouse => {
            const p = mapToItem(null, mouse.x, mouse.y)
            if (root.overVideoCover(p) || (root.quickPanelShown && (proPage.overlayContains(p.x,p.y) || nodePage.overlayContains(p.x,p.y)))) { mouse.accepted = false; return }
            start = Qt.point(mouse.x / width, mouse.y / height)
            drawing = veyra.protectionDrawShape.length > 0
            moving = !drawing && veyra.beginExclusionMove(start.x,start.y)
            if (!drawing && !moving) mouse.accepted = false
        }
        onPositionChanged: mouse => {
            if (!pressed) return
            if (drawing) veyra.updateProtectionDraw(start.x,start.y,mouse.x/width,mouse.y/height)
            else if (moving) veyra.updateExclusionMove(mouse.x/width,mouse.y/height)
        }
        onReleased: mouse => {
            if (drawing) veyra.commitProtectionDraw(start.x,start.y,mouse.x/width,mouse.y/height)
            else if (moving) { veyra.updateExclusionMove(mouse.x/width,mouse.y/height); veyra.endExclusionMove(true) }
            drawing = false; moving = false
        }
        onCanceled: { veyra.endExclusionMove(false); if (drawing) veyra.beginProtectionDraw(""); drawing=false; moving=false }
        Shortcut {
            sequence: "Escape"
            enabled: maskInput.moving
            onActivated: { veyra.endExclusionMove(false); maskInput.moving=false }
        }
    }
    // AppShell VideoSurface: a double click on the picture toggles fullscreen
    // (1.4.4 did both directions), unless locked. Windowed, only a double click
    // inside the picture counts; with the quick panel up, not one on the panel.
    // A click on the picture of a playing file pauses or resumes it (field request
    // 2026-10-02), and takes the keyboard back from a slider so the arrows seek again.
    // Live sources ignore it: a click to focus the window mid-game must not stop the
    // capture. The second click of a double click undoes the first; the double click
    // itself toggles fullscreen below.
    function pictureContains(p) {
        return p.x >= videoHost.x && p.y >= videoHost.y && p.x <= videoHost.x + videoHost.width
            && p.y <= videoHost.y + videoHost.height
    }
    // Anything floating over the picture is cut out of the video window (videoCover:
    // the dock, popovers, tips). A click on it belongs to it, never to the picture:
    // the window-wide handlers below used to see the dock's 专业模式 click too, so
    // leaving 极简 also paused or resumed the film (field report 2026-10-05).
    function overVideoCover(scenePoint) {
        const visit = item => {
            if (!item || !item.visible || item.opacity <= 0) return false
            if (item.objectName === "videoCover" || item.videoCover === true) {
                const q = item.mapFromItem(null, scenePoint.x, scenePoint.y)
                if (q.x >= 0 && q.y >= 0 && q.x <= item.width && q.y <= item.height) return true
            }
            for (let i = 0; i < item.children.length; ++i) if (visit(item.children[i])) return true
            return false
        }
        return visit(root.contentItem.parent ? root.contentItem.parent : root.contentItem)
    }
    TapHandler {
        id: pictureTap
        // Scaled game input deliberately catches mouse events instead of
        // forwarding wrong coordinates; they must not control the player either.
        enabled: !root.fullLocked && !screenCrop.editing && !veyra.gameInputActive
        // Judged where the press landed: by the release the page may already have changed.
        property bool pressOnCover: false
        property bool pressOnPicture: false
        onPressedChanged: if (pressed) {
            pressOnCover = root.overVideoCover(point.scenePosition)
            pressOnPicture = root.pictureContains(point.scenePosition)
        }
        onTapped: (eventPoint, button) => {
            const p = eventPoint.position
            // A transport button can enter fullscreen before this handler sees
            // release. Its press still belongs to the transport, not the newly
            // enlarged picture underneath it.
            if (pressOnCover || !pressOnPicture) return
            if (root.page !== "min" && root.page !== "pro" && root.page !== "node" && !root.fullscreen) return
            if (dialogs.dialog !== "" || !veyra.hasSource) return
            if (root.fullscreen && root.quickPanelShown && (proPage.overlayContains(p.x, p.y) || nodePage.overlayContains(p.x, p.y))) return
            // 游戏模式: a click on the picture beside the panel returns to the game.
            if (root.fullscreen && root.quickPanelShown && veyra.gameMode) { root.closeQuickPanel(); return }
            // Do not re-test geometry on release: a picture double click may
            // already have left fullscreen before this second tap restores play.
            root.contentItem.forceActiveFocus()
            if (veyra.isCapture || veyra.duration <= 0) return
            if (veyra.protectionDrawShape.length > 0 || veyra.exclusionOverlayVisible || veyra.compareMode === 2) return
            if (pictureTap.tapCount <= 2) veyra.togglePlayPause()
        }
    }
    TapHandler {
        enabled: !root.fullLocked && !veyra.gameInputActive
        property bool pressOnCover: false
        property bool pressOnPicture: false
        onPressedChanged: if (pressed) {
            pressOnCover = root.overVideoCover(point.scenePosition)
            pressOnPicture = root.pictureContains(point.scenePosition)
        }
        onDoubleTapped: (eventPoint, button) => {
            const p = eventPoint.position
            if (pressOnCover || !pressOnPicture) return
            if (root.fullscreen) {
                if (root.quickPanelShown && (proPage.overlayContains(p.x, p.y) || nodePage.overlayContains(p.x, p.y))) return
                root.toggleFullscreen()
                return
            }
            if (root.page !== "min" && root.page !== "pro" && root.page !== "node") return
            if (dialogs.dialog !== "" || !veyra.hasSource) return
            if (veyra.protectionDrawShape.length > 0 || veyra.exclusionOverlayVisible || veyra.compareMode === 2) return
            if (p.x < videoHost.x || p.y < videoHost.y || p.x > videoHost.x + videoHost.width
                || p.y > videoHost.y + videoHost.height) return
            root.toggleFullscreen()
        }
    }
    // PushFPS 4.2.1: the before/after line follows the mouse wherever the picture itself takes clicks (full
    // screen, the minimal page, the node page). Until now only Professional mode's own picture moved it, so in
    // full screen, where a comparison is looked at, the line stood where it was (a user's request). A press on
    // anything floating over the picture belongs to that; this handler only watches (it never takes a grab),
    // and the tap handlers above already leave the picture alone while the split is on.
    function moveCompareLine(sceneX) {
        if (videoHost.width > 0) veyra.setCompareSplitAt(Math.max(0, Math.min(1, (sceneX - videoHost.x) / videoHost.width)))
    }
    readonly property bool compareLineMoves: veyra.compareMode === 2 && veyra.hasSource === true && !root.fullLocked
                                             && !veyra.gameInputActive && dialogs.dialog === ""
                                             && veyra.protectionDrawShape.length === 0 && !veyra.exclusionOverlayVisible
                                             && (root.fullscreen || root.page === "min" || root.page === "node")
    PointHandler {
        id: compareLineDrag
        objectName: "pushfps-compare-drag"
        acceptedButtons: Qt.LeftButton
        enabled: root.compareLineMoves
        property bool onPicture: false
        onActiveChanged: {
            onPicture = active && !root.overVideoCover(point.scenePressPosition) && root.pictureContains(point.scenePressPosition)
            if (onPicture) root.moveCompareLine(point.scenePosition.x)
        }
        onPointChanged: if (active && onPicture) root.moveCompareLine(point.scenePosition.x)
    }
    // A click on the picture of a PC stream takes the keyboard and mouse back after a release
    // (Ctrl+Alt+Shift+Z). While captured the clicks belong to the host and never get here.
    TapHandler {
        enabled: veyra.moonlight && veyra.moonlight.state.streaming === true && !veyra.moonlightCaptured && dialogs.dialog === ""
        property bool pressOnCover: false
        property bool pressOnPicture: false
        onPressedChanged: if (pressed) {
            pressOnCover = root.overVideoCover(point.scenePosition)
            pressOnPicture = root.pictureContains(point.scenePosition)
        }
        onTapped: eventPoint => {
            const p = eventPoint.position
            if (pressOnCover || !pressOnPicture) return
            if (root.page !== "min" && root.page !== "pro" && root.page !== "node") return
            if (p.x < videoHost.x || p.y < videoHost.y || p.x > videoHost.x + videoHost.width
                || p.y > videoHost.y + videoHost.height) return
            veyra.moonlightCapture(true)
        }
    }
    // Stream statistics on and off. While a PC stream has the keyboard, the capture filter sees the keys
    // first and toggles the same switch.
    Shortcut {
        sequence: "Ctrl+Alt+Shift+S"
        enabled: (veyra.moonlight && veyra.moonlight.state.streaming === true) || (veyra.xbox && veyra.xbox.state.streaming === true)
        onActivated: veyra.moonlightStatsVisible = !veyra.moonlightStatsVisible
    }
    StreamHud {
        id: streamHud
        x: 16
        y: root.fullscreen ? 16 : 52
        z: 90
    }
    PlaylistWindow {
        id: playlistWindow
        owner: root
        playlist: veyra.playlist
        onAddFiles: veyra.addPlaylistFilesDialog()
        onAddFolder: veyra.addPlaylistFolderDialog()
    }
    FullscreenBar {
        id: fullBar
        owner: root
        // Windowed cinema: the pill straddles the picture edge (D1). It hides under
        // an open dialog, which lives in the main window below it.
        cinema: root.cinema && !root.fullscreen
        pillTop: (root.maximized ? root.height : root.pictureHeight) - 46
        shown: !root.onboardingActive && veyra.hasSource && !screenCrop.editing && !root.pillHidden && !veyra.gameInputActive && (root.fullscreen ? root.fullControls && !root.fullLocked
                                                   : root.cinema && root.shownPage === "min"
                                                     && root.leavingPage === "" && dialogs.dialog === ""
                                                     && root.visibility !== Window.Minimized)
        onRequestPage: p => { if (root.fullscreen) root.toggleFullscreen(); root.goPage(p) }
        onRequestDialog: k => dialogs.open(k)
        onRequestFullscreen: root.toggleFullscreen()
        onRequestLock: root.toggleLock()
        onRequestHide: root.setPillHidden(true)
        onRequestPlaylist: playlistWindow.toggle()
        onRequestMove: if (!root.fullscreen && !root.maximized) root.startSystemMove()
        onActivity: root.pointerActivity()
    }
    // The cinema / fullscreen pill can be hidden for games (capture card, streams); the
    // dock's eye brings it back. Remembered across sessions.
    readonly property bool pillHidden: veyra.preferences.cinePillHidden === true
    function setPillHidden(hidden) {
        veyra.setPreference("cinePillHidden", hidden)
        toast.show(hidden ? qsTr("播放条已隐藏 · 鼠标移到顶部，点胶囊里的眼睛可重新打开") : qsTr("播放条已显示"), false)
    }
    function toggleLock() {
        fullLocked = !fullLocked
        if (fullLocked) dock.opened = false
        else { fullControls = true; fullHide.restart() }
        toast.show(fullLocked ? qsTr("已锁定全屏 · Ctrl+L 解锁，Esc 退出全屏") : qsTr("已解锁全屏"), false)
        veyra.logUi("ui-fullscreen", "locked=" + fullLocked + " shortcut=Ctrl+L")
    }
    // Subtitles keep clear of the control pill: fullscreen bar 24px margin + pill,
    // windowed cinema pill straddling the picture's bottom edge.
    Binding { target: veyra; property: "subtitleBottomInset"; value: fullBar.shown ? (root.fullscreen ? 84 : 56) : 0 }
    // Rebindable (设置 → 快捷键); the hold-to-compare key is handled by the bridge's
    // application event filter because it needs the key release too.
    Shortcut { sequence: veyra.shortcuts.playPause; enabled: !screenCrop.editing; onActivated: veyra.togglePlayPause() }
    Shortcut { sequence: veyra.shortcuts.fullscreen; onActivated: root.toggleFullscreen() }
    Shortcut { sequence: veyra.shortcuts.screenshot; onActivated: veyra.takeScreenshot() }
    Shortcut { sequence: veyra.shortcuts.record; autoRepeat: false; onActivated: veyra.toggleRecording("shortcut") } // a held key must not toggle at the repeat rate
    Shortcut { sequence: veyra.shortcuts.replay; autoRepeat: false; onActivated: veyra.saveReplay("shortcut") }
    // PushFPS clip markers: only while a recording runs, so the key does nothing (and says nothing) otherwise.
    Shortcut { sequence: veyra.shortcuts.marker || ""; autoRepeat: false; enabled: veyra.recording === true; onActivated: veyra.pushAddMarker("shortcut") }
    // PushFPS: read the text on the picture (PushReadTextDialog.qml). The key opens the dialog, which reads
    // at once; pressed again with the dialog open it reads the picture as it is now.
    Shortcut {
        sequence: veyra.shortcuts.readText || ""
        autoRepeat: false
        enabled: veyra.hasSource === true
        onActivated: { if (dialogs.dialog === "readtext") dialogs.readTextAgain(); else dialogs.open("readtext") }
    }
    Shortcut {
        sequence: veyra.shortcuts.toggleMode
        enabled: dialogs.dialog === ""
        onActivated: root.goPage(root.page === "min" ? "pro" : "min")
    }
    Shortcut { sequences: ["F11", "Alt+Return", "Alt+Enter"]; onActivated: root.toggleFullscreen() }
    // In 游戏模式 Esc closes the adjust panel and gives the input back to the game.
    Shortcut {
        sequence: "Esc"
        enabled: root.fullscreen && !screenCrop.editing && !maskInput.moving && !dialogs.captureGuideOpen
        onActivated: {
            // Visible editing operations own the first Escape. Hidden pages and
            // the hidden playlist must never compete with fullscreen's shortcut.
            if (root.quickPanelShown && veyra.protectionDrawShape.length > 0)
                veyra.beginProtectionDraw("")
            else if (root.quickPanelShown && root.page === "node" &&
                     (nodePage.wireFrom >= 0 || nodePage.marked.length > 0))
                nodePage.clearSelection()
            else if (veyra.gameMode && root.quickPanelShown) root.closeQuickPanel()
            else root.toggleFullscreen()
        }
    }
    // Home: the fullscreen quick-adjust panel, from 极简 or 专业 (list or node view).
    Shortcut {
        sequence: "Home"
        enabled: root.fullscreen && !root.fullLocked && dialogs.dialog === "" &&
                 !veyra.gameMode && (root.page === "min" || root.page === "pro" || root.page === "node")
        onActivated: root.toggleQuickPanel()
    }
    // 5 s steps (field request 2026-10-02).
    Shortcut { sequence: "Left"; onActivated: veyra.seekBy(-5) }
    Shortcut { sequence: "Right"; onActivated: veyra.seekBy(5) }
    Shortcut { sequence: "Up"; enabled: !dialogs.captureGuideOpen; onActivated: veyra.volume = Math.min(veyra.maximumVolume, veyra.volume + 0.05) }
    Shortcut { sequence: "Down"; enabled: !dialogs.captureGuideOpen; onActivated: veyra.volume = Math.max(0, veyra.volume - 0.05) }
    Shortcut { sequence: "Ctrl+O"; onActivated: veyra.openFileDialog() }
    Shortcut { sequence: "Ctrl+U"; onActivated: veyra.openNetworkDialog() }
    Shortcut { sequence: "Ctrl+E"; onActivated: root.page = "exp" }
    Shortcut {
        sequence: veyra.shortcuts.lock
        enabled: root.fullscreen
        onActivated: root.toggleLock()
    }
    // Subtitle keys, as in 1.4.4: B on/off, Z/X -/+50 ms (Shift: 1 s), T/Y cycle
    // the primary/secondary track.
    Shortcut { sequence: "B"; onActivated: veyra.toggleSubtitles() }
    Shortcut { sequence: "Z"; onActivated: veyra.nudgeSubtitle(-50) }
    Shortcut { sequence: "Shift+Z"; onActivated: veyra.nudgeSubtitle(-1000) }
    Shortcut { sequence: "X"; onActivated: veyra.nudgeSubtitle(50) }
    Shortcut { sequence: "Shift+X"; onActivated: veyra.nudgeSubtitle(1000) }
    Shortcut { sequence: "T"; onActivated: veyra.cycleSubtitle(false) }
    Shortcut { sequence: "Y"; onActivated: veyra.cycleSubtitle(true) }

    // Toasts: a refused edit, a finished export, a failed open.
    // PushFPS 4.1: at the bottom of the window. It used to come up at the top, over the navigation
    // dock, which is exactly where the mouse goes to change page (the owner's note, 20 November 2026).
    UpdateBanner {
        id: updateBanner
        z: 35
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 16
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(500, root.width - 32)
        installable: veyra.updates.canInstall
        // Signed releases install in one click (UpdateChecker::installUpdate); others open the release page.
        onOpenRequested: veyra.updates.canInstall ? veyra.updates.installUpdate() : veyra.updates.openRelease()
    }
    Connections {
        target: veyra.updates
        function onUpdateAvailable(version) { updateBanner.show(version) }
    }
    // pages.css .canvas-toast: top 12px, padding 7px 12px, from translate -8px and
    // opacity 0; .on: opacity .2s, translate .45s --spring; hidden after 2600 ms.
    Rectangle {
        id: toast
        // Under the dock (z 40): an opened dock stays usable over a toast.
        z: 30
        property string message: ""
        property bool isError: false
        property bool on: false
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 12
        // Over the picture: cut out of the video window.
        objectName: "videoCover"
        property real coverRadius: 9
        width: Math.min(toastText.implicitWidth + 24 + 2, root.width - 48)
        height: toastText.implicitHeight + 14 + 2
        radius: 9
        color: toast.isError ? "#2A1414" : "#132218"
        border.width: 1
        border.color: toast.isError ? Qt.rgba(1, 0.365, 0.365, 0.4) : Qt.rgba(0.239, 0.863, 0.518, 0.35)
        opacity: on ? 1 : 0
        visible: opacity > 0.01
        transform: Translate { id: toastShift; y: toast.on ? 0 : -8
            Behavior on y { NumberAnimation { duration: Theme.d(450); easing.bezierCurve: Theme.spring } } }
        Behavior on opacity { NumberAnimation { duration: Theme.d(200) } }
        function show(m, bad) { toast.message = m; toast.isError = bad; toast.on = true; toastTimer.restart() }
        Timer { id: toastTimer; interval: 2600; onTriggered: toast.on = false }
        Text {
            id: toastText
            anchors.centerIn: parent
            width: Math.min(implicitWidth, root.width - 72)
            text: toast.message
            color: toast.isError ? "#FFC9C9" : "#BDF3D2"
            font.family: Theme.fontUi
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
    }

    // PushFPS: after a recording is saved, offer to edit or export it.
    PushRecordingSaved {
        z: 31
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 24
        goPage: p => root.goPage(p)
    }
    // PushFPS: measured latency and frame rate over the picture (quick setup switch). It sits where the
    // user dragged it, inside the picture's rectangle (the top left corner until then).
    PushLatencyBadge {
        z: 31
        areaX: videoHost.x
        areaY: videoHost.y
        areaWidth: videoHost.width
        areaHeight: videoHost.height
        visible: wanted && stats.running === true && videoHost.width > 200 && root.page !== "home" && root.page !== "set"
    }

    // RivaTuner, NVIDIA's overlay or GamePP started after Veyra (QmlPlayerBridge): RTSS's
    // OSD is kept out until a restart; the other two cannot be, and froze 2.0 in the field.
    VConfirm {
        id: overlayRestart
        objectName: "overlay-restart-confirm"
        property string kind: "rtss"
        readonly property string product: kind === "nvidia" ? qsTr("NVIDIA 信息浮窗") : qsTr("游戏加加（GamePP）")
        title: kind === "rtss" ? qsTr("检测到 RTSS 屏显服务") : qsTr("检测到 %1").arg(product)
        text: kind === "rtss"
              ? qsTr("检测到 RTSS 屏显服务正在运行。开启兼容绘制需重启 PushFPS，重启后 OSD 仅显示在视频上。现在重启吗？当前播放或串流将停止。")
              : qsTr("%1 正在运行。它会同时挂接界面和视频两个画面，可能让 PushFPS 卡死；开启兼容绘制需重启 PushFPS，重启后界面改用软件绘制，它只作用在视频上。现在重启吗？当前播放或串流将停止。").arg(product)
        acceptText: qsTr("重启")
        rejectText: qsTr("稍后")
        onAccepted: veyra.restartForOverlayCompatibility()
    }

    Connections {
        target: veyra
        function onPlaylistRequested() { onboarding.close(); playlistWindow.open() }
        function onNotice(text, isError) { toast.show(text, isError) }
        function onOverlayRestartSuggested(kind) { onboarding.close(); overlayRestart.kind = kind; overlayRestart.open() }
        function onNavigate(p) {
            onboarding.close()
            // PushFPS: the first-run tour, asked for from anywhere.
            if (p === "tour") { root.startTour("navigate"); return }
            // (4.3.1) A capture card was connected from the home page: the bridge asks where the
            // picture goes. The new home page shows it; everywhere else it is Professional mode.
            if (p === "afterconnect") {
                if (root.staysHomeOnConnect) veyra.logUi("ui-page", "a capture card connected from the home page plays there")
                else root.goPage("pro")
                return
            }
            // A dialog key opens the dialog; anything else is a page.
            if (p === "network" || p === "capture" || p === "ps5" || p === "moonlight" || p === "xbox" || p === "screen"
                || p === "subtitle" || p === "translation" || p === "audio" || p === "save" || p === "manage" || p === "optimize" || p === "games" || p === "hud" || p === "latencyguide" || p === "crop" || p === "stream" || p === "compare" || p === "record" || p === "micfilters" || p === "layout" || p === "frameinput" || p === "qualitytest" || p === "latencytest" || p === "crash" || p === "cardtest" || p === "diagnose" || p === "readtext") {
                dialogs.open(p)
                return
            }
            root.goPage(p === "export" ? "exp" : p)
        }
    }

    // The film's aspect drives the window in cinema mode. Reported only: the
    // engine's own values, never an invented ratio.
    Connections {
        target: veyra
        function onSnapshotChanged() {
            if (root.fullscreen && veyra.fullscreenMemorySafe === false) root.toggleFullscreen()
            if (root.testAspect > 0) return
            if (veyra.previewAspect > 0.2 && Math.abs(veyra.previewAspect - root.filmAspect) > 0.02)
                root.fitToFilm(veyra.previewAspect)
        }
        function onCompareChanged() { if(root.testAspect <= 0) root.fitToFilm(veyra.previewAspect) }
    }

    // Motion probe (G0.5, --motion-probe): starts one motion 800 ms after load and logs
    // "motion-probe,<name>,<ms>,<value>" every frame for a second, to line up against the
    // design's curve from shotpage.js ?probe=. Sampled per rendered frame, so the spacing
    // follows the display rate rather than a fixed 10 ms step.
    Loader {
        id: probeSwitch
        active: root.test.motionProbe === "switch"
        x: 40; y: 120; z: 100
        sourceComponent: VSwitch {}
    }
    Loader {
        id: probeSeg
        active: root.test.motionProbe === "seg"
        x: 40; y: 160; z: 100
        sourceComponent: VSeg {
            options: [{ id: "a", label: qsTr("自动") }, { id: "b", label: qsTr("有限 / Limited") }, { id: "c", label: qsTr("完整 / Full") }]
            current: "a"
        }
    }
    Timer {
        running: root.test.motionProbe !== undefined
        interval: 800
        onTriggered: {
            const name = root.test.motionProbe
            if (name === "dock") dock.opened = true
            else if (name === "page") { root.page = "pro"; probe.t0 = Date.now() + 150; probe.running = true; return }
            else if (name === "switch") probeSwitch.item.checked = true
            else if (name === "menu") proPage.openTestMenu("source")
            else if (name === "dialog") dialogs.open("capture")
            else if (name === "seg") { probe.x0 = probeSeg.item.indicatorX; probeSeg.item.current = "c"; probe.x1 = probeSeg.item.targetX }
            probe.t0 = Date.now()
            probe.running = true
        }
    }
    FrameAnimation {
        id: probe
        property real t0: 0
        property real x0: 0
        property real x1: 1
        running: false
        onTriggered: {
            const ms = Date.now() - t0
            const name = root.test.motionProbe
            let v
            if (name === "dock") v = dock.barY
            else if (name === "page") { if (ms < 0) return; v = proPage.probeRise.target.opacity.toFixed(4) + "," + proPage.probeRise.ty.toFixed(3) }
            else if (name === "menu") v = proPage.testMenuScale.toFixed(4)
            else if (name === "dialog") v = dialogs.motionScale.toFixed(4)
            else if (name === "seg") v = ((probeSeg.item.indicatorX - x0) / (x1 - x0)).toFixed(4)
            else if (probeSwitch.item) v = probeSwitch.item.children[0].x - 3
            else return
            console.log("motion-probe," + name + "," + ms + "," + v)
            if (ms > 1000) running = false
        }
    }

    // 设置 → 启动窗口大小: a fixed size or the last one, clamped to the screen
    // and centred. A --size test switch resizes after this, so tests keep theirs.
    function applyStartupSize() {
        const choice = veyra.preferences.windowSize || "1280x800"
        const text = choice === "last" ? (veyra.preferences.lastWindow || "1280x800") : choice
        const wh = text.split("x").map(Number)
        if (wh.length !== 2 || !(wh[0] > 0) || !(wh[1] > 0)) return
        // Centred on the monitor the pointer is on when Veyra starts (the one it was
        // launched from), not across the whole desktop.
        const a = veyra.launchScreenAvailable()
        root.width = Math.max(root.minimumWidth, Math.min(wh[0], a.width - 40))
        root.height = Math.max(root.minimumHeight, Math.min(wh[1], a.height - 40))
        root.x = a.x + Math.round((a.width - root.width) / 2)
        root.y = a.y + Math.round((a.height - root.height) / 2)
    }
    onClosing: if (!root.fullscreen && !root.maximized && !root.cinema) veyra.rememberWindowSize(root.width, root.height)
    // 日落 startup intro (DuskIntro.qml, 2026-11-15): on a plain start to the home page in the
    // dusk palette, unless 设置 → 外观 → 启动动画 is off, animations are reduced or a test
    // switch is set. Its clock starts with the first frame on screen; a page change (a file
    // from the command line, a resumed source) fades it out at once.
    Loader {
        id: introLoader
        anchors.fill: parent
        z: 1000
        active: false
        // PushFPS redesign: each design has its own (the new one: PushIntroModern.qml, the logo on the new graphite).
        sourceComponent: Theme.modern ? modernIntro : sunsetIntro
        onLoaded: item.target = root.introTarget()
    }
    Component {
        id: sunsetIntro
        PushIntro {
            skipText: qsTr("单击或按任意键跳过")
            musicClock: () => { const s = vyMusic.position(); return s >= 0 ? s * 1000 : -1 }
            onFrame: target = root.introTarget()
            onRevealHome: homePage.introHold = false
            onFinished: root.endIntro()
        }
    }
    Component {
        id: modernIntro
        PushIntroModern {
            skipText: qsTr("单击或按任意键跳过")
            onRevealHome: homePage.introHold = false
            onFinished: root.endIntro()
        }
    }
    function introTarget() {
        const w = homePage.wordmarkItem
        return w ? w.mapToItem(introLoader, 0, 0, w.width, w.height) : Qt.rect(0, 0, 0, 0)
    }
    function endIntro() {
        const i = introLoader.item
        if (i) veyra.logUi("ui-intro", (i.dismissing ? "dismissed" : "finished") + " ms=" + Math.round(i.ms) + " frames=" + i.frames
                           + " worstFrameMs=" + Math.round(i.worstFrameMs) + " size=" + root.width + "x" + root.height)
        homePage.introHold = false
        introLoader.active = false
        root.contentItem.forceActiveFocus()
    }
    Connections {
        target: root
        enabled: introLoader.item !== null
        function onFrameSwapped() {
            const i = introLoader.item
            if (!i || i.running || i.dismissing || i.ms !== 0) return
            // The music and the intro start together; the intro then follows the music's clock.
            i.followMusic = root.musicOn && root.musicSounding
            i.start()
            veyra.logUi("ui-intro", "started music=" + i.followMusic)
        }
        function onPageChanged() { if (introLoader.item && root.page !== "home") introLoader.item.dismiss() }
    }
    Component.onCompleted: {
        if (root.test.noUpdateCheck !== true) veyra.updates.startAutomaticCheck()
        // --lang <code>: screenshots in another language (saved in that run's own --data-dir).
        if (root.test.lang !== undefined) veyra.setPreference("language", root.test.lang)
        if (root.test.prefs !== undefined) {
            for (const pair of root.test.prefs) {
                const at = pair.indexOf("=")
                if (at <= 0) continue
                const raw = pair.slice(at + 1)
                veyra.setPreference(pair.slice(0, at), raw === "true" ? true : raw === "false" ? false : (raw !== "" && !isNaN(Number(raw)) ? Number(raw) : raw))
            }
        }
        if (root.test.prefs !== undefined) { const shot2 = {}; for (const pair of root.test.prefs) { const at = pair.indexOf("="); if (at > 0) { const raw = pair.slice(at + 1); shot2[pair.slice(0, at)] = raw === "true" ? true : raw === "false" ? false : raw } } Theme.shot = shot2 }
        applyStartupSize()
        // Open on the configured page: the user's saved preference, or the
        // override a test passes on the command line.
        if (veyra.initialPage.length > 0)
            root.page = veyra.initialPage === "pro" && veyra.nodeMode === 1 ? "node" : veyra.initialPage
        if (root.test.tab === "tuning") homePage.showTuning()
        else if (root.test.tab !== undefined && root.page === "set") settingsPage.section = root.test.tab  // PushFPS: settings section for screenshots
        else if (root.test.tab !== undefined && root.test.tab !== "home-preview") proPage.tab = root.test.tab
        if (root.testAspect > 0) root.fitToFilm(root.testAspect)
        if (root.test.dialog !== undefined) dialogs.open(root.test.dialog)
        videoHost.syncRect()
        // Independent of animation, palette, reduced motion and initial page. Never restart.
        // PushFPS: not in screenshot and test runs (the corner music button would show in every picture).
        if (root.musicOn && vyMusic.available && !veyra.hasSource && !veyra.openingSource && Object.keys(root.test).length === 0) {
            vyMusic.prepare(veyra.volume)
            vyMusic.play()
        }
        // --intro-at <ms>: the intro held at one moment, for screenshots.
        if (root.test.introAt !== undefined) {
            homePage.introHold = true
            introLoader.active = true
            if (introLoader.item) introLoader.item.ms = Math.max(1, Number(root.test.introAt))
        }
        if (root.test.menu !== undefined) testMenuTimer.start()
        if (root.test.scrollTo !== undefined) testScrollTimer.start()
        if (root.test.fullBar !== undefined) testFullTimer.start()
        // PushFPS: the Sunset City intro (PushIntro.qml), see brandIntro.
        if (root.brandIntro && veyra.preferences.introAnimation !== false
                && !veyra.reducedMotion && root.page === "home" && Object.keys(root.test).length === 0) {
            homePage.introHold = true
            introLoader.active = true
        }
    }

    // Startup music plays once, continuing after the intro or without it. Media-open
    // intent or the shared setting/button permanently stops it for this launch.
    // PushFPS 连接后自动全屏 (pushAutoFullscreen): when a capture card or stream starts showing a
    // picture, go fullscreen once. Leaving fullscreen is the player's choice and is not undone.
    readonly property string pushLiveKind: veyra.sourceKind
    onPushLiveKindChanged: {
        const live = ["capture", "ps5", "moonlight", "xbox"].indexOf(pushLiveKind) >= 0
        if (live && veyra.preferences.pushAutoFullscreen === true && !root.fullscreen && Object.keys(root.test).length === 0) {
            veyra.logUi("ui-fullscreen", "automatic for " + pushLiveKind)
            Qt.callLater(root.toggleFullscreen)
        }
    }
    // PushFPS 窗口置顶 (pushAlwaysOnTop): keeps the player window above other windows, applied
    // with Win32 SetWindowPos so the native video window is never recreated. Fullscreen and
    // restore can reset the z-order, so it is applied again after every visibility change.
    readonly property bool pushOnTop: veyra.preferences.pushAlwaysOnTop === true
    function applyOnTop() { if (typeof veyra.pushSetAlwaysOnTop === "function") veyra.pushSetAlwaysOnTop(root, root.pushOnTop) }
    onPushOnTopChanged: applyOnTop()
    onVisibilityChanged: Qt.callLater(root.applyOnTop)

    // PushFPS "Sunset City" intro (PushIntro.qml) and the startup music, each with its own switch in
    // 设置 → 外观 (introAnimation unset = on; introMusic unset = off, PushFPS default since 3.0.0). The music is the file Veyra's runtime
    // package installs (media/startup-music.mp3); PushFPS never ships or copies it.
    readonly property bool brandIntro: veyra.preferences.introAnimation !== false
    readonly property bool musicOn: typeof vyMusic !== "undefined" && veyra.preferences.introMusic === true
    readonly property bool musicSounding: typeof vyMusic !== "undefined" && (vyMusic.state === "loading" || vyMusic.state === "playing")
    function stopStartupMusicForSession() {
        if (typeof vyMusic === "undefined") return
        vyMusic.pause()
        veyra.logUi("ui-music", "stopped for this launch (button)")
    }
    readonly property bool sourceOpen: veyra.hasSource
    onSourceOpenChanged: if (sourceOpen && typeof vyMusic !== "undefined") vyMusic.pause()
    // Bottom right of the home page, above the intro too, while the music plays.
    Rectangle {
        id: musicButton
        objectName: "music-toggle"
        z: 1001
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 16
        width: 34
        height: 34
        radius: 17
        visible: root.musicSounding && !veyra.hasSource
        color: musicHover.hovered ? Theme.card3 : Qt.rgba(0, 0, 0, 0.28)
        border.width: 1
        border.color: Theme.stroke2
        VIcon {
            anchors.centerIn: parent
            name: "music"
            size: 16
            color: vyMusic.muted ? Theme.t3 : Theme.t1
        }
        // Muted: the note crossed out.
        Rectangle {
            visible: vyMusic.muted
            anchors.centerIn: parent
            width: 22
            height: 2
            radius: 1
            rotation: -45
            color: Theme.t2
        }
        HoverHandler { id: musicHover; cursorShape: Qt.PointingHandCursor }
        TapHandler {
            onTapped: root.stopStartupMusicForSession()
        }
        ToolTip.visible: musicHover.hovered
        ToolTip.text: qsTr("停止本次启动音乐")
    }
    // PushFPS (screenshots): --scroll-to <objectName> once the settings page is laid out.
    Timer { id: testScrollTimer; interval: 1500; onTriggered: (root.page === "home" ? homePage : settingsPage).scrollToObject(root.test.scrollTo) }
    // The menu anchors on laid-out buttons, so it opens once the page has settled.
    // --full-bar shown|hidden: enter fullscreen with the control window held shown
    // or held hidden, for the present-timing comparison (G2.5).
    Timer { id: testFullTimer; interval: 1500; onTriggered: { root.toggleFullscreen(); if (root.test.fullBar === "hidden") root.fullControls = false } }
    Timer {
        id: testMenuTimer
        interval: 600
        onTriggered: root.test.menu === "min-source" ? minPage.openSourceMenuForTest() : proPage.openTestMenu(root.test.menu)
    }
}
