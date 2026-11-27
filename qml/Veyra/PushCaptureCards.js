// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Capture cards and what is known about them in PushFPS. "tested" means a person
// ran PushFPS (or Veyra, said so) with this card and a real console; nothing here
// is inferred from specifications. "reported" is a user's capture card report that
// says the card works: the note gives the report's own console and graphics card,
// and nothing the report did not say. The same list is in README.md; reports come in
// through the "Capture card report" issue form.
.pragma library

var reportUrl = "https://github.com/DosukaSOL/PushFPS/issues/new?template=capture-card-report.yml"

// match: lower-case fragments of the name Windows gives the device. The first card that
// matches wins, so a longer name stands before a shorter one it contains ("hd60 s+"
// before "hd60 s").
var cards = [
    { name: "Elgato 4K X", match: ["4k x"], status: "tested",
      note: "Tested by the PushFPS developer with an RTX 5090: frame generation on console games and videos." },
    { name: "T911 4K60", match: ["t911"], status: "veyra",
      note: "Tested by Veyra's developer (Veyra 2.1.6 measurements: 4K60 NV12 with an RTX 5070)." },
    { name: "Elgato HD60 X", match: ["hd60 x"], status: "tested", note: "Tested with PushFPS on a console." },
    { name: "Elgato 4K S", match: ["4k s"], status: "tested", note: "Tested with PushFPS on a console." },
    { name: "Elgato HD60 S+", match: ["hd60 s+"], status: "untested", note: "" },
    { name: "Elgato HD60 S", match: ["hd60 s"], status: "reported",
      note: "Reported working by a user: PS5 at 1080p60 with a Radeon RX 7600 (PushFPS 3.2.2)." },
    { name: "Elgato HD60 Pro", match: ["hd60 pro"], status: "reported",
      note: "Reported working by a user (the report gave no further details)." },
    { name: "ezcap Game Link Raw", match: ["game link raw", "gamelink raw"], status: "reported",
      note: "Reported working by a user: Xbox Series X over USB 3.0 with a Radeon RX 9060 XT." },
    { name: "AVerMedia Live Streamer Ultra HD (GC571)", match: ["live streamer ultra hd", "gc571"], status: "reported",
      note: "Reported working by a user: PS5 at 1440p60 NV12 over PCIe with an RTX 3060 Ti (PushFPS 4.0.0)." },
    { name: "FEELWORLD C4K", match: ["feelworld"], status: "reported",
      note: "Reported working by a user: PS5 Pro at 4K 30 FPS with an RTX 5080, 30 to 120 FPS (PushFPS 4.4.5)." },
    { name: "Monster Capture Card", match: ["monster"], status: "reported",
      note: "Reported working by a user: PS5 over USB 3.0 with an RTX 2080 SUPER (PushFPS 4.5.4)." },
    { name: "Elgato 4K Pro / 4K60 Pro", match: ["4k pro", "4k60 pro"], status: "untested", note: "" },
    { name: "AVerMedia Live Gamer 4K 2.1 (GC575)", match: ["gc575", "live gamer 4k"], status: "untested", note: "" },
    { name: "AVerMedia Live Gamer Ultra 2.1 (GC553G2)", match: ["gc553g2", "ultra 2.1"], status: "reported",
      note: "Reported working by a user: PS5 Pro with an RTX 4080 SUPER (PushFPS 3.2.5)." },
    { name: "AVerMedia Live Gamer Ultra (GC553)", match: ["gc553", "live gamer ultra"], status: "untested", note: "" },
    { name: "Magewell Pro Capture HDMI 4K Plus LT", match: ["4k plus lt"], status: "reported",
      note: "Reported working by a user: PS5 Pro over PCIe with an RTX 5090." },
    { name: "Magewell USB Capture HDMI 4K Plus", match: ["usb capture hdmi 4k"], status: "untested", note: "" },
    { name: "Blackmagic DeckLink / UltraStudio", match: ["decklink", "ultrastudio"], status: "untested", note: "" }
]

function find(deviceName) {
    var n = String(deviceName || "").toLowerCase()
    if (n.length === 0) return null
    for (var i = 0; i < cards.length; ++i)
        for (var j = 0; j < cards[i].match.length; ++j)
            if (n.indexOf(cards[i].match[j]) >= 0) return cards[i]
    return null
}
