// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// What "Test my graphics card" makes of the benchmark's runs (PushCardTest.js), also when the runs come as a
// list from C++, which Qt 6.8 hands to QML with a length and items but not as an Array.
'use strict'
const t = require('./load.js')
const C = t.load('PushCardTest.js')

const run = { width: 1920, height: 1080, tier: 'Fast', precision: 'fp32', networkMs: { p50: 4 },
              pairWallMs: { '2x': { p95: 6 }, '3x': { p95: 12 }, '4x': { p95: 30 } } }
const asArray = C.rows([run])
t.same(asArray.length, 1, 'one run, one row')
t.same(asArray[0].for30, 3, 'a 30 FPS game: 3X fits (12 ms within 26.7), 4X does not')
t.same(asArray[0].for60, 3, 'a 60 FPS game: 3X fits (12 ms within 13.3), 4X does not')
const fromCpp = { length: 1, 0: run }
t.check(!Array.isArray(fromCpp), 'the stand-in is not an Array')
t.same(C.rows(fromCpp), asArray, 'a list from C++ gives the same rows')
t.same(C.tierFor(fromCpp, 1080, 30, 3), 'Fast', 'and the same advice')
t.same(C.rows(null), [], 'nothing: no rows')
t.same(C.rows(undefined), [], 'nothing: no rows')
t.same(C.rows('abc'), [], 'text is not a list of runs')
t.same(C.rows({ runs: [run] }), [], 'an object without a length is not a list')
t.done('push_card_test')
