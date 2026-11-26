// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// Game presets (PushGamePresetsDialog + PushGamePresets.js), the latency readout,
// the capture card status and Discord sharing, against the fixture bridge.
import QtQuick
import QtTest
import Veyra
import "../qml/Veyra/PushGamePresets.js" as Presets
Item {
    id: root
    width: 900; height: 1000
    Item { id: host; anchors.fill: parent; property string dialog: "games"; function close() { dialog = "" } }
    PushGamePresetsDialog { id: games; dialogHost: host }
    PushLatencyBadge { id: badge; x: 10; y: 10 }
    // The same readout the way the main window places it: over a picture's rectangle, where it can be dragged.
    // Above everything else here, as in the main window, so nothing below takes its mouse.
    PushLatencyBadge { id: placed; z: 100; areaX: 100; areaY: 300; areaWidth: 700; areaHeight: 400 }
    PushQuickSetup { id: quick; width: 560; y: 400 }
    PushRecordingSaved { id: card; x: 10; y: 900 }

    TestCase {
        name: "PushPresets"
        when: windowShown
        function init() {
            veyra.preferences = ({ pushGameFps: 30 })
            veyra.fgEnabled = false; veyra.fgMultiplier = 1; veyra.outputRateMode = 0; veyra.outputCustomFps = 0
            veyra.displayRefreshHz = 240
            veyra.exclusionState = ({ enabled: false, feather: 6, regions: [] })
            veyra.lastRegions = null; veyra.lastCopiedText = ""; veyra.lastCopiedFile = ""; veyra.discordOpened = 0; veyra.lastSharedFile = ""
            veyra.liveStats = ({ running: false })
            veyra.captureDeviceLabel = ""
            host.dialog = "games"
            games.protectHud = true
            games.consoleFilter = ""
            games.selectedName = "GTA VI"
            veyra.fgAdaptive = false; veyra.contentRate = 0
            veyra.fgMultiplierChoices = [{id:"1"},{id:"2"},{id:"3"},{id:"4"}]
        }
        function cleanup() { init() }

        // --- presets ---------------------------------------------------------
        function test_library_is_valid() {
            verify(Presets.builtIn.length >= 5)
            for (const p of Presets.builtIn) {
                const s = Presets.sanitize(p)
                verify(s !== null, p.name)
                compare(s.regions.length, p.regions.length, p.name + " keeps every region")
                for (const r of s.regions) verify(r.left < r.right && r.top < r.bottom && r.right <= 1 && r.bottom <= 1, p.name)
            }
            compare(Presets.builtIn[0].name, "GTA VI")
        }
        function test_apply_gta_vi() {
            games.apply(games.selected)
            compare(veyra.lastRegions.length, 3, "minimap, health/focus and weapon/wanted/cash areas")
            compare(veyra.fgMultiplier, 4, "auto target on a 240 Hz screen: 120 FPS from 30")
            compare(veyra.outputCustomFps, 120)
            compare(veyra.preferences.pushGameFps, 30)
            verify(veyra.fgAdaptive, "the base PS5 dips below 30: adaptive multiplier")
            compare(veyra.contentRate, 1, "which needs the measured game frame rate")
            verify(games.message.indexOf("GTA VI") >= 0)
            verify(!games.messageError)
        }
        function test_apply_without_hud_protection() {
            games.protectHud = false
            games.apply(games.selected)
            compare(veyra.lastRegions.length, 0)
        }
        function test_auto_target_on_a_60_hz_screen() {
            veyra.displayRefreshHz = 60
            games.apply(games.selected)
            compare(veyra.fgMultiplier, 3, "adaptive: the ceiling a 20 FPS dip needs for 60")
            compare(veyra.outputCustomFps, 60)
            // A preset without the adaptive multiplier: plain 2X.
            games.selectedName = "Open world · minimap bottom left"
            games.apply(games.selected)
            compare(veyra.fgMultiplier, 2)
            compare(veyra.outputCustomFps, 60)
            verify(!veyra.fgAdaptive)
        }
        function test_auto_target_on_a_90_hz_screen() {
            veyra.displayRefreshHz = 90
            compare(Presets.resolveTarget("auto", 90), "90")
            compare(Presets.resolveTarget("auto", 89.95), "90")
            compare(Presets.resolveTarget("auto", 100), "60")
            compare(Presets.resolveTarget("120", 90), "120", "a chosen target stays")
            games.selectedName = "Open world · minimap bottom left"
            games.apply(games.selected)
            compare(veyra.fgMultiplier, 3, "30 FPS x 3 = 90")
            compare(veyra.outputCustomFps, 90)
            compare(veyra.outputRateMode, 2)
            // Frame generation that only doubles cannot reach 90: 60 instead.
            veyra.fgMultiplierChoices = [{id:"1"},{id:"2"}]
            games.apply(games.selected)
            compare(veyra.fgMultiplier, 2)
            compare(veyra.outputCustomFps, 60)
            // The current settings saved as a preset keep the 90 FPS target.
            veyra.fgMultiplierChoices = [{id:"1"},{id:"2"},{id:"3"},{id:"4"}]
            games.apply(games.selected)
            compare(games.currentAsPreset("Mine").target, "90")
            compare(Presets.sanitize({name: "x", fps: 30, target: "90", regions: []}).target, "90")
        }
        function test_share_code_round_trip() {
            const code = games.shareCode(games.selected)
            verify(code.indexOf("PUSHFPS1:") === 0)
            const back = Presets.decode(code, Qt.atob)
            compare(back.name, "GTA VI")
            compare(back.regions.length, 3)
            compare(back.adaptive, true)
            fuzzyCompare(back.regions[0].left, 0.012, 0.001)
            const unicode = Presets.decode(Presets.encode({ name: "原神 ★ test", fps: 60, regions: [] }, Qt.btoa), Qt.atob)
            compare(unicode.name, "原神 ★ test")
            compare(unicode.fps, 60)
            compare(Presets.decode("PUSHFPS1:not-base64!!", Qt.atob), null)
            compare(Presets.decode("hello", Qt.atob), null)
            compare(Presets.decode("", Qt.atob), null)
        }
        function test_bad_values_are_cleaned() {
            const p = Presets.sanitize({ name: "  x  ", fps: 999, target: "9000", feather: -5,
                                         regions: [{ l: -1, t: 2, r: 0.5, b: 0.5 }, { l: 0.1, t: 0.1, r: 0.1, b: 0.9 }, {}, {}, {}, {}] })
            compare(p.name, "x")
            compare(p.fps, 30)
            compare(p.target, "auto")
            compare(p.feather, 0)
            compare(p.regions.length, 1, "empty and zero-width regions dropped")
            compare(p.regions[0].left, 0); compare(p.regions[0].top, 0.5); compare(p.regions[0].bottom, 1)
            compare(Presets.sanitize({ name: "" }), null)
            const many = Presets.sanitize({ name: "m", regions: [0, 1, 2, 3, 4, 5].map(i => ({ l: i * 0.1, t: 0, r: i * 0.1 + 0.05, b: 0.1 })) })
            compare(many.regions.length, 4, "at most four regions")
        }
        function test_save_import_and_delete_own_presets() {
            veyra.exclusionState = ({ enabled: true, feather: 4, regions: [{ left: 0.1, top: 0.1, right: 0.3, bottom: 0.3, ellipse: true }] })
            veyra.fgEnabled = true; veyra.outputRateMode = 2; veyra.outputCustomFps = 120
            verify(games.saveMine("My Racer"))
            const stored = JSON.parse(veyra.preferences.pushGamePresets)
            compare(stored.length, 1)
            compare(stored[0].target, "120")
            compare(stored[0].regions[0].ellipse, true)
            compare(games.selectedName, "My Racer")
            verify(games.selected.mine)
            verify(!games.saveMine("GTA VI"), "built-in names are reserved")
            const code = games.shareCode(games.selected)
            games.deleteMine("My Racer")
            compare(JSON.parse(veyra.preferences.pushGamePresets).length, 0)
            verify(games.addImported(Presets.decode(code, Qt.atob)))
            compare(JSON.parse(veyra.preferences.pushGamePresets)[0].name, "My Racer")
            verify(!games.addImported(null))
            verify(games.messageError)
            verify(games.addImported(Presets.decode(games.shareCode(Presets.builtIn[0]), Qt.atob)))
            verify(games.all.some(p => p.name.indexOf("GTA VI ") === 0 && p.mine), "a shared copy of a built-in gets its own name")
        }
        // --- Switch and Switch 2 ---------------------------------------------------
        function test_switch_library() {
            const sw = Presets.builtIn.map(Presets.sanitize).filter(p => p.console === "switch")
            const sw2 = Presets.builtIn.map(Presets.sanitize).filter(p => p.console === "switch2")
            verify(sw.length >= 10, "Switch games: " + sw.length)
            verify(sw2.length >= 8, "Switch 2 games: " + sw2.length)
            for (const p of sw.concat(sw2)) {
                compare(p.fps, 30, p.name)
                verify(p.name.length <= Presets.kMaxName, p.name)
                verify(games.noteFor(p).length > 20, p.name + " has a translated note")
            }
            const names = Presets.builtIn.map(p => p.name)
            compare(new Set(names).size, names.length, "names are unique")
            verify(sw.some(p => p.name === "Pokémon Legends: Arceus"))
            verify(sw2.find(p => p.name === "Elden Ring").adaptive, "Elden Ring dips: adaptive")
            verify(!sw.find(p => p.name === "Pokémon Legends: Arceus").adaptive)
        }
        function test_console_filter_follows_the_quick_setup() {
            veyra.preferences = ({ pushGameFps: 30, pushConsole: "switch2" })
            host.dialog = ""; wait(0); host.dialog = "games"; wait(0)
            compare(games.consoleFilter, "switch2")
            verify(games.listed.length > 0 && games.listed.every(p => p.console === "switch2"))
            compare(games.selected.console, "switch2", "the first Switch 2 game is selected")
            findChild(games, "pushfps-game-preset-console").picked("")
            verify(games.listed.length === games.all.length)
        }
        function test_apply_a_steady_switch_game() {
            games.selectedName = "Pokémon Legends: Arceus"
            games.apply(games.selected)
            compare(veyra.preferences.pushConsole, "switch")
            compare(veyra.preferences.pushGameFps, 30)
            compare(veyra.fgAdaptive, false)
            compare(veyra.fgMultiplier, 4, "30 FPS to 120 on a 240 Hz screen")
            compare(veyra.lastRegions.length, 0)
        }
        function test_apply_a_game_that_dips_turns_on_adaptive() {
            games.selectedName = "Elden Ring"
            games.apply(games.selected)
            compare(veyra.preferences.pushConsole, "switch2")
            verify(veyra.fgAdaptive, "adaptive on")
            compare(veyra.contentRate, 1, "duplicate removal on, to measure the game's frame rate")
            compare(veyra.fgMultiplier, 4, "ceiling: the largest offered multiplier (4X here) for a 120 FPS target")
            compare(veyra.outputCustomFps, 120)
            compare(veyra.lastRegions.length, 3, "HP bars, items and runes")
            verify(games.noteFor(games.selected).indexOf(qsTr("帧率会波动，已开启自适应倍率。")) >= 0)
        }
        function test_adaptive_is_skipped_when_only_2x_exists() {
            veyra.fgMultiplierChoices = [{id:"1"},{id:"2"}]
            games.selectedName = "Elden Ring"
            games.apply(games.selected)
            compare(veyra.fgAdaptive, false)
            compare(veyra.fgMultiplier, 2)
        }
        function test_adaptive_ceiling() {
            compare(Presets.adaptiveCeiling([2, 3, 4, 6], 120), 6)
            compare(Presets.adaptiveCeiling([2, 3, 4], 120), 4)
            compare(Presets.adaptiveCeiling([2, 3, 4, 6], 60), 3)
            compare(Presets.adaptiveCeiling([2, 3, 4, 5, 6, 7, 8], 144), 7)
            compare(Presets.adaptiveCeiling([2], 120), 2)
            compare(Presets.adaptiveCeiling([], 120), 0)
        }
        function test_share_code_keeps_console_and_adaptive() {
            const back = Presets.decode(Presets.encode(Presets.builtIn.find(p => p.name === "Cyberpunk 2077"), Qt.btoa), Qt.atob)
            compare(back.console, "switch2")
            compare(back.adaptive, true)
            compare(back.regions.length, 2)
        }
        function test_copy_button() {
            const b = findChild(games, "pushfps-game-preset-copy")
            verify(b !== null)
            b.clicked()
            verify(veyra.lastCopiedText.indexOf("PUSHFPS1:") === 0)
        }

        // --- latency readout -------------------------------------------------
        function test_latency_badge_is_dragged_anywhere_over_the_picture_and_stays_there() {
            veyra.preferences = ({ pushLatencyOverlay: true })
            veyra.liveStats = ({ running: true, latencyMs: 14.4, latencyP95Ms: 19.6, submitFps: 119.7 })
            placed.refresh()
            verify(placed.visible && placed.movable)
            const grip = findChild(placed, "pushfps-latency-drag")
            verify(grip !== null && grip.enabled)
            // Until it is moved it sits where it always did: the picture's top left corner, 12 in.
            compare(placed.x, 112); compare(placed.y, 312)
            const left = 112, top = 312, right = 100 + 700 - placed.width - 12, bottom = 300 + 400 - placed.height - 12
            // Drag it by its middle 200 right and 150 down, in two moves. A position is given from the
            // grip's own corner, and the grip moves with the readout: each move is 100 and 75 further.
            mousePress(grip, placed.width / 2, placed.height / 2)
            mouseMove(grip, placed.width / 2 + 100, placed.height / 2 + 75, -1, Qt.LeftButton)
            verify(placed.dragging, "it follows the mouse while the button is held")
            fuzzyCompare(placed.x, left + 100, 0.5); fuzzyCompare(placed.y, top + 75, 0.5)
            mouseMove(grip, placed.width / 2 + 100, placed.height / 2 + 75, -1, Qt.LeftButton)
            mouseRelease(grip, placed.width / 2, placed.height / 2)
            verify(!placed.dragging)
            fuzzyCompare(placed.x, left + 200, 1.5); fuzzyCompare(placed.y, top + 150, 1.5)
            // Kept as shares of the room it has, not as pixels.
            fuzzyCompare(veyra.preferences.pushLatencyX, 1000 * 200 / (right - left), 2)
            fuzzyCompare(veyra.preferences.pushLatencyY, 1000 * 150 / (bottom - top), 2)
            // Dragged far past the picture's corner it stops at the edge, inside the picture.
            mousePress(grip, 5, 5)
            mouseMove(grip, 880 - placed.x, 980 - placed.y, -1, Qt.LeftButton)   // the window's far corner, past the picture's
            verify(placed.dragging)
            mouseRelease(grip, 5, 5)
            fuzzyCompare(placed.x, right, 0.5); fuzzyCompare(placed.y, bottom, 0.5)
            compare(veyra.preferences.pushLatencyX, 1000); compare(veyra.preferences.pushLatencyY, 1000)
            // The picture changes size (the window is resized, full screen): it keeps its corner and stays inside.
            placed.areaWidth = 400; placed.areaHeight = 250
            fuzzyCompare(placed.x, 100 + 400 - placed.width - 12, 0.5); fuzzyCompare(placed.y, 300 + 250 - placed.height - 12, 0.5)
            placed.areaWidth = 700; placed.areaHeight = 400
            // A picture narrower than the readout leaves it no room: it stays at the picture's edge.
            placed.areaWidth = 60
            compare(placed.x, 112)
            placed.areaWidth = 700
            // A click is not a drag, and a double click sends it back to the corner.
            mouseClick(grip, 5, 5)
            compare(veyra.preferences.pushLatencyX, 1000)
            mouseDoubleClickSequence(grip, 5, 5)
            compare(veyra.preferences.pushLatencyX, 0); compare(veyra.preferences.pushLatencyY, 0)
            compare(placed.x, left); compare(placed.y, top)
            // A value that is no place (a damaged file) is the corner; one out of range is kept inside.
            veyra.preferences = ({ pushLatencyOverlay: true, pushLatencyX: "abc", pushLatencyY: 5000 })
            compare(placed.x, left); fuzzyCompare(placed.y, bottom, 0.5)
            veyra.preferences = ({ pushLatencyOverlay: true, pushLatencyX: 500, pushLatencyY: 500 })
            fuzzyCompare(placed.x, (left + right) / 2, 0.5); fuzzyCompare(placed.y, (top + bottom) / 2, 0.5)
            // The readout without a picture's rectangle (as before 4.0) cannot be dragged and stays put.
            verify(!badge.movable && !findChild(badge, "pushfps-latency-drag").enabled)
            compare(badge.x, 10); compare(badge.y, 10)
            veyra.preferences = ({})
        }
        function test_latency_badge() {
            verify(!badge.visible, "off by default")
            veyra.preferences = ({ pushLatencyOverlay: true })
            veyra.liveStats = ({ running: true, latencyMs: 14.4, latencyP95Ms: 19.6, submitFps: 119.7, submitLowFps: 112.2 })
            badge.refresh()
            verify(badge.visible)
            compare(findChild(badge, "pushfps-latency-ms").text, qsTr("延迟 %1 ms").arg("14"))
            compare(badge.level, Theme.accent)
            veyra.liveStats = ({ running: true, latencyMs: 75 })
            badge.refresh()
            verify(badge.level !== Theme.accent)
            veyra.liveStats = ({ running: true })
            badge.refresh()
            compare(findChild(badge, "pushfps-latency-ms").text, qsTr("延迟 测量中"))
        }
        function test_latency_badge_in_a_larger_size() {
            veyra.preferences = ({ pushLatencyOverlay: true })
            veyra.liveStats = ({ running: true, latencyMs: 14.4, latencyP95Ms: 19.6, submitFps: 119.7 })
            badge.refresh()
            const text = findChild(badge, "pushfps-latency-ms")
            compare(badge.k, 1); compare(badge.implicitHeight, 30); compare(text.font.pixelSize, 12)
            const standard = badge.implicitWidth
            veyra.preferences = ({ pushLatencyOverlay: true, pushLatencySize: 1 })
            compare(badge.k, 1.4); compare(badge.implicitHeight, 42)
            verify(badge.implicitWidth > standard, "everything in it grows")
            veyra.preferences = ({ pushLatencyOverlay: true, pushLatencySize: 2 })
            compare(badge.k, 1.8); compare(badge.implicitHeight, 54)
            // A value from somewhere else is the nearest size, never a broken one.
            veyra.preferences = ({ pushLatencyOverlay: true, pushLatencySize: 9 })
            compare(badge.k, 1.8)
            veyra.preferences = ({ pushLatencyOverlay: true, pushLatencySize: "x" })
            compare(badge.k, 1)
            // The chip is dark in every theme, so its words are white in every theme.
            Theme.mode = "light"
            compare(String(text.color), "#ffffff")
            Theme.mode = "default"
            veyra.preferences = ({})
        }

        // --- capture card list -------------------------------------------------
        function test_capture_card_status() {
            const row = findChild(quick, "pushfps-card-row")
            verify(!row.visible, "no card, no row")
            veyra.captureDeviceLabel = "Game Capture 4K X"
            verify(row.visible)
            verify(findChild(quick, "pushfps-card-status").visible)
            compare(findChild(quick, "pushfps-card-status").text, qsTr("已测试"))
            // A card nothing is known of carries no mark at all: it is never called "not tested".
            veyra.captureDeviceLabel = "USB Video"
            verify(row.visible)
            verify(!findChild(quick, "pushfps-card-status").visible)
        }
        // Without a card chosen there is nothing to call tested or to report, so there is no row; a chosen card
        // that is unplugged is named as that and nothing more.
        function test_capture_card_row_without_a_card() {
            const row = findChild(quick, "pushfps-card-row")
            const status = findChild(quick, "pushfps-card-status")
            veyra.captureDeviceLabel = qsTr("未选择设备")
            verify(!row.visible, "no row while no device is selected")
            veyra.captureDeviceLabel = qsTr("设备未连接")
            verify(row.visible)
            verify(!status.visible, "an unplugged card carries no mark")
            compare(row.hint, "")
            veyra.captureDeviceLabel = "USB Video"
            verify(row.visible && !status.visible, "a card nothing is known of: the row, and no mark")
            compare(row.hint, qsTr("欢迎在 GitHub 上分享这张卡的使用结果"))
            veyra.captureDeviceLabel = ""
        }
        // Capture card reports from users: said as a user's report, not as PushFPS's own test, and
        // a card whose name contains another's (HD60 S+ and HD60 S, Ultra and Ultra 2.1) keeps its own.
        function test_capture_cards_reported_by_users() {
            const row = findChild(quick, "pushfps-card-row")
            veyra.captureDeviceLabel = "Game Capture HD60 S"
            compare(findChild(quick, "pushfps-card-status").text, qsTr("已测试"))
            compare(row.hint, qsTr("有 PushFPS 用户报告可用"))
            veyra.captureDeviceLabel = "Game Capture HD60 S+"
            verify(!findChild(quick, "pushfps-card-status").visible, "its own entry, with nothing known: no mark")
            veyra.captureDeviceLabel = "Game Capture HD60 X"
            compare(row.hint, qsTr("PushFPS 已实测"))
            veyra.captureDeviceLabel = "AVerMedia Live Gamer Ultra 2.1"
            compare(findChild(quick, "pushfps-card-status").text, qsTr("已测试"))
            compare(row.hint, qsTr("有 PushFPS 用户报告可用"))
            // Reported in November 2026: an Elgato HD60 Pro and an ezcap Game Link Raw.
            veyra.captureDeviceLabel = "Game Capture HD60 Pro"
            compare(row.hint, qsTr("有 PushFPS 用户报告可用"))
            veyra.captureDeviceLabel = "ezcap Game Link RAW"
            compare(row.hint, qsTr("有 PushFPS 用户报告可用"))
            // Reported on 21 November 2026: an AVerMedia Live Streamer Ultra HD (GC571), by the name Windows gives it.
            veyra.captureDeviceLabel = "Live Streamer Ultra HD-Video"
            compare(row.hint, qsTr("有 PushFPS 用户报告可用"))
            // Reported on 23 November 2026 (issue 19): a FEELWORLD C4K.
            veyra.captureDeviceLabel = "FEELWORLD C4K"
            compare(row.hint, qsTr("有 PushFPS 用户报告可用"))
            // Reported on 26 November 2026 (issue 32): a Monster Capture Card.
            veyra.captureDeviceLabel = "Monster Capture Card"
            compare(row.hint, qsTr("有 PushFPS 用户报告可用"))
            veyra.captureDeviceLabel = "AVerMedia Live Gamer Ultra-Video"
            verify(!findChild(quick, "pushfps-card-status").visible)
            veyra.captureDeviceLabel = "USB Video"
        }
        function test_tools_open_their_dialogs() {
            const list = []
            const handler = p => list.push(p)
            veyra.navigate.connect(handler)
            findChild(quick, "pushfps-optimize").clicked()
            findChild(quick, "pushfps-game-presets").clicked()
            veyra.navigate.disconnect(handler)
            verify(list.indexOf("optimize") >= 0 && list.indexOf("games") >= 0, list.join(","))
            compare(list[0], "pro", "the picture is shown before it is measured")
        }

        // --- Discord ------------------------------------------------------------
        function test_share_to_discord_without_webhook() {
            card.show("C:/Videos/PushFPS/PushFPS-Replay-1.mp4", true)
            tryVerify(() => card.opacity > 0.99, 2000)
            const b = findChild(card, "pushfps-recording-discord")
            verify(b !== null && b.visible)
            b.clicked()
            compare(veyra.lastCopiedFile, "C:/Videos/PushFPS/PushFPS-Replay-1.mp4")
            compare(veyra.discordOpened, 1)
            verify(card.shareText.length > 0)
            card.on = false
        }
        function test_send_to_discord_with_webhook() {
            veyra.preferences = ({ pushDiscordWebhook: "https://discord.com/api/webhooks/1/abc" })
            card.show("C:/r.mp4", false)
            tryVerify(() => card.opacity > 0.99, 2000)
            findChild(card, "pushfps-recording-discord").clicked()
            compare(veyra.lastSharedFile, "C:/r.mp4")
            verify(card.sending)
            veyra.pushShareFinished(true, "sent")
            verify(!card.sending)
            compare(card.shareText, "sent")
            card.on = false
        }
    }
}
