// PushFPS quick setup: game frame rate, target frame rate, console and game
// profiles. UI logic only, against the fixture bridge in QuickSmokeTests.cpp.
import QtQuick
import QtTest
import Veyra
Item {
    id: root
    width: 640; height: 640
    PushQuickSetup { id: quick; x: 10; y: 10; width: 560 }
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
        function test_rows_are_shown() {
            for (const n of ["pushfps-console-row", "pushfps-game-row", "pushfps-game-fps-row",
                             "pushfps-target-row", "pushfps-enhance-row", "pushfps-low-latency-row"])
                verify(control(n).visible, n)
            verify(quick.height > 0)
        }
    }
}
