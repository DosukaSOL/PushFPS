// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Professional mode's panel (4.2.1): the panel over the full-screen picture is as wide as its right edge
// is dragged, and the quick setup stands there as two cards, the Picture section in the Quality tab and
// the rest in the Frame Gen tab.
import QtQuick
import QtTest
import Veyra
import VeyraTest 1.0   // the Professional page is private to the shell's folder; the test program registers it here

Item {
    id: fixture
    width: 1600; height: 900
    // A page takes its parent, size, visibility and input from the shell (VPage.qml): outside the shell a test
    // sets all five, or the page is in no scene and nothing in it reads visible.
    ProPage { id: page; parent: fixture; width: fixture.width; height: fixture.height; visible: true; enabled: true; tab: "fg" }

    TestCase {
        name: "PushProPanel"
        when: windowShown
        property var savedPreferences: ({})
        function init() { savedPreferences = veyra.preferences; veyra.preferences = ({}); page.overlay = false; page.tab = "fg" }
        function cleanup() { Theme.design = "legacy"; page.overlay = false; page.onboardingActive = false; page.tab = "fg"; veyra.preferences = savedPreferences }
        function control(parent, name) { const c = findChild(parent, name); verify(c !== null, name); return c }
        // What stands under an item's middle, from the page inwards (for a press that did not arrive).
        function under(item) {
            const at = item.mapToItem(page, item.width / 2, item.height / 2)
            let node = page, x = at.x, y = at.y, path = Math.round(at.x) + "," + Math.round(at.y) + " of " + Math.round(item.width) + "x" + Math.round(item.height)
                + (item.visible ? "" : " (not visible)") + (item.enabled ? "" : " (not enabled)") + ":"
            for (let depth = 0; depth < 24 && node; ++depth) {
                const child = node.childAt(x, y)
                if (!child) break
                const p = node.mapToItem(child, x, y)
                x = p.x; y = p.y; node = child
                path += " > " + (child.objectName || String(child).split("(")[0])
            }
            return path
        }

        // The redesign: in the new design every tab's settings stand in one list, each under its name, so
        // nothing is looked for from tab to tab, and AI picture enhancement's card comes first. The five
        // tabs stay beside it. Legacy has the five tabs and the card where 4.2.1 had it.
        function test_new_design_lists_every_tab_in_one_place() {
            const shown = name => { const c = control(page, name); return c.visible && c.height > 0 }
            verify(!Theme.modern, "a test starts in Legacy")
            page.tab = "quality"
            verify(findChild(page, "pro-tab-all") === null, "Legacy has no such tab")
            const picture = control(page, "list-pushfps-picture")
            verify(picture.parent !== control(page, "list-pushfps-picture-first"), "Legacy: over the enhancement's own card, as in 4.2.1")
            verify(!control(page, "pro-all-quality").visible)
            // The new design starts on everything.
            Theme.design = "new"
            tryCompare(page, "tab", "all")
            verify(page.allTabs)
            tryVerify(function() { return findChild(page, "pro-tab-all") !== null && control(page, "pro-tab-all").visible }, 3000)
            for (const id of ["quality", "fg", "color", "audio", "display"]) {
                tryVerify(function() { return control(page, "pro-tab-" + id).visible }, 3000, id + " keeps its own tab")
                tryVerify(function() { return shown("pro-all-" + id) }, 3000, id + " has its name in the list")
            }
            // Each part once, under the one before it.
            let bottom = -1
            for (const name of ["pro-all-quality", "list-pushfps-picture", "pro-all-fg", "list-pushfps-quick", "pro-all-color", "pro-all-audio", "pro-all-display"]) {
                const item = control(page, name)
                const under = bottom
                // (A layout gives an item its place at its next pass, not when it is shown.)
                tryVerify(function() { return item.visible && item.height > 0 && item.mapToItem(page, 0, 0).y >= under - 0.5 }, 3000,
                          name + " stands under what comes before it")
                bottom = item.mapToItem(page, 0, 0).y
            }
            compare(picture.parent, control(page, "list-pushfps-picture-first"), "AI picture enhancement's card is the first card")
            tryVerify(function() { return control(page, "pushfps-enhance-row").visible }, 3000, "with its switch")
            // A tab still shows that tab alone.
            const tabFg = control(page, "pro-tab-fg")
            mouseClick(tabFg)
            compare(page.tab, "fg", "a press on the tab; under the pointer stands " + under(tabFg))
            tryVerify(function() { return !picture.visible && control(page, "list-pushfps-quick").visible && !control(page, "pro-all-fg").visible }, 3000)
            mouseClick(control(page, "pro-tab-all"))
            compare(page.tab, "all")
            // A tab chosen before the design changes is kept; Legacy never stays on a tab it does not have.
            Theme.design = "legacy"
            tryCompare(page, "tab", "quality")
            tryVerify(function() { return picture.visible && picture.parent !== control(page, "list-pushfps-picture-first") }, 3000)
            page.tab = "color"
            Theme.design = "new"
            compare(page.tab, "color", "what was chosen stays chosen")
        }

        // 4.3.1: in the All tab each tab's block folds by its title and stays folded; a tab of its own
        // always shows its block.
        function test_all_tab_blocks_fold_by_their_titles() {
            Theme.design = "new"
            page.tab = "all"
            const blocks = { quality: "list-pushfps-picture", fg: "list-pushfps-quick", audio: "", display: "", color: "" }
            const head = id => control(page, "pro-all-" + id)
            for (const id in blocks) {
                tryVerify(function() { return head(id).visible && head(id).height > 20 && head(id).open }, 3000, id + " has a title that is open")
                verify(page.blockOpen(id) && page.blockShown(id))
            }
            const quick = control(page, "list-pushfps-quick"), picture = control(page, "list-pushfps-picture")
            tryVerify(function() { return quick.visible && picture.visible }, 3000)
            // A press on the title folds the block under it, and only that one.
            // (A layout gives an item its place at its next pass, not when it is shown.)
            const colour = head("color")
            const gap = () => colour.mapToItem(page, 0, 0).y - (head("fg").mapToItem(page, 0, 0).y + head("fg").height)
            tryVerify(function() { return gap() > 200 }, 3000, "open: the Frame Gen block stands between its title and the next (" + gap() + ")")
            head("fg").toggled(false)
            compare(veyra.preferences.pushProFolded, "fg", "the fold is a preference")
            tryVerify(function() { return !quick.visible && !head("fg").open }, 3000, "the block is not drawn")
            verify(head("fg").visible, "its title stays, to open it again")
            verify(picture.visible && page.blockOpen("quality") && !page.blockOpen("fg"))
            tryVerify(function() { return gap() >= 0 && gap() < 24 }, 3000, "what stood under it moves up: a folded block takes no room (" + gap() + ")")
            // A second one; the list keeps both.
            head("display").toggled(false)
            compare(veyra.preferences.pushProFolded, "fg,display")
            page.setBlockOpen("audio", true)
            compare(veyra.preferences.pushProFolded, "fg,display", "opening what is open changes nothing")
            // A tab of its own shows its block whatever is folded in the list.
            page.tab = "fg"
            tryVerify(function() { return quick.visible && !head("fg").visible }, 3000, "the Frame Gen tab is the Frame Gen block")
            verify(page.blockShown("fg") && !page.blockShown("quality"))
            page.tab = "all"
            tryVerify(function() { return !quick.visible && head("fg").visible }, 3000, "and the list still has it folded")
            // Opened again by its title.
            head("fg").toggled(true)
            compare(veyra.preferences.pushProFolded, "display")
            tryVerify(function() { return quick.visible && head("fg").open && gap() > 200 }, 3000)
            // The title is the button: a press on it does what the call does (the first title: it is in view).
            mouseClick(head("quality"))
            compare(veyra.preferences.pushProFolded, "display,quality", "a press on the title folds; under the pointer stands " + under(head("quality")))
            tryVerify(function() { return !picture.visible }, 3000)
            mouseClick(head("quality"))
            compare(veyra.preferences.pushProFolded, "display")
            tryVerify(function() { return picture.visible }, 3000)
            // What was saved wrongly folds nothing it should not.
            veyra.setPreference("pushProFolded", "nonsense,,quality")
            tryVerify(function() { return !picture.visible && quick.visible }, 3000)
            verify(page.blockOpen("display") && !page.blockOpen("quality"))
            veyra.setPreference("pushProFolded", "")
            tryVerify(function() { return picture.visible }, 3000)
            // Legacy has no such list: its tabs show their blocks, folded or not.
            veyra.setPreference("pushProFolded", "quality,fg")
            Theme.design = "legacy"
            tryCompare(page, "tab", "quality")
            tryVerify(function() { return picture.visible && !head("quality").visible }, 3000)
        }
        // 4.3.1: the settings side is hidden with the title bar's button, and the picture takes its room.
        function test_the_settings_side_is_hidden_and_the_picture_takes_its_room() {
            Theme.design = "new"
            page.tab = "all"
            const panel = control(page, "pro-inspector"), split = control(page, "pro-inspector-split")
            tryVerify(function() { return panel.visible && panel.width >= 376 }, 3000)
            const wide = page.videoViewport.width
            verify(!page.sideCollapsed && split.visible)
            veyra.setPreference("pushSideCollapsed", true)
            tryVerify(function() { return page.sideCollapsed && !panel.visible && !split.visible }, 3000, "the panel is not drawn, nor the handle beside it")
            compare(page.videoViewport.width, page.width - 28, "the picture's column is the page's width inside its margins")
            verify(page.videoViewport.width > wide + 370, "it took the panel's room: " + page.videoViewport.width)
            verify(page.videoViewport.x + page.videoViewport.width <= page.width)
            // The panel over the full-screen picture has its own way in and out: it is never hidden by this.
            page.overlay = true
            tryVerify(function() { return panel.visible && !page.sideCollapsed }, 3000, "the overlay panel is shown when asked for")
            page.overlay = false
            tryVerify(function() { return !panel.visible }, 3000)
            // The tour points at the panel: it is shown while the tour runs.
            page.onboardingActive = true
            tryVerify(function() { return panel.visible && Math.abs(page.videoViewport.width - wide) < 1 }, 3000)
            page.onboardingActive = false
            tryVerify(function() { return !panel.visible }, 3000)
            // Shown again: everything is where it was.
            veyra.setPreference("pushSideCollapsed", false)
            tryVerify(function() { return panel.visible && split.visible && Math.abs(page.videoViewport.width - wide) < 1 }, 3000)
            // Legacy has no such button, so a saved choice hides nothing there.
            veyra.setPreference("pushSideCollapsed", true)
            Theme.design = "legacy"
            tryVerify(function() { return panel.visible && !page.sideCollapsed }, 3000)
        }
        // 4.3.1: the motion source row is there for every method. For the methods that work out the motion
        // themselves it is greyed out and says so, instead of being gone (NVIDIA VFG) or doing nothing.
        function test_motion_source_row_stays_and_says_when_it_does_not_apply() {
            page.tab = "fg"
            const row = control(page, "list-fg-motion-row"), seg = control(page, "list-fg-motion")
            const was = veyra.fgBackendName
            const usual = "dlss"
            veyra.fgBackendName = usual
            tryVerify(function() { return row.visible && !row.ownMotion }, 3000)
            const hint = row.hint
            verify(hint.length > 0 && seg.options.length === 2 && seg.options[0].disabled !== true && seg.options[1].disabled !== true, "a choice that can be made")
            for (const own of ["vfg", "amdfrc", "pushflow"]) {
                veyra.fgBackendName = own
                tryVerify(function() { return row.ownMotion }, 3000, own)
                verify(row.visible, own + ": the row is still there")
                verify(seg.options[0].disabled === true && seg.options[1].disabled === true, own + ": greyed out")
                compare(seg.current, "", own + ": nothing reads as chosen")
                verify(row.hint.length > 0 && row.hint !== hint, own + ": it says why")
            }
            for (const other of ["dlss", "fsr3", "fsr4", "xess", "anr"]) {
                veyra.fgBackendName = other
                tryVerify(function() { return !row.ownMotion && row.visible }, 3000, other)
                verify(seg.options[0].disabled !== true && seg.options[1].disabled !== true, other)
                compare(row.hint, hint)
            }
            veyra.fgBackendName = was
        }

        // Crosshair Hold has its switch in Professional mode's own list (the quick setup card leaves the HUD
        // switches to it there), off unless switched on.
        // The half precision test needs the graphics card to itself. With something playing its button stops the
        // playback and starts the test once that has ended (a user's report: it only ever said "stop playback
        // first", also for a paused source, so the test could not be started from the interface).
        function test_half_precision_test_stops_playback_itself() {
            page.tab = "fg"
            const was = veyra.fgBackendName, ran = veyra.running
            veyra.fgBackendName = "pushflow"
            const row = control(page, "list-pushflow-half-row"), button = control(page, "list-pushflow-half-test")
            tryVerify(function() { return row.visible }, 3000)
            // nothing plays: the test starts at once, as it always did
            veyra.running = false
            let tests = veyra.halfTests, stops = veyra.playbackStops
            const idle = button.text
            button.clicked()
            compare(veyra.halfTests, tests + 1)
            compare(veyra.playbackStops, stops)
            verify(row.testing)
            tryVerify(function() { return !row.testing }, 3000)        // the fixture's test is over at the next poll
            // something plays: the button says so, stops it, waits, then tests
            veyra.running = true
            tryVerify(function() { return button.text !== idle }, 3000, "the button says it will stop playback")
            tests = veyra.halfTests; stops = veyra.playbackStops
            button.clicked()
            compare(veyra.playbackStops, stops + 1)
            compare(veyra.halfTests, tests, "no test while it still plays")
            verify(row.stopping && !button.enabled)
            wait(500)
            compare(veyra.halfTests, tests, "still none")
            veyra.running = false
            tryVerify(function() { return veyra.halfTests === tests + 1 && !row.stopping }, 3000, "started once playback has ended")
            tryVerify(function() { return !row.testing && button.text === idle }, 3000)
            // a test whose result says half precision is not faster reads as that, not as a failure
            const prefs = veyra.preferences
            veyra.preferences = ({ pushFlowHalfProbe: JSON.stringify({ ok: false, reason: "probe-stopped" }) })
            tryVerify(function() { return row.hint.length > 0 }, 3000)
            const failed = row.hint
            veyra.preferences = ({ pushFlowHalfProbe: JSON.stringify({ ok: false, reason: "not-faster" }) })
            tryVerify(function() { return row.hint.length > 0 && row.hint !== failed }, 3000)
            veyra.preferences = prefs
            veyra.fgBackendName = was; veyra.running = ran
        }

        function test_crosshair_hold_has_its_switch_in_the_frame_generation_list() {
            page.tab = "fg"
            const row = control(page, "list-fg-crosshair-row"), sw = control(page, "list-fg-crosshair")
            const was = veyra.fgBackendName
            veyra.fgBackendName = "vfg"
            tryVerify(function() { return row.visible }, 3000)
            verify(veyra.preferences.pushCrosshairHold !== true && !sw.checked, "off unless switched on")
            const off = row.hint
            verify(off.length > 0)
            sw.toggled(true)
            compare(veyra.preferences.pushCrosshairHold, true)
            tryVerify(function() { return sw.checked && row.hint !== off }, 3000)
            const on = row.hint
            veyra.fgBackendName = "anr"
            tryVerify(function() { return row.hint !== on && row.hint !== off }, 3000, "a method it does not work with yet is said")
            veyra.fgBackendName = "dlss"
            tryVerify(function() { return row.hint === on }, 3000, "DLSS frame generation is one it works with")
            veyra.fgBackendName = "pushflow"
            tryVerify(function() { return row.hint === on }, 3000)
            // The row says what the engine says (read once a second): the pass runs and holds a crosshair, or it
            // runs and has found none yet; the switch alone never reads as running.
            const stats = veyra.liveStats
            veyra.liveStats = ({ running: true, crosshairHoldState: 1, crosshairHoldPixels: 240 })
            tryVerify(function() { return row.hint !== on && row.hint !== off }, 4000, "a pass that holds a crosshair says so")
            const holding = row.hint
            veyra.liveStats = ({ running: true, crosshairHoldState: 1, crosshairHoldPixels: 0 })
            tryVerify(function() { return row.hint !== holding && row.hint !== on && row.hint !== off }, 4000, "a pass that has found no crosshair says so")
            veyra.liveStats = ({ running: true, crosshairHoldState: 3, crosshairHoldPixels: 0 })
            tryVerify(function() { return row.hint === on }, 4000, "no pass in the engine: on, not running")
            veyra.liveStats = stats
            sw.toggled(false)
            compare(veyra.preferences.pushCrosshairHold, false)
            tryVerify(function() { return !sw.checked && row.hint === off }, 3000)
            veyra.fgBackendName = was
        }

        function test_character_hold_has_its_switch_in_the_frame_generation_list() {
            page.tab = "fg"
            const row = control(page, "list-fg-character-row"), sw = control(page, "list-fg-character")
            const was = veyra.fgBackendName
            veyra.fgBackendName = "vfg"
            tryVerify(function() { return row.visible }, 3000)
            verify(veyra.preferences.pushCharacterHold !== true && !sw.checked, "off unless switched on")
            const off = row.hint
            verify(off.length > 0 && row.help.length > 0)
            sw.toggled(true)
            compare(veyra.preferences.pushCharacterHold, true)
            tryVerify(function() { return sw.checked && row.hint !== off }, 3000)
            const on = row.hint
            veyra.fgBackendName = "anr"
            tryVerify(function() { return row.hint !== on && row.hint !== off }, 3000, "a method it does not work with is said")
            veyra.fgBackendName = "vfg"
            tryVerify(function() { return row.hint === on }, 3000)
            const stats = veyra.liveStats
            veyra.liveStats = ({ running: true, characterHoldState: 1, characterHoldBlocks: 180 })
            tryVerify(function() { return row.hint !== on && row.hint !== off }, 4000, "a pass that has found people says so")
            const holding = row.hint
            veyra.liveStats = ({ running: true, characterHoldState: 1, characterHoldBlocks: 0 })
            tryVerify(function() { return row.hint !== holding && row.hint !== on && row.hint !== off }, 4000, "a pass that has found nobody says so")
            const looking = row.hint
            veyra.liveStats = ({ running: true, characterHoldState: 2, characterHoldReason: "no-files" })
            tryVerify(function() { return row.hint !== looking && row.hint !== holding && row.hint !== on && row.hint !== off }, 4000, "an install without the model says that")
            veyra.liveStats = ({ running: true, characterHoldState: 3 })
            tryVerify(function() { return row.hint === on }, 4000, "no pass in the engine: on, not running")
            veyra.liveStats = stats
            sw.toggled(false)
            compare(veyra.preferences.pushCharacterHold, false)
            tryVerify(function() { return !sw.checked && row.hint === off }, 3000)
            veyra.fgBackendName = was
        }

        function test_overlay_panel_is_as_wide_as_its_edge_is_dragged() {
            const panel = control(page, "pro-inspector"), grip = control(page, "pro-overlay-split")
            verify(!grip.visible, "docked beside the picture: the splitter between them is the handle")
            const docked = panel.width
            page.overlay = true
            tryVerify(function() { return grip.visible && grip.height > 300 }, 3000, "over the picture the panel's own edge is")
            compare(page.overlayWidth, 376, "the width it always had until it is dragged")
            compare(panel.width, 376 + page.overlayGrip)
            verify(page.overlayContains(panel.x + panel.width - 2, panel.y + 100), "a press on the grip belongs to the panel, not the picture")
            // Drag the edge 150 pixels to the right.
            const start = grip.mapToItem(page, grip.width / 2, 200)
            mousePress(page, start.x, start.y)
            mouseMove(page, start.x + 60, start.y)
            compare(Math.round(page.overlayWidth), 436, "the panel follows the mouse")
            mouseMove(page, start.x + 150, start.y)
            compare(Math.round(panel.width), 526 + page.overlayGrip)
            verify(veyra.preferences.pushOverlayPanelWidth === undefined, "nothing is saved while it is dragged")
            mouseRelease(page, start.x + 150, start.y)
            compare(veyra.preferences.pushOverlayPanelWidth, 526, "the width is remembered")
            compare(Math.round(page.overlayWidth), 526)
            // Never narrower than it was, never wider than leaves the picture room.
            const again = grip.mapToItem(page, grip.width / 2, 200)
            mousePress(page, again.x, again.y)
            mouseMove(page, again.x - 400, again.y)
            compare(page.overlayWidth, 376)
            mouseMove(page, again.x + 500, again.y)
            compare(page.overlayWidth, page.overlayMax)
            verify(page.overlayMax <= page.width * 0.6 + 0.5 && page.overlayMax >= 376)
            mouseRelease(page, again.x + 500, again.y)
            compare(veyra.preferences.pushOverlayPanelWidth, Math.round(page.overlayMax))
            // A double click puts it back, and its own release does not undo that.
            const edge = grip.mapToItem(page, grip.width / 2, 200)
            mouseDoubleClickSequence(page, edge.x, edge.y)
            compare(veyra.preferences.pushOverlayPanelWidth, 376)
            compare(page.overlayWidth, 376)
            // A saved width larger than a smaller window allows is held to the limit, not lost.
            veyra.setPreference("pushOverlayPanelWidth", 4000)
            compare(page.overlayWidth, page.overlayMax)
            compare(veyra.preferences.pushOverlayPanelWidth, 4000)
            // Docked again: its width is its own, as it was.
            page.overlay = false
            tryVerify(function() { return !grip.visible && Math.abs(panel.width - docked) < 1 }, 3000, "the docked panel keeps its own width")
        }
        function test_the_picture_settings_stand_in_the_quality_tab() {
            const picture = control(page, "list-pushfps-picture"), rest = control(page, "list-pushfps-quick")
            compare(picture.scope, "picture")
            compare(rest.scope, "rest")
            verify(picture.proPage && rest.proPage)
            page.tab = "quality"
            tryVerify(function() { return picture.visible && !rest.visible }, 3000)
            verify(control(picture, "pushfps-section-picture").visible && control(picture, "pushfps-crop-row").visible, "the Picture section, with the page's own enhancement card")
            verify(!control(picture, "pushfps-tools-row").visible && !control(picture, "pushfps-target-row").visible, "and nothing else of the quick setup")
            verify(findChild(page, "list-nr-stage") !== null && findChild(page, "list-sr") !== null)
            page.tab = "fg"
            tryVerify(function() { return rest.visible && !picture.visible }, 3000)
            verify(control(rest, "pushfps-tools-row").visible && control(rest, "pushfps-target-row").visible, "the Frame Gen tab keeps the rest")
            verify(!control(rest, "pushfps-section-picture").visible && !control(rest, "pushfps-crop-row").visible && !control(rest, "pushfps-fine-row").visible,
                   "and no longer the picture's settings")
            // 4.2.2: it says where they went, and its button opens that tab, where the enhancement's own
            // switch stands again.
            verify(control(rest, "pushfps-picture-moved-row").visible)
            control(rest, "pushfps-picture-moved-open").clicked()
            compare(page.tab, "quality")
            tryVerify(function() { return picture.visible && control(picture, "pushfps-enhance-row").visible }, 3000, "the enhancement's switch is in the Quality tab's picture card")
            verify(!control(picture, "pushfps-picture-moved-row").visible)
        }
    }
}
