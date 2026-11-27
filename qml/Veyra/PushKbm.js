// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// The keyboard and mouse as a controller: the saved preference "pushKbm" read and changed for the
// interface (PushKbmDialog.qml, the quick setup row). Pure (no qsTr, no Qt), tested with Node
// (tests/js/push_kbm_test.js). The program reads the same preference in bridge/PushKbm.cpp and puts
// every value back in its range there (remoteplay/PushKbm.h), so nothing here can make it unsafe.
.pragma library

// Mouse inputs, as remoteplay/PushKbm.h numbers them (keys are Windows' virtual-key codes).
var mouseCodes = [0x101, 0x102, 0x103, 0x104, 0x105, 0x110, 0x111]
var presets = ["gta6", "gta6classic", "rdr2"]

function copyMap(map) {
    var out = {}
    for (var k in map) out[k] = [Number(map[k][0]) || 0, Number(map[k][1]) || 0]
    return out
}

// The saved text over the defaults (veyra.pushKbmDefaults()). Text that does not read is the defaults.
function config(text, defaults) {
    var d = defaults || {}
    var saved = {}
    if (typeof text === "string" && text.length > 0) {
        try { var parsed = JSON.parse(text); if (parsed && typeof parsed === "object" && !Array.isArray(parsed)) saved = parsed } catch (e) { saved = {} }
    }
    var pick = function (key, fallback) { return saved[key] !== undefined ? saved[key] : (d[key] !== undefined ? d[key] : fallback) }
    var map = copyMap(d.map || {})
    var savedMap = saved.map && typeof saved.map === "object" ? saved.map : {}
    for (var k in savedMap) if (Array.isArray(savedMap[k])) map[k] = [Number(savedMap[k][0]) || 0, Number(savedMap[k][1]) || 0]
    return {
        on: pick("on", false) === true,
        preset: String(pick("preset", "gta6")),
        pad: String(pick("pad", "")),
        sens: Number(pick("sens", 50)),
        vert: Number(pick("vert", 100)),
        dz: Number(pick("dz", 15)),
        curve: Number(pick("curve", 100)),
        invert: pick("invert", false) === true,
        release: Number(pick("release", 0x7B)),
        map: map
    }
}

function text(cfg) { return JSON.stringify(cfg) }

function changed(cfg, key, value) {
    var out = JSON.parse(JSON.stringify(cfg))
    out[key] = value
    return out
}

// Another layout: its inputs (veyra.pushKbmPresetMap(id)) replace every binding.
function withPreset(cfg, id, presetMap) {
    if (presets.indexOf(id) < 0 || !presetMap) return cfg
    var out = changed(cfg, "preset", id)
    out.map = copyMap(presetMap)
    return out
}

// One input slot changed. An input can press one control only, so it leaves wherever else it was; the
// release key cannot be bound (null: refused). The layout is "custom" from then on.
function withBinding(cfg, controlId, slot, code) {
    code = Number(code) || 0
    if (code !== 0 && code === cfg.release) return null
    if (slot !== 0 && slot !== 1) return cfg
    var out = JSON.parse(JSON.stringify(cfg))
    if (!out.map[controlId]) out.map[controlId] = [0, 0]
    if (code !== 0)
        for (var k in out.map) for (var s = 0; s < 2; ++s) if (out.map[k][s] === code) out.map[k][s] = 0
    out.map[controlId][slot] = code
    out.preset = "custom"
    return out
}

// The release key: a keyboard key, taken out of whatever control had it.
function withRelease(cfg, code) {
    code = Number(code) || 0
    if (code < 0x08 || code > 0xFE) return cfg
    var out = JSON.parse(JSON.stringify(cfg))
    for (var k in out.map) for (var s = 0; s < 2; ++s) if (out.map[k][s] === code) { out.map[k][s] = 0; out.preset = "custom" }
    out.release = code
    return out
}

// Which buttons' names to show: the choice made, else Xbox's while an Xbox plays or the console chosen is
// an Xbox, else PlayStation's.
function padOf(cfg, consoleId, target) {
    if (cfg && (cfg.pad === "ps" || cfg.pad === "xbox")) return cfg.pad
    return target === "xbox" || consoleId === "xbox" ? "xbox" : "ps"
}

// The controls the dialog lists for this pad: Create is a PS5 button only (on an Xbox it would be View
// a second time).
function controlsFor(all, pad) {
    var out = []
    for (var i = 0; i < all.length; ++i) if (!(pad === "xbox" && all[i] === "create")) out.push(all[i])
    return out
}

// Which of the status lines applies (the words are the interface's): off, using, ready, waiting.
function status(state, on) {
    if (!on) return "off"
    if (state && state.captured === true) return "using"
    if (state && state.available === true) return "ready"
    return "waiting"
}

// Waiting (switched on, nothing takes them): the one step that fixes what is missing, as an action id the
// interface words and runs, or "" when there is nothing to press (connect a console, reconnect a stream).
// `waiting` is the bridge's kWaitingIds (remoteplay/PushKbm.h); `paired` whether a PS5 is paired.
//   control-on  switch on "Control the PS5 through Remote Play" (a capture card brings only the picture)
//   pair        open PS5 streaming to pair the PS5 once (what controller control needs)
//   reconnect   reconnect PS5 controller control
//   capture     open the capture card window (the PS5 asks for its sign-in PIN there)
//   ps5 / xbox  open that window (view only, or the Xbox controller switched off)
function fixFor(waiting, paired) {
    switch (String(waiting || "")) {
    case "capture-off": return paired === true ? "control-on" : "pair"
    case "capture-pair": return "pair"
    case "capture-failed": return "reconnect"
    case "capture-pin": return "capture"
    case "viewonly": return "ps5"
    case "xboxpad": return "xbox"
    }
    return ""
}
