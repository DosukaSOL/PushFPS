// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
import QtQuick
import QtQuick.Layouts
import "EffectSupport.js" as Support
import "PushCaptureCards.js" as Cards
import "PushGamePresets.js" as Presets
import "PushCompareClip.js" as CompareClip
import "PushCrop.js" as Crop
import "PushUpscale.js" as Upscale
import "PushKbm.js" as Kbm

// PushFPS quick setup: the choices most console players make, above the detailed
// controls. Every control here only sets existing settings (frame generation on/off,
// multiplier, output cap, NR layers, NVIDIA low latency, display sync), so Professional mode always
// shows exactly what was chosen and nothing here adds engine state.
VGroup {
    id: quick
    // True once Optimise for my PC applied its choice (record pushBenchmark, applied; Undo clears it).
    readonly property bool optimized: {
        try { return JSON.parse(String(veyra.preferences.pushBenchmark || "{}")).applied === true } catch (e) { return false }
    }
    // Smooth frame pacing (row pushfps-smooth): V-Sync + a variable refresh request, with the output
    // capped (at the display when no target is set; a chosen target such as 60 FPS stays). Off returns
    // to Automatic, no VRR request, and drops only the follow-the-display cap. Before 3.1.6 the switch
    // also needed the follow-the-display cap, so choosing a 60 FPS target turned it off and live play
    // skipped frames the recording still had (field report 2026-11-17).
    readonly property bool smoothPacing: veyra.displaySync === 1 && veyra.variableRefresh === true && veyra.outputRateMode !== 0
    function setSmoothPacing(on) {
        veyra.displaySync = on ? 1 : 2
        veyra.variableRefresh = on
        if (on && veyra.outputRateMode === 0) veyra.outputRateMode = 1
        else if (!on && veyra.outputRateMode === 1) veyra.outputRateMode = 0
        veyra.setPreference("pushSmoothPacing", on)
    }
    // A capped target that does not divide the refresh rate (60 on 144 Hz) without a variable refresh
    // display: frames stay on screen for uneven times however well they are made.
    // A fixed target the screen cannot show (120 FPS on a 60 Hz screen) while every frame waits for a
    // refresh: the engine then puts out the screen's refresh rate (PushDisplayCap.h, 4.1; before, a
    // window moved to a slower screen fell back to the game's own frame rate). The target stays
    // chosen, and it is the output again on a screen that can show it.
    readonly property bool screenLimitedTarget: {
        const hz = Number(veyra.displayRefreshHz || 0)
        return veyra.outputRateMode === 2 && veyra.displaySync === 1 && hz >= 1 && Number(veyra.outputCustomFps) > hz * 1.02
    }
    readonly property bool unevenTarget: {
        const hz = Number(veyra.displayRefreshHz || 0)
        if (quick.screenLimitedTarget) return false
        if (veyra.outputRateMode !== 2 || hz < 1 || Number(veyra.vrrState || 0) >= 2) return false
        const ratio = hz / Math.max(1, Number(veyra.outputCustomFps))
        return Math.abs(ratio - Math.round(ratio)) > 0.05
    }
    // One-tap lowest latency (row pushfps-lowest-latency): only switches that cut waiting without
    // touching the picture: the one-frame present queue (NVIDIA low latency on NVIDIA, the low
    // queue elsewhere), fullscreen on connect (a fullscreen window skips the desktop compositor's
    // extra refresh), and on a G-SYNC/FreeSync screen smooth frame pacing with variable refresh
    // (no tearing, no skipped frames, almost no wait below the refresh rate). Frame generation,
    // AI enhancement, HUD lock and pacing repair stay as they are. Turning it off restores what
    // was set before (preference pushLowestLatency: the saved values as JSON).
    readonly property bool vrrCapable: Number(veyra.vrrState || 0) >= 2
    readonly property bool lowestLatency: (veyra.nvidiaLowLatencyAvailable === true ? Number(veyra.nvidiaLowLatency || 0) > 0 : veyra.fgLowQueue === true)
        && veyra.preferences.pushAutoFullscreen === true
        && (!quick.vrrCapable || (quick.smoothPacing && veyra.variableRefresh === true))
    function setLowestLatency(on) {
        const nvidia = veyra.nvidiaLowLatencyAvailable === true
        if (on) {
            veyra.setPreference("pushLowestLatency", JSON.stringify({
                nvidia: Number(veyra.nvidiaLowLatency || 0), queue: veyra.fgLowQueue === true,
                fullscreen: veyra.preferences.pushAutoFullscreen === true, display: veyra.displaySync,
                vrr: veyra.variableRefresh === true, rate: veyra.outputRateMode }))
            if (nvidia) veyra.nvidiaLowLatency = 1
            else veyra.fgLowQueue = true
            veyra.setPreference("pushAutoFullscreen", true)
            if (quick.vrrCapable) quick.setSmoothPacing(true)
            // Full screen on connect only acts at the next connection; with a picture playing the
            // window goes full screen now (Main.qml leaves it alone when it already is), or the
            // switch seems to do nothing (a report after 4.5.0).
            if (veyra.running === true) veyra.navigate("fullscreen")
            veyra.logUi("push-latency", "lowest latency on nvidia=" + nvidia + " vrr=" + quick.vrrCapable + " playing=" + (veyra.running === true))
            return
        }
        let saved = null
        try { saved = JSON.parse(String(veyra.preferences.pushLowestLatency || "")) } catch (e) { saved = null }
        if (!saved || typeof saved !== "object") saved = { nvidia: 0, queue: false, fullscreen: false }
        if (nvidia) veyra.nvidiaLowLatency = Number(saved.nvidia || 0) > 0 ? 1 : 0
        else veyra.fgLowQueue = saved.queue === true
        veyra.setPreference("pushAutoFullscreen", saved.fullscreen === true)
        if (typeof saved.display === "number" && typeof saved.rate === "number") {
            veyra.displaySync = saved.display
            veyra.variableRefresh = saved.vrr === true
            veyra.outputRateMode = saved.rate
        }
        veyra.logUi("push-latency", "lowest latency off (previous settings restored)")
    }
    readonly property string lowestLatencyHint: {
        const ms = Number(quick.liveStats.latencyMs || 0)
        const now = ms > 0 ? qsTr("现在电脑处理延迟 %1 毫秒。").arg(ms.toFixed(0)) : ""
        const capture = Number(quick.liveStats.captureFps || 0)
        const tip = capture > 0 && capture < 100 && (quick.consoleId === "ps5" || quick.consoleId === "ps5pro" || quick.consoleId === "xbox")
                  ? qsTr("主机输出 120 Hz 时每帧还能早约 8 毫秒到达。") : ""
        if (!quick.lowestLatency) return qsTr("一键减少等待：画质、补帧和 AI 增强都不变")
        return now + (quick.vrrCapable ? qsTr("单帧队列、全屏、可变刷新已开启。") : qsTr("单帧队列和全屏已开启；屏幕不支持可变刷新，帧同步保持不变以免画面撕裂。")) + tip
    }
    objectName: "pushfps-quick-setup"
    // Home shows the short form: no low-latency row and no intro line.
    property bool compact: false
    // Home only: the first-run setup tip, until a capture card has been used once.
    property bool showSetupTip: false
    // Set where the picture is already visible next to the quick setup (Professional mode).
    property bool compactPro: false
    // The three places the quick setup stands in, by name. Each shows a setting once: what the page beside
    // it already has (Professional mode's own list under it, streaming mode's buttons) is left to that page.
    readonly property bool proPage: quick.compactPro && !quick.compact
    readonly property bool streamingPage: quick.compactPro && quick.compact
    // PushFPS 4.2.1: which part of the quick setup this card is. "" is all of it (the home page, streaming
    // mode). Professional mode has two cards: "picture" (the Picture section alone, in its Quality tab, with
    // that tab's own enhancement settings; a user asked for them to stand together) and "rest" (everything
    // else, in its Frame Gen tab). A hidden section is hidden as a whole; nothing in a row changes.
    property string scope: ""
    // Asks the page this card stands in for one of its tabs (Professional mode: "quality", "fg", ...).
    signal requestTab(string tab)
    // PushFPS redesign (the new home page; both are what they always were unless a page sets them):
    // the page shows the card's title itself, pinned over the list (showHeader false), and gives the
    // six tools a place of its own under the picture (toolsHost: the item they are drawn in, as wide as
    // that item; toolsHeight is how high they are there). The tools are the same items wherever they
    // are drawn: nothing about what a tile does or shows changes.
    property bool showHeader: true
    property Item toolsHost: null
    // The short form leaves five rows to Professional mode: smooth frame pacing, the G-SYNC set-up, low
    // latency, connecting at start-up and full screen on connecting. It is what the home page and
    // streaming mode always had (`compact`), and what they have in Legacy. The new design shows them
    // there too (the owner: nothing lost, nothing hidden, no going from page to page for a setting).
    property bool shortForm: compact
    readonly property real toolsHeight: toolsGrid.implicitHeight
    // The sections by name, for a page that lists them (the new home page's index over the list).
    function sectionItem(id) {
        for (let i = 0; i < quick.sectionItems.length; ++i)
            if (quick.sectionItems[i] && quick.sectionItems[i].sectionId === id) return quick.sectionItems[i]
        return null
    }
    // A row by its object name, for a page that scrolls to it (null when it is not shown).
    function rowItem(name) {
        const find = item => {
            if (!item) return null
            if (item.objectName === name) return item
            const kids = item.children || []
            for (let i = 0; i < kids.length; ++i) { const hit = find(kids[i]); if (hit) return hit }
            return null
        }
        for (let i = 0; i < quick.sectionItems.length; ++i) {
            const hit = find(quick.sectionItems[i])
            if (hit) return hit.visible ? hit : null
        }
        return null
    }
    readonly property bool scopeMain: quick.scope !== "picture"
    readonly property bool scopePicture: quick.scope !== "rest"

    // --- sections (PushSection.qml) ----------------------------------------------
    // Which sections are folded is remembered (preference pushQuickFolded: their ids, with commas); every
    // section is open until its title is clicked.
    readonly property var foldedSections: String(veyra.preferences.pushQuickFolded || "").split(",").filter(s => s.length > 0)
    function sectionOpen(id) { return quick.foldedSections.indexOf(id) < 0 }
    function setSectionOpen(id, open) {
        const list = quick.foldedSections.filter(s => s !== id)
        if (!open) list.push(id)
        veyra.setPreference("pushQuickFolded", list.join(","))
    }
    // Opens the section a row stands in (a jump to that row, as the screenshot run's --scroll-to makes).
    function reveal(rowName) {
        for (let i = 0; i < quick.sectionItems.length; ++i) {
            const s = quick.sectionItems[i]
            if (s && !s.open && s.holds(rowName)) quick.setSectionOpen(s.sectionId, true)
        }
    }
    readonly property var sectionItems: [secSetup, secFg, secPicture, secHud, secLatency, secStart]
    // --- the first-run tour (PushTour.qml) ---------------------------------------
    // What a step of the tour points at here, or null where this PC does not offer it (AI picture
    // enhancement that cannot be switched on is not presented as a choice).
    function tourItem(key) {
        if (key === "console") return secSetup
        if (key === "target") return targetRow
        if (key === "enhance") return quick.nrPossible ? enhanceRow : null
        if (key === "tools") return toolsGrid
        return null
    }
    // Opens the section a step's row stands in: folded, the tour would point at nothing.
    function showTourStep(key) {
        const id = key === "console" ? "setup" : key === "target" ? "fg" : key === "enhance" ? "picture" : ""
        if (id !== "" && !quick.sectionOpen(id)) quick.setSectionOpen(id, true)
    }
    // What a section's title says beside it: the choice that matters most there, in words that exist already.
    function sectionSummary(id) {
        if (id === "setup") {
            const c = quick.consoles.find(o => o.id === quick.consoleId)
            return (c ? c.label + " · " : "") + quick.baseFps + " FPS"
        }
        if (id === "fg") {
            const t = quick.currentTarget
            return t === "off" ? qsTr("关闭", "off") : t === "monitor" ? qsTr("匹配显示器") : quick.isTarget(t) ? t + " FPS" : ""
        }
        return ""
    }
    // Console games send their frames inside a 60 Hz signal: 30 FPS for most games
    // (GTA VI), 40 in some 120 Hz modes, 60 in performance modes. The player picks it
    // (设置 key pushGameFps); the multiplier is chosen from it, and the output cap holds
    // the result at the target.
    readonly property int baseFps: {
        const n = Number(veyra.preferences.pushGameFps || 30)
        return n === 40 || n === 60 ? n : 30
    }
    function setGameFps(id) {
        const n = parseInt(id)
        if (!veyra.setPreference("pushGameFps", n)) return
        // Keep the chosen target: the multiplier is worked out again for the new rate.
        // A target the game already reaches (60 FPS for a 60 FPS game) has nothing to
        // generate, so frame generation goes off instead of doubling past it.
        const t = quick.currentTarget
        if (quick.isTarget(t)) quick.applyTarget(t, n)
    }
    // The choices that generate frames ("off" and "" do not).
    function isTarget(id) { return id === "60" || id === "90" || id === "120" || id === "monitor" }
    // A 90 Hz screen (laptops, handhelds, some monitors): 120 FPS cannot be shown there, 90 can, and a
    // 30 FPS game reaches it with exactly 3X. The 90 FPS choice is offered on such a screen, and kept
    // visible wherever it is already the target (a profile from another screen, Professional mode).
    readonly property bool screen90: {
        const hz = Number(veyra.displayRefreshHz || 0)
        return hz > 85 && hz < 95
    }
    // A screen whose refresh rate is a whole number of 90s (180, 270, 360 Hz): 90 FPS divides it evenly, so a
    // 30 FPS game at exactly 3X is paced evenly there as well. Offered beside 120 (a user's request for a
    // 360 Hz screen, 20 November 2026).
    readonly property bool screenFits90: {
        const hz = Number(veyra.displayRefreshHz || 0)
        const n = Math.round(hz / 90)
        return n >= 2 && Math.abs(hz - n * 90) <= n * 1.5
    }
    readonly property var targetOptions: {
        const list = [{ id: "off", label: qsTr("关闭", "off") }]
        // 60 FPS is only a target when the game runs below it; 90 only for such a game too
        // (a 60 FPS game would need one and a half frames per frame, which cannot be paced evenly).
        if (quick.baseFps < 60) list.push({ id: "60", label: "60 FPS" })
        if (quick.currentTarget === "90" || ((quick.screen90 || quick.screenFits90) && quick.baseFps < 60)) list.push({ id: "90", label: "90 FPS" })
        if (!quick.screen90 || quick.currentTarget === "120") list.push({ id: "120", label: "120 FPS" })
        list.push({ id: "monitor", label: qsTr("匹配显示器") })
        return list
    }

    readonly property bool fgPossible: Support.available(veyra, "frame-generation")
    readonly property bool nrPossible: Support.available(veyra, "nr")

    // Which quick choice matches the current settings; "" when Professional mode
    // set something else (a custom cap, or no cap at all).
    readonly property string currentTarget: {
        if (!veyra.fgEnabled) return "off"
        if (veyra.outputRateMode === 1) return "monitor"
        if (veyra.outputRateMode === 2 && Math.abs(veyra.outputCustomFps - 60) < 0.5) return "60"
        if (veyra.outputRateMode === 2 && Math.abs(veyra.outputCustomFps - 90) < 0.5) return "90"
        if (veyra.outputRateMode === 2 && Math.abs(veyra.outputCustomFps - 120) < 0.5) return "120"
        return ""
    }

    function targetFps(id) {
        if (id === "60") return 60
        if (id === "90") return 90
        if (id === "120") return 120
        if (id === "monitor") return veyra.displayRefreshHz > 0 ? veyra.displayRefreshHz : 60
        return 0
    }

    // The largest multiplier the current backend offers whose output stays at or
    // under the target; the smallest offered one when even that overshoots.
    // Adaptive multiplier (EnhancementSettings::fgAdaptive): the multiplier becomes a
    // ceiling and the engine picks, per frame pair, the one closest to the target from
    // the game's measured frame rate. Only worth it when the backend offers more than 2X;
    // XeSS paces its own presentation and keeps its fixed multiplier.
    readonly property var offeredMultipliers: (veyra.fgMultiplierChoices || []).map(o => parseInt(o.id)).filter(m => m >= 2)
    readonly property bool adaptivePossible: quick.fgPossible && !veyra.presentationOwned && quick.offeredMultipliers.some(m => m > 2)
    readonly property bool adaptive: quick.adaptivePossible && veyra.fgAdaptive === true
    function setAdaptive(on) {
        veyra.fgAdaptive = on
        // The game's frame rate is measured by automatic duplicate removal.
        if (on && Number(veyra.contentRate) === 0) veyra.contentRate = 1
        const t = quick.currentTarget
        if (quick.isTarget(t)) quick.applyTarget(t)
    }
    // Live: the multiplier the last frame pair used, the measured game frame rate and, for
    // Frame Pacing Repair, the share of real frames it re-timed over the last second.
    property var liveStats: ({})
    property var lastPacing: null
    // Going live: polled while a stream runs (or every few seconds) for the tile.
    property var liveStream: ({})
    Timer {
        interval: quick.liveStream.active ? 1000 : 3000; repeat: true
        running: quick.visible && typeof veyra.pushStreamStatus === "function"
        triggeredOnStart: true
        onTriggered: quick.liveStream = veyra.pushStreamStatus() || ({})
    }
    property real pacingShare: -1
    Timer {
        interval: 1000; repeat: true
        running: (quick.adaptive || quick.pacingOn || veyra.hudLock === true || veyra.fgBackendName === "pushflow" || veyra.preferences.pushCrosshairHold === true || veyra.preferences.pushSubtitleHold === true) && veyra.hasSource && veyra.fgEnabled && quick.visible
        triggeredOnStart: true
        onTriggered: {
            const st = veyra.pushLiveStats()
            const p = quick.lastPacing
            if (p && st.pacingFrames > p.pacingFrames) quick.pacingShare = (st.pacingRepaired - p.pacingRepaired) / (st.pacingFrames - p.pacingFrames)
            quick.lastPacing = { pacingFrames: Number(st.pacingFrames || 0), pacingRepaired: Number(st.pacingRepaired || 0) }
            quick.liveStats = st
        }
        onRunningChanged: if (!running) { quick.liveStats = ({}); quick.lastPacing = null; quick.pacingShare = -1 }
    }

    // Frame Pacing Repair (EnhancementSettings::pacingRepair) and Live HUD Lock (::hudLock):
    // both work on graph frame generation; XeSS paces and generates inside its presenter.
    readonly property bool ownTechPossible: quick.fgPossible && !veyra.presentationOwned
    readonly property bool pacingOn: quick.ownTechPossible && veyra.pacingRepair === true
    function setPacingRepair(on) {
        veyra.pacingRepair = on
        // The repair re-times the frames duplicate removal finds.
        if (on && Number(veyra.contentRate) === 0) veyra.contentRate = 1
    }

    function multiplierFor(fps, base) {
        const gameFps = base || quick.baseFps
        const offered = (veyra.fgMultiplierChoices || []).map(o => parseInt(o.id))
            .filter(m => m >= 2).sort((a, b) => a - b)
        if (offered.length === 0) return 0
        let pick = offered[0]
        for (let i = 0; i < offered.length; ++i)
            if (gameFps * offered[i] <= fps + 0.5) pick = offered[i]
        return pick
    }

    // base: the game's frame rate; defaults to the saved one (a profile passes its own,
    // since the preference it just wrote may not have reached the binding yet).
    function applyTarget(id, base) {
        const gameFps = base || quick.baseFps
        if (id === "off") { veyra.fgEnabled = false; return }
        const fps = targetFps(id)
        if (fps <= gameFps + 0.5) { veyra.fgEnabled = false; return }
        // 90 from a 60 FPS game is one and a half frames per frame: it cannot be paced evenly.
        if (id === "90" && gameFps >= 60) { veyra.fgEnabled = false; return }
        const m = quick.adaptive ? Presets.adaptiveCeiling(quick.offeredMultipliers, fps) : multiplierFor(fps, gameFps)
        if (m < 2) return
        veyra.fgMultiplier = m
        // A console game below the capture rate repeats every picture (30 FPS in a 60 Hz
        // signal: AABBCC). Generating between two identical pictures wastes the work and
        // smears, so the quick setup always removes duplicates first; automatic removal
        // passes a game that has none untouched. A chosen mode (60->30) stays.
        if (Number(veyra.contentRate) === 0) veyra.contentRate = 1
        // XeSS paces its own presentation (presentationOwned); the cap is then
        // greyed in Professional mode too, and the multiplier alone decides.
        if (!veyra.presentationOwned) {
            if (id === "monitor") veyra.outputRateMode = 1
            else { veyra.outputCustomFps = fps; veyra.outputRateMode = 2 }
        }
    }

    function setEnhancement(on) {
        if (on) {
            if (veyra.nrLayers.length === 0) veyra.addEffect("nr")
            else veyra.setAllNrEnabled(true)
        } else if (veyra.nrLayers.length > 0) {
            veyra.setAllNrEnabled(false)
        }
    }

    // --- the look of AI picture enhancement --------------------------------------
    // Two of the enhancement model's own controls: how far it reworks light and colour ("tone") and
    // how much fine detail it adds ("structure"), both 0..1 with 1 the model's full strength (what
    // PushFPS has always used, and what Reset returns to). Professional mode shows the same two
    // values per layer; here every enhancement layer gets the same pair, and the first one is shown.
    // Nothing is stored apart from those layer values and the look kept in each profile.
    readonly property var looks: [
        { id: "subtle", label: qsTr("柔和"), tone: 0.4, structure: 0.5 },
        { id: "balanced", label: qsTr("均衡"), tone: 0.7, structure: 0.8 },
        { id: "strong", label: qsTr("强烈"), tone: 1, structure: 1 }
    ]
    readonly property var lookLayer: quick.nrPossible && veyra.nrLayers.length > 0 ? veyra.nrLayers[0] : null
    function lookValue(v) { const n = Number(v); return isFinite(n) ? Math.max(0, Math.min(1, n)) : 1 }
    readonly property real lookTone: lookLayer && lookLayer.tone !== undefined ? lookValue(lookLayer.tone) : 1
    readonly property real lookStructure: lookLayer && lookLayer.structure !== undefined ? lookValue(lookLayer.structure) : 1
    readonly property string lookId: {
        for (const l of quick.looks)
            if (Math.abs(l.tone - quick.lookTone) < 0.005 && Math.abs(l.structure - quick.lookStructure) < 0.005) return l.id
        return ""
    }
    // The AMD enhancement runtime (4) has no such controls (Professional mode greys them out there too).
    readonly property bool lookShown: quick.nrPossible && veyra.nrAnyEnabled === true && quick.lookLayer !== null && Number(quick.lookLayer.runtime) !== 4
    // undefined leaves that value as it is.
    function setLook(tone, structure) {
        if (!quick.nrPossible) return false
        let ok = true
        const layers = veyra.nrLayers.map(l => l.index)
        for (const index of layers) {
            if (tone !== undefined) ok = veyra.setNrLayerParameter(index, "tone", Math.round(quick.lookValue(tone) * 100) / 100) && ok
            if (structure !== undefined) ok = veyra.setNrLayerParameter(index, "structure", Math.round(quick.lookValue(structure) * 100) / 100) && ok
        }
        return ok && layers.length > 0
    }
    function applyLook(id) {
        for (const l of quick.looks) if (l.id === id) return quick.setLook(l.tone, l.structure)
        return false
    }
    function resetLook() { return quick.setLook(1, 1) }
    // The size AI picture enhancement works at (the layer's sizePolicy: Professional mode's "internal
    // processing resolution"). The standard is 1080p whatever the picture's size: on a 4K picture the
    // enhancement's change is worked out at a quarter of the pixels and added to the 4K picture.
    // "Full size" works it out on every pixel, which is sharper and about four times the work at 4K.
    // Ids as the engine has them: 0 the standard (up to 1080p), 5 up to 1440p, 1 the picture's own size.
    readonly property var enhanceSizes: [
        { id: 0, label: "1080p" }, { id: 5, label: "1440p" }, { id: 1, label: qsTr("完整大小") }
    ]
    readonly property int enhanceSize: lookLayer && lookLayer.sizePolicy !== undefined && isFinite(Number(lookLayer.sizePolicy)) ? Number(lookLayer.sizePolicy) : 0
    readonly property bool enhanceSizeKnown: quick.enhanceSizes.some(s => s.id === quick.enhanceSize)
    function setEnhanceSize(id) {
        if (!quick.nrPossible || !quick.enhanceSizes.some(s => s.id === Number(id))) return false
        let ok = true
        const layers = veyra.nrLayers.map(l => l.index)
        for (const index of layers) ok = veyra.setNrLayerParameter(index, "sizePolicy", Number(id)) && ok
        return ok && layers.length > 0
    }
    // Split view while tuning: the picture without the enhancement on the left, with it on the right
    // (the comparison Professional mode has under Display). Frame generation pauses while it is on,
    // so a split this panel turned on never stays: it ends when the look controls go away, when this
    // panel is hidden, and after two minutes.
    readonly property bool lookSplit: veyra.compareMode === 2
    readonly property bool compareSwapped: veyra.preferences.pushCompareSwap === true

    // --- upscale to my screen (4.2.1; PushUpscale.js has the rules) ------------------------------------
    // A capture smaller than the screen is only stretched to it by the window: sharp in a small window, soft
    // in full screen (a user's report). The upscalers were in Professional mode and nowhere else. This row
    // switches the existing super resolution stage on with an upscaler the card can run.
    property real screenHighOverride: 0   // tests
    readonly property real screenHigh: quick.screenHighOverride > 0 ? quick.screenHighOverride
                                       : Math.round(Screen.height * Math.max(1, Screen.devicePixelRatio))
    readonly property var upscaleChoices: Upscale.offered(veyra.srBackendChoices)
    // Whether any upscaler can run on this card. (The bridge's "sr" answers for the upscaler chosen now, which
    // on a card without the NVIDIA upscalers is one it cannot run: asking that would switch this row off on
    // exactly the cards it takes another upscaler for.)
    readonly property bool upscalePossible: quick.upscaleChoices.length > 0
    readonly property bool upscaleOn: veyra.srEnabled === true
    readonly property string upscaleNow: typeof veyra.srAlgorithmOf === "function" ? String(veyra.srAlgorithmOf(veyra.videoSrQuality)) : ""
    readonly property bool upscaleGains: Upscale.gains(Number(veyra.sourceHeight || 0), quick.screenHigh)
    // The FSR version really running (AMD's own name for it, e.g. 4.1.1), read while FSR upscales.
    property string fsrUpscalerVersion: ""
    readonly property string upscaleHint: {
        if (!quick.upscalePossible && !quick.upscaleOn) return Upscale.note(veyra.srBackendChoices, "6") || Support.reason(veyra, "sr")
        if (quick.upscaleOn) {
            if (quick.upscaleNow === "5" && quick.fsrUpscalerVersion.length > 0) return qsTr("正在使用 %1").arg("FSR " + quick.fsrUpscalerVersion)
            const name = Upscale.label(veyra.srBackendChoices, quick.upscaleNow)
            return name.length > 0 ? qsTr("正在使用 %1").arg(name) : qsTr("放大到屏幕分辨率")
        }
        if (quick.upscaleGains) return qsTr("画面是 %1p，屏幕是 %2p：开启后全屏更清晰").arg(Number(veyra.sourceHeight || 0)).arg(quick.screenHigh)
        return qsTr("放大到屏幕分辨率")
    }
    function setUpscale(on) {
        if (on) {
            const id = Upscale.method(veyra.srBackendChoices, quick.upscaleNow)
            if (id === "") return
            if (id !== quick.upscaleNow) veyra.videoSrQuality = veyra.srQualityFor(id, veyra.srTierOf(veyra.videoSrQuality))
            const size = Upscale.targetIndex(quick.screenHigh, veyra.srTargetIndex)
            if (size > 0) veyra.srTargetIndex = size
        }
        veyra.srEnabled = on
    }
    Timer {
        objectName: "pushfps-upscale-timer"
        interval: 2000; repeat: true
        running: quick.visible && quick.scopePicture && !quick.proPage && veyra.hasSource === true && quick.upscaleOn && quick.upscaleNow === "5"
        triggeredOnStart: true
        onTriggered: {
            const st = veyra.pushLiveStats ? veyra.pushLiveStats() : null
            quick.fsrUpscalerVersion = st ? String(st.fsrUpscalerVersion || "") : ""
        }
        onRunningChanged: if (!running) quick.fsrUpscalerVersion = ""
    }
    property bool lookCompare: false
    function setLookCompare(on) {
        if (on) {
            if (veyra.hasSource !== true) return
            quick.lookCompare = true
            veyra.compareMode = 2
        } else {
            quick.lookCompare = false
            if (veyra.compareMode === 2) veyra.compareMode = 0
        }
    }
    onLookSplitChanged: if (!lookSplit) lookCompare = false
    onLookShownChanged: if (!lookShown && lookCompare) setLookCompare(false)
    onVisibleChanged: if (!visible && lookCompare) setLookCompare(false)
    Timer {
        objectName: "pushfps-look-compare-timer"
        interval: 120000
        running: quick.lookCompare && quick.lookSplit
        onTriggered: quick.setLookCompare(false)
    }

    // --- fine-tuning: shaping the change the enhancement makes ---------------------
    // The enhancement is composed as a change on top of the picture, and that change can be shaped
    // without touching the picture itself. Four of the values are Veyra's own, per enhancement layer
    // (Professional mode shows them in each layer; here every layer gets the same value and the
    // first one is shown): total, brighten, darken, color. Six are PushFPS's (pipeline/
    // PushEnhanceShape.h), live preferences that apply to every layer: the tonal ranges, the hue and
    // the two soft limits. Everything at its default is exactly the enhancement as it always was.
    // `to` is the slider's upper end as a value (2 = 200 %), `neutral` what leaves the change alone.
    readonly property var fineSpecs: [
        { id: "total", layer: "total", to: 2, neutral: 1, label: qsTr("修改强度"),
          hint: qsTr("AI 对画面的改动量：0% 完全不改，100% 是模型的原样，200% 加倍") },
        { id: "brighten", layer: "brighten", to: 2, neutral: 1, label: qsTr("提亮"), hint: qsTr("保留多少 AI 带来的提亮。0% 取消这部分改动") },
        { id: "darken", layer: "darken", to: 2, neutral: 1, label: qsTr("压暗"), hint: qsTr("保留多少 AI 带来的压暗。0% 取消这部分改动") },
        { id: "maxBrighten", pref: "pushShapeMaxBrighten", to: 1, neutral: 0, label: qsTr("提亮上限"),
          hint: qsTr("每个像素最多能被提亮多少（按黑到白的亮度范围算，柔和地限制）。0% 为不限制") },
        { id: "maxDarken", pref: "pushShapeMaxDarken", to: 1, neutral: 0, label: qsTr("压暗上限"),
          hint: qsTr("每个像素最多能被压暗多少。0% 为不限制") },
        { id: "shadows", pref: "pushShapeShadows", to: 2, neutral: 1, label: qsTr("暗部"), hint: qsTr("画面暗的部分保留多少 AI 的改动") },
        { id: "midtones", pref: "pushShapeMidtones", to: 2, neutral: 1, label: qsTr("中间调"), hint: qsTr("画面中等亮度的部分保留多少 AI 的改动") },
        { id: "highlights", pref: "pushShapeHighlights", to: 2, neutral: 1, label: qsTr("高光"), hint: qsTr("画面亮的部分保留多少 AI 的改动") },
        { id: "color", layer: "color", to: 2, neutral: 1, label: qsTr("色彩变化"), hint: qsTr("AI 改变色彩浓淡的程度。0% 保持原画的色彩") },
        { id: "hue", pref: "pushShapeHue", to: 2, neutral: 1, label: qsTr("色相偏移"), hint: qsTr("AI 改变色相的程度。0% 保持原画的色相") }
    ]
    function fineSpec(id) { for (const f of quick.fineSpecs) if (f.id === id) return f; return null }
    // Shown for every enhancement runtime: the change is composed the same way for all of them.
    readonly property bool fineShown: quick.nrPossible && veyra.nrAnyEnabled === true && quick.lookLayer !== null
    readonly property bool fineOpen: veyra.preferences.pushFineTuneOpen === true
    function fineValue(id) {
        const f = quick.fineSpec(id)
        if (!f) return 0
        let v = f.neutral
        if (f.layer) { if (quick.lookLayer && quick.lookLayer[f.layer] !== undefined) v = Number(quick.lookLayer[f.layer]) }
        else if (veyra.preferences[f.pref] !== undefined) v = Number(veyra.preferences[f.pref]) / 100
        return isFinite(v) ? Math.max(0, Math.min(f.to, v)) : f.neutral
    }
    function setFine(id, value) {
        const f = quick.fineSpec(id)
        if (!f || !quick.nrPossible || !isFinite(Number(value))) return false
        const v = Math.round(Math.max(0, Math.min(f.to, Number(value))) * 100) / 100
        if (!f.layer) return veyra.setPreference(f.pref, Math.round(v * 100)) === true
        let ok = true
        const layers = veyra.nrLayers.map(l => l.index)
        for (const index of layers) ok = veyra.setNrLayerParameter(index, f.layer, v) && ok
        return ok && layers.length > 0
    }
    readonly property bool fineNeutral: {
        for (const f of quick.fineSpecs) if (Math.abs(quick.fineValue(f.id) - f.neutral) >= 0.005) return false
        return true
    }
    function resetFine() {
        for (const f of quick.fineSpecs) if (Math.abs(quick.fineValue(f.id) - f.neutral) >= 0.005) quick.setFine(f.id, f.neutral)
    }
    function fineSnapshot() {
        const o = {}
        for (const f of quick.fineSpecs) o[f.id] = quick.fineValue(f.id)
        return o
    }
    // Only values that differ are written (see the look, above).
    function applyFine(o) {
        if (!o || typeof o !== "object" || !quick.nrPossible) return
        for (const f of quick.fineSpecs) {
            const v = o[f.id]
            if (typeof v !== "number" || !isFinite(v)) continue
            if (Math.abs(Math.max(0, Math.min(f.to, v)) - quick.fineValue(f.id)) >= 0.005) quick.setFine(f.id, v)
        }
    }
    function fineText(id) {
        const f = quick.fineSpec(id), v = quick.fineValue(id)
        return f && f.neutral === 0 && v < 0.005 ? qsTr("不限制") : Math.round(v * 100) + "%"
    }

    // --- console and game profiles ---------------------------------------------
    // A console profile remembers the quick choices per console: switching console
    // keeps what was set for the old one and brings back the new one's. A game
    // profile is a named snapshot (console, game frame rate, target, enhancement, low
    // latency) plus, when the current Professional settings are a saved preset, that
    // preset's name, which is applied first. Stored as JSON in the pushProfiles
    // preference (validated by the bridge); nothing here adds engine state.
    readonly property var consoles: [
        { id: "ps5", label: "PlayStation 5" }, { id: "ps5pro", label: "PS5 Pro" },
        { id: "xbox", label: "Xbox Series X|S" }, { id: "switch", label: "Nintendo Switch" },
        { id: "switch2", label: "Nintendo Switch 2" },
        { id: "other", label: qsTr("其他主机") }
    ]
    readonly property string consoleId: {
        const c = String(veyra.preferences.pushConsole || "ps5")
        return consoles.some(x => x.id === c) ? c : "ps5"
    }
    readonly property var profiles: {
        try {
            const p = JSON.parse(veyra.preferences.pushProfiles || "{}")
            return p && typeof p === "object" && !Array.isArray(p) ? p : ({})
        } catch (e) { return ({}) }
    }
    readonly property var games: Array.isArray(profiles.games) ? profiles.games.filter(g => g && typeof g.name === "string") : []
    readonly property string activeGame: typeof profiles.game === "string" && games.some(g => g.name === profiles.game) ? profiles.game : ""
    readonly property int maxGames: 50

    function snapshot() {
        const s = { fps: quick.baseFps, target: quick.currentTarget, enhance: veyra.nrAnyEnabled === true,
                 adaptive: veyra.fgAdaptive === true, pacing: veyra.pacingRepair === true, hudLock: veyra.hudLock === true,
                 lowLatency: veyra.nvidiaLowLatencyAvailable === true && (veyra.nvidiaLowLatency ?? 0) > 0 }
        // The enhancement's look, when there is an enhancement layer to read it from.
        if (quick.lookLayer !== null) s.look = { tone: quick.lookTone, structure: quick.lookStructure, fine: quick.fineSnapshot() }
        // Crop and resize (PushCrop.js): the values while it is on, false while it is off.
        const c = quick.cropState
        s.crop = c.on && !Crop.neutral(c) ? { l: c.l, t: c.t, r: c.r, b: c.b, zoom: c.zoom, x: c.x, y: c.y } : false
        return s
    }
    function writeProfiles(p) {
        p.v = 1
        return veyra.setPreference("pushProfiles", JSON.stringify(p))
    }
    // The chosen game profile and the current console's profile follow what is changed while they are
    // chosen. Until 4.5.1 a profile only held what it was saved with: a change made after choosing it
    // played on, and the next time the profile was chosen (after a restart or an update, where people
    // noticed it) the old values came back, which read as "profiles are not saved". A value the quick
    // setup cannot read just now (a target set by hand in Professional mode, no enhancement layer to read
    // the look from) keeps what the profile had. Nothing is written for a few seconds after a profile is
    // applied, so the settings it asked for are not taken back before they have arrived.
    readonly property string liveProfile: JSON.stringify(quick.snapshot())
    property double profileQuietUntil: 0
    property bool keepsProfiles: true   // tests that drive profiles step by step switch it off
    // A change still waiting is kept before another profile is chosen.
    function flushProfiles() { if (quick.keepsProfiles && profileKeep.running && Date.now() >= quick.profileQuietUntil) { profileKeep.stop(); quick.keepProfiles() } }
    function quietProfiles() { quick.profileQuietUntil = Date.now() + 3000; if (quick.keepsProfiles) profileKeep.restart() }
    onLiveProfileChanged: if (quick.keepsProfiles) profileKeep.restart()
    Timer {
        id: profileKeep
        objectName: "pushfps-profile-keep"
        interval: 1500
        onTriggered: {
            const wait = quick.profileQuietUntil - Date.now()
            if (wait > 0) { profileKeep.interval = Math.max(100, wait); profileKeep.restart(); return }
            profileKeep.interval = 1500
            quick.keepProfiles()
        }
    }
    function mergedSnapshot(old) {
        const s = quick.snapshot()
        const out = (old && typeof old === "object") ? JSON.parse(JSON.stringify(old)) : {}
        for (const k in s) {
            if (k === "target" && (!quick.fgPossible || ["off", "60", "90", "120", "monitor"].indexOf(s.target) < 0)) continue
            // What this PC cannot run right now is not read as "off" (applySnapshot skips it the same way).
            if (k === "enhance" && !quick.nrPossible) continue
            if (k === "adaptive" && !quick.adaptivePossible) continue
            if ((k === "pacing" || k === "hudLock") && !quick.ownTechPossible) continue
            if (k === "lowLatency" && veyra.nvidiaLowLatencyAvailable !== true) continue
            out[k] = s[k]
        }
        return out
    }
    // Returns whether anything was written.
    function keepProfiles() {
        const p = copyProfiles()
        const before = JSON.stringify(p)
        if (!p.consoles || typeof p.consoles !== "object" || Array.isArray(p.consoles)) p.consoles = {}
        p.consoles[quick.consoleId] = quick.mergedSnapshot(p.consoles[quick.consoleId])
        if (quick.activeGame.length > 0 && Array.isArray(p.games)) {
            const i = p.games.findIndex(g => g && g.name === quick.activeGame)
            if (i >= 0 && p.games[i].console === quick.consoleId) {
                const g = quick.mergedSnapshot(p.games[i])
                g.name = p.games[i].name; g.console = p.games[i].console; g.preset = p.games[i].preset || ""
                p.games[i] = g
            }
        }
        p.v = 1
        if (JSON.stringify(p) === before) return false
        return writeProfiles(p)
    }
    function copyProfiles() { return JSON.parse(JSON.stringify(quick.profiles)) }
    function applySnapshot(s) {
        if (!s) return
        const fps = s.fps === 40 || s.fps === 60 ? s.fps : 30
        veyra.setPreference("pushGameFps", fps)
        if (typeof s.adaptive === "boolean" && quick.adaptivePossible) {
            veyra.fgAdaptive = s.adaptive
            if (s.adaptive && Number(veyra.contentRate) === 0) veyra.contentRate = 1
        }
        if (quick.ownTechPossible && typeof s.pacing === "boolean") quick.setPacingRepair(s.pacing)
        if (quick.ownTechPossible && typeof s.hudLock === "boolean") veyra.hudLock = s.hudLock
        if (quick.fgPossible && ["off", "60", "90", "120", "monitor"].indexOf(s.target) >= 0)
            quick.applyTarget(s.target, fps)
        if (quick.nrPossible && typeof s.enhance === "boolean" && s.enhance !== (veyra.nrAnyEnabled === true))
            quick.setEnhancement(s.enhance)
        // Profiles saved before 3.2.2 have no look: the current one stays. So does a look that already
        // matches: layers given different values in Professional mode are only made equal when a
        // profile really asks for another look.
        if (quick.nrPossible && s.look && typeof s.look === "object" && isFinite(Number(s.look.tone)) && isFinite(Number(s.look.structure))) {
            const tone = quick.lookValue(s.look.tone), structure = quick.lookValue(s.look.structure)
            if (Math.abs(tone - quick.lookTone) >= 0.005 || Math.abs(structure - quick.lookStructure) >= 0.005)
                quick.setLook(tone, structure)
        }
        // The fine-tuning kept with the look (profiles saved before 3.2.2 have none: it stays as it is).
        if (s.look && typeof s.look === "object") quick.applyFine(s.look.fine)
        if (veyra.nvidiaLowLatencyAvailable === true && typeof s.lowLatency === "boolean")
            veyra.nvidiaLowLatency = s.lowLatency ? 1 : 0
        // Crop and resize: a profile saved with one sets it, one saved without switches it off (the
        // values stay), and a profile from before 3.2.6, which knows nothing of it, leaves it alone.
        if (s.crop === false) { if (quick.cropState.on) veyra.setPreference("pushCropOn", false) }
        else if (s.crop && typeof s.crop === "object") quick.setCrop(Crop.copy(Crop.read({}), s.crop))
    }
    // Crop and resize of the console picture (PushCropDialog.qml; preferences and maths in PushCrop.js).
    readonly property var cropState: Crop.read(veyra.preferences)
    function setCrop(c) {
        const wanted = Crop.values(c), saved = Crop.values(quick.cropState)
        for (const k of Crop.keys) if (wanted[k] !== saved[k]) veyra.setPreference(k, wanted[k])
        const on = !Crop.neutral(c)
        if (quick.cropState.on !== on) veyra.setPreference("pushCropOn", on)
    }
    readonly property string cropHint: {
        const c = quick.cropState
        if (Crop.neutral(c)) return qsTr("显示完整画面")
        if (!c.on) return qsTr("已关闭，设置保留")
        if (veyra.presentationOwned === true) return qsTr("XeSS 补帧自己呈现画面，裁剪和缩放对它不生效")
        return qsTr("左 %1% · 上 %2% · 右 %3% · 下 %4% · 大小 %5%").arg(c.l / 10).arg(c.t / 10).arg(c.r / 10).arg(c.b / 10).arg(c.zoom)
    }
    function switchConsole(id) {
        if (id === quick.consoleId) return
        quick.flushProfiles()
        quick.quietProfiles()
        const p = copyProfiles()
        if (!p.consoles || typeof p.consoles !== "object") p.consoles = {}
        p.consoles[quick.consoleId] = snapshot()
        p.game = ""
        if (!writeProfiles(p)) return
        if (!veyra.setPreference("pushConsole", id)) return
        applySnapshot(p.consoles[id])
    }
    function saveGame(name) {
        name = String(name || "").trim().slice(0, 40)
        if (name.length === 0) return false
        const p = copyProfiles()
        const list = Array.isArray(p.games) ? p.games.filter(g => g && g.name !== name) : []
        if (list.length >= quick.maxGames) return false
        const presetName = veyra.currentPresetName || ""
        const g = snapshot()
        g.name = name
        g.console = quick.consoleId
        g.preset = (veyra.presets || []).some(x => x.name === presetName) ? presetName : ""
        list.push(g)
        list.sort((a, b) => a.name.localeCompare(b.name))
        p.games = list
        p.game = name
        return writeProfiles(p)
    }
    function applyGame(name) {
        quick.flushProfiles()
        quick.quietProfiles()
        const g = quick.games.find(x => x.name === name)
        const p = copyProfiles()
        p.game = g ? name : ""
        if (!g) { writeProfiles(p); return }
        if (g.preset) {
            const preset = (veyra.presets || []).find(x => x.name === g.preset)
            if (preset) veyra.applyPresetIndex(preset.index)
        }
        if (quick.consoles.some(c => c.id === g.console)) veyra.setPreference("pushConsole", g.console)
        applySnapshot(g)
        writeProfiles(p)
    }
    function deleteGame(name) {
        const p = copyProfiles()
        p.games = (Array.isArray(p.games) ? p.games : []).filter(g => g && g.name !== name)
        if (p.game === name) p.game = ""
        writeProfiles(p)
    }
    // A capture card that captures 120 Hz (the 4K X; the 4K S at 1080p/1440p) gets each
    // frame in 8 ms instead of 17, and lets 40 FPS (120 Hz mode) games through; Switch 2 docked
    // output tops out at 60 Hz at 4K, so the extra line is for PlayStation and Xbox. A card that only
    // passes 120 Hz through (HD60 X: 60 Hz capture) stays at 60 Hz.
    readonly property string hz120Tip: qsTr("采集卡能以 120 Hz 采集时（如 4K X，或 1080p/1440p 下的 4K S），也可输出 120 Hz：每帧早约 8 毫秒到达，40 FPS 的 120 Hz 模式也能使用。只支持 120 Hz 直通的采集卡请保持 60 Hz。")
    readonly property string setupTip: quick.consoleTip + (quick.consoleId === "ps5" || quick.consoleId === "xbox" ? " " + quick.hz120Tip : "")
    readonly property string consoleTip: {
        switch (quick.consoleId) {
        case "xbox": return qsTr("Xbox 请在 设置 → 常规 → 电视和显示选项 中选择 60 Hz。游戏没有 HDCP，可以直接采集。")
        case "switch2": return qsTr("Switch 2 请放在底座上，输出 60 Hz。游戏没有 HDCP，可以直接采集。")
        case "switch": return qsTr("Switch 请放在底座上，输出 1080p 60 Hz。游戏没有 HDCP，可以直接采集。")
        case "other": return qsTr("主机请输出 60 Hz，并关闭 HDCP（如有此选项），否则采集画面可能为黑屏。")
        default: return qsTr("主机请输出 60 Hz。PlayStation 需在 设置 → 系统 → HDMI 中关闭 HDCP，否则采集画面为黑屏。")
        }
    }

    readonly property string targetHint: {
        if (!quick.fgPossible) return Support.status(veyra, "frame-generation").reason || ""
        if (quick.currentTarget === "") return qsTr("已在专业模式中手动设置")
        if (quick.currentTarget === "off") return qsTr("主机游戏保持原有帧率")
        if (quick.adaptive) return qsTr("自适应：按游戏的实时帧率在 2× 到 %1× 之间选择，尽量接近 %2 FPS")
            .arg(veyra.fgMultiplier).arg(Math.round(quick.targetFps(quick.currentTarget)))
        const m = veyra.fgMultiplier
        const reach = quick.baseFps * m
        const want = quick.targetFps(quick.currentTarget)
        // Short of the target because this frame generation offers no more; when it does offer more,
        // the next step would overshoot (40 FPS x 3 on a 90 FPS target), which is not its limit.
        if (reach + 0.5 < want && !quick.offeredMultipliers.some(x => x > m))
            return qsTr("当前补帧方式最高 %1×，%2 FPS 游戏约为 %3 FPS").arg(m).arg(quick.baseFps).arg(reach)
        return qsTr("%1 FPS 游戏将以 %2 FPS 运行（%3× 补帧）").arg(quick.baseFps)
            .arg(Math.round(Math.min(reach, want))).arg(m)
    }

    // Header: what this card is, and the one-line explanation in the full form.
    ColumnLayout {
        visible: quick.scopeMain && quick.showHeader
        Layout.fillWidth: true
        Layout.topMargin: 12
        Layout.bottomMargin: 2
        spacing: 3
        Text {
            text: qsTr("快速设置")
            color: Theme.t1
            font.family: Theme.fontUi
            font.pixelSize: Theme.fsH3
            font.weight: Font.DemiBold; font.variableAxes: Theme.axesDemiBold
        }
        Text {
            visible: !quick.compact
            Layout.fillWidth: true
            text: qsTr("为主机游戏选好帧率和画面，其他细节在专业模式里")
            color: Theme.t3
            font.family: Theme.fontUi
            font.pixelSize: Theme.fsHint
            wrapMode: Text.WordWrap
        }
    }

    // First-run tip, as a quiet info strip rather than a paragraph above the rows.
    Rectangle {
        objectName: "pushfps-setup-tip"
        visible: quick.scopeMain && quick.showSetupTip && !veyra.hasCaptureSession
        Layout.fillWidth: true
        Layout.topMargin: 8
        Layout.bottomMargin: 2
        implicitHeight: tipRow.implicitHeight + 16
        radius: 9
        color: Theme.ink(0.035)
        border.width: 1
        border.color: Theme.stroke
        RowLayout {
            id: tipRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 9
            VIcon { Layout.alignment: Qt.AlignTop; name: "info"; size: 14; color: Theme.accent }
            Text {
                Layout.fillWidth: true
                text: quick.setupTip
                color: Theme.t2
                font.family: Theme.fontUi
                font.pixelSize: Theme.fsHint
                lineHeight: 1.1
                wrapMode: Text.WordWrap
            }
        }
    }

    // PushFPS tools: measure this PC, start from a game's preset, protect the HUD,
    // record a before/after clip. They wrap onto a second line in narrow panels.
    readonly property bool compareRecording: veyra.recording && veyra.recordingState.compare === true
    function openOnPicture(dialog) {
        // The picture has to be on screen while it is sampled (a hidden window presents differently).
        if (veyra.hasSource && !quick.compactPro) veyra.navigate("pro")
        veyra.navigate(dialog)
    }
    // (The tools' place in this card. They are drawn in it unless the page gave them another: an item
    // moved out of a layout and back would come back at its end, so it is this place that stays.)
    Item {
        id: toolsSlot
        visible: quick.scopeMain && quick.toolsHost === null
        Layout.fillWidth: true
        Layout.topMargin: 8
        Layout.bottomMargin: 6
        implicitHeight: toolsGrid.implicitHeight
    }
    GridLayout {
        id: toolsGrid
        objectName: "pushfps-tools-row"
        parent: quick.toolsHost !== null ? quick.toolsHost : toolsSlot
        width: parent ? parent.width : 0
        visible: quick.scopeMain
        // Two equal columns; one in a very narrow panel. (Under the new home page's picture: three, or two.)
        columns: quick.toolsHost !== null ? (width >= 600 ? 3 : 2) : width >= 360 ? 2 : 1
        columnSpacing: 8
        rowSpacing: 8
        PushToolTile {
            objectName: "pushfps-optimize"
            // Grey until Optimise for my PC has chosen and applied settings (green would read as
            // "already on"); green while that result is kept, grey again after Undo.
            primary: quick.optimized
            iconName: "zap"
            text: qsTr("为我的电脑优化")
            sub: qsTr("测试每种组合，选出最好的")
            tip: qsTr("在实际画面上测试每种组合，自动选出最好的一种")
            onClicked: quick.openOnPicture("optimize")
        }
        // Check my setup (PushDiagnoseDialog.qml): what in the running picture costs quality, with the change for each.
        PushToolTile {
            objectName: "pushfps-diagnose"
            iconName: "activity"
            text: qsTr("检查我的设置")
            sub: qsTr("找出影响画质的设置")
            tip: qsTr("看看正在运行的画面里，是什么在影响清晰度、流畅度和声音，每一项都有一键更改")
            onClicked: veyra.navigate("diagnose")
        }
        PushToolTile {
            objectName: "pushfps-game-presets"
            iconName: "gamepad"
            text: qsTr("游戏预设")
            sub: qsTr("GTA VI 等，HUD 保持清晰")
            tip: qsTr("GTA VI 等游戏的推荐设置，小地图和 HUD 保持清晰")
            onClicked: veyra.navigate("games")
        }
        PushToolTile {
            objectName: "pushfps-hud"
            iconName: "scan"
            text: qsTr("HUD 保护")
            sub: qsTr("自动识别，可手动修正")
            tip: qsTr("自动找出小地图和 HUD，让它们不补帧；方框可以手动修正")
            onClicked: quick.openOnPicture("hud")
        }
        PushToolTile {
            objectName: "pushfps-compare-clip"
            visible: !quick.streamingPage   // streaming mode has its own button for it
            iconName: "split"
            readonly property int clipSeconds: CompareClip.seconds(veyra.preferences)
            readonly property bool clipUntilStop: CompareClip.untilStop(veyra.preferences)
            readonly property string clipLayout: {
                const n = CompareClip.layout(veyra.preferences)
                return n === 3 ? qsTr("滑动揭示") : n === 2 ? qsTr("竖屏") : n === 1 ? qsTr("并排") : qsTr("分屏")
            }
            text: quick.compareRecording ? qsTr("停止对比片段") : qsTr("对比片段")
            // While it records: how long it has run (a clip without a length has no other clock).
            sub: quick.compareRecording ? qsTr("正在录制…") + " " + CompareClip.elapsed((veyra.recordingState || {}).seconds)
                 : veyra.recording ? qsTr("录制中不可用")
                 : (clipUntilStop ? qsTr("手动停止") : qsTr("%1 秒").arg(clipSeconds)) + " · " + clipLayout
            tip: qsTr("录制前后对比视频：一边是主机原本的帧率，一边是 PushFPS。可选分屏、并排、竖屏（TikTok、Shorts、Reels）或滑动揭示，以及时长和画质")
            // Opens with nothing playing too, so the layout and length can be set beforehand;
            // the dialog says a picture is needed and only its start button records.
            enabled: quick.compareRecording || !veyra.recording
            onClicked: {
                if (quick.compareRecording) veyra.stopRecording("compare-button")
                else veyra.navigate("compare")
            }
        }
        PushToolTile {
            objectName: "pushfps-stream"
            visible: !quick.streamingPage   // streaming mode has its own Go live button
            // Six tiles since 4.1 (Check my setup): two even columns, so this one no longer takes a whole row.
            Layout.fillWidth: true
            iconName: "cast"
            readonly property var streamState: quick.liveStream
            text: streamState.active ? qsTr("直播中") : qsTr("直播")
            sub: streamState.active ? (streamState.live ? qsTr("%1 个平台").arg((streamState.outputs || []).filter(o => o.state === "live").length) : qsTr("正在连接平台…"))
                             : qsTr("Twitch、YouTube、Kick、TikTok，可同时推")
            tip: qsTr("把 PushFPS 的画面直接推到直播平台，可以同时推多个平台")
            onClicked: veyra.navigate("stream")
        }
    }

    // The game is set to 30 FPS but every 60 Hz capture frame looks new to the automatic duplicate
    // removal (a capture card that adds noise to repeated pictures, field report 2026-11-17: GTA V
    // fidelity mode measured as 60). Then frame generation works from 60 and the console side of a
    // comparison clip reads 60. One tap switches to the fixed 60 -> 30 mode (every second sample).
    readonly property bool cadenceMismatch: quick.liveStats.running === true && veyra.fgEnabled === true && quick.baseFps === 30
        && Math.abs(Number(quick.liveStats.captureFps || 0) - 60) < 3 && Number(quick.liveStats.contentFps || 0) > 45
        && Number(veyra.contentRate) !== 5 && Number(veyra.contentRate) !== 6
    // Frame generation health (engine/PushFgHealth.h): frame generation is on and nothing generated
    // is shown, so the picture plays at the game's own frame rate. Shown with its cause for as long
    // as it lasts: 2 the method is not running, 3 the card does not keep up, 4 nothing comes out.
    // (1, a target equal to the game's rate, is a setting and not shown here.)
    // Polled on its own, for every method, whenever frame generation is on (the timer above runs
    // only for the features that need the other numbers).
    property int fgIdle: 0
    property string fgIdleText: ""
    function readFgHealth() {
        const st = veyra.pushLiveStats() || ({})
        quick.fgIdle = st.running === true && veyra.fgEnabled === true ? Number(st.fgIdle || 0) : 0
        quick.fgIdleText = quick.fgIdle >= 2 ? String(st.fgIdleText || "") : ""
    }
    Timer {
        objectName: "pushfps-fg-idle-timer"
        interval: 1000; repeat: true
        running: veyra.hasSource && veyra.fgEnabled && quick.visible && quick.scopeMain
        triggeredOnStart: true
        onTriggered: quick.readFgHealth()
        onRunningChanged: if (!running) { quick.fgIdle = 0; quick.fgIdleText = "" }
    }
    // The frame generation method (PushFlow, DLSS, FSR, ...) and PushFlow's tier. Streaming mode has no
    // Professional page beside it, so they were chosen here there; since 4.1 the home page has them
    // too, wherever the card offers more than one method: an AMD card starts on FSR, and until now the
    // only ways from there to PushFlow or AMD FRC were Professional mode and the optimiser.
    readonly property int methodsOffered: {
        let n = 0
        const list = veyra.fgBackendChoices || []
        for (let i = 0; i < list.length; ++i) if (list[i] && list[i].disabled !== true) ++n
        return n
    }
    readonly property bool methodShown: quick.compactPro || (quick.fgPossible && quick.methodsOffered > 1)
    readonly property bool pushFlowOffered: (veyra.fgBackendChoices || []).some(o => o && o.id === "pushflow" && o.disabled !== true)
    // An AMD card on FSR or AMD FRC: ghosting is what people report there, and PushFlow is the other
    // method such a card has. Said as a thing to try, never switched for them (nobody has measured
    // the two against each other on an AMD card).
    readonly property bool amdMethodTip: veyra.amdNrGpu === true && quick.pushFlowOffered
                                         && (veyra.fgBackendName === "fsr3" || veyra.fgBackendName === "fsr4" || veyra.fgBackendName === "amdfrc")
    // --- a screen wider than the picture (ultrawide) -----------------------------
    // A console sends a 16:9 picture whatever screen it is shown on, so a 21:9 or 32:9 screen has room
    // left over at the sides. What to do with it is Veyra's aspect choice (Professional mode, Display):
    // Fit (bars), Fill and crop, Stretch. PushFPS adds ambient fill (engine/PushAmbient.h, preference
    // pushAmbientFill): Fit with a soft, dimmed copy of the picture in the bars. The row is shown on a
    // screen clearly wider than 16:9, and wherever one of these is already on, so it can be undone;
    // on the home page too (it is rare enough not to crowd it), but not beside streaming mode's preview.
    property real screenAspectOverride: 0   // tests
    readonly property real screenAspect: quick.screenAspectOverride > 0 ? quick.screenAspectOverride
                                         : (Screen.height > 0 ? Screen.width / Screen.height : 0)
    readonly property bool wideScreen: quick.screenAspect > 1.9
    readonly property bool ambientOn: veyra.preferences.pushAmbientFill === true
    // XeSS frame generation presents through its own path, which has no ambient fill.
    readonly property bool ambientPossible: veyra.presentationOwned !== true
    readonly property int aspectNow: Number(veyra.aspectMode || 0)
    readonly property string fillId: quick.aspectNow === 0 ? (quick.ambientOn && quick.ambientPossible ? "ambient" : "fit")
                                     : quick.aspectNow === 2 ? "crop" : quick.aspectNow === 3 ? "stretch" : ""
    readonly property bool fillShown: quick.wideScreen || quick.aspectNow !== 0 || quick.ambientOn
    readonly property var fillOptions: {
        const list = [{ id: "fit", label: qsTr("黑边") }]
        if (quick.ambientPossible) list.push({ id: "ambient", label: qsTr("氛围填充") })
        list.push({ id: "crop", label: qsTr("裁切") })
        list.push({ id: "stretch", label: qsTr("拉伸") })
        return list
    }
    function setFill(id) {
        if (id !== "fit" && id !== "ambient" && id !== "crop" && id !== "stretch") return
        if (id === "ambient" && !quick.ambientPossible) return
        const mode = id === "crop" ? 2 : id === "stretch" ? 3 : 0
        if (quick.aspectNow !== mode) veyra.aspectMode = mode
        if (quick.ambientOn !== (id === "ambient")) veyra.setPreference("pushAmbientFill", id === "ambient")
    }
    // How much of a 16:9 picture is lost (crop: of its height) or how much wider it becomes (stretch).
    readonly property int fillCropPercent: quick.screenAspect > 16 / 9 ? Math.round((1 - (16 / 9) / quick.screenAspect) * 100) : 0
    readonly property int fillStretchPercent: quick.screenAspect > 16 / 9 ? Math.round((quick.screenAspect / (16 / 9) - 1) * 100) : 0
    readonly property string fillHint: {
        if (quick.fillId === "fit") return qsTr("主机输出的是 16:9 画面：完整显示，两侧留黑边")
        if (quick.fillId === "ambient") return qsTr("完整显示，不裁切也不变形；黑边换成画面本身柔和、变暗的延伸")
        if (quick.fillId === "crop") return quick.fillCropPercent > 0
            ? qsTr("铺满屏幕，但上下共裁掉约 %1% 的画面，顶部和底部的 HUD 可能看不到").arg(quick.fillCropPercent)
            : qsTr("铺满窗口，超出的部分被裁掉")
        if (quick.fillId === "stretch") return quick.fillStretchPercent > 0
            ? qsTr("铺满屏幕，但画面被横向拉宽约 %1%").arg(quick.fillStretchPercent)
            : qsTr("铺满窗口，画面比例会改变")
        return qsTr("已在专业模式中手动设置")
    }
    // G-SYNC set-up. The screen can do variable refresh and PushFPS asks for it, but the NVIDIA driver
    // has not confirmed it for this program: usually the driver only allows G-SYNC in full screen, or
    // not for PushFPS at all, and frames then stay on screen for uneven times (a 60 FPS output on a
    // 144 Hz screen: 2-3-2-3 refreshes). The button runs the driver-profile check and, when the profile
    // can be set for PushFPS alone, the repair (NvidiaVrrProfileController; a restart is needed after).
    // Field report 2026-11-18: an RTX 5090 user could not get G-SYNC until he found "Repair" under
    // Professional -> Display.
    readonly property var gsyncTools: ("nvidiaVrrProfile" in veyra) ? veyra.nvidiaVrrProfile : null
    property bool gsyncWanted: false     // the button was pressed; the check is running
    property bool gsyncChecked: false    // a check has finished since the button was pressed
    readonly property bool gsyncUnconfirmed: quick.gsyncTools !== null && veyra.hasSource === true && quick.smoothPacing
                                             && Number(veyra.vrrState || 0) === 3 && veyra.vrrAmd !== true
    Connections {
        target: quick.gsyncTools
        ignoreUnknownSignals: true
        function onChanged() {
            if (quick.gsyncTools.busy) return
            if (quick.gsyncWanted) {
                quick.gsyncWanted = false
                quick.gsyncChecked = true
                if (quick.gsyncTools.canRepair) { quick.gsyncTools.repair(); return }
            }
            // Checked and nothing (left) to set: remembered, so the row does not come back at every
            // start on screens whose driver never reports variable refresh as active (G-SYNC Compatible).
            if (quick.gsyncChecked && !quick.gsyncTools.canRepair) veyra.setPreference("pushGsyncChecked", true)
        }
    }
    // Until 4.1 this row was NVIDIA's only: on an AMD or Intel card it stood there greyed out saying
    // "needs an NVIDIA card", although the engine's own low queue (one frame waiting for the screen
    // instead of three) works on every card and was only reachable through "Lowest latency" above or
    // Professional mode. Now the row is that queue there. On NVIDIA nothing changes.
    readonly property bool nvidiaLatency: veyra.nvidiaLowLatencyAvailable === true

    // --- the rows, in sections --------------------------------------------------------
    // Each section is a title and its rows (PushSection.qml). A section whose rows are all hidden takes no
    // room, and folding one only changes its height: a row's own `visible` never depends on the fold.
    PushSection {
        id: secSetup
        sectionId: "setup"
        visible: quick.scopeMain
        title: qsTr("主机与游戏")
        summary: quick.sectionSummary("setup")
        open: quick.sectionOpen("setup")
        onToggled: open => quick.setSectionOpen("setup", open)
        // The capture card in use, and what is known of it. A card with nothing known gets no "not
        // tested" mark (the owner's rule, 21 November 2026: say it where there is something to say):
        // the tag is only for a card that was tested or reported working.
        VRow {
            id: cardRow
            objectName: "pushfps-card-row"
            readonly property var card: Cards.find(veyra.captureDeviceLabel)
            // Without a card chosen the label is the bridge's "no device selected", and for a chosen card that is
            // unplugged "device not connected": neither is a card to call tested or not, or to report.
            readonly property bool noCard: veyra.captureDeviceLabel === qsTr("未选择设备")
            readonly property bool named: !noCard && veyra.captureDeviceLabel !== qsTr("设备未连接")
            visible: String(veyra.captureDeviceLabel || "").length > 0 && !noCard
            label: qsTr("采集卡")
            hint: !named ? ""
                : card && card.status === "tested" ? qsTr("PushFPS 已实测")
                : card && card.status === "veyra" ? qsTr("Veyra 作者已实测")
                : card && card.status === "reported" ? qsTr("有 PushFPS 用户报告可用")
                : qsTr("欢迎在 GitHub 上分享这张卡的使用结果")
            hintBelow: true
            RowLayout {
                spacing: 6
                Text {
                    text: veyra.captureDeviceLabel || ""
                    color: Theme.t1; font.family: Theme.fontUi; font.pixelSize: 12
                    elide: Text.ElideRight; Layout.maximumWidth: 180
                }
                VTag {
                    objectName: "pushfps-card-status"
                    readonly property bool known: cardRow.card !== null && cardRow.card.status !== "untested"
                    visible: cardRow.named && known
                    kind: "ok"
                    text: qsTr("已测试")
                }
                VButton {
                    visible: cardRow.named && !(cardRow.card && cardRow.card.status === "tested")
                    ghost: true
                    text: qsTr("报告")
                    onClicked: Qt.openUrlExternally(Cards.reportUrl)
                }
            }
        }
        VRow {
            objectName: "pushfps-console-row"
            label: qsTr("游戏主机")
            VSelect {
                objectName: "pushfps-console"
                implicitWidth: 190
                options: quick.consoles
                value: (quick.consoles.find(c => c.id === quick.consoleId) || quick.consoles[0]).label
                onPicked: id => quick.switchConsole(id)
            }
        }
        VRow {
            id: gameRow
            objectName: "pushfps-game-row"
            label: qsTr("游戏配置")
            hint: quick.activeGame.length > 0 ? qsTr("此配置会记住你接下来做的更改") : qsTr("把当前设置存为某个游戏的配置，之后一键切换")
            property bool naming: false
            RowLayout {
                spacing: 4
                VSelect {
                    objectName: "pushfps-game"
                    visible: !gameRow.naming
                    implicitWidth: 150
                    options: [{ id: "", label: qsTr("无") }].concat(quick.games.map(g => ({ id: g.name, label: g.name })))
                    value: quick.activeGame.length > 0 ? quick.activeGame : qsTr("无")
                    onPicked: id => quick.applyGame(id)
                }
                DialogTextField {
                    id: gameName
                    objectName: "pushfps-game-name"
                    visible: gameRow.naming
                    implicitWidth: 150
                    placeholder: qsTr("游戏名称")
                }
                VButton {
                    objectName: "pushfps-game-save"
                    icon: true
                    iconName: gameRow.naming ? "check" : "plus"
                    tip: gameRow.naming ? qsTr("保存") : (quick.activeGame.length > 0 ? qsTr("更新或另存为游戏配置") : qsTr("存为游戏配置"))
                    enabled: gameRow.naming ? gameName.text.trim().length > 0 : quick.games.length < quick.maxGames || quick.activeGame.length > 0
                    onClicked: {
                        if (gameRow.naming) { if (quick.saveGame(gameName.text)) gameRow.naming = false; return }
                        gameName.text = quick.activeGame
                        gameRow.naming = true
                    }
                }
                VButton {
                    objectName: "pushfps-game-delete"
                    icon: true
                    iconName: gameRow.naming ? "x" : "trash"
                    tip: gameRow.naming ? qsTr("取消") : qsTr("删除此游戏配置")
                    visible: gameRow.naming || quick.activeGame.length > 0
                    onClicked: {
                        if (gameRow.naming) { gameRow.naming = false; return }
                        quick.deleteGame(quick.activeGame)
                    }
                }
            }
        }
        VRow {
            objectName: "pushfps-game-fps-row"
            label: qsTr("游戏帧率")
            help: qsTr("游戏自己每秒画出多少张新画面。主机输出固定的 60 Hz 或 120 Hz 信号，30 FPS 的游戏在 60 Hz 下每张画面重复一次。PushFPS 去掉重复的画面，再根据游戏帧率决定要生成几帧")
            hint: qsTr("大多数主机游戏为 30 FPS；120 Hz 模式的游戏常为 40；性能模式选 60")
            VSeg {
                objectName: "pushfps-game-fps"
                enabled: quick.fgPossible
                opacity: enabled ? 1 : 0.4
                options: [{ id: "30", label: "30" }, { id: "40", label: "40" }, { id: "60", label: "60" }]
                current: String(quick.baseFps)
                onPicked: id => quick.setGameFps(id)
            }
        }
        VRow {
            objectName: "pushfps-cadence-row"
            visible: quick.cadenceMismatch
            label: qsTr("检测到 60 FPS")
            hint: qsTr("游戏帧率设为 30，但每一帧采集画面都被当作新画面（%1 FPS）。采集卡可能给重复画面加了噪点。改用固定的「60→30」去重，按 30 FPS 补帧").arg(Math.round(Number(quick.liveStats.contentFps || 0)))
            hintColor: Theme.warn
            hintBelow: true
            VButton {
                objectName: "pushfps-cadence-fix"
                text: qsTr("改为 60→30")
                onClicked: { veyra.contentRate = 5; veyra.logUi("push-quick", "content rate 60->30 from the cadence hint") }
            }
        }
        // PushFPS: the keyboard and mouse as a controller (PushKbmDialog.qml, bridge/PushKbm.cpp). The switch
        // and the way to the layout; the console only ever sees a controller.
        VRow {
            id: kbmRow
            objectName: "pushfps-kbm-row"
            readonly property var cfg: Kbm.config(String(veyra.preferences.pushKbm || ""), veyra.pushKbmDefaults())
            readonly property var kbmState: veyra.pushKbm || ({})
            readonly property string releaseName: veyra.pushKbmInputName(kbmRow.cfg.release)
            label: qsTr("鼠标和键盘")
            help: qsTr("用这台电脑的鼠标和键盘玩主机游戏：主机看到的是一个手柄，手柄也可以同时用（例如用手柄开车，下车后用鼠标瞄准）。用于 PS5 远程游玩、开着“用远程游玩控制 PS5”的采集卡，以及 Xbox 串流。")
            hint: {
                switch (Kbm.status(kbmRow.kbmState, kbmRow.cfg.on)) {
                case "off": return qsTr("已关闭")
                case "using": return qsTr("正在使用：主机收到的是鼠标和键盘 · 按 %1 交还").arg(kbmRow.releaseName)
                case "ready": return qsTr("可以开始：点击画面或按 %1").arg(kbmRow.releaseName)
                }
                // Waiting: what is missing (a capture card alone cannot press anything on the console).
                return String(kbmRow.kbmState.waitingText || "") || qsTr("用于 PS5 远程游玩、开着“用远程游玩控制 PS5”的采集卡，以及 Xbox 串流")
            }
            hintBelow: true
            hintColor: Kbm.status(kbmRow.kbmState, kbmRow.cfg.on) === "waiting" && String(kbmRow.kbmState.waiting || "") !== "" ? Theme.warn : Theme.t3
            RowLayout {
                spacing: 8
                VButton {
                    objectName: "pushfps-kbm-setup"
                    text: qsTr("按键设置")
                    onClicked: veyra.navigate("kbm")
                }
                VSwitch {
                    objectName: "pushfps-kbm"
                    checked: kbmRow.cfg.on
                    onToggled: v => veyra.setPreference("pushKbm", Kbm.text(Kbm.changed(kbmRow.cfg, "on", v)))
                }
            }
        }
        // The one step that lets them reach the console, under the row that says what is missing.
        PushKbmFixRow {
            objectName: "pushfps-kbm-fix-row"
            switchedOn: kbmRow.cfg.on
            kbmState: kbmRow.kbmState
        }
    }
    PushSection {
        id: secFg
        sectionId: "fg"
        visible: quick.scopeMain
        title: qsTr("补帧", "section")
        summary: quick.sectionSummary("fg")
        open: quick.sectionOpen("fg")
        onToggled: open => quick.setSectionOpen("fg", open)
        VRow {
            id: targetRow
            objectName: "pushfps-target-row"
            label: qsTr("目标帧率")
            help: qsTr("补帧后每秒显示的画面数。生成的帧插在两张真实画面之间，让运动更顺滑；手柄的反应速度不会因此变快")
            hint: quick.targetHint
            hintBelow: true
            hintColor: quick.fgPossible ? Theme.t3 : Theme.warn
            VSeg {
                objectName: "pushfps-target"
                enabled: quick.fgPossible
                opacity: enabled ? 1 : 0.4
                options: quick.targetOptions
                current: quick.currentTarget
                onPicked: id => quick.applyTarget(id)
            }
        }
        VRow {
            objectName: "pushfps-fg-idle-row"
            visible: quick.fgIdle >= 2
            label: qsTr("补帧没有出画面")
            hint: quick.fgIdleText
            hintColor: Theme.warn
            hintBelow: true
            // What the session really does, to send along (bridge/PushTools.cpp).
            VButton {
                objectName: "pushfps-fg-idle-report"
                text: qsTr("复制性能报告")
                ghost: true
                onClicked: { veyra.pushCopyPerformanceReport(); veyra.logUi("push-quick", "performance report copied from the frame generation health row") }
            }
        }
        VRow {
            objectName: "pushfps-fg-backend-row"
            visible: quick.methodShown && !quick.proPage   // Professional mode's own list has the method
            label: qsTr("补帧方式")
            hint: quick.amdMethodTip ? qsTr("看到重影或拖影？试试 PushFlow：PushFPS 自己的补帧，任何显卡都能用。「为我的电脑优化」会在你的显卡上实测每一种方式")
                                     : veyra.fgProviderText
            hintBelow: true
            VSelect {
                objectName: "pushfps-fg-backend"
                options: veyra.fgBackendChoices
                value: (options.find(o => o.id === veyra.fgBackendName) || options[0]).label
                onPicked: id => veyra.fgBackendName = id
            }
        }
        VRow {
            objectName: "pushfps-pushflow-tier-row"
            visible: quick.methodShown && !quick.proPage && veyra.fgBackendName === "pushflow"
            label: qsTr("PushFlow 档位")
            // While it runs: the precision this graphics card passed with and the network's time per frame.
            hint: quick.liveStats.pushFlowP50Ms > 0
                  ? qsTr("正在运行：%1 · 每帧 %2 ms（95%：%3 ms）").arg(quick.liveStats.pushFlowPrecision === "fp16" ? qsTr("半精度") : qsTr("全精度"))
                        .arg(Number(quick.liveStats.pushFlowP50Ms).toFixed(1)).arg(Number(quick.liveStats.pushFlowP95Ms).toFixed(1))
                  : ""
            hintBelow: true
            VSeg {
                objectName: "pushfps-pushflow-tier"
                options: [{ id: "0", label: qsTr("快速") }, { id: "1", label: qsTr("均衡") }, { id: "2", label: qsTr("高质量") }]
                current: String(Math.min(veyra.vfgQuality, 2))
                onPicked: id => veyra.vfgQuality = Number(id)
            }
        }
        // NVIDIA VFG's own quality (4.1). Until now it was only in Professional mode's list, so a card with
        // room to spare ran the standard model without anyone having chosen that. The three plain tiers
        // are offered here; the mixed ones stay in Professional mode and show as none chosen.
        VRow {
            objectName: "pushfps-vfg-quality-row"
            visible: quick.fgPossible && !quick.proPage && veyra.fgBackendName === "vfg"
            label: qsTr("VFG 质量")
            help: qsTr("NVIDIA 帧生成有三个模型。「高」在物体边缘、头发和细小物体上最稳，每帧也最吃显卡；显卡跟不上时目标帧率会掉，换回「中」即可。")
            readonly property int tier: Number(veyra.vfgQuality || 0)
            hint: tier > 2 ? qsTr("已在专业模式里设为混合档；在这里选一项就会改用它")
                : tier === 2 ? qsTr("高：物体边缘最稳，最吃显卡")
                : tier === 0 ? qsTr("低：最省显卡，物体边缘更容易破碎")
                : qsTr("中：标准。物体边缘破碎时可以试试「高」")
            hintBelow: true
            VSeg {
                objectName: "pushfps-vfg-quality"
                options: [{ id: "0", label: qsTr("低") }, { id: "1", label: qsTr("中") }, { id: "2", label: qsTr("高") }]
                current: Number(veyra.vfgQuality || 0) <= 2 ? String(Number(veyra.vfgQuality || 0)) : ""
                onPicked: id => veyra.vfgQuality = Number(id)
            }
        }
        VRow {
            objectName: "pushfps-adaptive-row"
            visible: !quick.proPage   // Professional mode's own list has this switch
            label: qsTr("自适应倍率")
            help: qsTr("很多主机游戏的帧率并不固定：30 FPS 的游戏会掉到 25，40 FPS 模式会在 35 到 45 之间波动。开启后 PushFPS 实时测量游戏帧率，每一对画面都选择最接近目标帧率的补帧倍率，类似 NVIDIA DLSS 4.5 的动态多帧生成")
            hint: !quick.fgPossible ? (Support.status(veyra, "frame-generation").reason || "")
                : !quick.adaptivePossible ? qsTr("当前补帧方式只有 2×，无需自适应")
                : quick.adaptive && quick.liveStats.fgMultiplierNow > 1 && quick.liveStats.contentFps > 0
                  ? qsTr("现在 %1×：游戏 %2 FPS → 约 %3 FPS").arg(quick.liveStats.fgMultiplierNow)
                        .arg(Math.round(quick.liveStats.contentFps)).arg(Math.round(quick.liveStats.contentFps * quick.liveStats.fgMultiplierNow))
                  : qsTr("游戏帧率波动时自动调整倍率，输出尽量贴近目标")
            VSwitch {
                objectName: "pushfps-adaptive"
                enabled: quick.adaptivePossible && veyra.fgEnabled
                opacity: enabled ? 1 : 0.4
                checked: quick.adaptive
                onToggled: checked => quick.setAdaptive(checked)
            }
        }
        VRow {
            objectName: "pushfps-pacing-row"
            visible: !quick.proPage   // Professional mode's own list has this switch
            label: qsTr("帧节奏修复")
            help: qsTr("很多锁 30 FPS 的主机游戏出帧并不均匀：有的画面停留 17 毫秒，有的停留 50 毫秒，即使帧率没错，画面也会顿挫。PushFPS 把每一帧放回均匀的位置再补帧，主机自己做不到这一点。每帧最多晚一次刷新显示")
            hint: !quick.ownTechPossible ? (quick.fgPossible ? qsTr("XeSS 自己控制节奏，不能同时使用") : (Support.status(veyra, "frame-generation").reason || ""))
                : quick.pacingOn && quick.pacingShare >= 0
                  ? (quick.pacingShare > 0.05 ? qsTr("正在修复：%1% 的画面重新对齐").arg(Math.round(quick.pacingShare * 100))
                                              : qsTr("这款游戏的节奏已经均匀，无需修复"))
                  : qsTr("修复忽快忽慢的出帧（如 17/50 毫秒交替），让 30 FPS 游戏动起来更顺")
            VSwitch {
                objectName: "pushfps-pacing"
                enabled: quick.ownTechPossible && veyra.fgEnabled
                opacity: enabled ? 1 : 0.4
                checked: quick.pacingOn
                onToggled: checked => quick.setPacingRepair(checked)
            }
        }
        // Smooth frame pacing: every frame waits for a refresh (V-Sync; variable refresh where the monitor
        // has it) and the output is capped at the refresh rate, so each generated frame is shown once.
        // Off: Veyra's Automatic, Present(0,0): the newest frame replaces one not shown yet (lowest
        // latency, but frame generation output can skip a frame and repeat the next).
        VRow {
            objectName: "pushfps-smooth-row"
            visible: !quick.shortForm
            label: qsTr("流畅帧同步")
            help: qsTr("等屏幕刷新再显示每一帧，并把输出限制在屏幕刷新率：补出来的每一帧都完整显示，玩起来和录像一样流畅。屏幕支持 G-SYNC/FreeSync 时会请求可变刷新，几乎不增加延迟")
            hint: quick.screenLimitedTarget
                  ? qsTr("这块屏幕是 %2 Hz：现在按 %2 FPS 输出。换到更快的屏幕会回到 %1 FPS").arg(Math.round(veyra.outputCustomFps)).arg(Math.round(veyra.displayRefreshHz))
                  : quick.smoothPacing && quick.unevenTarget
                  ? qsTr("%1 FPS 不能整除 %2 Hz：每帧显示时间不均匀。请选「匹配显示器」，或把屏幕设为 %1 的整数倍").arg(Math.round(veyra.outputCustomFps)).arg(Math.round(veyra.displayRefreshHz))
                  : quick.smoothPacing ? qsTr("每一帧都完整显示；没有可变刷新时最多多等一次屏幕刷新")
                                       : qsTr("延迟最低，但补帧画面可能偶尔跳帧，直播画面比录像卡顿")
            VSwitch {
                objectName: "pushfps-smooth"
                checked: quick.smoothPacing
                onToggled: checked => quick.setSmoothPacing(checked)
            }
        }
        VRow {
            objectName: "pushfps-gsync-row"
            // Once checked with nothing left to set, the row goes: the driver's report is then all there is.
            visible: !quick.shortForm && quick.gsyncUnconfirmed && veyra.preferences.pushGsyncChecked !== true
                     && (!quick.gsyncChecked || quick.gsyncWanted || quick.gsyncTools.busy || quick.gsyncTools.canRepair)
            label: "G-SYNC"
            help: qsTr("屏幕支持可变刷新时，画面一准备好屏幕就刷新：不撕裂，每一帧显示的时间都一样长。NVIDIA 驱动要为每个程序单独允许窗口和全屏 G-SYNC")
            hint: quick.gsyncWanted || (quick.gsyncTools && quick.gsyncTools.busy) ? qsTr("正在检查驱动配置…")
                : qsTr("屏幕支持可变刷新，但驱动还没有确认对 PushFPS 生效，画面可能不如录像流畅。点「一键设置」为 PushFPS 允许窗口和全屏 G-SYNC，然后重启 PushFPS")
            hintBelow: true
            VButton {
                objectName: "pushfps-gsync-fix"
                text: qsTr("一键设置")
                enabled: quick.gsyncTools !== null && !quick.gsyncTools.busy && !quick.gsyncWanted
                onClicked: { quick.gsyncWanted = true; quick.gsyncTools.check() }
            }
        }
    }
    PushSection {
        id: secPicture
        sectionId: "picture"
        visible: quick.scopePicture
        first: quick.scope === "picture"
        title: qsTr("画面")
        summary: quick.sectionSummary("picture")
        open: quick.sectionOpen("picture")
        onToggled: open => quick.setSectionOpen("picture", open)
        VRow {
            id: enhanceRow
            objectName: "pushfps-enhance-row"
            // (4.2.2) Shown on every page again. 4.2.1 left it out of Professional mode's picture card
            // because that page's own enhancement card has a switch; but that switch cannot be used
            // until a layer exists, and this one adds the first layer. With no layer the enhancement
            // could not be switched on in Professional mode at all (a user: "the toggle and option is gone").
            label: qsTr("AI 画面增强")
            help: qsTr("用 AI 去掉采集卡压缩造成的色块和噪点，让细节更清楚。和补帧无关，可以单独开关")
            // Not possible: the reason, and when the card could run it but its files were left out at
            // installation (they are the installer's optional extras), how to get them.
            readonly property var support: Support.status(veyra, "nr")
            // (4.5.2) Its files left out at installation: one button installs them (UpdateChecker::installExtras,
            // the signed update path for the version installed, with the installer's optional extras).
            readonly property var updates: veyra.updates || null
            readonly property bool extrasOffered: !quick.nrPossible && support.missingFiles === true
                                                  && updates !== null && updates.canInstallExtras === true
            readonly property string installing: updates ? String(updates.installState || "") : ""
            readonly property bool busy: installing === "verifying" || installing === "downloading" || installing === "installing"
            hint: quick.nrPossible ? qsTr("关闭时只做补帧。开启后每帧都经过 NVIDIA 神经渲染（DLSS 5 NR）增强，画面更清晰，但更耗 GPU")
                  : extrasOffered && busy ? (installing === "downloading"
                        ? qsTr("正在下载… %1%").arg(Math.round(Number(updates.installProgress || 0) * 100))
                        : installing === "installing" ? qsTr("正在启动安装程序；PushFPS 会自动关闭，装好后重新打开")
                        : qsTr("正在验证签名…"))
                  : extrasOffered ? qsTr("安装时没有选择它的组件。点「安装」即可补装：PushFPS 会关闭，下载并安装组件后重新打开，设置会保留")
                  : (support.reason || "") + (support.missingFiles === true
                        ? " " + qsTr("它属于安装时可选的实验性附加组件：重新运行 Install-PushFPS.cmd，询问时回答 y 即可，设置会保留") : "")
            hintBelow: !quick.nrPossible && support.missingFiles === true
            RowLayout {
                spacing: 8
                VButton {
                    objectName: "pushfps-enhance-install"
                    visible: enhanceRow.extrasOffered
                    text: qsTr("安装")
                    primary: true
                    enabled: !enhanceRow.busy
                    opacity: enabled ? 1 : 0.5
                    onClicked: enhanceRow.updates.installExtras()
                }
                VSwitch {
                    objectName: "pushfps-enhance"
                    enabled: quick.nrPossible
                    opacity: enabled ? 1 : 0.4
                    checked: veyra.nrAnyEnabled
                    onToggled: checked => quick.setEnhancement(checked)
                }
            }
        }
        // The enhancement's look (shown while AI picture enhancement is on).
        VRow {
            objectName: "pushfps-look-row"
            visible: quick.lookShown
            label: qsTr("增强风格")
            help: qsTr("AI 画面增强改动画面的力度。「强烈」是完整效果，也是一直以来的默认值；觉得画面不像原作、过于锐利或人脸不自然时，选「均衡」或「柔和」。也可以用下面两个滑块自己调")
            hint: quick.lookId === "" ? qsTr("自定义：已用滑块调整") : quick.lookId === "strong" ? qsTr("完整效果（默认）") : qsTr("比默认更接近原画")
            RowLayout {
                spacing: 8
                VSeg {
                    objectName: "pushfps-look"
                    options: quick.looks.map(l => ({ id: l.id, label: l.label }))
                    current: quick.lookId
                    onPicked: id => quick.applyLook(id)
                }
                VButton {
                    objectName: "pushfps-look-reset"
                    text: qsTr("重置")
                    enabled: quick.lookId !== "strong"
                    opacity: enabled ? 1 : 0.4
                    onClicked: quick.resetLook()
                }
            }
        }
        // The size the enhancement works at (shown with the look; the AMD runtime stays at 1080p).
        VRow {
            objectName: "pushfps-enhance-size-row"
            visible: quick.lookShown
            label: qsTr("增强的处理大小")
            help: qsTr("AI 画面增强在多大的画面上计算。标准是 1080p：画面是 4K 时，增强的改动按四分之一的像素算出来，再加到 4K 画面上，细节会软一些。选「完整大小」就在每个像素上计算，更清晰，4K 时工作量约为四倍，需要较强的显卡。补帧开始跳帧时请改回 1080p。")
            hint: !quick.enhanceSizeKnown ? qsTr("已在专业模式里设为其他大小；在这里选一项就会改用它")
                : quick.enhanceSize === 1 ? qsTr("在画面的每个像素上计算：最清晰，最吃显卡")
                : quick.enhanceSize === 5 ? qsTr("最高按 1440p 计算：比标准清晰，比完整大小省")
                : qsTr("最高按 1080p 计算（默认）：最省显卡")
            VSeg {
                objectName: "pushfps-enhance-size"
                options: quick.enhanceSizes.map(s => ({ id: String(s.id), label: s.label }))
                current: quick.enhanceSizeKnown ? String(quick.enhanceSize) : ""
                onPicked: id => quick.setEnhanceSize(Number(id))
            }
        }
        VRow {
            objectName: "pushfps-look-tone-row"
            visible: quick.lookShown
            label: qsTr("光影")
            hint: qsTr("AI 重新处理明暗和色彩的程度。0% 保持原画的光影和颜色")
            value: Math.round(quick.lookTone * 100) + "%"
            VSlider {
                objectName: "pushfps-look-tone"
                valueFromModel: true
                implicitWidth: 150
                from: 0; to: 1; stepSize: 0.01
                inputScale: 100
                value: quick.lookTone
                resettable: true; defaultValue: 1
                onMoved: v => { if (quick.lookShown) quick.setLook(v, undefined) }
            }
        }
        VRow {
            objectName: "pushfps-look-detail-row"
            visible: quick.lookShown
            label: qsTr("细节")
            hint: qsTr("AI 补充细小纹理、接触阴影和反光的程度。0% 不添加细节")
            value: Math.round(quick.lookStructure * 100) + "%"
            VSlider {
                objectName: "pushfps-look-detail"
                valueFromModel: true
                implicitWidth: 150
                from: 0; to: 1; stepSize: 0.01
                inputScale: 100
                value: quick.lookStructure
                resettable: true; defaultValue: 1
                onMoved: v => { if (quick.lookShown) quick.setLook(undefined, v) }
            }
        }
        VRow {
            objectName: "pushfps-look-compare-row"
            visible: quick.lookShown
            label: qsTr("与原画对比")
            help: qsTr("把画面分成两半：一边是没有 AI 画面增强的原画，另一边是增强后的画面，方便边调边看。在专业模式的画面上或全屏时，按住鼠标左键可以拖动分割线；两侧可以交换")
            hint: quick.lookSplit ? qsTr("对比时补帧暂停；关闭对比后自动恢复。两分钟后自动关闭")
                                  : veyra.hasSource !== true ? qsTr("先连接画面")
                                  : quick.compareSwapped ? qsTr("左边增强后，右边原画") : qsTr("左边原画，右边增强后")
            RowLayout {
                spacing: 8
                // PushFPS 4.2.1: which side is which (a user's request). Shown while the split is on, and
                // while the sides are swapped, so a swap is never left behind unseen.
                VButton {
                    objectName: "pushfps-look-compare-swap"
                    ghost: true
                    visible: quick.lookSplit || quick.compareSwapped
                    text: qsTr("交换两侧")
                    onClicked: veyra.setPreference("pushCompareSwap", !quick.compareSwapped)
                }
                VSwitch {
                    objectName: "pushfps-look-compare"
                    enabled: veyra.hasSource === true || quick.lookSplit
                    opacity: enabled ? 1 : 0.4
                    checked: quick.lookSplit
                    onToggled: checked => quick.setLookCompare(checked)
                }
            }
        }
        // Fine-tuning (ten sliders): folded away until asked for.
        VRow {
            objectName: "pushfps-fine-row"
            visible: quick.fineShown
            label: qsTr("精细调整")
            help: qsTr("调整的是 AI 画面增强对画面的改动，而不是画面本身：可以只保留提亮或压暗，只让它作用于暗部、中间调或高光，限制每个像素最多变化多少，或者保持原画的色彩和色相。全部为默认值时不做任何额外处理。对所有增强层同时生效，并随每个主机和游戏的配置保存")
            hint: quick.fineNeutral ? qsTr("默认：AI 的改动保持原样") : qsTr("已调整 AI 的改动")
            RowLayout {
                spacing: 8
                VButton {
                    objectName: "pushfps-fine-reset"
                    text: qsTr("重置")
                    visible: !quick.fineNeutral
                    onClicked: quick.resetFine()
                }
                VButton {
                    objectName: "pushfps-fine-toggle"
                    ghost: true
                    text: quick.fineOpen ? qsTr("收起") : qsTr("展开")
                    onClicked: veyra.setPreference("pushFineTuneOpen", !quick.fineOpen)
                }
            }
        }
        Repeater {
            model: quick.fineShown && quick.fineOpen ? quick.fineSpecs : []
            delegate: VRow {
                required property var modelData
                objectName: "pushfps-fine-row-" + modelData.id
                label: modelData.label
                hint: modelData.hint
                value: quick.fineText(modelData.id)
                VSlider {
                    objectName: "pushfps-fine-" + modelData.id
                    valueFromModel: true
                    implicitWidth: 150
                    from: 0; to: modelData.to; stepSize: 0.01
                    inputScale: 100
                    value: quick.fineValue(modelData.id)
                    resettable: true; defaultValue: modelData.neutral
                    onMoved: v => { if (quick.fineShown) quick.setFine(modelData.id, v) }
                }
            }
        }
        // PushFPS 4.2.1: upscale to my screen. Until 4.2.2 it was left out on Professional mode's page,
        // whose own upscaler card stands in the same tab. That card's switch answers for the upscaler
        // chosen now: on a card without NVIDIA's upscalers it is greyed out until another one is picked in
        // the list under it, which reads as "upscaling cannot be used here". This row takes an upscaler
        // the card can run, so it stands on every page (the 4.2.2 lesson: a switch is never hidden as a
        // duplicate while the other one cannot be used in every state).
        VRow {
            objectName: "pushfps-upscale-row"
            label: qsTr("放大到屏幕")
            help: qsTr("采集的画面比屏幕小时，全屏只是把它拉大，看起来发软。开启后由显卡把画面放大到屏幕分辨率，边缘和文字更清晰，也更吃显卡。放大方式可以在专业模式的「画质」里更换")
            hint: quick.upscaleHint
            VSwitch {
                objectName: "pushfps-upscale"
                enabled: quick.upscalePossible || quick.upscaleOn
                opacity: enabled ? 1 : 0.4
                checked: quick.upscaleOn
                onToggled: checked => quick.setUpscale(checked)
            }
        }
        VRow {
            objectName: "pushfps-fill-row"
            visible: quick.fillShown && !(quick.compact && quick.compactPro)
            label: qsTr("宽屏填充")
            help: qsTr("主机只输出 16:9 画面，带鱼屏两侧会空出来。这里选择怎么处理：留黑边、用画面本身的柔和延伸填满（氛围填充）、裁掉上下铺满，或者拉伸铺满。游戏本身不会显示更宽的视野。录像和直播不受影响")
            hint: quick.fillHint
            hintBelow: true
            VSeg {
                objectName: "pushfps-fill"
                options: quick.fillOptions
                current: quick.fillId
                onPicked: id => quick.setFill(id)
            }
        }
        // Crop and resize of the console picture: the button opens the editor on the live picture, the
        // switch shows the whole picture again without losing the values.
        VRow {
            objectName: "pushfps-crop-row"
            visible: !(quick.compact && quick.compactPro)
            label: qsTr("裁剪和缩放")
            help: qsTr("裁掉画面的任意一边，例如游戏自带的黑边或采集卡多出来的边框，并调整画面在屏幕上的大小和位置。只改变屏幕上的显示；录制和直播默认保留完整画面，可以在里面改")
            hint: quick.cropHint
            hintBelow: true
            RowLayout {
                spacing: 8
                VButton {
                    objectName: "pushfps-crop-edit"
                    ghost: true
                    text: qsTr("调整…")
                    onClicked: quick.openOnPicture("crop")
                }
                VSwitch {
                    objectName: "pushfps-crop"
                    enabled: !Crop.neutral(quick.cropState)
                    opacity: enabled ? 1 : 0.4
                    checked: quick.cropState.on
                    onToggled: checked => veyra.setPreference("pushCropOn", checked)
                }
            }
        }
    }
    // (4.2.2) Professional mode's Frame Gen tab had AI picture enhancement until 4.2.1 moved the Picture
    // section to the Quality tab. Someone who looks for it here is told where it is and taken there.
    VRow {
        objectName: "pushfps-picture-moved-row"
        visible: quick.proPage && quick.scope === "rest"
        label: qsTr("AI 画面增强")
        hint: qsTr("开关、风格、微调和其他画面设置现在都在「画质」页")
        VButton {
            objectName: "pushfps-picture-moved-open"
            ghost: true
            text: qsTr("打开")
            onClicked: quick.requestTab("quality")
        }
    }
    PushSection {
        id: secHud
        sectionId: "hud"
        visible: quick.scopeMain
        title: qsTr("HUD")
        summary: quick.sectionSummary("hud")
        open: quick.sectionOpen("hud")
        onToggled: open => quick.setSectionOpen("hud", open)
        // Crosshair Hold (pipeline/PushOverlay.h): off unless switched on. The crosshair is found in the middle of the
        // screen, taken out of what frame generation is given and laid back over what it makes. For every method the
        // graph runs itself (NVIDIA VFG, AMD FRC, PushFlow, DLSS frame generation, FSR): not XeSS, not ANR.
        VRow {
            id: crosshairHold
            objectName: "pushfps-crosshair-row"
            visible: !quick.proPage   // Professional mode's own list has this switch
            label: qsTr("准星保持")
            help: qsTr("准星静止而背后的画面在动时，补帧会把准星当成背景一起移动：它从原位消失，在两侧各出现一个较淡的重影。开启后，PushFPS 在屏幕中央找出准星，补帧前先把它从画面里取出，补帧后再原样放回。准星刚出现或其线条正在收放时，需要先静止片刻才会被识别。")
            readonly property int holdState: Number(quick.liveStats.crosshairHoldState || 0)
            // What the engine says it holds (pixels of the zone in the frame sampled last): the row says whether the
            // pass really runs and whether it has a crosshair, never only that the switch is on.
            readonly property int holdPixels: Number(quick.liveStats.crosshairHoldPixels || 0)
            readonly property bool on: veyra.preferences.pushCrosshairHold === true
            readonly property bool methodOk: ["vfg", "amdfrc", "pushflow", "dlss", "fsr3", "fsr4"].indexOf(veyra.fgBackendName) >= 0
            hint: !quick.ownTechPossible ? (quick.fgPossible ? qsTr("XeSS 自己生成画面，不能同时使用") : (Support.status(veyra, "frame-generation").reason || ""))
                : on && !methodOk ? qsTr("这种补帧方式暂不支持，请换一种补帧方式")
                : on && holdState === 2 ? qsTr("没能在这台电脑上启动，补帧照常运行（原因见日志）")
                : on && holdState === 1 ? (holdPixels > 0 ? qsTr("正在运行：已找到准星，补出的每一帧里都原样放回")
                                                           : qsTr("正在运行：画面中央还没有找到准星（背后的画面动起来才会识别）"))
                : on ? qsTr("已开启：有画面播放并且补帧开启后开始运行")
                : qsTr("已关闭：画面移动时准星可能闪烁或出现重影")
            VSwitch {
                objectName: "pushfps-crosshair"
                enabled: quick.ownTechPossible && veyra.fgEnabled
                opacity: enabled ? 1 : 0.4
                checked: quick.ownTechPossible && crosshairHold.on   // by id: a row's control is not the row's direct child
                onToggled: checked => veyra.setPreference("pushCrosshairHold", checked)
            }
        }
        // Subtitle Hold (pipeline/PushOverlay.h, subtitleZoneFor): the same machinery in the band along the bottom where
        // a game draws its subtitles. Off unless switched on: not every player uses subtitles.
        VRow {
            id: subtitleHold
            objectName: "pushfps-subtitle-row"
            visible: !quick.proPage   // Professional mode's own list has this switch
            label: qsTr("字幕保持")
            help: qsTr("字幕静止而背后的画面在动时（例如高速开车），补帧会把字幕当成背景一起拉动，文字会上下拖影、出现重影。开启后，PushFPS 在屏幕下方找出字幕，补帧前先把它从画面里取出，补帧后再原样放回。新的一行字幕出现时，需要先静止片刻才会被识别。不用字幕时不必开启。")
            readonly property int holdState: Number(quick.liveStats.subtitleHoldState || 0)
            readonly property int holdPixels: Number(quick.liveStats.subtitleHoldPixels || 0)
            readonly property bool on: veyra.preferences.pushSubtitleHold === true
            readonly property bool methodOk: ["vfg", "amdfrc", "pushflow", "dlss", "fsr3", "fsr4"].indexOf(veyra.fgBackendName) >= 0
            hint: !quick.ownTechPossible ? (quick.fgPossible ? qsTr("XeSS 自己生成画面，不能同时使用") : (Support.status(veyra, "frame-generation").reason || ""))
                : on && !methodOk ? qsTr("这种补帧方式暂不支持，请换一种补帧方式")
                : on && holdState === 2 ? qsTr("没能在这台电脑上启动，补帧照常运行（原因见日志）")
                : on && holdState === 1 ? (holdPixels > 0 ? qsTr("正在运行：已找到字幕，补出的每一帧里都原样放回")
                                                           : qsTr("正在运行：屏幕下方还没有找到字幕（背后的画面动起来才会识别）"))
                : on ? qsTr("已开启：有画面播放并且补帧开启后开始运行")
                : qsTr("已关闭：画面快速移动时字幕可能拖影")
            VSwitch {
                objectName: "pushfps-subtitle"
                enabled: quick.ownTechPossible && veyra.fgEnabled
                opacity: enabled ? 1 : 0.4
                checked: quick.ownTechPossible && subtitleHold.on   // by id: a row's control is not the row's direct child
                onToggled: checked => veyra.setPreference("pushSubtitleHold", checked)
            }
        }
        // The still rule (pipeline/PushHudStill.h): off unless switched on (since 4.0.1). What stands still in front of a
        // moving picture keeps its real pixels in generated frames, with every method the graph runs.
        VRow {
            id: hudStill
            objectName: "pushfps-hudstill-row"
            visible: !quick.proPage   // Professional mode's own list has this switch
            label: qsTr("保持 HUD 清晰")
            help: qsTr("补帧会给画面的每个位置算一个运动。HUD 静止而背后的画面在动时，HUD 的像素会被当成背景一起拉动，文字、血条和准星就会发糊、重影或消失。开启后，在两张真实画面里完全相同、并且周围大部分在动的像素，会在补出来的帧里直接使用真实画面的像素。它不需要预热，每一对画面单独判断；画面整体静止时（菜单、镜头不动）不会改动任何像素。对带噪点的信号（模拟连接、压缩很重的串流）它找不到完全相同的像素，补帧就和原来一样。")
            readonly property int stillState: Number(quick.liveStats.hudStillState || 0)
            readonly property bool on: veyra.preferences.pushHudStill === true
            hint: !quick.ownTechPossible ? (quick.fgPossible ? qsTr("XeSS 自己生成画面，不能同时使用") : (Support.status(veyra, "frame-generation").reason || ""))
                : on && stillState === 2 ? qsTr("没能在这台电脑上启动，补帧照常运行（原因见日志）")
                : on ? qsTr("已开启：镜头移动时画面可能变糊，物体边缘可能破碎；出现这种情况请关闭")
                : qsTr("已关闭：HUD 交给补帧处理")
            VSwitch {
                objectName: "pushfps-hudstill"
                enabled: quick.ownTechPossible && veyra.fgEnabled
                opacity: enabled ? 1 : 0.4
                checked: quick.ownTechPossible && hudStill.on   // by id: a row's control is not the row's direct child
                onToggled: checked => veyra.setPreference("pushHudStill", checked)
            }
        }
        VRow {
            objectName: "pushfps-hudlock-row"
            visible: !quick.proPage   // Professional mode's own list has this switch
            label: qsTr("实时 HUD 锁定")
            help: qsTr("补出来的帧会把画面上静止的文字、准星和血条跟着背景一起拉动，看起来发糊或抖动。开启后 PushFPS 每一帧都会找出静止的 HUD 像素，补帧时直接用主机原来的像素，不需要画框")
            // What actually runs (EnhanceGraph::hudLockState): a pass that could not start says so.
            readonly property int hudState: Number(quick.liveStats.hudLockState || 0)
            hint: !quick.ownTechPossible ? (quick.fgPossible ? qsTr("XeSS 自己生成画面，不能同时使用") : (Support.status(veyra, "frame-generation").reason || ""))
                : veyra.hudLock === true && hudState === 2 ? qsTr("实时 HUD 锁定没能在这台电脑上启动，补帧照常运行（原因见日志）")
                : veyra.hudLock === true && hudState === 1 ? qsTr("正在运行：文字和准星在补出来的帧里保持清晰")
                : qsTr("每一帧自动找出 HUD，文字和准星在补出来的帧里保持清晰")
            VSwitch {
                objectName: "pushfps-hudlock"
                enabled: quick.ownTechPossible && veyra.fgEnabled
                opacity: enabled ? 1 : 0.4
                checked: quick.ownTechPossible && veyra.hudLock === true
                onToggled: checked => veyra.hudLock = checked
            }
        }
    }
    PushSection {
        id: secLatency
        sectionId: "latency"
        visible: quick.scopeMain
        title: qsTr("延迟与声音")
        summary: quick.sectionSummary("latency")
        open: quick.sectionOpen("latency")
        onToggled: open => quick.setSectionOpen("latency", open)
        VRow {
            objectName: "pushfps-lowest-latency-row"
            label: qsTr("最低延迟")
            help: qsTr("一键打开所有不影响画质的低延迟设置：显卡只排队一帧（NVIDIA 显卡用 NVIDIA 低延迟），连接后进入全屏（绕过桌面合成，少等一次刷新），屏幕支持 G-SYNC/FreeSync 时用可变刷新同步（不撕裂、不跳帧）。补帧、AI 增强、HUD 锁定和帧节奏修复保持不变。关闭后恢复之前的设置")
            hint: quick.lowestLatencyHint
            VSwitch {
                objectName: "pushfps-lowest-latency"
                checked: quick.lowestLatency
                onToggled: checked => quick.setLowestLatency(checked)
            }
        }
        VRow {
            objectName: "pushfps-low-latency-row"
            visible: !quick.shortForm
            label: quick.nvidiaLatency ? qsTr("英伟达低延迟") : qsTr("低延迟队列")
            help: quick.nvidiaLatency ? qsTr("减少在显卡里排队等待的画面，从按下按键到画面反应更快。只在 NVIDIA 显卡上可用")
                                      : qsTr("只让一帧画面排队等待屏幕，而不是三帧：从按下按键到画面反应更快。任何显卡都能用")
            hint: quick.nvidiaLatency ? qsTr("减少处理排队，降低输入延迟") : qsTr("减少排队等待屏幕的画面，降低输入延迟")
            VSwitch {
                objectName: "pushfps-low-latency"
                checked: quick.nvidiaLatency ? (veyra.nvidiaLowLatency ?? 0) > 0 : veyra.fgLowQueue === true
                onToggled: checked => {
                    if (quick.nvidiaLatency) veyra.nvidiaLowLatency = checked ? 1 : 0
                    else veyra.fgLowQueue = checked
                }
            }
        }
        VRow {
            objectName: "pushfps-latency-overlay-row"
            label: qsTr("在画面上显示延迟")
            // Two sentences on two lines: the first keeps the translations it has in every language.
            hint: qsTr("实测电脑处理延迟（采集卡画面到达到交给显示器）和帧率；采集卡、显示器和游戏的延迟另计") + "\n"
                  + qsTr("可以用鼠标把它拖到画面上的任何位置；双击它，它就回到左上角")
            RowLayout {
                spacing: 8
                VButton {
                    objectName: "pushfps-latency-guide-open"
                    ghost: true
                    text: qsTr("测量总延迟")
                    iconName: "gauge"
                    onClicked: veyra.navigate("latencyguide")
                }
                VSwitch {
                    objectName: "pushfps-latency-overlay"
                    checked: veyra.preferences.pushLatencyOverlay === true
                    onToggled: checked => veyra.setPreference("pushLatencyOverlay", checked)
                }
            }
        }
        // Lip sync by hand (sink/PushAudioTrim.h): milliseconds on top of the automatic sound delay.
        // Automatic sync cannot see how much later than its sound a capture card hands over its picture,
        // nor what the screen and the sound device add afterwards, so the last step is the user's.
        VRow {
            id: lipSync
            objectName: "pushfps-lipsync-row"
            readonly property int trim: {
                const value = Number(veyra.preferences.pushAudioTrimMs)
                return isFinite(value) ? Math.max(-250, Math.min(250, Math.round(value))) : 0
            }
            // Automatic sync is what the trim adds to; in manual mode and with sync off it does nothing.
            readonly property bool automatic: veyra.audioSyncMode === undefined || veyra.audioSyncMode === 0
            // The delay the sound really has now (0 when nothing live plays).
            readonly property real applied: veyra.audioSyncLive === true ? Number(veyra.audioCompensationMs) || 0 : -1
            // Sound cannot be played before it arrives: a trim further back than the delay there is has no more effect.
            readonly property bool atLimit: automatic && trim < 0 && applied >= 0 && applied < 0.5
            function set(value) { veyra.setPreference("pushAudioTrimMs", Math.max(-250, Math.min(250, Math.round(value)))) }
            label: qsTr("声音对口型")
            help: qsTr("声音和嘴型对不上时在这里微调。自动同步会让声音延后画面在 PushFPS 里所花的时间，但它看不到采集卡交出画面比声音晚多少，也看不到显示器、电视音箱、回音壁或无线耳机之后再加的延迟。声音先到就往“晚”调，声音晚到就往“早”调；声音不能早于它到达的时间，所以“早”有上限。")
            hint: !automatic ? qsTr("声音同步设为手动或关闭时微调不起作用（音频设置里改回自动估算）")
                : atLimit ? qsTr("已经最早：声音一到就播放，再往早调没有效果")
                : trim === 0 ? (applied >= 0 ? qsTr("自动：声音延后 %1 毫秒，与画面对齐").arg(Math.round(applied)) : qsTr("自动：声音跟随画面的延迟"))
                : trim > 0 ? qsTr("声音比自动再晚 %1 毫秒").arg(trim)
                : qsTr("声音比自动早 %1 毫秒").arg(-trim)
            RowLayout {
                spacing: 6
                VButton {
                    objectName: "pushfps-lipsync-earlier"
                    ghost: true
                    enabled: lipSync.automatic && lipSync.trim > -250 && !lipSync.atLimit
                    text: qsTr("早 10")
                    onClicked: lipSync.set(lipSync.trim - 10)
                }
                Text {
                    objectName: "pushfps-lipsync-value"
                    text: (lipSync.trim > 0 ? "+" : "") + lipSync.trim + " ms"
                    color: lipSync.trim === 0 ? Theme.t2 : Theme.t1
                    font.family: Theme.fontMono; font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    Layout.preferredWidth: 62
                }
                VButton {
                    objectName: "pushfps-lipsync-later"
                    ghost: true
                    enabled: lipSync.automatic && lipSync.trim < 250
                    text: qsTr("晚 10")
                    onClicked: lipSync.set(lipSync.trim + 10)
                }
                VButton {
                    objectName: "pushfps-lipsync-reset"
                    ghost: true
                    visible: lipSync.trim !== 0
                    text: qsTr("归零")
                    onClicked: lipSync.set(0)
                }
            }
        }
    }
    PushSection {
        id: secStart
        sectionId: "start"
        visible: quick.scopeMain
        title: qsTr("连接与启动")
        summary: quick.sectionSummary("start")
        open: quick.sectionOpen("start")
        onToggled: open => quick.setSectionOpen("start", open)
        // Plug and play: Veyra's 启动后自动继续上次内容 (autoResume: the last capture card with its
        // settings, or the last stream) and PushFPS's fullscreen-on-connect (Main.qml).
        VRow {
            objectName: "pushfps-auto-connect-row"
            visible: !quick.shortForm
            label: qsTr("启动时自动连接")
            hint: qsTr("打开 PushFPS 时直接连接上次的采集卡（沿用上次的设置）或串流")
            VSwitch {
                objectName: "pushfps-auto-connect"
                checked: !!veyra.preferences.autoResume
                onToggled: v => veyra.setPreference("autoResume", v)
            }
        }
        VRow {
            objectName: "pushfps-auto-fullscreen-row"
            visible: !quick.shortForm
            label: qsTr("连接后自动全屏")
            hint: qsTr("采集卡或串流画面出现时进入全屏；按 F11 或 Esc 退出")
            VSwitch {
                objectName: "pushfps-auto-fullscreen"
                checked: veyra.preferences.pushAutoFullscreen === true
                onToggled: v => veyra.setPreference("pushAutoFullscreen", v)
            }
        }
        // (4.3.1) The new design's home page shows the picture, so a capture card connected from it
        // plays there (Main.qml staysHomeOnConnect). Whoever would rather land in Professional mode,
        // as before, says so here. Legacy's home page has no picture and always goes there: no row.
        VRow {
            objectName: "pushfps-connect-pro-row"
            visible: !quick.shortForm && Theme.modern
            label: qsTr("连接采集卡后打开专业模式")
            hint: veyra.preferences.pushAutoFullscreen === true
                  ? qsTr("“连接后自动全屏”已开启：连接后总是进入专业模式并全屏")
                  : qsTr("关闭时留在首页，画面显示在首页中间")
            VSwitch {
                objectName: "pushfps-connect-pro"
                checked: veyra.preferences.pushConnectToPro === true
                onToggled: v => veyra.setPreference("pushConnectToPro", v)
            }
        }
    }
}
