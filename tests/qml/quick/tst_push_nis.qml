// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// NVIDIA Image Scaling in the super-resolution editor (SrStageEditor), against the
// fixture bridge in QuickSmokeTests.cpp: the sharpness row, its preference and the
// single-pass rule (no NIS as the second pass of double SR).
import QtQuick
import QtTest
import Veyra
Item {
    id: root
    width: 700; height: 900
    SrStageEditor { id: editor; width: 600; namePrefix: "t" }

    TestCase {
        name: "PushNis"
        when: windowShown
        function init() {
            veyra.preferences = ({exportStopsPlayback: false})
            veyra.videoSrQuality = 2
            veyra.dualSrQuality = 2
            veyra.dualSrEnabled = false
        }
        function cleanup() { init() }

        function test_sharpness_row_only_for_nis() {
            const row = findChild(editor, "t-nis-sharpness-row")
            verify(row !== null)
            verify(!row.visible, "hidden for RTX Video SR")
            veyra.videoSrQuality = 6
            verify(row.visible, "shown for NIS")
            compare(editor.nisSharpness, 50, "NVIDIA's default when nothing is saved")
            compare(row.value, "50%")
        }
        function test_sharpness_preference() {
            veyra.videoSrQuality = 6
            const slider = findChild(editor, "t-nis-sharpness")
            verify(slider !== null)
            slider.moved(73.4)
            compare(veyra.preferences.nisSharpness, 73, "rounded percent saved")
            compare(editor.nisSharpness, 73)
            slider.moved(250)
            compare(veyra.preferences.nisSharpness, 100, "clamped to 100")
            veyra.preferences = ({nisSharpness: "junk"})
            compare(editor.nisSharpness, 50, "a bad saved value falls back to 50")
            veyra.preferences = ({nisSharpness: -5})
            compare(editor.nisSharpness, 50)
        }
        function test_nis_is_single_pass() {
            veyra.videoSrQuality = 5
            veyra.dualSrEnabled = true
            const second = findChild(editor, "t-dual-sr-algorithm")
            verify(second !== null)
            verify(second.options.length > 0)
            verify(!second.options.some(o => o.id === "6"), "NIS is not offered for the second pass")
            verify(second.options.some(o => o.id === "5"), "the other algorithms stay")
        }
        function test_dual_toggle_disabled_for_nis() {
            veyra.videoSrQuality = 6
            let row = findChild(editor, "t-dual-sr-toggle")
            while (row && row.label === undefined) row = row.parent
            verify(row !== null, "the double SR row")
            verify(!row.enabled, "double SR cannot be turned on with NIS")
            veyra.videoSrQuality = 5
            verify(row.enabled)
        }
    }
}
