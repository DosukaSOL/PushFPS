// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// HUD protection (PushHudDialog: automatic detection plus the box editor), the
// comparison clip and HUD buttons of the quick setup, the "?" help marks (VHelp,
// VRow.help) and the PushFPS wordmark, against the fixture bridge.
import QtQuick
import QtQuick.Layouts
import QtTest
import Veyra
Item {
    id: root
    width: 900; height: 1100
    Item { id: host; anchors.fill: parent; property string dialog: ""; function close() { dialog = "" } }
    PushHudDialog { id: hud; dialogHost: host; timeoutEnabled: false }
    PushQuickSetup { id: quick; width: 560; y: 700 }
    PushWordmark { id: mark; x: 10; y: 10; name: "PushFPS" }
    PushWordmark { id: otherMark; x: 10; y: 60; name: "Veyra" }
    ColumnLayout {
        y: 100; width: 420
        VRow { id: helpRow; label: "Frame rate"; help: "What it does"; VSwitch {} }
        VRow { id: plainRow; label: "Plain"; VSwitch {} }
    }

    function done(regions, extra) {
        return Object.assign({ running: false, done: true, failed: false, taken: 12, target: 12, regions: regions, preview: "",
                               tooStill: false, motion: 20 }, extra || {})
    }

    TestCase {
        name: "PushHud"
        when: windowShown
        function init() {
            // Close first: closing a dialog that is still sampling cancels it (counted below).
            host.dialog = ""
            veyra.hasSource = true
            veyra.exclusionState = ({ enabled: false, feather: 6, regions: [] })
            veyra.lastRegions = null
            veyra.hudState = ({ running: false, done: false, failed: false, taken: 0, target: 0, regions: [], preview: "" })
            veyra.hudStarts = 0; veyra.hudCancels = 0; veyra.hudLastSamples = 0
            veyra.compareStarts = 0; veyra.recording = false; veyra.recordingState = ({ saved: 0, active: false, path: "" })
            host.dialog = ""
            hud.phase = "idle"
            hud.clearBoxes()
            hud.message = ""
        }
        function open() {
            host.dialog = "hud"
            verify(hud.shown)
        }

        function test_opens_with_the_current_regions_and_takes_a_picture() {
            veyra.exclusionState = ({ enabled: true, feather: 10, regions: [{ left: 0.05, top: 0.7, right: 0.25, bottom: 0.95, ellipse: true },
                                                                           { left: 0.8, top: 0.05, right: 0.97, bottom: 0.15 }] })
            open()
            compare(hud.boxes.count, 2)
            compare(hud.feather, 10)
            compare(hud.boxes.get(0).ellipse, true)
            compare(veyra.hudLastSamples, 1, "a single frame for the editor")
            compare(hud.phase, "picture")
            veyra.hudState = root.done([], { taken: 1, target: 1, preview: "file:///tmp/none.png" })
            tryCompare(hud, "phase", "idle", 2000)
            compare(hud.picture, "file:///tmp/none.png")
            compare(hud.boxes.count, 2, "a picture alone keeps the boxes")
        }
        function test_detection_proposes_boxes_and_apply_sets_them() {
            open()
            hud.phase = "idle"
            hud.detect()
            compare(hud.phase, "watching")
            compare(veyra.hudLastSamples, 12)
            compare(hud.actions.length, 1, "only Cancel while watching")
            veyra.hudState = root.done([{ left: 0.7, top: 0.7, right: 0.95, bottom: 0.95 }, { left: 0.02, top: 0.02, right: 0.3, bottom: 0.1 }])
            tryCompare(hud, "phase", "idle", 2000)
            compare(hud.boxes.count, 2)
            verify(!hud.messageError)
            verify(hud.message.indexOf("2") >= 0, hud.message)
            compare(hud.actions.length, 2)
            verify(hud.apply())
            compare(veyra.lastRegions.length, 2)
            fuzzyCompare(veyra.lastRegions[0].left, 0.7, 1e-6)
            fuzzyCompare(veyra.lastRegions[1].bottom, 0.1, 1e-6)
            compare(veyra.exclusionState.feather, 6, "keeps the feather it opened with")
        }
        function test_boxes_found_in_a_list_from_cpp_are_kept() {
            // The bridge hands its regions over as a QVariantList, which Qt 6.8 gives QML as a list that has a length
            // and items but is not an Array: an object shaped like that stands in for it here.
            open()
            hud.phase = "idle"
            hud.detect()
            const fromCpp = { length: 1, 0: { left: 0.6, top: 0.6, right: 0.9, bottom: 0.9 } }
            verify(!Array.isArray(fromCpp))
            veyra.hudState = root.done(fromCpp)
            tryCompare(hud, "phase", "idle", 2000)
            compare(hud.boxes.count, 1, "the box found is kept")
            verify(!hud.messageError)
        }
        function test_a_still_picture_keeps_the_boxes_and_explains() {
            veyra.exclusionState = ({ enabled: true, feather: 6, regions: [{ left: 0.1, top: 0.1, right: 0.2, bottom: 0.2 }] })
            open()
            hud.phase = "idle"
            hud.detect()
            veyra.hudState = root.done([], { tooStill: true, motion: 0.5 })
            tryCompare(hud, "phase", "idle", 2000)
            verify(hud.messageError)
            compare(hud.boxes.count, 1, "nothing replaced")
        }
        function test_nothing_found_says_so() {
            open()
            hud.phase = "idle"
            hud.detect()
            veyra.hudState = root.done([])
            tryCompare(hud, "phase", "idle", 2000)
            verify(!hud.messageError && hud.message.length > 0)
            compare(hud.boxes.count, 0)
        }
        function test_failure_is_reported() {
            open()
            hud.phase = "idle"
            hud.detect()
            veyra.hudState = ({ running: false, done: false, failed: true, taken: 0, target: 12, regions: [], error: "format" })
            tryCompare(hud, "phase", "idle", 2000)
            verify(hud.messageError)
            compare(hud.message, "format")
        }
        function test_cancel_and_close_stop_the_detection() {
            open()
            hud.phase = "idle"
            hud.detect()
            hud.cancel()
            compare(hud.phase, "idle")
            compare(veyra.hudCancels, 1)
            hud.detect()
            host.dialog = ""
            compare(veyra.hudCancels, 2, "closing cancels it")
            compare(hud.phase, "idle")
        }
        function test_no_picture_without_a_source() {
            veyra.hasSource = false
            open()
            compare(veyra.hudStarts, 0)
            hud.detect()
            verify(hud.messageError)
            compare(hud.phase, "idle")
            compare(findChild(hud, "pushfps-hud-detect").enabled, false)
        }
        function test_manual_editing() {
            open()
            hud.phase = "idle"
            compare(hud.addBox(), 0)
            compare(hud.selected, 0)
            hud.moveBox(0, 0.95, 0.95)
            let b = hud.boxes.get(0)
            verify(Math.abs(b.r - 1) < 1e-6 && Math.abs(b.b - 1) < 1e-6, "kept inside the picture")
            verify(Math.abs((b.r - b.l) - 0.2) < 1e-6 && Math.abs((b.b - b.t) - 0.16) < 1e-6, "size kept while moving")
            hud.resizeBox(0, 0, 0.99, 0.99)
            b = hud.boxes.get(0)
            verify(b.r - b.l >= hud.minSize - 1e-6 && b.b - b.t >= hud.minSize - 1e-6, "a corner cannot cross the other side")
            hud.resizeBox(0, 0, 0.5, 0.6)
            b = hud.boxes.get(0)
            verify(Math.abs(b.l - 0.5) < 1e-6 && Math.abs(b.t - 0.6) < 1e-6 && Math.abs(b.r - 1) < 1e-6, "top-left corner resizes")
            hud.resizeBox(0, 3, 0.8, 0.9)
            b = hud.boxes.get(0)
            verify(Math.abs(b.r - 0.8) < 1e-6 && Math.abs(b.b - 0.9) < 1e-6, "bottom-right corner resizes")
            hud.addBox(); hud.addBox(); hud.addBox()
            compare(hud.boxes.count, 4)
            compare(hud.addBox(), -1, "four at most")
            hud.removeBox(1)
            compare(hud.boxes.count, 3)
            hud.clearBoxes()
            compare(hud.boxes.count, 0)
            verify(hud.apply())
            compare(veyra.lastRegions.length, 0)
            compare(veyra.exclusionState.enabled, false, "no boxes turns HUD protection off")
        }
        function test_draw_a_box_with_the_mouse() {
            open()
            hud.phase = "idle"
            const area = findChild(hud, "pushfps-hud-draw")
            verify(area !== null)
            tryVerify(() => area.width > 100 && area.height > 50, 3000)
            const p = area.mapToItem(root, 0, 0)
            if (p.x < 0 || p.y < 0 || p.x + area.width > root.width || p.y + area.height > root.height) skip("editor not fully inside the test window")
            mousePress(area, area.width * 0.1, area.height * 0.1)
            mouseMove(area, area.width * 0.3, area.height * 0.25)
            mouseRelease(area, area.width * 0.3, area.height * 0.25)
            compare(hud.boxes.count, 1)
            const b = hud.boxes.get(0)
            verify(Math.abs(b.l - 0.1) < 0.02 && Math.abs(b.r - 0.3) < 0.02 && Math.abs(b.t - 0.1) < 0.02 && Math.abs(b.b - 0.25) < 0.02,
                   [b.l, b.t, b.r, b.b].join(","))
            // A click without a drag draws nothing.
            mouseClick(area, area.width * 0.6, area.height * 0.6)
            compare(hud.boxes.count, 1)
        }

        // --- quick setup buttons ---------------------------------------------------
        function test_quick_setup_hud_and_comparison_buttons() {
            const list = []
            const handler = p => list.push(p)
            veyra.navigate.connect(handler)
            findChild(quick, "pushfps-hud").clicked()
            veyra.navigate.disconnect(handler)
            compare(list.join(","), "pro,hud", "the picture is shown before it is sampled")
            const clip = findChild(quick, "pushfps-compare-clip")
            verify(clip.enabled)
            const opened = []
            const onOpen = p => opened.push(p)
            veyra.navigate.connect(onOpen)
            clip.clicked()
            veyra.navigate.disconnect(onOpen)
            compare(opened.join(","), "compare", "the tile opens the comparison dialog (layout and length)")
            compare(veyra.compareStarts, 0, "nothing records before the dialog's start button")
            veyra.recording = true
            veyra.recordingState = ({ saved: 0, active: true, path: "", compare: true })
            compare(clip.text, qsTr("停止对比片段"))
            clip.clicked()
            compare(veyra.recording, false, "the button stops the comparison clip")
            veyra.recording = true
            veyra.recordingState = ({ saved: 0, active: true, path: "", compare: false })
            compare(clip.enabled, false, "not while a normal recording runs")
        }

        // --- help marks and wordmark ---------------------------------------------
        function test_help_mark_beside_the_label() {
            const mark = findChild(helpRow, "vrow-help")
            verify(mark !== null && mark.visible)
            compare(mark.text, "What it does")
            verify(!mark.open)
            mark.pinned = true
            verify(mark.open, "clicking opens the explanation")
            mark.pinned = false
            mark.forceActiveFocus()
            if (mark.activeFocus) verify(mark.open, "keyboard focus opens it too")
            const none = findChild(plainRow, "vrow-help")
            verify(none === null || !none.visible, "rows without help have no mark")
        }
        function test_wordmark_colours() {
            compare(mark.head, "Push")
            compare(mark.tail, "FPS")
            compare(String(mark.tailColor), String(Theme.brandGreen))
            compare(String(mark.color), Theme.light ? String(Theme.t1) : "#ffffff")
            verify(mark.text.indexOf(String(Theme.brandGreen)) > 0 && mark.text.indexOf("FPS") > 0, mark.text)
            compare(otherMark.head, "Veyra")
            compare(otherMark.text, "Veyra", "a name without FPS is one colour")
        }
    }
}
