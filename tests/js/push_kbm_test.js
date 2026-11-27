// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// The keyboard and mouse settings as the interface reads and changes them (qml/Veyra/PushKbm.js).
'use strict'
const t = require('./load.js')
const K = t.load('PushKbm.js')

const defaults = { on: false, preset: 'gta6', sens: 50, vert: 100, dz: 15, curve: 100, invert: false, release: 0x7B,
                   map: { cross: [32, 0], circle: [0xA2, 0], moveUp: [87, 0], r2: [0x101, 0] } }

let c = K.config('', defaults)
t.same(c.on, false, 'nothing saved: off')
t.same(c.map.cross, [32, 0], 'nothing saved: the default inputs')
t.same(c.release, 0x7B, 'nothing saved: F12 gives the keyboard back')
t.same(K.config('{oops', defaults).sens, 50, 'text that does not read is the defaults')
t.same(K.config('[1,2]', defaults).preset, 'gta6', 'an array is not settings')

c = K.config(JSON.stringify({ on: true, sens: 70, map: { cross: [65, 66] } }), defaults)
t.same([c.on, c.sens, c.map.cross, c.map.moveUp], [true, 70, [65, 66], [87, 0]], 'saved values over the defaults, the rest kept')

const p = K.withPreset(c, 'rdr2', { cross: [0xA0, 0], square: [32, 0] })
t.same([p.preset, p.map.cross, p.map.square, p.map.moveUp], ['rdr2', [0xA0, 0], [32, 0], undefined], 'a layout replaces every binding')
t.check(K.withPreset(c, 'nope', {}) === c, 'an unknown layout changes nothing')

let b = K.withBinding(c, 'circle', 1, 65)
t.same([b.map.circle, b.map.cross, b.preset], [[0xA2, 65], [0, 66], 'custom'], 'an input moves to the control it was given to and the layout is custom')
t.check(K.withBinding(c, 'circle', 0, 0x7B) === null, 'the release key cannot be bound')
b = K.withBinding(c, 'cross', 0, 0)
t.same(b.map.cross, [0, 66], 'a slot can be cleared')
t.check(K.withBinding(c, 'cross', 2, 65) === c, 'there are two slots')
b = K.withBinding(c, 'l3', 0, 70)
t.same(b.map.l3, [70, 0], 'a control nothing had gets its slots')

const r = K.withRelease(c, 65)
t.same([r.release, r.map.cross, r.preset], [65, [0, 66], 'custom'], 'a new release key leaves the control that had it')
t.check(K.withRelease(c, 0x101) === c, 'a mouse button cannot be the release key')

t.same(K.padOf({ pad: 'xbox' }, 'ps5', 'ps5'), 'xbox', 'the names chosen win')
t.same(K.padOf({ pad: '' }, 'ps5', 'xbox'), 'xbox', 'an Xbox stream shows Xbox names')
t.same(K.padOf({ pad: '' }, 'xbox', ''), 'xbox', 'an Xbox console shows Xbox names')
t.same(K.padOf({ pad: '' }, 'ps5pro', ''), 'ps', 'otherwise PlayStation names')

t.same(K.controlsFor(['cross', 'create', 'home'], 'xbox'), ['cross', 'home'], 'an Xbox has no Create button of its own')
t.same(K.controlsFor(['cross', 'create', 'home'], 'ps'), ['cross', 'create', 'home'], 'a PS5 lists Create')

t.same([K.status({}, false), K.status({ captured: true }, true), K.status({ available: true }, true), K.status({}, true)],
       ['off', 'using', 'ready', 'waiting'], 'the status line follows the state')

// A capture card brings only the picture: the fix is controller control (or pairing the PS5 first).
t.same([K.fixFor('capture-off', true), K.fixFor('capture-off', false), K.fixFor('capture-pair', true),
        K.fixFor('capture-failed', true), K.fixFor('capture-pin', true), K.fixFor('viewonly', true), K.fixFor('xboxpad', false)],
       ['control-on', 'pair', 'pair', 'reconnect', 'capture', 'ps5', 'xbox'], 'each missing step has its button')
t.same([K.fixFor('', true), K.fixFor('nosource', true), K.fixFor('source', true), K.fixFor('pc', true),
        K.fixFor('capture-starting', true), K.fixFor('connecting', true), K.fixFor(undefined, true)],
       ['', '', '', '', '', '', ''], 'nothing to press where waiting is all there is to do')

const round = K.config(K.text(c), defaults)
t.same(round, c, 'what is written reads back the same')
t.done('push_kbm')
