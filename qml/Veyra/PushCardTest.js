// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// PushFPS graphics card test: what the benchmark's timings mean for playing.
//
// PushFlowBench (tools/pushflow_bench) reports, per picture size and tier, how long all generated
// frames of one pair of game frames take ("pairWallMs", for 2X, 3X and 4X). A game frame arrives
// every 1000 / gameFps milliseconds, and the frames between two of them have to be ready well inside
// that: here within 80 % of it, at the 95th percentile of the measured pairs.
.pragma library

var kBudgetShare = 0.8

function sizeLabel(run) {
    var h = Number(run.height || 0)
    return h === 2160 ? "4K" : h === 1440 ? "1440p" : h === 1080 ? "1080p" : (Number(run.width || 0) + " × " + h)
}
function pairMs(run, multiplier) {
    var s = run && run.pairWallMs ? run.pairWallMs[multiplier + "x"] : null
    var v = s ? Number(s.p95) : NaN
    return isFinite(v) && v > 0 ? v : NaN
}
function fits(run, gameFps, multiplier) {
    var ms = pairMs(run, multiplier)
    return isFinite(ms) && gameFps > 0 && ms <= kBudgetShare * 1000 / gameFps
}
// The largest of 2X, 3X, 4X that fits (0: none). 3X only counts when 2X fits, 4X when 3X does.
function best(run, gameFps) {
    var m = 0
    for (var k = 2; k <= 4; ++k) { if (!fits(run, gameFps, k)) break; m = k }
    return m
}
// One row per run, in the benchmark's order: { size, tier, networkMs, for30, for60 }.
function rows(runs) {
    var out = []
    // A list from C++ (the bridge's QVariantList) is not an Array to Array.isArray in Qt 6.8; it has a length.
    var list = runs && typeof runs.length === "number" ? Array.prototype.slice.call(runs) : []
    for (var i = 0; i < list.length; ++i) {
        var r = list[i]
        if (!r || typeof r !== "object") continue
        var net = r.networkMs ? Number(r.networkMs.p50) : NaN
        out.push({ size: sizeLabel(r), height: Number(r.height || 0), tier: String(r.tier || ""), precision: String(r.precision || ""),
                   networkMs: isFinite(net) ? net : 0, for30: best(r, 30), for60: best(r, 60) })
    }
    return out
}
// For a picture height: the best-looking tier that still reaches `multiplier` for a `gameFps` game
// ("" when none does). Tiers from best picture to fastest.
function tierFor(runs, height, gameFps, multiplier) {
    var order = ["Quality", "Balanced", "Fast"]
    var list = rows(runs)
    for (var t = 0; t < order.length; ++t)
        for (var i = 0; i < list.length; ++i)
            if (list[i].height === height && list[i].tier === order[t] && (gameFps === 60 ? list[i].for60 : list[i].for30) >= multiplier)
                return order[t]
    return ""
}
