// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Wide-screen fill (FillRow.qml): the Windows quick setup's rules (PushQuickSetup.qml fillId, fillShown),
// on the preview's own fill (FrameView::fill: 0 bars, 1 fill and crop, 2 stretch) and the Windows
// preference pushAmbientFill. No Qt in here.
.pragma library

// A screen clearly wider than 16:9 (Windows: wideScreen).
function wide(screenAspect) { return Number(screenAspect) > 1.9 }

// The Windows row is shown on such a screen only. (On Windows it is also shown wherever a choice other than
// bars is on, so it can be undone; the preview always has its own shape rows for that, and a row that came
// and went with the choice moved it from under the mouse: the user's report.) On any other screen the
// preview's own rows (PictureSettings.qml home-fill-row, Professional mode's display-aspect) carry the choice.
function shown(screenAspect) { return wide(screenAspect) }

// The choice shown: "fit", "ambient", "crop" or "stretch".
function current(fillMode, ambientOn) {
    const m = Number(fillMode)
    return m === 1 ? "crop" : m === 2 ? "stretch" : ambientOn === true ? "ambient" : "fit"
}

// What a choice sets: the fill mode and the preference.
function settingsFor(id) {
    if (id === "fit") return { fillMode: 0, ambient: false }
    if (id === "ambient") return { fillMode: 0, ambient: true }
    if (id === "crop") return { fillMode: 1, ambient: false }
    if (id === "stretch") return { fillMode: 2, ambient: false }
    return null
}

// Applies a choice through the app object (fillMode and the preference), as every fill row does.
function choose(app, id) {
    const s = settingsFor(id)
    if (!s) return
    if (app.fillMode !== s.fillMode) app.fillMode = s.fillMode
    if ((app.preferences.pushAmbientFill === true) !== s.ambient) app.setPreference("pushAmbientFill", s.ambient)
}

// How much of a 16:9 picture is lost (crop: of its height) or how much wider it becomes (stretch).
function cropPercent(screenAspect) { return screenAspect > 16 / 9 ? Math.round((1 - (16 / 9) / screenAspect) * 100) : 0 }
function stretchPercent(screenAspect) { return screenAspect > 16 / 9 ? Math.round((screenAspect / (16 / 9) - 1) * 100) : 0 }
