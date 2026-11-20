// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// The still rule (pipeline/PushHudStill.h): what stands still in front of a moving picture keeps
// its real pixels in generated frames, and nothing else does. Here the rule itself, on pictures
// described as "which pixels are the same in both real frames". The shaders are compared with
// this same function on a graphics card in tests/integration/PushGpuTests.cpp. Pure: builds anywhere.
#include "veyra/pipeline/PushHudStill.h"
#include <cstdio>
#include <string>
using namespace veyra::pipeline::hudstill;
static int failures = 0;
static void check(bool ok, const std::string& label) { std::printf("%s %s\n", ok ? "PASS" : "FAIL", label.c_str()); if (!ok) ++failures; }
struct Picture {
    unsigned w, h; std::vector<uint8_t> same;
    Picture(unsigned w_, unsigned h_, bool still) : w(w_), h(h_), same(size_t(w_) * h_, still ? 1 : 0) {}
    void set(unsigned x, unsigned y, bool s) { if (x < w && y < h) same[size_t(y) * w + x] = s ? 1 : 0; }
    void box(unsigned x0, unsigned y0, unsigned x1, unsigned y1, bool s) { for (unsigned y = y0; y < y1; ++y) for (unsigned x = x0; x < x1; ++x) set(x, y, s); }
    std::vector<uint8_t> keep() const { return decide(same, w, h); }
};
static unsigned count(const std::vector<uint8_t>& v) { unsigned n = 0; for (auto b : v) n += b ? 1u : 0u; return n; }
int main() {
    const unsigned W = 320, H = 192;
    {   // A HUD in front of a moving world: thin strokes, a filled bar, a small block.
        Picture p(W, H, false);
        for (unsigned y = 20; y < 36; ++y) p.set(40, y, true);            // an upright stroke, one pixel wide
        for (unsigned x = 60; x < 90; ++x) p.set(x, 28, true);            // a level stroke
        for (unsigned i = 0; i < 14; ++i) p.set(100 + i, 20 + i, true);   // a slanted stroke
        p.box(40, 150, 140, 158, true);                                   // a bar
        p.box(250, 30, 262, 42, true);                                    // an icon
        const auto keep = p.keep();
        // A stroke one pixel wide has little company at its very end, as a chance match in a moving
        // texture has: its last two pixels are left to frame generation. (Strokes two pixels wide, and
        // strokes that meet others, are kept whole.)
        bool strokes = true;
        for (unsigned y = 22; y < 34; ++y) strokes = strokes && keep[size_t(y) * W + 40];
        for (unsigned x = 62; x < 88; ++x) strokes = strokes && keep[size_t(28) * W + x];
        check(strokes, "upright and level strokes one pixel wide are kept, up to their last two pixels");
        check(!keep[size_t(20) * W + 40] && !keep[size_t(21) * W + 40] && !keep[size_t(35) * W + 40], "those last two at each end are not");
        Picture wide(W, H, false);
        wide.box(40, 20, 42, 26, true);                                   // a short stroke two pixels wide
        const auto kept = wide.keep();
        bool whole = true; for (unsigned y = 20; y < 26; ++y) for (unsigned x = 40; x < 42; ++x) whole = whole && kept[size_t(y) * W + x];
        check(whole, "a short stroke two pixels wide is kept whole");
        unsigned slanted = 0; for (unsigned i = 0; i < 14; ++i) slanted += keep[size_t(20 + i) * W + 100 + i];
        check(slanted == 10, "a slanted stroke is kept up to its last two pixels (" + std::to_string(slanted) + " of 14)");
        bool bar = true; for (unsigned y = 150; y < 158; ++y) for (unsigned x = 40; x < 140; ++x) bar = bar && keep[size_t(y) * W + x];
        check(bar, "a filled bar is kept whole");
        bool icon = true; for (unsigned y = 30; y < 42; ++y) for (unsigned x = 250; x < 262; ++x) icon = icon && keep[size_t(y) * W + x];
        check(icon, "a small block is kept whole");
        bool only = true; for (size_t i = 0; i < keep.size(); ++i) only = only && (!keep[i] || p.same[i]);
        check(only, "and nothing that moves is touched");
    }
    {   // Chance matches in a moving texture have no company.
        Picture p(W, H, false);
        for (unsigned y = 5; y < H; y += 11) for (unsigned x = 3; x < W; x += 13) p.set(x, y, true);     // single pixels
        for (unsigned y = 8; y < H; y += 23) for (unsigned x = 9; x < W; x += 29) { p.set(x, y, true); p.set(x + 1, y, true); }   // pairs
        check(count(p.keep()) == 0, "single pixels and pairs that happen to match are left to frame generation");
    }
    {   // A still picture (a menu, a still camera): nothing is touched, whatever crosses it.
        Picture p(W, H, true);
        check(count(p.keep()) == 0, "a picture that stands still everywhere: nothing kept");
        p.box(150, 80, 170, 100, false); p.box(210, 80, 230, 100, false);   // a ball's old and new place; in between both frames show the world
        const auto keep = p.keep();
        check(count(keep) == 0, "a ball crossing a still world: its path is not touched");
    }
    {   // A character who stands still, the world moving round him: his outline is kept, his middle is left alone.
        Picture p(W, H, false);
        p.box(110, 40, 210, 192, true);
        const auto keep = p.keep();
        unsigned edge = 0; for (unsigned y = 60; y < 180; ++y) edge += keep[size_t(y) * W + 111];
        check(edge >= 110, "the pixels at his outline keep their real value (" + std::to_string(edge) + " of 120)");
        check(!keep[size_t(140) * W + 160], "deep inside, where everything round stands still, nothing is touched");
    }
    {   // The line between "mostly moves" and "mostly still" is the surrounding's half.
        Picture half(160, 160, false);
        half.box(0, 0, 160, 86, true);                   // the top stands still, down to row 85
        const auto keep = half.keep();
        check(!keep[size_t(8) * 160 + 80], "far inside a still region: left alone");
        check(keep[size_t(85) * 160 + 80] != 0, "at its edge, where less than half the surrounding stands still: kept");
        Picture more(160, 160, false);
        more.box(0, 0, 160, 96, true);                   // three of the five block rows round row 95 stand still
        check(!more.keep()[size_t(95) * 160 + 80], "where more than half the surrounding stands still: left alone");
    }
    {   // Edges of the picture and odd sizes.
        Picture p(37, 21, false);
        p.box(0, 0, 37, 2, true); p.box(35, 0, 37, 21, true);
        const auto keep = p.keep();
        check(keep.size() == size_t(37) * 21 && keep[0] && keep[36] && keep[size_t(20) * 37 + 36], "strokes along the picture's edge are kept, in a picture that is no multiple of a block");
        check(decide({}, 0, 0).empty() && count(decide(std::vector<uint8_t>(10, 1), 4, 4)) == 0, "a size that does not fit its data keeps nothing");
    }
    {   // The switch.
        check(!wanted(), "off unless switched on (4.0 had it on for everyone; 4.0.1 went back)");
        setWanted(true); check(wanted(), "on"); setWanted(false); check(!wanted(), "and off again");
        check(kPairStill > 2.0f / 255.0f && kPairStill < 3.0f / 255.0f, "two steps of 255 are still the same, three are not");
    }
    {   // A picture that is no multiple of the blocks: the cut blocks at the right and the bottom count
        // only the pixels they have. A stroke in the cut corner, in a surrounding that moves, is kept;
        // the same stroke with the cut blocks standing still around it is not.
        const unsigned w = 167, h = 91;                       // 10 whole blocks and 7 px; 5 whole blocks and 11 px
        std::vector<uint8_t> same(size_t(w) * h, 0);
        for (unsigned y = 82; y < 90; ++y) { same[size_t(y) * w + 163] = 1; same[size_t(y) * w + 164] = 1; }
        auto keep = decide(same, w, h);
        check(keep[size_t(85) * w + 163] && keep[size_t(85) * w + 164], "in a cut block at the picture's corner: a stroke in a moving surrounding is kept");
        for (unsigned y = 40; y < h; ++y) for (unsigned x = 120; x < w; ++x) same[size_t(y) * w + x] = 1;
        keep = decide(same, w, h);
        check(!keep[size_t(85) * w + 163], "in a cut block: a still surrounding leaves it alone");
        // Exactly half still is not "mostly moves": the test is under a half.
        const unsigned w2 = 80, h2 = 80;                      // 5 x 5 blocks: every block has all 25 as its surrounding
        std::vector<uint8_t> half(size_t(w2) * h2, 0);
        for (unsigned y = 0; y < h2; ++y) for (unsigned x = 0; x < w2 / 2; ++x) half[size_t(y) * w2 + x] = 1;
        keep = decide(half, w2, h2);
        size_t any = 0; for (auto k : keep) any += k;
        check(any == 0, "a surrounding that is exactly half still is left alone");
        half[size_t(10) * w2 + 39] = 0;                        // one pixel fewer: under a half now
        keep = decide(half, w2, h2);
        any = 0; for (auto k : keep) any += k;
        check(any > 0, "one pixel under a half: the surrounding counts as moving");
    }
    std::printf("%s (%d failures)\n", failures ? "FAILED" : "ALL PASSED", failures);
    return failures ? 1 : 0;
}
