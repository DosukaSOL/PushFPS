// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Character Hold (pipeline/PushCharacter.h): a figure the camera follows keeps its own small motion while the
// world behind it passes quickly; with its matte it is taken from one real frame, placed where frame generation
// put the figure, and laid over what frame generation made wherever that differs from it. Here the rule itself
// on drawn pictures with known motions, so the true in-between picture is known: a frame that is right must be
// left alone, and what is broken in a frame must come back. The matte is the drawn figure's own shape (the network that makes it in the
// program is not part of the rule). The shaders are compared with these same functions on a graphics card in
// tests/integration/PushGpuTests.cpp. Pure: builds anywhere.
#include "veyra/pipeline/PushCharacter.h"
#include <cmath>
#include <cstdio>
#include <string>
using namespace veyra::pipeline::character;
static int failures = 0;
static void check(bool ok, const std::string& label) { std::printf("%s %s\n", ok ? "PASS" : "FAIL", label.c_str()); if (!ok) ++failures; }

// Smooth noise with detail down to a few pixels (a world to pass behind the figure, and the figure's own surface).
static float noise(float x, float y, uint32_t seed) {
    float sum = 0.0f, amp = 1.0f;
    for (int cell : {64, 24, 9, 4}) {
        const float fx = x / float(cell), fy = y / float(cell);
        const int ix = int(std::floor(fx)), iy = int(std::floor(fy));
        const float tx = fx - float(ix), ty = fy - float(iy);
        const auto corner = [&](int cx, int cy) {
            uint32_t h = uint32_t(cx) * 374761393u + uint32_t(cy) * 668265263u + seed * 2246822519u + uint32_t(cell) * 3266489917u;
            h = (h ^ (h >> 13)) * 1274126177u; h ^= h >> 16;
            return float(h & 0xffffu) / 65535.0f - 0.5f;
        };
        sum += amp * ((corner(ix, iy) * (1 - tx) + corner(ix + 1, iy) * tx) * (1 - ty) + (corner(ix, iy + 1) * (1 - tx) + corner(ix + 1, iy + 1) * tx) * ty);
        amp *= 0.6f;
    }
    return sum;
}
// A figure: a head and a body, with a surface of its own. Position (cx, cy) of its middle; `scale` its size.
struct Figure { float cx, cy, scale; uint32_t seed; };
static float figureMatte(const Figure& f, float x, float y) {
    const float bx = (x - f.cx) / (150.0f * f.scale), by = (y - f.cy - 140.0f * f.scale) / (330.0f * f.scale);
    const float hx = (x - f.cx) / (85.0f * f.scale), hy = (y - f.cy + 255.0f * f.scale) / (100.0f * f.scale);
    const float body = 1.0f - std::sqrt(bx * bx + by * by), head = 1.0f - std::sqrt(hx * hx + hy * hy);
    const float edge = 0.03f;                              // a soft edge, a few pixels
    const float m = std::max(body, head) / edge;
    return m < 0.0f ? 0.0f : (m > 1.0f ? 1.0f : m);
}
static void figureColour(const Figure& f, float x, float y, float* rgb) {
    const float n = noise(x - f.cx, y - f.cy, f.seed);     // the surface moves with the figure
    rgb[0] = 0.55f + 0.5f * n; rgb[1] = 0.25f + 0.4f * n; rgb[2] = 0.22f + 0.3f * n;
}
struct Scene {
    std::vector<float> rgb, matte;                         // planar picture and matte at the matte's size
    std::vector<uint8_t> matte8, fine, coarse;
    Picture p1() const { return {kFineW, kFineH, fine.data()}; }
    Picture p0() const { return {kCoarseW, kCoarseH, coarse.data()}; }
};
// The world moved by (wx, wy), and any figures.
static Scene draw(float wx, float wy, const std::vector<Figure>& figures) {
    Scene s;
    const size_t plane = size_t(kMatteW) * kMatteH;
    s.rgb.resize(plane * 3); s.matte.assign(plane, 0.0f);
    for (int y = 0; y < kMatteH; ++y)
        for (int x = 0; x < kMatteW; ++x) {
            const size_t i = size_t(y) * kMatteW + x;
            const float n = noise(float(x) - wx, float(y) - wy, 7u);
            float c[3] = {0.45f + 0.6f * n, 0.48f + 0.55f * n, 0.40f + 0.5f * n};
            float a = 0.0f;
            for (const Figure& f : figures) {
                const float m = figureMatte(f, float(x), float(y));
                if (m <= 0.0f) continue;
                float fc[3]; figureColour(f, float(x), float(y), fc);
                for (int k = 0; k < 3; ++k) c[k] = c[k] * (1 - m) + fc[k] * m;
                a = a + m - a * m;
            }
            for (int k = 0; k < 3; ++k) s.rgb[size_t(k) * plane + i] = std::clamp(c[k], 0.0f, 1.0f);
            s.matte[i] = a;
        }
    matted(s.rgb.data(), s.matte.data(), s.matte8, s.fine, s.coarse);
    return s;
}
static float planar(const Scene& s, int c, float x, float y) { return between(s.rgb.data() + size_t(c) * kMatteW * kMatteH, kMatteW, kMatteH, x, y); }
// One generated frame through the rule. made: the frame as the method made it (planar, the matte's size).
struct Held {
    double figureError = 0, madeError = 0;      // inside the true figure: the result and the made frame against the truth, of 255
    double changedInFigure = 0;                 // share of the true figure's pixels the rule changed by more than 3 of 255
    double outside = 0;                         // values written where nothing is laid over
    double changedAway = 0;                     // pixels changed where the true figure is not, and not beside it
    double patchError = 0, patchMade = 0;       // the same two errors inside `patch` (a region the method broke)
    size_t figurePixels = 0, patchPixels = 0, placed = 0;
};
struct Patch { float x0 = 0, y0 = 0, x1 = 0, y1 = 0; bool has(int x, int y) const { return float(x) >= x0 && float(x) < x1 && float(y) >= y0 && float(y) < y1; } };
static Held hold(const Scene& older, const Scene& newer, const std::vector<float>& made, const Scene& truth, const Field& field, float t, Patch patch = {}) {
    Held h;
    const Scene& source = sourceIsNewer(t) ? newer : older;
    std::vector<uint8_t> made1;
    madeFine(made.data(), made1);
    const Placing p = place(field, source.p1(), {kFineW, kFineH, made1.data()}, t);
    std::vector<uint16_t> apart; std::vector<uint8_t> differ;
    differMap(p, source.p1(), {kFineW, kFineH, made1.data()}, apart, differ);
    for (uint8_t f : p.found) h.placed += f;
    const size_t plane = size_t(kMatteW) * kMatteH;
    size_t changed = 0;
    for (int y = 0; y < kMatteH; ++y)
        for (int x = 0; x < kMatteW; ++x) {
            const size_t i = size_t(y) * kMatteW + x;
            const Laid laid = laidAt(p, differ.data(), source.matte8.data(), x, y, kMatteW, kMatteH);
            double error = 0, before = 0, moved = 0;
            for (int c = 0; c < 3; ++c) {
                const size_t k = size_t(c) * plane + i;
                const float out = laid.weight < kLeast ? made[k] : made[k] * (1 - laid.weight) + planar(source, c, laid.x, laid.y) * laid.weight;
                error += std::fabs(out - truth.rgb[k]); before += std::fabs(made[k] - truth.rgb[k]); moved = std::max<double>(moved, std::fabs(out - made[k]));
                if (laid.weight < kLeast && out != made[k]) h.outside += 1;
            }
            const bool inFigure = truth.matte[i] > 0.5f;
            if (inFigure) { h.figureError += error / 3; h.madeError += before / 3; ++h.figurePixels; if (moved > 3.0 / 255.0) ++changed; }
            else if (truth.matte[i] == 0.0f && older.matte[i] == 0.0f && newer.matte[i] == 0.0f && moved > 3.0 / 255.0) h.changedAway += 1;
            if (inFigure && patch.has(x, y)) { h.patchError += error / 3; h.patchMade += before / 3; ++h.patchPixels; }
        }
    if (h.figurePixels) { h.figureError = h.figureError / double(h.figurePixels) * 255.0; h.madeError = h.madeError / double(h.figurePixels) * 255.0; h.changedInFigure = double(changed) / double(h.figurePixels); }
    if (h.patchPixels) { h.patchError = h.patchError / double(h.patchPixels) * 255.0; h.patchMade = h.patchMade / double(h.patchPixels) * 255.0; }
    return h;
}
// The plain mix of the two real frames: what a method makes of a figure it cannot follow at all.
static std::vector<float> mixOf(const Scene& older, const Scene& newer, float t) {
    std::vector<float> made(older.rgb.size());
    for (size_t k = 0; k < made.size(); ++k) made[k] = older.rgb[k] * (1 - t) + newer.rgb[k] * t;
    return made;
}
// A frame that is right, except that inside `patch` the method showed the world where the figure is (what
// frame generation does to hair and shoulders before a fast world).
static std::vector<float> broken(const Scene& truth, const Scene& worldOnly, Patch patch) {
    std::vector<float> made = truth.rgb;
    const size_t plane = size_t(kMatteW) * kMatteH;
    for (int y = 0; y < kMatteH; ++y)
        for (int x = 0; x < kMatteW; ++x)
            if (patch.has(x, y)) for (int c = 0; c < 3; ++c) made[size_t(c) * plane + size_t(y) * kMatteW + x] = worldOnly.rgb[size_t(c) * plane + size_t(y) * kMatteW + x];
    return made;
}
struct Seen { double x = 0, y = 0, trust = 0, least = 1; int blocks = 0; };
// The field over the blocks that lie well inside a figure of the newer frame.
static Seen inside(const Scene& newer, const Field& f) {
    Seen s;
    for (int by = 0; by < kBlocksY; ++by)
        for (int bx = 0; bx < kBlocksX; ++bx) {
            const Span b = fineBlock(bx, by);
            bool all = true;
            for (int y = b.y0; y < b.y1 && all; ++y)
                for (int x = b.x0; x < b.x1; ++x) if (newer.fine[(size_t(y) * kFineW + x) * 4 + 3] < 250) { all = false; break; }
            if (!all) continue;
            const int i = by * kBlocksX + bx;
            s.x += f.x[i]; s.y += f.y[i]; s.trust += f.trust[i]; s.least = std::min<double>(s.least, f.trust[i]); ++s.blocks;
        }
    if (s.blocks) { s.x /= s.blocks; s.y /= s.blocks; s.trust /= s.blocks; }
    return s;
}

int main() {
    // The numbers the rule rests on.
    {
        const uint8_t a[4] = {10, 20, 30, 255}, b[4] = {13, 20, 26, 0};
        check(difference(a, b) == 2 * 7 + 3 * 255 && difference(a, b) == difference(b, a) && difference(a, a) == 0, "a pixel's difference is twice the colours' and three times the matte's, either way round");
        check(parabola(10, 4, 10) == 0.0f && parabola(10, 4, 6) > 0.0f && parabola(6, 4, 10) < 0.0f && parabola(5, 5, 5) == 0.0f && std::fabs(parabola(100, 0, 0)) <= 0.5f,
              "the parabola leans to the cheaper side, stays within half a pixel and says nothing on a flat row");
        check(toByte(-1.0f) == 0 && toByte(2.0f) == 255 && toByte(0.5f) == 128 && smoothStep(0.25f, 0.25f, 0.5f) == 0.0f && smoothStep(0.5f, 0.25f, 0.5f) == 1.0f, "bytes and steps keep to their ends");
        const Span w = coarseWindow(0, 0), last = coarseWindow(kBlocksX - 1, kBlocksY - 1), f = fineWindow(3, 2), own = fineBlock(kBlocksX - 1, kBlocksY - 1);
        check(w.x0 == 0 && w.y0 == 0 && w.x1 == 12 && w.y1 == 12 && last.x1 == kCoarseW && last.y1 == kCoarseH && f.x0 == 40 && f.x1 == 72 && f.y0 == 24 && f.y1 == 56 && own.y1 == kFineH && own.count() == 16 * 12,
              "a block's windows stay inside the picture");
        const uint8_t solid[4] = {200, 100, 50, 255}, half[4] = {158, 108, 83, 128}, same[3] = {200, 100, 50}, other[3] = {210, 100, 44};
        check(blended(200, 255) == 200 && blended(200, 0) == kGreyByte && blended(200, 128) == (200 * 128 + kGreyByte * 127 + 127) / 255 && apartFrom(same, solid) == 0 && apartFrom(other, solid) == 2 * 16 &&
              apartFrom(same, half) <= 2 && apartFrom(other, other) >= 0 && apartFrom(solid, other) <= kCap,
              "a generated pixel is compared as a matted pixel with the source's matte");
        check(sourceIsNewer(0.5f) && !sourceIsNewer(0.49f) && shareOf(0.5f) == 1.0f && std::fabs(shareOf(2.0f / 3) - 2.0f / 3) < 1e-6f && std::fabs(shareOf(1.0f / 3) + 2.0f / 3) < 1e-6f,
              "the nearer real frame is the source, and its share of the half-motion follows the time");
        // The differ map of one pixel: nothing under the lower bound, everything from the upper one, and it reaches two pixels.
        Placing all; all.trust.assign(kBlockCount, 1.0f); all.x.assign(kBlockCount, 0.0f); all.y.assign(kBlockCount, 0.0f);
        // A block's place: its own where the frame fits it, its neighbours' where it does not.
        {
            std::vector<float> px(kBlockCount, 2.0f), py(kBlockCount, -1.0f), fit(kBlockCount, 1.0f), sx(kBlockCount, 0.5f), sy(kBlockCount, 0.0f);
            const int lost = 10 * kBlocksX + 20, odd = 10 * kBlocksX + 30;
            px[lost] = 9.0f; fit[lost] = 0.0f; px[odd] = 3.0f;
            const Motion a = settleAt(px.data(), py.data(), fit.data(), sx.data(), sy.data(), 20, 10), b = settleAt(px.data(), py.data(), fit.data(), sx.data(), sy.data(), 30, 10);
            std::fill(fit.begin(), fit.end(), 0.0f);
            const Motion c = settleAt(px.data(), py.data(), fit.data(), sx.data(), sy.data(), 20, 10);
            check(std::fabs(a.x - 2.0f) < 1e-5f && std::fabs(a.y + 1.0f) < 1e-5f && b.x == 3.0f && c.x == 0.5f && c.y == 0.0f && fitOf(10.0f, true) == 1.0f && fitOf(10.0f, false) == 0.0f && fitOf(kFitBad, true) == 0.0f,
                  "a block the frame does not fit takes its neighbours' place, a block it fits keeps its own, and with nothing near the search's answer stands");
        }
        std::vector<uint16_t> apart(size_t(kFineW) * kFineH, uint16_t(1 + kDifferLo));
        const bool quiet = differAt(all, apart.data(), 100, 100) == 0;
        for (int y = 99; y <= 101; ++y) for (int x = 199; x <= 201; ++x) apart[size_t(y) * kFineW + x] = uint16_t(1 + kDifferHi);
        const bool loud = differAt(all, apart.data(), 200, 100) == 255 && differAt(all, apart.data(), 202, 102) == 255 && differAt(all, apart.data(), 204, 100) == 85 && differAt(all, apart.data(), 205, 100) == 0;
        Placing none = all; none.trust.assign(kBlockCount, 0.0f);
        check(quiet && loud && differAt(none, apart.data(), 200, 100) == 0, "the differ map starts above the lower bound, is full at the upper one, reaches a little beyond what differs and is nothing where nothing is trusted");
    }
    // One figure, followed by the camera: it moves 6 across and 2 down a frame (at the matte's size) while the
    // world passes at 90. Half its motion at the fine size is 1.5 and 0.5.
    const Figure start{900.0f, 520.0f, 1.0f, 3u};
    const auto moved = [&](float t, float vx, float vy) { return Figure{start.cx + vx * t, start.cy + vy * t, start.scale, start.seed}; };
    {
        const Scene older = draw(0, 0, {moved(0, 6, 2)}), newer = draw(90, 4, {moved(1, 6, 2)}), half = draw(45, 2, {moved(0.5f, 6, 2)});
        bool grey = true, mean = true;
        for (int y = 0; y < kFineH; y += 7)
            for (int x = 0; x < kFineW; x += 5) {
                const uint8_t* p = older.fine.data() + (size_t(y) * kFineW + x) * 4;
                if (p[3] == 0 && !(p[0] == toByte(kGrey) && p[1] == p[0] && p[2] == p[0])) grey = false;
            }
        for (int y = 0; y < kCoarseH; y += 3)
            for (int x = 0; x < kCoarseW; x += 3)
                for (int c = 0; c < 4; ++c) {
                    const auto at = [&](int dx, int dy) { return int(older.fine[(size_t(2 * y + dy) * kFineW + size_t(2 * x + dx)) * 4 + c]); };
                    if (older.coarse[(size_t(y) * kCoarseW + x) * 4 + c] != uint8_t((at(0, 0) + at(1, 0) + at(0, 1) + at(1, 1) + 2) >> 2)) mean = false;
                }
        check(grey && mean, "a matted picture is grey where the matte is not, and the coarse one is the fine one's mean");
        const Field f = search(older.p0(), newer.p0(), older.p1(), newer.p1());
        const Seen s = inside(newer, f);
        std::printf("  followed figure: %d blocks inside, half-motion %.2f, %.2f (true 1.50, 0.50), trust mean %.2f least %.2f\n", s.blocks, s.x, s.y, s.trust, s.least);
        check(s.blocks > 40 && std::fabs(s.x - 1.5) < 0.3 && std::fabs(s.y - 0.5) < 0.3, "the figure's own motion is found, not the world's");
        check(s.trust > 0.95 && s.least > 0.8, "and it is trusted");
        // A frame that is right is left alone: the figure's own pixels stay the method's.
        const Scene worldHalf = draw(45, 2, {});
        const Held right = hold(older, newer, half.rgb, half, f, 0.5f);
        std::printf("  a frame that is right: %zu blocks placed; %.2f %% of the figure changed by more than 3 of 255; the figure %.2f of 255 from the truth\n", right.placed, 100.0 * right.changedInFigure, right.figureError);
        check(right.placed > 100 && right.changedInFigure < 0.01 && right.figureError < 0.5, "a frame in which the figure is right is left alone");
        check(right.outside == 0 && right.changedAway == 0, "and nothing is touched where nothing is laid over");
        // The method showed the world where the head is: the head comes back from the real frame, the rest stays.
        const Figure at = moved(0.5f, 6, 2);
        const Patch head{at.cx - 110.0f, at.cy - 380.0f, at.cx + 110.0f, at.cy - 150.0f};
        const Held mended = hold(older, newer, broken(half, worldHalf, head), half, f, 0.5f, head);
        std::printf("  the head shown as the world: %.2f of 255 from the truth where the made frame is %.2f (%zu pixels); the whole figure %.2f\n", mended.patchError, mended.patchMade, mended.patchPixels, mended.figureError);
        check(mended.patchPixels > 5000 && mended.patchMade > 20.0 && mended.patchError < 6.0, "what the method broke comes back from the real frame, to a few of 255");
        check(mended.outside == 0 && mended.changedAway == 0, "and the world away from the figure stays the method's");
        // The search's answer is only where the looking starts: with it a pixel and a half off, both still hold.
        Field off = f;
        for (int i = 0; i < kBlockCount; ++i) off.x[i] += 1.5f;
        const Held rightOff = hold(older, newer, half.rgb, half, off, 0.5f), mendedOff = hold(older, newer, broken(half, worldHalf, head), half, off, 0.5f, head);
        std::printf("  with the search 1.5 off: a right frame changed on %.2f %% of the figure; the broken head %.2f of 255 from the truth\n", 100.0 * rightOff.changedInFigure, mendedOff.patchError);
        check(rightOff.changedInFigure < 0.01 && rightOff.figureError < 0.5 && mendedOff.patchError < 6.0, "the figure is placed where the method put it, not where the search said");
        // A method that cannot follow the figure at all (the plain mix of the two frames) leaves a faint double
        // picture when the figure moves this little: it is not made worse.
        const Held h = hold(older, newer, mixOf(older, newer, 0.5f), half, f, 0.5f);
        std::printf("  the plain mix: the figure is %.2f of 255 from the true picture where the mix is %.2f\n", h.figureError, h.madeError);
        check(h.figureError < 6.0 && h.figureError <= h.madeError && h.outside == 0, "a faint double picture is not made worse");
        // A third and two thirds of the way: the nearer real frame is the source.
        const Scene third = draw(30, 4.0f / 3, {moved(1.0f / 3, 6, 2)}), twoThirds = draw(60, 8.0f / 3, {moved(2.0f / 3, 6, 2)});
        const Figure at3 = moved(1.0f / 3, 6, 2), at6 = moved(2.0f / 3, 6, 2);
        const Patch head3{at3.cx - 110.0f, at3.cy - 380.0f, at3.cx + 110.0f, at3.cy - 150.0f}, head6{at6.cx - 110.0f, at6.cy - 380.0f, at6.cx + 110.0f, at6.cy - 150.0f};
        const Held a = hold(older, newer, broken(third, draw(30, 4.0f / 3, {}), head3), third, f, 1.0f / 3, head3), b = hold(older, newer, broken(twoThirds, draw(60, 8.0f / 3, {}), head6), twoThirds, f, 2.0f / 3, head6);
        const Held a0 = hold(older, newer, third.rgb, third, f, 1.0f / 3), b0 = hold(older, newer, twoThirds.rgb, twoThirds, f, 2.0f / 3);
        std::printf("  at a third the broken head %.2f, at two thirds %.2f of 255; right frames changed on %.2f %% and %.2f %% of the figure\n", a.patchError, b.patchError, 100.0 * a0.changedInFigure, 100.0 * b0.changedInFigure);
        check(a.patchError < 6.0 && b.patchError < 6.0 && a0.changedInFigure < 0.01 && b0.changedInFigure < 0.01, "at other times the nearer real frame is placed by its share of the motion");
    }
    // A figure that moves visibly (28 across and 8 down a frame), not followed at all: the mix is two figures, and one comes back.
    {
        const Scene older = draw(0, 0, {moved(0, 28, 8)}), newer = draw(90, 4, {moved(1, 28, 8)}), half = draw(45, 2, {moved(0.5f, 28, 8)});
        const Field f = search(older.p0(), newer.p0(), older.p1(), newer.p1());
        const Seen s = inside(newer, f);
        const Held h = hold(older, newer, mixOf(older, newer, 0.5f), half, f, 0.5f);
        std::printf("  a fast figure: half-motion %.2f, %.2f (true 7.00, 2.00); as the plain mix it is %.2f of 255 from the truth, laid over %.2f\n", s.x, s.y, h.madeError, h.figureError);
        check(std::fabs(s.x - 7.0) < 0.4 && std::fabs(s.y - 2.0) < 0.4 && h.madeError > 8.0 && h.figureError < 6.0 && h.figureError * 2 < h.madeError && h.outside == 0,
              "a figure the method did not follow at all comes back to a few of 255");
    }
    // The figure stands still on the screen: nothing is invented.
    {
        const Scene older = draw(0, 0, {start}), newer = draw(120, 0, {start});
        const Field f = search(older.p0(), newer.p0(), older.p1(), newer.p1());
        const Seen s = inside(newer, f);
        check(s.blocks > 40 && std::fabs(s.x) < 0.1 && std::fabs(s.y) < 0.1 && s.trust > 0.95, "a figure that stands still has no motion and is trusted");
    }
    // No figure at all: no block is judged, and nothing would be laid over anywhere.
    {
        const Scene older = draw(0, 0, {}), newer = draw(70, 9, {});
        const Field f = search(older.p0(), newer.p0(), older.p1(), newer.p1());
        int active = 0;
        for (const Block& b : f.blocks) active += b.active ? 1 : 0;
        const Held h = hold(older, newer, mixOf(older, newer, 0.5f), draw(35, 4.5f, {}), f, 0.5f);
        check(active == 0 && h.placed == 0 && h.outside == 0 && h.changedAway == 0, "a picture without a person is left alone");
    }
    // A figure that crosses the screen further than the search reaches: it is not laid over.
    {
        const Scene older = draw(0, 0, {moved(0, 260, 0)}), newer = draw(20, 0, {moved(1, 260, 0)});
        const Field f = search(older.p0(), newer.p0(), older.p1(), newer.p1());
        const std::vector<float> made = mixOf(older, newer, 0.5f);
        std::vector<uint8_t> made1;
        madeFine(made.data(), made1);
        const Placing p = place(f, newer.p1(), {kFineW, kFineH, made1.data()}, 0.5f);
        std::vector<uint16_t> apart; std::vector<uint8_t> differ;
        differMap(p, newer.p1(), {kFineW, kFineH, made1.data()}, apart, differ);
        size_t over = 0, pixels = 0;
        for (int y = 0; y < kMatteH; y += 3)
            for (int x = 0; x < kMatteW; x += 3) {
                const Laid l = laidAt(p, differ.data(), newer.matte8.data(), x, y, kMatteW, kMatteH);
                if (newer.matte[size_t(y) * kMatteW + x] > 0.5f) { ++pixels; if (l.weight > 0.5f) ++over; }
            }
        std::printf("  out of reach: laid over on %.1f %% of where its matte would land\n", pixels ? 100.0 * double(over) / double(pixels) : 0.0);
        check(pixels > 1000 && double(over) < 0.25 * double(pixels), "a figure that moves further than the search reaches is left as the method made it");
    }
    // Two figures with different motions, apart from each other: each keeps its own.
    {
        const Figure left{520.0f, 540.0f, 0.8f, 5u}, right{1400.0f, 520.0f, 0.8f, 9u};
        const Scene older = draw(0, 0, {left, right});
        const Scene newer = draw(-80, 0, {Figure{left.cx + 10, left.cy, left.scale, left.seed}, Figure{right.cx - 14, right.cy + 6, right.scale, right.seed}});
        const Field f = search(older.p0(), newer.p0(), older.p1(), newer.p1());
        const auto at = [&](float x, float y) { const int bx = int(x / 2 / 16), by = int(y / 2 / 16); return Motion{f.x[by * kBlocksX + bx], f.y[by * kBlocksX + bx]}; };
        const Motion l = at(525, 640), r = at(1393, 620);
        std::printf("  two figures: %.2f, %.2f (true 2.50, 0.00) and %.2f, %.2f (true -3.50, 1.50)\n", l.x, l.y, r.x, r.y);
        check(std::fabs(l.x - 2.5f) < 0.4f && std::fabs(l.y) < 0.4f && std::fabs(r.x + 3.5f) < 0.4f && std::fabs(r.y - 1.5f) < 0.4f, "two figures each keep their own motion");
    }
    check(!wanted(), "Character Hold is off unless switched on");
    setWanted(true);
    check(wanted(), "and on when switched on");
    setWanted(false);
    std::printf("%s (%d failed)\n", failures ? "FAILED" : "ALL PASS", failures);
    return failures ? 1 : 0;
}
