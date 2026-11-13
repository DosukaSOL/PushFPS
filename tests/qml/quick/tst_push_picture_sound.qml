// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PushFPS picture editor (presets, gamma on the tone curve), sound profile and the
// recording-saved card, against the fixture bridge in QuickSmokeTests.cpp.
import QtQuick
import QtTest
import Veyra
Item {
    id: root
    width: 700; height: 900
    property var pages: []
    Column {
        width: 560
        PushPicture { id: pic; width: 560 }
        PushSoundProfile { id: sound; width: 560 }
    }
    PushRecordingSaved { id: card; x: 10; y: 700; goPage: p => root.pages.push(p) }

    TestCase {
        name: "PushPictureSound"
        when: windowShown
        function neutralColour() {
            return {index: 0, enabled: false, exposure: 0, contrast: 0, highlights: 0, shadows: 0, saturation: 0,
                    vibrance: 0, texture: 0, temperature: 0,
                    curves: [[{x:0,y:0},{x:1,y:1}],[{x:0,y:0},{x:1,y:1}],[{x:0,y:0},{x:1,y:1}],[{x:0,y:0},{x:1,y:1}],[{x:0,y:0},{x:1,y:1}]]}
        }
        function init() {
            veyra.preferences = ({exportStopsPlayback: false})
            veyra.colourState = neutralColour()
            veyra.colorEnabled = false
            veyra.colourUndoCount = 0
            veyra.fgEnabled = false
            veyra.lastOpenedPath = ""
            veyra.lastExportFiles = []
            veyra.recordingFolderOpened = false
            root.pages = []
        }
        function cleanup() { init() }

        // --- picture -------------------------------------------------------
        function test_neutral_is_natural() {
            compare(pic.currentPreset, "natural")
            compare(pic.gamma, 1.0)
            verify(pic.available)
        }
        function test_presets_apply_and_are_recognised() {
            for (const p of pic.presets) {
                pic.applyPreset(p)
                compare(pic.currentPreset, p.id, p.id)
                verify(veyra.colourState.enabled, "the colour stage is switched on")
                compare(pic.gamma, p.gamma, p.id + " gamma")
            }
            pic.applyPreset(pic.presets[1])
            compare(veyra.colourState.vibrance, 30)
            pic.applyPreset(pic.presets[0])
            compare(veyra.colourState.vibrance, 0, "Natural resets what the presets set")
            compare(veyra.colourState.curves[0].length, 2, "Natural restores the identity curve")
        }
        function test_one_slider_makes_it_custom() {
            pic.applyPreset(pic.presets[1])
            pic.setParam("contrast", 40)
            compare(pic.currentPreset, "custom")
        }
        function test_gamma_curve() {
            pic.setGamma(1.15)
            const pts = veyra.colourState.curves[0]
            compare(pts.length, 8)
            compare(pts[0].x, 0); compare(pts[0].y, 0, "black stays black")
            for (let i = 1; i < pts.length; ++i) {
                verify(pts[i].x > pts[i - 1].x, "points sorted by x")
                verify(pts[i].y >= pts[i - 1].y, "curve rises")
                verify(pts[i].y >= 0 && pts[i].y <= 1, "points in range")
            }
            verify(pts[3].y > pts[3].x, "gamma above 1 brightens the midtones")
            compare(pic.gamma, 1.15, "the card reads its own curve back")
            pic.setGamma(0.8)
            verify(veyra.colourState.curves[0][3].y < 0.4, "gamma below 1 darkens the midtones")
            compare(pic.gamma, 0.8)
            pic.setGamma(1.0)
            compare(veyra.colourState.curves[0].length, 2, "gamma 1 is the identity curve")
            pic.setGamma(5)
            compare(pic.gamma, 1.6, "gamma clamps to 1.6")
        }
        function test_hand_drawn_curve_is_custom() {
            veyra.setColourCurve(0, [{x: 0, y: 0}, {x: 0.5, y: 0.7}, {x: 1, y: 1}])
            compare(pic.gamma, -1)
            compare(pic.currentPreset, "custom")
            const row = findChild(pic, "pushfps-picture-gamma")
            verify(row !== null)
            verify(row.hint.length > 0, "the gamma row explains the custom curve")
        }
        function test_no_colour_node() {
            const s = neutralColour(); s.index = -1; veyra.colourState = s
            verify(!pic.available)
            compare(pic.currentPreset, "")
        }

        // --- sound ---------------------------------------------------------
        function test_sound_preset() {
            compare(sound.preset, 0)
            sound.setPreset("3")
            compare(veyra.preferences.soundPreset, 3)
            compare(sound.preset, 3)
            verify(findChild(sound, "pushfps-sound-curve").visible, "curve shows for an active preset")
            compare(findChild(sound, "pushfps-eq-band-0"), null, "no EQ bands outside Custom")
        }
        function test_custom_eq() {
            sound.setPreset("6")
            wait(0)
            verify(findChild(sound, "pushfps-eq-band-4") !== null, "five EQ bands in Custom")
            sound.setBand(2, 13.3)
            compare(veyra.preferences.soundEq, "0,0,12,0,0", "gain clamps to +12")
            sound.setBand(0, -3.26)
            compare(veyra.preferences.soundEq, "-3.5,0,12,0,0", "half-dB steps")
            compare(sound.eq[0], -3.5)
        }
        function test_bad_sound_preferences() {
            veyra.preferences = ({soundPreset: 42, soundEq: "a,b"})
            compare(sound.preset, 0)
            compare(JSON.stringify(sound.eq), JSON.stringify([0, 0, 0, 0, 0]))
        }

        // --- recording card -------------------------------------------------
        function save(n, path, active) {
            veyra.recordingState = ({saved: n, active: active, path: path})
            veyra.recordingChanged()
        }
        function click(name) {
            tryVerify(() => card.visible && card.opacity > 0.99, 2000, "card fully shown")
            const b = findChild(card, name)
            verify(b !== null, name)
            mouseClick(b)
        }
        function test_card_after_a_saved_recording() {
            card.on = false
            card.lastSaved = 0
            save(0, "", true)
            verify(!card.on, "nothing while recording")
            save(1, "C:/Videos/PushFPS/rec-1.mp4", false)
            verify(card.on, "card shows after a save")
            compare(card.fileName, "rec-1.mp4")
            click("pushfps-recording-edit")
            compare(veyra.lastOpenedPath, "C:/Videos/PushFPS/rec-1.mp4")
            compare(root.pages[root.pages.length - 1], "pro")
            verify(!card.on)
            save(2, "C:/Videos/PushFPS/rec-2.mp4", false)
            click("pushfps-recording-export")
            compare(veyra.lastExportFiles[0], "C:/Videos/PushFPS/rec-2.mp4")
            compare(root.pages[root.pages.length - 1], "exp")
            save(3, "C:/Videos/PushFPS/rec-3.mp4", false)
            click("pushfps-recording-folder")
            verify(veyra.recordingFolderOpened)
        }
        function test_frame_generation_note() {
            card.lastSaved = 10
            veyra.fgEnabled = true
            save(11, "C:/r.mp4", false)
            verify(card.on)
            card.on = false
        }
    }
}
