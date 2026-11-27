// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PushFPS quick setup: game frame rate, target frame rate, console and game
// profiles. UI logic only, against the fixture bridge in QuickSmokeTests.cpp.
import QtQuick
import QtTest
import Veyra
Item {
    id: root
    width: 640; height: 640
    PushQuickSetup { id: quick; x: 10; y: 10; width: 560 }
    Component {
        id: gsyncMock
        QtObject {
            property bool busy: false
            property bool canRepair: false
            property bool canRestore: false
            property string summary: ""
            property string detail: ""
            property int checks: 0
            property int repairs: 0
            signal changed()
            function check() { checks++; busy = true }
            function repair() { repairs++ }
        }
    }
    TestCase {
        name: "PushQuickSetup"
        when: windowShown
        function init() {
            quick.keepsProfiles = false; quick.profileQuietUntil = 0
            Theme.design = "legacy"; quick.compact = false   // (a test that left the new design or the short form on would change every later one)
            veyra.preferences = ({exportStopsPlayback: false})
            veyra.fgMultiplier = 1
            veyra.fgEnabled = false
            veyra.outputRateMode = 0
            veyra.outputCustomFps = 0
            veyra.nrAnyEnabled = false
            veyra.presets = []
            veyra.currentPresetName = "自定义"
            veyra.lastAppliedPreset = -1
            veyra.fgAdaptive = false
            veyra.pacingRepair = false
            veyra.hudLock = false
            veyra.presentationOwned = false
            veyra.contentRate = 0
            veyra.fgMultiplierChoices = [{id:"1"},{id:"2"},{id:"3"},{id:"4"}]
            veyra.liveStats = ({running: false})
            veyra.nrLayers = [{index: 1}]
            veyra.compareMode = 0
            veyra.hasSource = true
            veyra.displayRefreshHz = 240
            veyra.aspectMode = 0
            quick.screenAspectOverride = 0
            veyra.audioSyncMode = 0; veyra.audioSyncLive = false; veyra.audioCompensationMs = 0
        }
        function cleanup() { init() }
        function control(name) { const c = findChild(quick, name); verify(c !== null, name); return c }

        function test_target_sets_multiplier_and_cap() {
            compare(quick.baseFps, 30)
            quick.applyTarget("120")
            compare(veyra.fgMultiplier, 4)
            verify(veyra.fgEnabled)
            compare(veyra.outputRateMode, 2)
            compare(veyra.outputCustomFps, 120)
            compare(quick.currentTarget, "120")
            quick.applyTarget("60")
            compare(veyra.fgMultiplier, 2)
            compare(veyra.outputCustomFps, 60)
            quick.applyTarget("monitor")
            compare(veyra.outputRateMode, 1)
            compare(veyra.fgMultiplier, 4, "240 Hz monitor: the largest offered multiplier")
            quick.applyTarget("off")
            verify(!veyra.fgEnabled)
            compare(quick.currentTarget, "off")
        }
        function test_target_removes_duplicate_frames() {
            compare(veyra.contentRate, 0)
            quick.applyTarget("120")
            compare(veyra.contentRate, 1, "a 30 FPS game in a 60 Hz capture: duplicates removed before generating")
            veyra.contentRate = 5
            quick.applyTarget("60")
            compare(veyra.contentRate, 5, "60->30 stays as chosen")
            veyra.contentRate = 0
            quick.applyTarget("off")
            compare(veyra.contentRate, 0, "nothing generated: nothing changed")
        }
        function test_game_fps_recomputes_the_multiplier() {
            quick.applyTarget("120")
            compare(veyra.fgMultiplier, 4)
            quick.setGameFps("60")
            compare(quick.baseFps, 60)
            compare(veyra.fgMultiplier, 2, "60 FPS game to 120: 2x")
            compare(quick.currentTarget, "120")
            quick.setGameFps("40")
            compare(veyra.fgMultiplier, 3, "40 FPS game to 120: 3x")
        }
        function test_target_the_game_already_reaches_turns_generation_off() {
            quick.applyTarget("60")
            verify(veyra.fgEnabled)
            quick.setGameFps("60")
            verify(!veyra.fgEnabled, "nothing to generate for a 60 FPS game at 60")
            const ids = control("pushfps-target").options.map(o => o.id)
            verify(ids.indexOf("60") < 0, "60 FPS is not offered for a 60 FPS game")
            quick.applyTarget("60")
            verify(!veyra.fgEnabled)
        }
        function test_90_fps_target_on_a_90_hz_screen() {
            const seg = control("pushfps-target")
            compare(seg.options.map(o => o.id).join(","), "off,60,120,monitor", "240 Hz: as before")
            veyra.displayRefreshHz = 90
            compare(seg.options.map(o => o.id).join(","), "off,60,90,monitor", "a 90 Hz screen cannot show 120")
            seg.picked("90")
            verify(veyra.fgEnabled)
            compare(veyra.fgMultiplier, 3, "30 FPS x 3")
            compare(veyra.outputRateMode, 2)
            compare(veyra.outputCustomFps, 90)
            compare(quick.currentTarget, "90")
            compare(seg.current, "90")
            verify(quick.targetHint.indexOf("90") >= 0, quick.targetHint)
            compare(veyra.contentRate, 1, "duplicates removed before generating")
            // 40 FPS game: 2X = 80, the largest that does not overshoot; the hint says 80, not "this method's limit".
            quick.setGameFps("40")
            compare(veyra.fgMultiplier, 2)
            compare(veyra.outputCustomFps, 90)
            compare(quick.currentTarget, "90")
            verify(quick.targetHint.indexOf("80") >= 0 && quick.targetHint.indexOf("90") < 0, quick.targetHint)
            // 60 FPS game: 90 would be one and a half frames per frame, so nothing is generated.
            quick.setGameFps("60")
            verify(!veyra.fgEnabled, "no uneven 1.5x")
            compare(seg.options.map(o => o.id).join(","), "off,monitor")
            quick.applyTarget("90")
            verify(!veyra.fgEnabled)
            // Back to a 30 FPS game on another screen: a 90 FPS target that is set stays offered beside 120.
            quick.setGameFps("30")
            quick.applyTarget("90")
            veyra.displayRefreshHz = 144
            compare(seg.options.map(o => o.id).join(","), "off,60,90,120,monitor")
            quick.applyTarget("120")
            compare(seg.options.map(o => o.id).join(","), "off,60,120,monitor")
            // A 120 FPS target carried to a 90 Hz screen stays visible until changed.
            veyra.displayRefreshHz = 90
            compare(seg.options.map(o => o.id).join(","), "off,60,90,120,monitor")
            // 89.9 and 90.1 Hz are 90 Hz screens; 100 Hz is not.
            quick.applyTarget("off")
            veyra.displayRefreshHz = 89.91; verify(quick.screen90)
            veyra.displayRefreshHz = 100; verify(!quick.screen90)
        }
        // A screen that is a whole number of 90s (360 Hz = 4 x 90): 90 FPS is offered beside 120.
        function test_90_fps_target_on_a_screen_that_is_a_multiple_of_90() {
            const seg = control("pushfps-target")
            veyra.displayRefreshHz = 360
            verify(quick.screenFits90 && !quick.screen90)
            compare(seg.options.map(o => o.id).join(","), "off,60,90,120,monitor")
            seg.picked("90")
            verify(veyra.fgEnabled)
            compare(veyra.fgMultiplier, 3, "30 FPS x 3")
            compare(veyra.outputRateMode, 2)
            compare(veyra.outputCustomFps, 90)
            compare(quick.currentTarget, "90")
            // A 60 FPS game is not offered 90 there either.
            quick.setGameFps("60")
            verify(!veyra.fgEnabled, "no uneven 1.5x")
            verify(seg.options.map(o => o.id).indexOf("90") < 0)
            quick.setGameFps("30")
            quick.applyTarget("off")
            // 180 and 270 Hz and their near values count; 144, 165, 240 and 90 itself (its own rule) do not.
            veyra.displayRefreshHz = 179.96; verify(quick.screenFits90)
            veyra.displayRefreshHz = 270; verify(quick.screenFits90)
            veyra.displayRefreshHz = 359.9; verify(quick.screenFits90)
            for (const hz of [60, 90, 120, 144, 165, 175, 240, 280, 0]) { veyra.displayRefreshHz = hz; verify(!quick.screenFits90, String(hz)) }
        }
        // The rows stand in titled sections. A fold changes the section's height and nothing a row says of itself.
        function test_rows_stand_in_sections_that_fold_and_are_remembered() {
            const ids = ["setup", "fg", "picture", "hud", "latency", "start"]
            for (const id of ids) { control("pushfps-section-" + id); verify(quick.sectionOpen(id), id + " is open until folded") }
            // Each row is in the section it belongs to.
            const where = { "pushfps-console-row": "setup", "pushfps-game-fps-row": "setup", "pushfps-target-row": "fg", "pushfps-smooth-row": "fg",
                            "pushfps-enhance-row": "picture", "pushfps-crop-row": "picture", "pushfps-hudlock-row": "hud", "pushfps-hudstill-row": "hud", "pushfps-crosshair-row": "hud",
                            "pushfps-lowest-latency-row": "latency", "pushfps-lipsync-row": "latency", "pushfps-auto-connect-row": "start" }
            for (const row in where) {
                const section = quick.sectionItems.find(s => s.sectionId === where[row])
                verify(section && section.holds(row), row + " in " + where[row])
                for (const other of quick.sectionItems) if (other !== section) verify(!other.holds(row), row + " only once")
            }
            const target = control("pushfps-target-row"), enhance = control("pushfps-enhance-row")
            verify(target.visible && enhance.visible)
            quick.setSectionOpen("picture", false)
            compare(veyra.preferences.pushQuickFolded, "picture", "the fold is a preference")
            verify(!quick.sectionOpen("picture") && quick.sectionOpen("fg"))
            verify(enhance.visible, "a folded row still says of itself that it is there")
            quick.setSectionOpen("hud", false)
            compare(veyra.preferences.pushQuickFolded, "picture,hud")
            // A jump to a row opens the section it stands in, and only that one.
            quick.reveal("pushfps-crop-row")
            verify(quick.sectionOpen("picture") && !quick.sectionOpen("hud"))
            quick.setSectionOpen("hud", true)
            compare(veyra.preferences.pushQuickFolded, "")
            // The title says what is chosen: the console and the game's rate, and the target.
            verify(quick.sectionSummary("setup").indexOf("PlayStation 5") >= 0 && quick.sectionSummary("setup").indexOf("30 FPS") >= 0, quick.sectionSummary("setup"))
            quick.applyTarget("120")
            compare(quick.sectionSummary("fg"), "120 FPS")
            quick.applyTarget("off")
            verify(quick.sectionSummary("fg").length > 0 && quick.sectionSummary("fg") !== "120 FPS")
        }
        // A folded section takes no room, however the fold got there: animated, or in one step (reduced
        // motion, or a frame that took longer than the animation). A layout keeps the size an item has when
        // its implicit height is 0 and nothing else says how high it should be, so a fold that reached 0 in
        // one step stayed as high as it was: the section read as folded and its rows were still drawn.
        function test_a_folded_section_takes_no_room() {
            const fold = control("pushfps-section-fold-picture")
            tryVerify(function() { return fold.height > 100 }, 3000, "open: the rows have their room (" + fold.height + ")")
            const was = Theme.reduced
            Theme.reduced = true
            quick.setSectionOpen("picture", false)
            tryVerify(function() { return fold.height < 0.5 }, 3000, "folded in one step: no room (" + fold.height + ")")
            quick.setSectionOpen("picture", true)
            tryVerify(function() { return fold.height > 100 }, 3000, "opened again in one step (" + fold.height + ")")
            Theme.reduced = was
            quick.setSectionOpen("picture", false)
            tryVerify(function() { return fold.height < 0.5 }, 3000, "folded with its animation: no room (" + fold.height + ")")
            quick.setSectionOpen("picture", true)
            tryVerify(function() { return fold.height > 100 }, 3000, "opened again (" + fold.height + ")")
        }
        // 4.3.1: a capture card connected from the new home page plays there; whoever would rather land in
        // Professional mode says so in the Connecting section. Legacy always goes there and has no such row.
        function test_mouse_and_keyboard_row_switches_and_opens_the_layout() {
            const row = control("pushfps-kbm-row"), sw = control("pushfps-kbm")
            veyra.pushKbm = ({built: true, on: false, available: false, captured: false, target: "", releaseName: "F12", listenControl: -1, listenSlot: -1})
            verify(quick.sectionItems.find(s => s.sectionId === "setup").holds("pushfps-kbm-row"), "it stands in the console and game section")
            verify(!sw.checked, "off until switched on")
            compare(row.hint, qsTr("已关闭"))
            sw.toggled(true)
            const saved = JSON.parse(veyra.preferences.pushKbm)
            verify(saved.on === true && saved.map.cross[0] === 32, "switched on with the GTA 6 keys")
            tryVerify(function() { return sw.checked }, 3000)
            veyra.pushKbm = Object.assign({}, veyra.pushKbm, {available: true})
            compare(row.hint, qsTr("可以开始：点击画面或按 %1").arg("F12"))
            let page = ""
            const opened = function(p) { page = p }
            veyra.navigate.connect(opened)
            control("pushfps-kbm-setup").clicked()
            veyra.navigate.disconnect(opened)
            compare(page, "kbm", "the layout opens in its own window")
            sw.toggled(false)
            verify(JSON.parse(veyra.preferences.pushKbm).on === false)
        }
        function test_professional_mode_on_connecting_is_a_choice_of_the_new_design() {
            const row = control("pushfps-connect-pro-row"), sw = control("pushfps-connect-pro")
            verify(!Theme.modern && !row.visible, "Legacy: no row, it has no picture on its home page")
            Theme.design = "new"
            tryVerify(function() { return row.visible }, 3000, "the new design offers it")
            verify(!sw.checked, "off until chosen: the picture plays on the home page")
            verify(quick.sectionItems.find(s => s.sectionId === "start").holds("pushfps-connect-pro-row"), "it stands in the Connecting section")
            sw.toggled(true)
            compare(veyra.preferences.pushConnectToPro, true)
            tryVerify(function() { return sw.checked }, 3000)
            sw.toggled(false)
            compare(veyra.preferences.pushConnectToPro, false)
            // With full screen on connecting the row says that this decides it.
            const plain = row.hint
            veyra.setPreference("pushAutoFullscreen", true)
            tryVerify(function() { return row.hint !== plain && row.hint.length > 0 }, 3000)
            veyra.setPreference("pushAutoFullscreen", false)
            compare(row.hint, plain)
            // The short form leaves the Connecting section out, this row with it.
            quick.compact = true
            tryVerify(function() { return !row.visible }, 3000)
            quick.compact = false
            Theme.design = "legacy"
            tryVerify(function() { return !row.visible }, 3000)
        }
        // Each page shows a setting once: what the page beside the quick setup has itself is left to that page.
        function test_each_page_shows_a_setting_once() {
            const proOwned = ["pushfps-adaptive-row", "pushfps-pacing-row", "pushfps-hudstill-row", "pushfps-hudlock-row", "pushfps-crosshair-row"]
            for (const row of proOwned) verify(control(row).visible, row + " on its own")
            verify(control("pushfps-compare-clip").visible && control("pushfps-stream").visible)
            // Professional mode: its own list under the quick setup has these switches and the method.
            quick.compactPro = true
            verify(quick.proPage && !quick.streamingPage)
            for (const row of proOwned) verify(!control(row).visible, row + " is left to Professional mode's list")
            verify(!control("pushfps-fg-backend-row").visible)
            verify(control("pushfps-target-row").visible && control("pushfps-enhance-row").visible && control("pushfps-lowest-latency-row").visible, "the rest stays")
            verify(control("pushfps-compare-clip").visible && control("pushfps-stream").visible)
            // Streaming mode: its own Go live and comparison clip buttons stand beside the quick setup.
            quick.compact = true
            verify(quick.streamingPage && !quick.proPage)
            verify(!control("pushfps-compare-clip").visible && !control("pushfps-stream").visible)
            for (const row of proOwned) verify(control(row).visible, row + " in streaming mode, which has no list of its own")
            verify(control("pushfps-optimize").visible && control("pushfps-game-presets").visible && control("pushfps-hud").visible)
            // The home page: everything.
            quick.compactPro = false
            verify(control("pushfps-compare-clip").visible && control("pushfps-stream").visible)
            for (const row of proOwned) verify(control(row).visible)
            quick.compact = false
        }
        // 4.2.1: Professional mode has the quick setup as two cards (the Picture section in its Quality tab,
        // the rest in its Frame Gen tab). A card shows its part and nothing of the other.
        SignalSpy { id: wantedTabs; target: quick; signalName: "requestTab" }
        function test_scope_shows_one_part_of_the_quick_setup() {
            const head = id => control("pushfps-section-" + id)
            const sections = ["setup", "fg", "hud", "latency", "start"]
            compare(quick.scope, "", "everything unless asked")
            verify(control("pushfps-tools-row").visible && head("setup").visible && head("picture").visible)
            verify(control("pushfps-crop-row").visible && control("pushfps-target-row").visible)
            quick.compactPro = true
            quick.scope = "picture"
            verify(!quick.scopeMain && quick.scopePicture)
            verify(!control("pushfps-tools-row").visible, "no tools in the picture card")
            for (const id of sections) verify(!head(id).visible, id + " is not in the picture card")
            verify(!control("pushfps-target-row").visible && !control("pushfps-lowest-latency-row").visible)
            verify(head("picture").visible && control("pushfps-crop-row").visible, "the Picture section is")
            // 4.2.2: the on/off row is in the picture card too. The page's own enhancement card cannot switch
            // the enhancement on before a layer exists; this switch adds the first layer.
            verify(control("pushfps-enhance-row").visible && control("pushfps-enhance").enabled, "the enhancement's own switch stands in the picture card")
            verify(!control("pushfps-picture-moved-row").visible, "nothing to point at from the picture card itself")
            veyra.nrLayers = []; veyra.nrAnyEnabled = false
            const before = veyra.nrLayers.length
            control("pushfps-enhance").toggled(true)
            verify(veyra.nrLayers.length > before || veyra.nrAnyEnabled === true, "switched on with no layer yet: a layer is added")
            verify(control("pushfps-upscale-row").visible, "upscale to my screen stands here too: the page's own upscaler card cannot be switched on for an upscaler the card cannot run")
            veyra.nrAnyEnabled = true
            veyra.nrLayers = [{index: 1, runtime: 2, tone: 1, structure: 1}]
            tryVerify(function() { return control("pushfps-look-row").visible && control("pushfps-fine-row").visible }, 3000, "the look and the fine-tuning stand in the picture card")
            quick.scope = "rest"
            verify(quick.scopeMain && !quick.scopePicture)
            // 4.2.2: the other card says where the picture's settings went and opens that tab.
            verify(control("pushfps-picture-moved-row").visible)
            wantedTabs.clear()
            control("pushfps-picture-moved-open").clicked()
            compare(wantedTabs.count, 1)
            compare(wantedTabs.signalArguments[0][0], "quality")
            verify(control("pushfps-tools-row").visible && head("setup").visible && control("pushfps-target-row").visible)
            verify(!head("picture").visible && !control("pushfps-crop-row").visible && !control("pushfps-look-row").visible && !control("pushfps-fine-row").visible,
                   "nothing of the picture in the other card")
            quick.scope = ""
            verify(control("pushfps-enhance-row").visible && control("pushfps-look-row").visible && control("pushfps-tools-row").visible, "one card again: everything")
            verify(!control("pushfps-picture-moved-row").visible, "one card: nothing moved")
            quick.compactPro = false
            verify(!control("pushfps-picture-moved-row").visible, "the home page never shows the pointer")
            verify(control("pushfps-upscale-row").visible)
        }
        // 4.2.1: upscale to my screen (PushUpscale.js has the rules; tests/js runs them one by one).
        function test_upscale_to_my_screen() {
            const wasChoices = veyra.srBackendChoices, wasQuality = veyra.videoSrQuality, wasTarget = veyra.srTargetIndex, wasHigh = veyra.sourceHeight
            const row = control("pushfps-upscale-row"), sw = control("pushfps-upscale")
            veyra.srEnabled = false
            veyra.sourceHeight = 1080
            quick.screenHighOverride = 2160
            verify(row.visible && sw.enabled && !sw.checked)
            verify(quick.upscaleGains && row.hint.indexOf("1080") >= 0 && row.hint.indexOf("2160") >= 0, "a picture smaller than the screen is said with both sizes: " + row.hint)
            // An NVIDIA card with the standard upscaler: switched on as it is, at 4K for a 4K screen.
            veyra.videoSrQuality = 2; veyra.srTargetIndex = 2
            sw.toggled(true)
            verify(veyra.srEnabled)
            compare(veyra.videoSrQuality, 2, "the upscaler already chosen is kept")
            compare(veyra.srTargetIndex, 2)
            tryVerify(function() { return sw.checked && row.hint.indexOf("RTX") >= 0 }, 3000, "what runs is named: " + row.hint)
            sw.toggled(false)
            verify(!veyra.srEnabled)
            compare(veyra.videoSrQuality, 2, "switching off changes nothing else")
            // An AMD card: the NVIDIA upscalers cannot run, so FSR is taken; a 1440p screen gets a 1440p target.
            veyra.srBackendChoices = [{id: "0", label: "DLSS SR", disabled: true}, {id: "rtx", label: "RTX", disabled: true},
                                      {id: "5", label: "FSR 3.1 / 4"}, {id: "6", label: "NVIDIA Image Scaling (NIS)"}]
            quick.screenHighOverride = 1440
            // The bridge answers "sr" for the upscaler chosen now, which this card cannot run: the row is for
            // any upscaler the card can run, so its switch stays usable.
            const wasCaps = veyra.effectCapabilities
            veyra.effectCapabilities = Object.assign({}, wasCaps, {sr: {available: false, reason: "needs NVIDIA"}})
            verify(quick.upscalePossible && sw.enabled, "usable on a card that cannot run the upscaler chosen now")
            verify(row.hint.indexOf("needs NVIDIA") < 0, "and not explained away with that upscaler's reason: " + row.hint)
            sw.toggled(true)
            verify(veyra.srEnabled)
            compare(veyra.videoSrQuality, 5, "FSR on a card without the NVIDIA upscalers")
            veyra.effectCapabilities = wasCaps
            compare(veyra.srTargetIndex, 1, "1440p for a 1440p screen")
            // The FSR version really running is read while it runs and said.
            veyra.liveStats = ({running: true, fsrUpscalerVersion: "4.1.1"})
            tryVerify(function() { return row.hint.indexOf("FSR 4.1.1") >= 0 }, 5000, "the running FSR version is said: " + row.hint)
            veyra.srEnabled = false
            tryVerify(function() { return quick.fsrUpscalerVersion === "" }, 3000, "and forgotten when it stops")
            // A larger size chosen in Professional mode is left alone.
            veyra.srTargetIndex = 3
            sw.toggled(true)
            compare(veyra.srTargetIndex, 3)
            veyra.srEnabled = false
            // No upscaler at all: the switch cannot be turned on, and nothing is set.
            veyra.srBackendChoices = [{id: "0", label: "DLSS SR", disabled: true}, {id: "6", label: "NIS", disabled: true, note: "needs a hardware card"}]
            verify(!quick.upscalePossible && !sw.enabled)
            compare(row.hint, "needs a hardware card", "why nothing can run is said")
            quick.setUpscale(true)
            verify(!veyra.srEnabled, "nothing to run: nothing switched on")
            // A picture as large as the screen: nothing is promised.
            veyra.srBackendChoices = wasChoices
            veyra.sourceHeight = 2160; quick.screenHighOverride = 2160
            verify(!quick.upscaleGains && row.hint.indexOf("2160") < 0)
            // Professional mode's page has the upscaler's own card, and this row beside it: that card's
            // switch is greyed out while the upscaler chosen is one the graphics card cannot run.
            quick.compactPro = true
            verify(row.visible)
            quick.compactPro = false
            veyra.videoSrQuality = wasQuality; veyra.srTargetIndex = wasTarget; veyra.sourceHeight = wasHigh
            veyra.srEnabled = false
            quick.screenHighOverride = 0
        }
        // 4.2.1: the split comparison's sides can be swapped.
        function test_comparison_sides_are_swapped_with_a_button() {
            veyra.nrAnyEnabled = true
            veyra.nrLayers = [{index: 1, runtime: 2, tone: 1, structure: 1}]
            const row = control("pushfps-look-compare-row"), swap = control("pushfps-look-compare-swap")
            tryVerify(function() { return row.visible }, 3000)
            verify(!swap.visible, "nothing to swap while nothing is compared")
            const sidesBefore = row.hint
            quick.setLookCompare(true)
            compare(veyra.compareMode, 2)
            tryVerify(function() { return swap.visible }, 3000, "the button comes with the split")
            swap.clicked()
            compare(veyra.preferences.pushCompareSwap, true)
            verify(quick.compareSwapped)
            quick.setLookCompare(false)
            compare(veyra.compareMode, 0)
            verify(swap.visible, "a swap is never left behind unseen")
            verify(row.hint !== sidesBefore, "the row says which side is which: " + row.hint)
            swap.clicked()
            compare(veyra.preferences.pushCompareSwap, false)
            tryVerify(function() { return !swap.visible && row.hint === sidesBefore }, 3000)
        }
        function test_90_fps_target_in_profiles_and_adaptive() {
            veyra.displayRefreshHz = 90
            quick.applyTarget("90")
            verify(quick.saveGame("Ninety"))
            compare(quick.games[0].target, "90")
            quick.applyTarget("off")
            quick.applyGame("Ninety")
            compare(quick.currentTarget, "90")
            compare(veyra.fgMultiplier, 3)
            quick.setAdaptive(true)
            compare(veyra.fgMultiplier, 4, "adaptive ceiling: a 20 FPS dip needs 4X to stay near 90")
            compare(veyra.outputCustomFps, 90)
            quick.setAdaptive(false)
            compare(veyra.fgMultiplier, 3)
            quick.deleteGame("Ninety")
        }
        // --- fine-tuning the enhancement's change ----------------------------------
        readonly property var fineIds: ["total", "brighten", "darken", "maxBrighten", "maxDarken", "shadows", "midtones", "highlights", "color", "hue"]
        function test_fine_tune_is_folded_until_asked_for() {
            verify(!control("pushfps-fine-row").visible, "nothing to tune while the enhancement is off")
            veyra.nrAnyEnabled = true
            verify(control("pushfps-fine-row").visible)
            verify(findChild(quick, "pushfps-fine-total") === null, "folded")
            verify(quick.fineNeutral)
            verify(!control("pushfps-fine-reset").visible)
            control("pushfps-fine-toggle").clicked()
            compare(veyra.preferences.pushFineTuneOpen, true)
            for (const id of fineIds) {
                const slider = control("pushfps-fine-" + id)
                const limit = id === "maxBrighten" || id === "maxDarken"
                compare(slider.to, limit ? 1 : 2, id)
                compare(slider.value, limit ? 0 : 1, id + " starts where it changes nothing")
                compare(slider.defaultValue, limit ? 0 : 1, id)
                verify(control("pushfps-fine-row-" + id).visible)
            }
            compare(control("pushfps-fine-row-total").value, "100%")
            verify(control("pushfps-fine-row-maxBrighten").value.indexOf("%") < 0, "no limit is said in words")
            control("pushfps-fine-toggle").clicked()
            compare(veyra.preferences.pushFineTuneOpen, false)
            verify(findChild(quick, "pushfps-fine-total") === null, "folded again")
            // Unfolded and the enhancement goes off: the sliders go with it.
            veyra.setPreference("pushFineTuneOpen", true)
            verify(findChild(quick, "pushfps-fine-total") !== null)
            veyra.nrAnyEnabled = false
            verify(findChild(quick, "pushfps-fine-total") === null)
        }
        function test_fine_tune_strength_and_direction_reach_every_layer() {
            veyra.nrLayers = [{index: 1}, {index: 4, total: 0.5, brighten: 1.4}]
            veyra.nrAnyEnabled = true
            veyra.setPreference("pushFineTuneOpen", true)
            control("pushfps-fine-total").moved(2)
            compare(veyra.nrLayers[0].total, 2); compare(veyra.nrLayers[1].total, 2)
            control("pushfps-fine-brighten").moved(0.78)
            compare(veyra.nrLayers[0].brighten, 0.78); compare(veyra.nrLayers[1].brighten, 0.78)
            control("pushfps-fine-darken").moved(0.789)
            compare(veyra.nrLayers[0].darken, 0.79, "two decimals")
            control("pushfps-fine-color").moved(0.59)
            compare(veyra.nrLayers[1].color, 0.59)
            compare(quick.fineValue("color"), 0.59)
            compare(control("pushfps-fine-row-total").value, "200%")
            verify(!quick.fineNeutral)
            verify(control("pushfps-fine-reset").visible)
            compare(veyra.preferences.pushShapeShadows, undefined, "the layer values are not preferences")
            // Professional mode allows a strength of 5: shown at the slider's end, not changed.
            veyra.nrLayers = [{index: 1, total: 5}]
            compare(quick.fineValue("total"), 2)
            compare(veyra.nrLayers[0].total, 5)
            // Out of range, not a number, not a control.
            verify(quick.setFine("total", 9)); compare(veyra.nrLayers[0].total, 2)
            verify(quick.setFine("total", -1)); compare(veyra.nrLayers[0].total, 0)
            verify(!quick.setFine("total", NaN)); compare(veyra.nrLayers[0].total, 0)
            verify(!quick.setFine("nothing", 1))
            veyra.nrLayers = []
            verify(!quick.fineShown)
            verify(!quick.setFine("total", 1), "no layer to set")
        }
        function test_fine_tune_ranges_hue_and_limits_are_live_preferences() {
            veyra.nrAnyEnabled = true
            veyra.setPreference("pushFineTuneOpen", true)
            control("pushfps-fine-shadows").moved(1.11)
            control("pushfps-fine-midtones").moved(1.5)
            control("pushfps-fine-highlights").moved(0.28)
            control("pushfps-fine-hue").moved(0.43)
            control("pushfps-fine-maxBrighten").moved(0.1)
            control("pushfps-fine-maxDarken").moved(0.05)
            compare(veyra.preferences.pushShapeShadows, 111)
            compare(veyra.preferences.pushShapeMidtones, 150)
            compare(veyra.preferences.pushShapeHighlights, 28)
            compare(veyra.preferences.pushShapeHue, 43)
            compare(veyra.preferences.pushShapeMaxBrighten, 10)
            compare(veyra.preferences.pushShapeMaxDarken, 5)
            compare(control("pushfps-fine-row-shadows").value, "111%")
            compare(control("pushfps-fine-row-maxBrighten").value, "10%")
            fuzzyCompare(control("pushfps-fine-hue").value, 0.43, 0.0001)
            compare(veyra.nrLayers[0].total, undefined, "the preferences do not touch the layers")
            verify(!quick.fineNeutral)
            // Reset: everything back to where it changes nothing, layer values included.
            control("pushfps-fine-total").moved(1.6)
            control("pushfps-fine-reset").clicked()
            verify(quick.fineNeutral)
            compare(veyra.preferences.pushShapeShadows, 100); compare(veyra.preferences.pushShapeHue, 100)
            compare(veyra.preferences.pushShapeMaxBrighten, 0); compare(veyra.preferences.pushShapeMaxDarken, 0)
            compare(veyra.nrLayers[0].total, 1)
            verify(!control("pushfps-fine-reset").visible)
            // A limit cannot pass 100 %, a range not 200 %.
            verify(quick.setFine("maxBrighten", 3)); compare(veyra.preferences.pushShapeMaxBrighten, 100)
            verify(quick.setFine("shadows", 3)); compare(veyra.preferences.pushShapeShadows, 200)
        }
        function test_fine_tune_is_kept_per_game_and_console_profile() {
            veyra.nrAnyEnabled = true
            quick.setFine("total", 1.5); quick.setFine("shadows", 0.4); quick.setFine("maxDarken", 0.2); quick.setFine("hue", 0)
            verify(quick.saveGame("Tuned"))
            compare(quick.games[0].look.fine.total, 1.5)
            compare(quick.games[0].look.fine.shadows, 0.4)
            compare(quick.games[0].look.fine.hue, 0)
            quick.resetFine()
            verify(quick.fineNeutral)
            quick.applyGame("Tuned")
            compare(veyra.nrLayers[0].total, 1.5)
            compare(veyra.preferences.pushShapeShadows, 40)
            compare(veyra.preferences.pushShapeMaxDarken, 20)
            compare(veyra.preferences.pushShapeHue, 0)
            compare(quick.fineValue("midtones"), 1, "what was left alone stays alone")
            // Each console keeps its own.
            quick.switchConsole("xbox")
            quick.resetFine()
            quick.switchConsole("ps5")
            compare(quick.fineValue("total"), 1.5, "PS5 tuning restored")
            quick.switchConsole("xbox")
            verify(quick.fineNeutral, "Xbox tuning restored")
            // A profile from before 3.2.2 (a look without fine-tuning, or no look) leaves it alone; junk is ignored.
            quick.setFine("total", 0.7)
            quick.applySnapshot({fps: 30, target: "off", enhance: true, look: {tone: 1, structure: 1}})
            quick.applySnapshot({fps: 30, target: "off", enhance: true})
            quick.applySnapshot({fps: 30, target: "off", enhance: true, look: {tone: 1, structure: 1, fine: {total: "x", shadows: null, other: 3}}})
            quick.applySnapshot({fps: 30, target: "off", enhance: true, look: {tone: 1, structure: 1, fine: 7}})
            compare(veyra.nrLayers[0].total, 0.7)
            // A value already in place is not written again (layers given different values in Professional mode stay different).
            veyra.nrLayers = [{index: 1, total: 0.7}, {index: 4, total: 1.9}]
            quick.applySnapshot({fps: 30, target: "off", enhance: true, look: {tone: 1, structure: 1, fine: {total: 0.7}}})
            compare(veyra.nrLayers[1].total, 1.9)
            quick.deleteGame("Tuned")
        }
        function test_fine_tune_shows_for_every_enhancement_runtime() {
            veyra.nrLayers = [{index: 1, runtime: 4}]
            veyra.nrAnyEnabled = true
            verify(!quick.lookShown, "the AMD runtime has no look values")
            verify(control("pushfps-fine-row").visible, "but its change is composed the same way")
            verify(quick.setFine("total", 0.5))
            compare(veyra.nrLayers[0].total, 0.5)
        }
        function test_fill_row_for_a_screen_wider_than_the_picture() {
            const row = control("pushfps-fill-row"), seg = control("pushfps-fill")
            verify(!row.visible, "not on the test screen")
            quick.screenAspectOverride = 16 / 9
            verify(!row.visible, "a 16:9 screen has nothing to fill")
            quick.screenAspectOverride = 3440 / 1440
            verify(row.visible, "a 21:9 screen")
            compare(seg.options.map(o => o.id).join(","), "fit,ambient,crop,stretch")
            compare(seg.current, "fit")
            seg.picked("ambient")
            compare(veyra.aspectMode, 0, "ambient fill is Fit with the bars filled")
            compare(veyra.preferences.pushAmbientFill, true)
            compare(seg.current, "ambient")
            seg.picked("crop")
            compare(veyra.aspectMode, 2)
            compare(veyra.preferences.pushAmbientFill, false, "one choice at a time")
            compare(seg.current, "crop")
            compare(quick.fillCropPercent, 26, "a 16:9 picture on 3440 x 1440 loses a quarter of its height")
            verify(quick.fillHint.indexOf("26") >= 0, quick.fillHint)
            seg.picked("stretch")
            compare(veyra.aspectMode, 3)
            compare(quick.fillStretchPercent, 34)
            verify(quick.fillHint.indexOf("34") >= 0, quick.fillHint)
            seg.picked("fit")
            compare(veyra.aspectMode, 0)
            compare(veyra.preferences.pushAmbientFill, false)
            compare(seg.current, "fit")
            quick.setFill("something else")
            compare(veyra.aspectMode, 0)
            // 32:9: half the picture would be cut.
            quick.screenAspectOverride = 32 / 9
            compare(quick.fillCropPercent, 50)
            compare(quick.fillStretchPercent, 100)
            // XeSS presents on its own path: no ambient fill there, and a saved choice reads as Fit.
            seg.picked("ambient")
            veyra.presentationOwned = true
            compare(seg.options.map(o => o.id).join(","), "fit,crop,stretch")
            compare(seg.current, "fit")
            quick.setFill("ambient")
            compare(seg.current, "fit")
            veyra.presentationOwned = false
            compare(seg.current, "ambient")
            // Back on a 16:9 screen the row stays while a choice other than Fit is on, so it can be undone.
            quick.screenAspectOverride = 16 / 9
            verify(row.visible, "ambient fill is still on")
            seg.picked("fit")
            verify(!row.visible)
            veyra.aspectMode = 2
            verify(row.visible && seg.current === "crop")
            verify(quick.fillHint.length > 0 && quick.fillHint.indexOf("%") < 0, "no made-up percentage without a wide screen")
            // A ratio forced in Professional mode is not one of the four: shown as set there.
            veyra.aspectMode = 6
            verify(row.visible)
            compare(seg.current, "")
            seg.picked("fit")
            compare(veyra.aspectMode, 0)
            // The home page's short form shows it too; streaming mode (short form beside its own preview) does not.
            quick.screenAspectOverride = 3440 / 1440
            quick.compact = true
            verify(row.visible)
            quick.compactPro = true
            verify(!row.visible)
            quick.compact = false; quick.compactPro = false
        }
        function test_console_profiles_keep_their_own_choices() {
            compare(quick.consoleId, "ps5")
            quick.applyTarget("120")
            quick.setEnhancement(true)
            quick.switchConsole("xbox")
            compare(quick.consoleId, "xbox")
            quick.applyTarget("off")
            quick.setEnhancement(false)
            quick.switchConsole("ps5")
            compare(quick.currentTarget, "120", "PS5 profile restored")
            verify(veyra.nrAnyEnabled)
            quick.switchConsole("xbox")
            compare(quick.currentTarget, "off", "Xbox profile restored")
            verify(!veyra.nrAnyEnabled)
            verify(quick.setupTip.indexOf("Xbox") === 0)
        }
        function test_game_profiles_save_apply_delete() {
            quick.setGameFps("30")
            quick.applyTarget("120")
            verify(quick.saveGame("  GTA VI  "))
            compare(quick.games.length, 1)
            compare(quick.activeGame, "GTA VI")
            compare(quick.games[0].console, "ps5")
            compare(quick.games[0].preset, "", "no saved preset matched")
            quick.applyTarget("off")
            quick.setGameFps("60")
            quick.applyGame("GTA VI")
            compare(quick.baseFps, 30)
            compare(quick.currentTarget, "120")
            compare(veyra.fgMultiplier, 4)
            // Saving the same name replaces it.
            verify(quick.saveGame("GTA VI"))
            compare(quick.games.length, 1)
            quick.deleteGame("GTA VI")
            compare(quick.games.length, 0)
            compare(quick.activeGame, "")
        }
        function test_a_chosen_profile_keeps_what_is_changed_afterwards() {
            quick.setGameFps("30")
            quick.applyTarget("120")
            verify(quick.saveGame("Racer"))
            quick.keepsProfiles = true
            // A change made while the profile is chosen is written back to it (after a short wait).
            quick.applyTarget("60")
            tryVerify(function() { return quick.games.length === 1 && quick.games[0].target === "60" }, 5000)
            compare(quick.games[0].name, "Racer")
            compare(quick.games[0].console, "ps5")
            compare(quick.profiles.consoles.ps5.target, "60", "the console's own profile follows too")
            // Another choice in quick succession: what was waiting is kept first.
            quick.applyTarget("off")
            quick.applyGame("")
            compare(quick.games[0].target, "off")
            compare(quick.activeGame, "")
            // With no game chosen, the game profile is left alone.
            quick.applyTarget("120")
            tryVerify(function() { return quick.profiles.consoles.ps5.target === "120" }, 8000)
            compare(quick.games[0].target, "off")
            // A target set by hand in Professional mode (none of the quick choices) keeps the profile's own.
            quick.applyGame("Racer")
            veyra.outputCustomFps = 75
            veyra.fgEnabled = true; veyra.fgMultiplier = 3; veyra.outputRateMode = 2
            compare(quick.currentTarget, "")
            wait(4800)
            compare(quick.games[0].target, "off")
            quick.keepsProfiles = false
            quick.deleteGame("Racer")
        }
        function test_game_profile_applies_its_preset() {
            veyra.presets = [{index: 0, name: "Default"}, {index: 3, name: "Sharp"}]
            veyra.currentPresetName = "Sharp"
            verify(quick.saveGame("Forza"))
            compare(quick.games[0].preset, "Sharp")
            quick.applyGame("Forza")
            compare(veyra.lastAppliedPreset, 3)
            // A preset that was deleted since is skipped, the rest still applies.
            veyra.presets = [{index: 0, name: "Default"}]
            veyra.lastAppliedPreset = -1
            quick.applyGame("Forza")
            compare(veyra.lastAppliedPreset, -1)
            compare(quick.activeGame, "Forza")
        }
        function test_bad_input_is_harmless() {
            verify(!quick.saveGame("   "))
            verify(quick.saveGame("x".repeat(80)))
            compare(quick.games[0].name.length, 40)
            veyra.preferences = ({pushProfiles: "{not json", pushConsole: "gamecube", pushGameFps: 999})
            compare(quick.games.length, 0)
            compare(quick.consoleId, "ps5")
            compare(quick.baseFps, 30)
            quick.applyGame("missing")
            compare(quick.activeGame, "")
        }
        // --- adaptive multiplier -------------------------------------------------
        function test_adaptive_makes_the_multiplier_a_ceiling() {
            quick.applyTarget("60")
            compare(veyra.fgMultiplier, 2)
            control("pushfps-adaptive").toggled(true)
            verify(veyra.fgAdaptive)
            compare(veyra.contentRate, 1, "duplicate removal on: the game's frame rate is measured")
            compare(veyra.fgMultiplier, 3, "60 FPS target: room for a dip to 20 FPS")
            quick.applyTarget("120")
            compare(veyra.fgMultiplier, 4, "the largest offered (4X) for 120 FPS")
            compare(veyra.outputCustomFps, 120)
            verify(quick.targetHint.length > 0)
            control("pushfps-adaptive").toggled(false)
            verify(!veyra.fgAdaptive)
            compare(veyra.fgMultiplier, 4, "back to the fixed choice: 30 FPS x 4 = 120")
            quick.applyTarget("60")
            compare(veyra.fgMultiplier, 2)
        }
        function test_adaptive_keeps_a_chosen_duplicate_mode() {
            veyra.contentRate = 5
            quick.applyTarget("120")
            quick.setAdaptive(true)
            compare(veyra.contentRate, 5, "60->30 stays as chosen")
        }
        function test_adaptive_needs_more_than_2x() {
            veyra.fgMultiplierChoices = [{id:"1"},{id:"2"}]
            quick.applyTarget("60")
            verify(!quick.adaptivePossible)
            verify(!control("pushfps-adaptive").enabled)
            veyra.fgAdaptive = true
            verify(!quick.adaptive, "a stored on means nothing without a choice above 2X")
        }
        function test_adaptive_live_hint() {
            quick.applyTarget("120")
            quick.setAdaptive(true)
            veyra.liveStats = ({running: true, fgMultiplierNow: 3, contentFps: 38})
            quick.liveStats = veyra.pushLiveStats()
            const row = control("pushfps-adaptive-row")
            compare(row.hint, qsTr("现在 %1×：游戏 %2 FPS → 约 %3 FPS").arg(3).arg(38).arg(114))
        }
        function test_profiles_remember_adaptive() {
            quick.applyTarget("120")
            quick.setAdaptive(true)
            verify(quick.saveGame("Elden"))
            quick.setAdaptive(false)
            quick.applyGame("Elden")
            verify(veyra.fgAdaptive, "the game profile brings adaptive back")
        }
        // --- Frame Pacing Repair and Live HUD Lock ----------------------------------
        function test_pacing_repair_turns_on_duplicate_removal() {
            quick.applyTarget("120")
            control("pushfps-pacing").toggled(true)
            verify(veyra.pacingRepair)
            compare(veyra.contentRate, 1, "the repair re-times what duplicate removal finds")
            control("pushfps-pacing").toggled(false)
            verify(!veyra.pacingRepair)
        }
        function test_pacing_live_hint() {
            quick.applyTarget("120")
            quick.setPacingRepair(true)
            const row = control("pushfps-pacing-row")
            quick.lastPacing = { pacingFrames: 100, pacingRepaired: 50 }
            veyra.liveStats = ({ running: true, pacingFrames: 130, pacingRepaired: 77 })
            // One timer tick, as the real one does.
            const st = veyra.pushLiveStats(), p = quick.lastPacing
            quick.pacingShare = (st.pacingRepaired - p.pacingRepaired) / (st.pacingFrames - p.pacingFrames)
            compare(row.hint, qsTr("正在修复：%1% 的画面重新对齐").arg(90))
            quick.pacingShare = 0
            compare(row.hint, qsTr("这款游戏的节奏已经均匀，无需修复"))
        }
        function test_hud_lock_switch() {
            quick.applyTarget("120")
            control("pushfps-hudlock").toggled(true)
            verify(veyra.hudLock)
            verify(control("pushfps-hudlock").checked)
            control("pushfps-hudlock").toggled(false)
            verify(!veyra.hudLock)
        }
        function test_hud_lock_reports_what_runs() {
            quick.applyTarget("120")
            veyra.hudLock = true
            quick.liveStats = ({ hudLockState: 2 })
            compare(control("pushfps-hudlock-row").hint, qsTr("实时 HUD 锁定没能在这台电脑上启动，补帧照常运行（原因见日志）"))
            quick.liveStats = ({ hudLockState: 1 })
            compare(control("pushfps-hudlock-row").hint, qsTr("正在运行：文字和准星在补出来的帧里保持清晰"))
            veyra.hudLock = false
            quick.liveStats = ({})
        }
        function test_own_tech_is_off_with_xess() {
            quick.applyTarget("120")
            veyra.presentationOwned = true
            verify(!control("pushfps-pacing").enabled)
            verify(!control("pushfps-hudlock").enabled)
            compare(control("pushfps-pacing-row").hint, qsTr("XeSS 自己控制节奏，不能同时使用"))
            veyra.presentationOwned = false
        }
        function test_profiles_remember_pacing_and_hud_lock() {
            quick.applyTarget("120")
            quick.setPacingRepair(true)
            veyra.hudLock = true
            verify(quick.saveGame("Bloodborne"))
            quick.setPacingRepair(false)
            veyra.hudLock = false
            quick.applyGame("Bloodborne")
            verify(veyra.pacingRepair && veyra.hudLock)
        }
        function test_rows_are_shown() {
            for (const n of ["pushfps-console-row", "pushfps-game-row", "pushfps-game-fps-row",
                             "pushfps-target-row", "pushfps-adaptive-row", "pushfps-pacing-row", "pushfps-hudlock-row", "pushfps-enhance-row", "pushfps-smooth-row", "pushfps-low-latency-row", "pushfps-lowest-latency-row"])
                verify(control(n).visible, n)
            verify(quick.height > 0)
        }
        function test_optimise_tile_is_green_only_after_optimising() {
            const tile = control("pushfps-optimize")
            veyra.setPreference("pushBenchmark", "")
            verify(!tile.primary, "grey before Optimise for my PC has run (green would look already on)")
            veyra.setPreference("pushBenchmark", JSON.stringify({ v: 3, applied: false, best: "" }))
            verify(!tile.primary, "grey when nothing was applied or it was undone")
            veyra.setPreference("pushBenchmark", JSON.stringify({ v: 3, applied: true, best: "VFG 4X" }))
            verify(tile.primary, "green while the optimised settings are kept")
            veyra.setPreference("pushBenchmark", "{not json")
            verify(!tile.primary)
        }
        // A 60 FPS target and smooth pacing together (3.1.6): turning it on keeps the cap, the switch
        // reads on with the cap, and turning it off keeps the target.
        function test_cadence_hint_offers_60_to_30() {
            veyra.setPreference("pushGameFps", 30)
            veyra.fgEnabled = true; veyra.contentRate = 1
            quick.liveStats = ({ running: true, captureFps: 60, contentFps: 59.9 })
            verify(quick.cadenceMismatch, "30 FPS set, 60 measured in a 60 Hz capture")
            control("pushfps-cadence-fix").clicked()
            compare(veyra.contentRate, 5, "fixed 60->30")
            verify(!quick.cadenceMismatch)
            quick.liveStats = ({ running: true, captureFps: 60, contentFps: 30 })
            veyra.contentRate = 1
            verify(!quick.cadenceMismatch, "no hint when 30 is measured")
            quick.liveStats = ({})
            veyra.fgEnabled = false
        }
        function test_missing_enhancement_files_say_how_to_get_them() {
            const row = control("pushfps-enhance-row")
            const before = veyra.effectCapabilities
            const how = "Install-PushFPS.cmd"
            // The card could run it, the files were left out at installation: the reason and the way to get them.
            veyra.effectCapabilities = ({ nr: { available: false, reason: "missing files", missingFiles: true } })
            verify(!quick.nrPossible)
            verify(row.hint.indexOf("missing files") === 0, "the reason comes first: " + row.hint)
            verify(row.hint.indexOf(how) > 0, "then how to install the files")
            verify(!control("pushfps-enhance").enabled, "the switch stays off until the files are there")
            // A card that cannot run it at all: only the reason. Nothing to install would help.
            veyra.effectCapabilities = ({ nr: { available: false, reason: "needs another card", missingFiles: false } })
            compare(row.hint, "needs another card")
            veyra.effectCapabilities = ({ nr: { available: false, reason: "needs another card" } })
            compare(row.hint, "needs another card", "also when the flag is not there at all")
            // Possible: the normal hint, without the installer.
            veyra.effectCapabilities = before
            verify(quick.nrPossible)
            verify(row.hint.indexOf(how) < 0)
        }
        function test_missing_enhancement_files_are_installed_with_one_button() {
            const row = control("pushfps-enhance-row")
            const button = control("pushfps-enhance-install")
            const before = veyra.effectCapabilities
            const asked = veyra.updates.extrasAsked
            veyra.effectCapabilities = ({ nr: { available: false, reason: "missing files", missingFiles: true } })
            // Not an installed copy that can repeat its install (portable, or the extras are there): the old words.
            veyra.updates.canInstallExtras = false
            verify(!row.extrasOffered, "no button without an install to repeat")
            verify(row.hint.indexOf("Install-PushFPS.cmd") > 0)
            veyra.updates.canInstallExtras = true
            verify(row.extrasOffered)
            verify(button.enabled)
            verify(row.hint.indexOf("Install-PushFPS.cmd") < 0, "the button says it: " + row.hint)
            button.clicked()
            compare(veyra.updates.extrasAsked, asked + 1)
            veyra.updates.installState = "downloading"
            veyra.updates.installProgress = 0.42
            verify(!button.enabled, "one install at a time")
            verify(row.hint.indexOf("42") >= 0, "the progress is shown: " + row.hint)
            veyra.updates.installState = "idle"
            veyra.updates.installProgress = 0
            // A card that cannot run it: no button, whatever the installer could do.
            veyra.effectCapabilities = ({ nr: { available: false, reason: "needs another card", missingFiles: false } })
            verify(!row.extrasOffered)
            veyra.effectCapabilities = before
            verify(!row.extrasOffered, "nothing to install once it runs")
            veyra.updates.canInstallExtras = false
        }
        function test_frame_generation_that_shows_no_frames_is_said_with_its_cause() {
            // engine/PushFgHealth.h reports it through pushLiveStats: 2 the method is not running,
            // 3 the card does not keep up, 4 nothing generated is shown.
            const row = findChild(quick, "pushfps-fg-idle-row")
            verify(row !== null)
            veyra.fgEnabled = true
            veyra.liveStats = ({ running: true, fgIdle: 0, fgIdleText: "" })
            quick.readFgHealth()
            compare(quick.fgIdle, 0)
            verify(!row.visible, "nothing is shown while frames are generated")
            for (const cause of [2, 3, 4]) {
                veyra.liveStats = ({ running: true, fgIdle: cause, fgIdleText: "cause " + cause })
                quick.readFgHealth()
                compare(quick.fgIdle, cause)
                verify(row.visible, "cause " + cause + " is shown")
                compare(row.hint, "cause " + cause, "with the bridge's own words")
            }
            // The row's button copies the performance report to send along.
            const copies = veyra.performanceReportCopies
            control("pushfps-fg-idle-report").clicked()
            compare(veyra.performanceReportCopies, copies + 1)
            verify(String(veyra.lastCopiedText).indexOf("pushfps-performance") >= 0)
            // A target equal to the game's own rate is a setting, not a fault.
            veyra.liveStats = ({ running: true, fgIdle: 1, fgIdleText: "" })
            quick.readFgHealth()
            verify(!row.visible, "the same rate is not shown as a fault")
            // Gone as soon as frames come out, when nothing plays, and with frame generation off.
            veyra.liveStats = ({ running: true, fgIdle: 4, fgIdleText: "nothing" })
            quick.readFgHealth()
            verify(row.visible)
            veyra.liveStats = ({ running: true, fgIdle: 0 })
            quick.readFgHealth()
            verify(!row.visible, "gone when frames are generated again")
            veyra.liveStats = ({ running: false, fgIdle: 4, fgIdleText: "nothing" })
            quick.readFgHealth()
            verify(!row.visible, "nothing playing: nothing to say")
            veyra.liveStats = ({ running: true, fgIdle: 4, fgIdleText: "nothing" })
            quick.readFgHealth()
            verify(row.visible)
            veyra.fgEnabled = false
            quick.readFgHealth()
            verify(!row.visible, "frame generation off: nothing to say")
            compare(quick.fgIdleText, "")
            veyra.liveStats = ({ running: false })
        }
        function test_smooth_frame_pacing_keeps_a_target() {
            veyra.displaySync = 2; veyra.variableRefresh = false
            veyra.outputCustomFps = 60; veyra.outputRateMode = 2
            const sw = findChild(quick, "pushfps-smooth")
            verify(!sw.checked)
            sw.toggled(true)
            compare(veyra.displaySync, 1)
            compare(veyra.outputRateMode, 2, "the 60 FPS target stays")
            verify(sw.checked, "on with a target")
            verify(!quick.unevenTarget, "60 divides 240 Hz")
            veyra.displayRefreshHz = 144
            verify(quick.unevenTarget, "60 on 144 Hz without VRR is uneven")
            // A target the screen cannot show (the window moved to a slower screen): the output is the
            // screen's refresh rate, the row says so, and the target stays chosen.
            const row = findChild(quick, "pushfps-smooth-row")
            veyra.outputCustomFps = 120; veyra.displayRefreshHz = 60
            verify(quick.screenLimitedTarget && !quick.unevenTarget, "120 on a 60 Hz screen is fitted, not uneven")
            verify(row.hint.indexOf("60") >= 0 && row.hint.indexOf("120") >= 0, "the row says what is put out and what comes back")
            verify(veyra.outputRateMode === 2 && veyra.outputCustomFps === 120, "the target stays as chosen: nothing is written")
            veyra.displayRefreshHz = 144
            verify(!quick.screenLimitedTarget, "back on a screen that can show it")
            veyra.displayRefreshHz = 59.94; veyra.outputCustomFps = 60
            verify(!quick.screenLimitedTarget, "60 on a 59.94 Hz screen fits")
            veyra.outputCustomFps = 60
            veyra.displayRefreshHz = 240
            sw.toggled(false)
            compare(veyra.displaySync, 2)
            compare(veyra.outputRateMode, 2, "off keeps the target")
            veyra.outputRateMode = 0
        }
        function test_smooth_frame_pacing() {
            veyra.displaySync = 2; veyra.variableRefresh = false; veyra.outputRateMode = 0
            const sw = findChild(quick, "pushfps-smooth")
            verify(sw, "smooth frame pacing switch exists")
            verify(!sw.checked, "off while display sync is Automatic")
            sw.toggled(true)
            compare(veyra.displaySync, 1, "on: V-Sync")
            compare(veyra.variableRefresh, true, "on: variable refresh requested")
            compare(veyra.outputRateMode, 1, "on: output capped at the display")
            compare(veyra.preferences.pushSmoothPacing, true)
            verify(sw.checked)
            sw.toggled(false)
            compare(veyra.displaySync, 2, "off: back to Automatic")
            compare(veyra.variableRefresh, false)
            compare(veyra.outputRateMode, 0, "off: no output cap")
            compare(veyra.preferences.pushSmoothPacing, false)
        }
        // G-SYNC set-up row: shown while the driver has not confirmed variable refresh; the button checks
        // the driver profile and repairs it when that is possible.
        function test_gsync_row_checks_then_repairs() {
            const tools = gsyncMock.createObject(root)
            veyra.nvidiaVrrProfile = tools
            veyra.hasSource = true
            veyra.displaySync = 1; veyra.variableRefresh = true; veyra.outputRateMode = 1
            veyra.vrrState = 3
            const row = findChild(root, "pushfps-gsync-row"), button = findChild(root, "pushfps-gsync-fix")
            verify(row && button, "the row exists")
            tryVerify(() => row.visible, 2000, "shown while variable refresh is requested but not confirmed")
            button.clicked()
            compare(tools.checks, 1, "the button starts with a check")
            tools.canRepair = true; tools.busy = false; tools.changed()
            compare(tools.repairs, 1, "a profile that can be set is repaired")
            tools.canRepair = false; tools.changed()
            tryVerify(() => !row.visible, 2000, "nothing left to set: the row goes")
            compare(veyra.preferences.pushGsyncChecked, true, "and that is remembered")
            veyra.vrrState = 0; veyra.displaySync = 2; veyra.variableRefresh = false
            veyra.nvidiaVrrProfile = null
            tools.destroy()
        }

        // FreeSync: a level answered by the AMD driver never offers NVIDIA's G-SYNC set-up.
        function test_gsync_row_is_not_offered_for_amd_freesync() {
            const tools = gsyncMock.createObject(root)
            veyra.nvidiaVrrProfile = tools
            veyra.hasSource = true
            veyra.displaySync = 1; veyra.variableRefresh = true; veyra.outputRateMode = 1
            veyra.vrrState = 3; veyra.vrrAmd = true
            const row = findChild(root, "pushfps-gsync-row")
            verify(row, "the row exists")
            wait(50)
            verify(!quick.gsyncUnconfirmed, "FreeSync requested: nothing for NVIDIA's profile tools to set")
            verify(!row.visible, "FreeSync requested: no G-SYNC set-up")
            veyra.vrrAmd = false
            // (the row itself may stay hidden: an earlier test remembered pushGsyncChecked)
            tryVerify(() => quick.gsyncUnconfirmed, 2000, "the same level from the NVIDIA driver asks for the set-up")
            veyra.vrrState = 0; veyra.displaySync = 2; veyra.variableRefresh = false
            veyra.nvidiaVrrProfile = null
            tools.destroy()
        }

        // The look of AI picture enhancement: presets, two sliders, reset, every layer.
        function test_look_presets_sliders_and_reset() {
            verify(!quick.lookShown, "enhancement off: no look controls")
            verify(!control("pushfps-look-row").visible)
            verify(!control("pushfps-look-tone-row").visible)
            veyra.nrAnyEnabled = true
            verify(quick.lookShown)
            verify(control("pushfps-look-row").visible)
            compare(quick.lookId, "strong", "a layer that was never tuned is at the model's full strength")
            verify(!control("pushfps-look-reset").enabled, "nothing to reset at the default")
            control("pushfps-look").picked("subtle")
            compare(veyra.nrLayers[0].tone, 0.4)
            compare(veyra.nrLayers[0].structure, 0.5)
            compare(quick.lookId, "subtle")
            compare(control("pushfps-look").current, "subtle")
            control("pushfps-look").picked("balanced")
            compare(veyra.nrLayers[0].tone, 0.7)
            compare(veyra.nrLayers[0].structure, 0.8)
            // A slider moves one value and leaves the other.
            control("pushfps-look-tone").moved(0.55)
            compare(veyra.nrLayers[0].tone, 0.55)
            compare(veyra.nrLayers[0].structure, 0.8)
            compare(quick.lookId, "", "no preset matches: custom")
            compare(control("pushfps-look").current, "")
            compare(control("pushfps-look-tone-row").value, "55%")
            control("pushfps-look-detail").moved(0.25)
            compare(veyra.nrLayers[0].structure, 0.25)
            compare(veyra.nrLayers[0].tone, 0.55)
            // Out-of-range input is clamped, never passed on.
            control("pushfps-look-tone").moved(7)
            compare(veyra.nrLayers[0].tone, 1)
            control("pushfps-look-detail").moved(-3)
            compare(veyra.nrLayers[0].structure, 0)
            verify(control("pushfps-look-reset").enabled)
            control("pushfps-look-reset").clicked()
            compare(veyra.nrLayers[0].tone, 1)
            compare(veyra.nrLayers[0].structure, 1)
            compare(quick.lookId, "strong")
            // Turning the enhancement off hides the controls and keeps the values.
            control("pushfps-look").picked("subtle")
            control("pushfps-enhance").toggled(false)
            verify(!quick.lookShown)
            compare(veyra.nrLayers[0].tone, 0.4)
        }
        function test_look_reaches_every_enhancement_layer() {
            veyra.nrLayers = [{index: 1, tone: 1, structure: 1}, {index: 4, tone: 0.2, structure: 0.9}]
            veyra.nrAnyEnabled = true
            compare(quick.lookTone, 1, "the first layer is the one shown")
            verify(quick.applyLook("balanced"))
            compare(veyra.nrLayers[0].tone, 0.7); compare(veyra.nrLayers[1].tone, 0.7)
            compare(veyra.nrLayers[0].structure, 0.8); compare(veyra.nrLayers[1].structure, 0.8)
            verify(!quick.applyLook("no-such-look"))
            // A profile whose look is the one already shown writes nothing: layers given different
            // values in Professional mode stay different.
            veyra.nrLayers = [{index: 1, tone: 1, structure: 1}, {index: 4, tone: 0.2, structure: 0.9}]
            quick.applySnapshot({fps: 30, target: "off", enhance: true, look: {tone: 1, structure: 1}})
            compare(veyra.nrLayers[1].tone, 0.2); compare(veyra.nrLayers[1].structure, 0.9)
            quick.applySnapshot({fps: 30, target: "off", enhance: true, look: {tone: 0.4, structure: 0.5}})
            compare(veyra.nrLayers[0].tone, 0.4); compare(veyra.nrLayers[1].tone, 0.4)
            compare(veyra.nrLayers[1].structure, 0.5)
            veyra.nrLayers = []
            verify(!quick.lookShown, "no layer: nothing to tune")
            verify(!quick.setLook(0.5, 0.5))
        }
        function test_look_is_kept_per_game_and_console_profile() {
            veyra.nrAnyEnabled = true
            quick.applyLook("subtle")
            verify(quick.saveGame("Quiet game"))
            compare(quick.games[0].look.tone, 0.4)
            compare(quick.games[0].look.structure, 0.5)
            quick.setLook(0.9, 0.3)
            verify(quick.saveGame("Loud game"))
            quick.applyGame("Quiet game")
            compare(quick.lookId, "subtle")
            quick.applyGame("Loud game")
            compare(veyra.nrLayers[0].tone, 0.9); compare(veyra.nrLayers[0].structure, 0.3)
            // Each console remembers its own look too.
            quick.applyLook("balanced")
            quick.switchConsole("xbox")
            quick.applyLook("strong")
            quick.switchConsole("ps5")
            compare(quick.lookId, "balanced", "PS5 look restored")
            quick.switchConsole("xbox")
            compare(quick.lookId, "strong", "Xbox look restored")
            // A profile saved before the look existed leaves the look alone.
            quick.applyLook("subtle")
            quick.applySnapshot({fps: 30, target: "off", enhance: true})
            compare(quick.lookId, "subtle")
            quick.applySnapshot({fps: 30, target: "off", enhance: true, look: {tone: "x", structure: 0.5}})
            compare(quick.lookId, "subtle", "a broken look is ignored")
            quick.deleteGame("Quiet game"); quick.deleteGame("Loud game")
        }
        function test_look_compare_splits_the_picture_and_never_stays_on() {
            veyra.nrAnyEnabled = true
            const sw = control("pushfps-look-compare")
            verify(control("pushfps-look-compare-row").visible)
            verify(sw.enabled && !sw.checked)
            sw.toggled(true)
            compare(veyra.compareMode, 2, "split view: original left, enhanced right")
            verify(sw.checked && quick.lookCompare)
            verify(control("pushfps-look-compare-timer").running, "it ends by itself")
            sw.toggled(false)
            compare(veyra.compareMode, 0)
            verify(!quick.lookCompare && !control("pushfps-look-compare-timer").running)
            // Turned off somewhere else (Professional mode's display panel): this panel follows.
            sw.toggled(true)
            veyra.compareMode = 0
            verify(!quick.lookCompare && !sw.checked)
            // Enhancement turned off while comparing: the split ends with it.
            sw.toggled(true)
            compare(veyra.compareMode, 2)
            control("pushfps-enhance").toggled(false)
            compare(veyra.compareMode, 0)
            // The timer ends it.
            veyra.nrAnyEnabled = true
            sw.toggled(true)
            control("pushfps-look-compare-timer").triggered()
            compare(veyra.compareMode, 0)
            // A split the user set elsewhere is shown as on, and is not this panel's to end by itself.
            veyra.compareMode = 2
            verify(sw.checked && !quick.lookCompare)
            verify(!control("pushfps-look-compare-timer").running)
            veyra.nrAnyEnabled = false
            compare(veyra.compareMode, 2, "left as the user set it")
            veyra.compareMode = 0
            // Nothing playing: nothing to compare.
            veyra.nrAnyEnabled = true
            veyra.hasSource = false
            verify(!sw.enabled)
            quick.setLookCompare(true)
            compare(veyra.compareMode, 0)
        }

        // Off unless switched on (4.0.1): on for everyone, 4.0 made camera pans soft and broke edges up.
        function test_the_still_rule_is_off_unless_switched_on() {
            veyra.fgEnabled = true; veyra.fgMultiplier = 2
            const row = control("pushfps-hudstill-row"), sw = control("pushfps-hudstill")
            // Nothing saved: off, and it says so.
            verify(!row.on && !sw.checked, "off by default")
            const off = row.hint
            verify(off.length > 0)
            // Switched on it is saved as on and warns what it can do to a moving picture.
            sw.toggled(true)
            compare(veyra.preferences.pushHudStill, true); verify(row.on && sw.checked && row.hint !== off)
            const warned = row.hint
            // A pass that is running says the same; one that could not start says that instead.
            veyra.liveStats = ({ running: true, hudStillState: 1 }); quick.liveStats = veyra.liveStats
            compare(row.hint, warned)
            veyra.liveStats = ({ running: true, hudStillState: 2 }); quick.liveStats = veyra.liveStats
            verify(row.hint !== warned, "a pass that could not start says so")
            // Switched off again it is saved as off.
            sw.toggled(false)
            compare(veyra.preferences.pushHudStill, false); verify(!row.on && !sw.checked)
            compare(row.hint, off)
            sw.toggled(true)
            // It is its own switch: Live HUD Lock is not touched by it.
            compare(veyra.hudLock, false)
            sw.toggled(false)   // left as it starts
            // Without frame generation there is nothing for it to do.
            veyra.fgEnabled = false
            verify(!sw.enabled)
        }

        // Subtitle Hold: off unless switched on (not everyone uses subtitles), its own switch apart from Crosshair
        // Hold's, and the row says what the engine says.
        function test_subtitle_hold_is_off_unless_switched_on() {
            const was = veyra.fgBackendName
            veyra.fgEnabled = true; veyra.fgMultiplier = 2; veyra.fgBackendName = "vfg"
            const row = control("pushfps-subtitle-row"), sw = control("pushfps-subtitle")
            verify(row.visible, "on the home page, in the HUD section")
            verify(!row.on && !sw.checked, "off by default")
            const off = row.hint
            verify(off.length > 0 && row.help.length > 0)
            sw.toggled(true)
            compare(veyra.preferences.pushSubtitleHold, true); verify(row.on && sw.checked && row.hint !== off)
            verify(veyra.preferences.pushCrosshairHold !== true, "Crosshair Hold is not switched with it")
            const waiting = row.hint
            veyra.liveStats = ({ running: true, subtitleHoldState: 1, subtitleHoldPixels: 0 }); quick.liveStats = veyra.liveStats
            const looking = row.hint
            verify(looking !== waiting, "a pass that runs and has found no subtitles says so")
            veyra.liveStats = ({ running: true, subtitleHoldState: 1, subtitleHoldPixels: 5000 }); quick.liveStats = veyra.liveStats
            verify(row.hint !== looking && row.hint !== waiting, "a pass that holds subtitles says so")
            // Crosshair Hold's state is not Subtitle Hold's.
            veyra.liveStats = ({ running: true, crosshairHoldState: 1, crosshairHoldPixels: 240 }); quick.liveStats = veyra.liveStats
            compare(row.hint, waiting)
            veyra.fgBackendName = "anr"; verify(!row.methodOk && sw.checked, "with ANR it says so and keeps the choice")
            veyra.fgBackendName = was
            veyra.liveStats = ({}); quick.liveStats = veyra.liveStats
            sw.toggled(false)
            compare(veyra.preferences.pushSubtitleHold, false); verify(!row.on && !sw.checked)
        }
        // Crosshair Hold: off unless switched on, its own switch, and it says when the method chosen cannot use it.
        function test_crosshair_hold_is_off_unless_switched_on() {
            const was = veyra.fgBackendName
            veyra.fgEnabled = true; veyra.fgMultiplier = 2; veyra.fgBackendName = "vfg"
            const row = control("pushfps-crosshair-row"), sw = control("pushfps-crosshair")
            verify(row.visible, "on the home page, in the HUD section")
            // Nothing saved: off, and it says so.
            verify(!row.on && !sw.checked, "off by default")
            const off = row.hint
            verify(off.length > 0 && row.help.length > 0)
            // Switched on it is saved as on. With nothing playing it says it is on, never that it runs.
            sw.toggled(true)
            compare(veyra.preferences.pushCrosshairHold, true); verify(row.on && sw.checked && row.hint !== off)
            const waiting = row.hint
            // The row says what the engine says: the pass runs and has no crosshair yet, or it runs and holds one.
            veyra.liveStats = ({ running: true, crosshairHoldState: 1, crosshairHoldPixels: 0 }); quick.liveStats = veyra.liveStats
            const looking = row.hint
            verify(looking !== waiting && looking !== off, "a pass that runs and has found no crosshair says so")
            veyra.liveStats = ({ running: true, crosshairHoldState: 1, crosshairHoldPixels: 240 }); quick.liveStats = veyra.liveStats
            const running = row.hint
            verify(running !== looking && running !== waiting && running !== off, "a pass that holds a crosshair says so")
            // A pass that could not be set up says that instead.
            veyra.liveStats = ({ running: true, crosshairHoldState: 2 }); quick.liveStats = veyra.liveStats
            verify(row.hint !== running && row.hint !== looking && row.hint !== waiting && row.hint !== off, "a pass that could not start says so")
            // The switch on while the engine runs no pass (not asked there, or not with what plays) never reads as running.
            for (const idle of [0, 3]) {
                veyra.liveStats = ({ running: true, crosshairHoldState: idle, crosshairHoldPixels: 240 }); quick.liveStats = veyra.liveStats
                compare(row.hint, waiting, "state " + idle)
            }
            veyra.liveStats = ({ running: true, crosshairHoldState: 1, crosshairHoldPixels: 240 }); quick.liveStats = veyra.liveStats
            // Every method the graph runs itself takes it (the list engine/GraphDescription.h crosshairHoldMethod
            // holds); with the one that cannot it says so, and the choice is kept.
            for (const own of ["vfg", "amdfrc", "pushflow", "dlss", "fsr3", "fsr4"]) { veyra.fgBackendName = own; verify(row.methodOk, own); compare(row.hint, running, own) }
            for (const method of ["anr"]) {
                veyra.fgBackendName = method
                verify(!row.methodOk, method)
                verify(row.hint !== running && row.hint !== looking && row.hint !== waiting && row.hint !== off, method + ": it says that this method cannot use it")
                verify(sw.checked, method + ": the choice itself is kept")
            }
            veyra.fgBackendName = "vfg"
            // It is its own switch: neither Live HUD Lock nor the still rule is touched by it.
            compare(veyra.hudLock, false); verify(veyra.preferences.pushHudStill !== true)
            // Switched off again it is saved as off.
            sw.toggled(false)
            compare(veyra.preferences.pushCrosshairHold, false); verify(!row.on && !sw.checked)
            compare(row.hint, off)
            // Without frame generation there is nothing for it to do.
            veyra.fgEnabled = false
            verify(!sw.enabled)
            veyra.fgBackendName = was
        }

        // The size AI picture enhancement works at: 1080p (the standard), 1440p or the picture's full size,
        // written to every enhancement layer as Professional mode's own setting.
        function test_enhancement_size_is_a_choice() {
            veyra.nrAnyEnabled = true
            veyra.nrLayers = [{ index: 1, runtime: 2 }, { index: 3, runtime: 2 }]
            const row = control("pushfps-enhance-size-row"), seg = control("pushfps-enhance-size")
            verify(row.visible, "shown with the look, while the enhancement is on")
            compare(quick.enhanceSize, 0); compare(seg.current, "0", "nothing set: the standard, 1080p")
            const standard = row.hint
            seg.picked("1")
            compare(veyra.nrLayers[0].sizePolicy, 1); compare(veyra.nrLayers[1].sizePolicy, 1, "every layer")
            compare(seg.current, "1"); verify(row.hint !== standard)
            seg.picked("5")
            compare(veyra.nrLayers[0].sizePolicy, 5)
            seg.picked("0")
            compare(veyra.nrLayers[0].sizePolicy, 0); compare(row.hint, standard)
            // A size set in Professional mode that is none of the three is shown as such and left alone.
            veyra.nrLayers = [{ index: 1, runtime: 2, sizePolicy: 3 }]
            compare(seg.current, ""); verify(!quick.enhanceSizeKnown && row.hint !== standard)
            verify(!quick.setEnhanceSize(3), "only the three sizes offered here are written from here")
            compare(veyra.nrLayers[0].sizePolicy, 3)
            // The AMD enhancement runtime works at 1080p at most: no row.
            veyra.nrLayers = [{ index: 1, runtime: 4 }]
            verify(!row.visible)
            veyra.nrLayers = [{ index: 1 }]
            veyra.nrAnyEnabled = false
        }
        // A card without NVIDIA's low latency has the low queue under the same switch, not a dead row.
        function test_low_latency_switch_works_on_every_card() {
            const had = veyra.nvidiaLowLatencyAvailable, mode = veyra.nvidiaLowLatency, queue = veyra.fgLowQueue
            const sw = control("pushfps-low-latency"), row = control("pushfps-low-latency-row")
            veyra.nvidiaLowLatencyAvailable = false; veyra.nvidiaLowLatency = 0; veyra.fgLowQueue = false
            verify(sw.enabled && !sw.checked, "usable on a card that is not NVIDIA's")
            const other = row.label
            sw.toggled(true)
            compare(veyra.fgLowQueue, true, "it is the low queue there")
            compare(veyra.nvidiaLowLatency, 0, "and NVIDIA's setting is left alone")
            verify(sw.checked)
            sw.toggled(false)
            compare(veyra.fgLowQueue, false)
            veyra.nvidiaLowLatencyAvailable = true
            verify(row.label !== other, "NVIDIA's own name on an NVIDIA card")
            sw.toggled(true)
            compare(veyra.nvidiaLowLatency, 1, "on NVIDIA it is NVIDIA low latency, as before")
            compare(veyra.fgLowQueue, false)
            veyra.nvidiaLowLatencyAvailable = had; veyra.nvidiaLowLatency = mode; veyra.fgLowQueue = queue
        }

        // The method is chosen on the home page too, wherever the card offers more than one; an AMD card
        // on FSR is told about PushFlow, and nothing is switched for it.
        function test_method_is_chosen_on_the_home_page_and_amd_is_told_about_pushflow() {
            const choices = veyra.fgBackendChoices, name = veyra.fgBackendName, amd = veyra.amdNrGpu, text = veyra.fgProviderText
            const row = control("pushfps-fg-backend-row"), pick = control("pushfps-fg-backend")
            veyra.fgBackendChoices = [{ id: "fsr3", label: "FSR 3.1" }]
            verify(!quick.compactPro && !row.visible, "one method: nothing to choose")
            veyra.fgBackendChoices = [{ id: "pushflow", label: "PushFlow" }, { id: "fsr3", label: "FSR 3.1" }, { id: "vfg", label: "VFG", disabled: true }]
            compare(quick.methodsOffered, 2, "a method this card cannot run is not counted")
            verify(row.visible)
            veyra.fgBackendName = "fsr3"; veyra.fgProviderText = "FSR"; veyra.amdNrGpu = false
            compare(row.hint, "FSR", "another maker's card: what the method says of itself")
            veyra.amdNrGpu = true
            verify(quick.amdMethodTip && row.hint !== "FSR" && row.hint.indexOf("PushFlow") >= 0, "an AMD card on FSR is told about PushFlow")
            compare(veyra.fgBackendName, "fsr3", "and nothing is switched for it")
            pick.picked("pushflow")
            compare(veyra.fgBackendName, "pushflow")
            verify(!quick.amdMethodTip, "on PushFlow there is nothing more to say")
            verify(control("pushfps-pushflow-tier-row").visible, "PushFlow's tier is chosen here too")
            // Without PushFlow's files the tip has nothing to offer.
            veyra.fgBackendName = "fsr3"
            veyra.fgBackendChoices = [{ id: "pushflow", label: "PushFlow", disabled: true }, { id: "fsr3", label: "FSR 3.1" }, { id: "amdfrc", label: "AMD FRC" }]
            verify(!quick.amdMethodTip)
            veyra.fgBackendChoices = choices; veyra.fgBackendName = name; veyra.amdNrGpu = amd; veyra.fgProviderText = text
        }
        function test_lip_sync_is_trimmed_by_hand_on_top_of_automatic() {
            const row = control("pushfps-lipsync-row"), earlier = control("pushfps-lipsync-earlier"), later = control("pushfps-lipsync-later")
            const value = control("pushfps-lipsync-value"), reset = control("pushfps-lipsync-reset")
            // Nothing set: automatic alone, nothing to reset.
            compare(row.trim, 0); compare(value.text, "0 ms"); verify(!reset.visible)
            verify(earlier.enabled && later.enabled)
            // Later in steps of ten, kept as one number, and never past a quarter of a second.
            later.clicked(); compare(veyra.preferences.pushAudioTrimMs, 10); compare(value.text, "+10 ms"); verify(reset.visible)
            later.clicked(); later.clicked(); compare(veyra.preferences.pushAudioTrimMs, 30)
            veyra.preferences = ({ pushAudioTrimMs: 245 })
            later.clicked(); compare(veyra.preferences.pushAudioTrimMs, 250); verify(!later.enabled, "250 is the end")
            // Earlier the same way.
            veyra.preferences = ({ pushAudioTrimMs: 0 })
            earlier.clicked(); earlier.clicked(); compare(veyra.preferences.pushAudioTrimMs, -20); compare(value.text, "-20 ms")
            veyra.preferences = ({ pushAudioTrimMs: -250 }); verify(!earlier.enabled, "-250 is the end")
            // Back to automatic alone.
            reset.clicked(); compare(veyra.preferences.pushAudioTrimMs, 0); verify(!reset.visible)
            // Sound cannot be played before it arrives: with the delay used up, earlier is switched off and said.
            veyra.audioSyncLive = true; veyra.audioCompensationMs = 24
            veyra.preferences = ({ pushAudioTrimMs: -10 })
            verify(!row.atLimit && earlier.enabled, "14 ms of delay are left")
            veyra.audioCompensationMs = 0
            verify(row.atLimit && !earlier.enabled, "none is left")
            verify(later.enabled, "later is always possible")
            // With sync set to manual or off the trim does nothing, and the row says so instead of pretending.
            veyra.preferences = ({ pushAudioTrimMs: 20 }); veyra.audioCompensationMs = 44
            veyra.audioSyncMode = 1
            verify(!row.automatic && !earlier.enabled && !later.enabled)
            verify(reset.visible, "a trim that is set can still be taken away")
            veyra.audioSyncMode = 0
            verify(earlier.enabled && later.enabled)
            // A value that is no number, or out of range, is no trim or the nearest end.
            veyra.preferences = ({ pushAudioTrimMs: "abc" }); compare(row.trim, 0)
            veyra.preferences = ({ pushAudioTrimMs: 9000 }); compare(row.trim, 250)
        }

        function test_lowest_latency_one_tap_keeps_quality_and_restores() {
            // A G-SYNC/FreeSync screen on an NVIDIA card, display sync Automatic before.
            veyra.nvidiaLowLatencyAvailable = true; veyra.nvidiaLowLatency = 0; veyra.vrrState = 4
            veyra.displaySync = 2; veyra.variableRefresh = false; veyra.outputRateMode = 0
            veyra.fgMultiplier = 3; veyra.nrAnyEnabled = true; veyra.hudLock = true; veyra.pacingRepair = true
            const sw = control("pushfps-lowest-latency")
            verify(!sw.checked)
            const nav = Qt.createQmlObject('import QtTest; SignalSpy { signalName: "navigate" }', quick)
            nav.target = veyra
            sw.toggled(true)
            compare(nav.count, 0, "nothing playing: full screen waits for the next connection")
            compare(veyra.nvidiaLowLatency, 1, "NVIDIA low latency: one-frame queue")
            compare(veyra.preferences.pushAutoFullscreen, true, "fullscreen on connect")
            compare(veyra.displaySync, 1); compare(veyra.variableRefresh, true); compare(veyra.outputRateMode, 1)
            verify(quick.lowestLatency && sw.checked)
            // Picture and frame generation untouched.
            compare(veyra.fgMultiplier, 3); verify(veyra.nrAnyEnabled); verify(veyra.hudLock); verify(veyra.pacingRepair)
            sw.toggled(false)
            compare(veyra.nvidiaLowLatency, 0, "restored")
            compare(veyra.preferences.pushAutoFullscreen, false)
            compare(veyra.displaySync, 2); compare(veyra.variableRefresh, false); compare(veyra.outputRateMode, 0)
            verify(!quick.lowestLatency)
            // Another GPU, no variable refresh: the low queue, display sync left alone (no tearing).
            veyra.nvidiaLowLatencyAvailable = false; veyra.vrrState = 0; veyra.fgLowQueue = false
            veyra.displaySync = 2
            sw.toggled(true)
            compare(veyra.fgLowQueue, true)
            compare(veyra.displaySync, 2, "no VRR: sync unchanged")
            verify(quick.lowestLatency)
            sw.toggled(false)
            compare(veyra.fgLowQueue, false)
            // With a picture playing it goes full screen now, not only at the next connection.
            veyra.running = true
            sw.toggled(true)
            compare(nav.count, 1); compare(nav.signalArguments[0][0], "fullscreen")
            sw.toggled(false)
            compare(nav.count, 1, "turning it off leaves the window as it is")
            veyra.running = false
            nav.destroy()
            veyra.vrrState = 0
        }
    }
}
