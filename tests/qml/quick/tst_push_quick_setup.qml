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

        // 4.0 had it on for everyone and it added artifacts around moving edges: off unless switched on.
        function test_the_still_rule_is_off_unless_switched_on() {
            veyra.fgEnabled = true; veyra.fgMultiplier = 2
            const row = control("pushfps-hudstill-row"), sw = control("pushfps-hudstill")
            // Nothing saved: off, and nothing is written by looking at it.
            verify(!row.on && !sw.checked, "off by default")
            verify(veyra.preferences.pushHudStill !== true, "nothing switches it on by itself")
            const untouched = row.hint
            sw.toggled(true)
            compare(veyra.preferences.pushHudStill, true); verify(row.on && sw.checked && row.hint !== untouched)
            // What runs is said: running, could not start, or just the description.
            veyra.liveStats = ({ running: true, hudStillState: 1 }); quick.liveStats = veyra.liveStats
            verify(row.hint.length > 0)
            const running = row.hint
            veyra.liveStats = ({ running: true, hudStillState: 2 }); quick.liveStats = veyra.liveStats
            verify(row.hint !== running, "a pass that could not start says so")
            // Switched off it is saved as off, and says so; switched on again it is saved as on.
            sw.toggled(false)
            compare(veyra.preferences.pushHudStill, false); verify(!row.on && !sw.checked)
            const off = row.hint
            sw.toggled(true)
            compare(veyra.preferences.pushHudStill, true); verify(row.on && sw.checked && row.hint !== off)
            // It is its own switch: Live HUD Lock is not touched by it.
            compare(veyra.hudLock, false)
            // Without frame generation there is nothing for it to do.
            veyra.fgEnabled = false
            verify(!sw.enabled)
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
            sw.toggled(true)
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
            veyra.vrrState = 0
        }
    }
}
