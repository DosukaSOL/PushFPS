// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Character Hold (pipeline/PushCharacter.h): a figure the camera follows keeps its own small motion while the
// world behind it passes quickly; with its matte it is taken from one real frame, moved by its own motion and
// laid over what frame generation made. Here the rule itself on drawn pictures with known motions, so the true
// in-between picture is known. The matte is the drawn figure's own shape (the network that makes it in the
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
// The generated frame as a method that breaks the figure up would make it: the plain mix of the two real frames.
struct Held { double figureError = 0, mixError = 0, outside = 0; size_t figurePixels = 0; };
static Held hold(const Scene& older, const Scene& newer, const Scene& truth, const Field& field, float t) {
    Held h;
    for (int y = 0; y < kMatteH; ++y)
        for (int x = 0; x < kMatteW; ++x) {
            const size_t i = size_t(y) * kMatteW + x;
            const Lay lay = layAt(field, x, y, kMatteW, kMatteH, t);
            const Scene& source = lay.newer ? newer : older;
            const float weight = weightOf(matteAt(source.matte8.data(), lay.x, lay.y, kMatteW, kMatteH), lay.trust);
            double error = 0, mixed = 0;
            for (int c = 0; c < 3; ++c) {
                const size_t k = size_t(c) * kMatteW * kMatteH + i;
                const float made = older.rgb[k] * (1 - t) + newer.rgb[k] * t;
                const float out = weight < kLeast ? made : made * (1 - weight) + planar(source, c, lay.x, lay.y) * weight;
                error += std::fabs(out - truth.rgb[k]); mixed += std::fabs(made - truth.rgb[k]);
                if (weight < kLeast && out != made) h.outside += 1;
            }
            if (truth.matte[i] > 0.5f) { h.figureError += error / 3; h.mixError += mixed / 3; ++h.figurePixels; }
        }
    if (h.figurePixels) { h.figureError = h.figureError / double(h.figurePixels) * 255.0; h.mixError = h.mixError / double(h.figurePixels) * 255.0; }
    return h;
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
        const Held h = hold(older, newer, half, f, 0.5f);
        std::printf("  at a half: the figure is %.2f of 255 from the true picture where the plain mix is %.2f; changed outside the matte: %.0f\n", h.figureError, h.mixError, h.outside);
        check(h.figureError < 6.0 && h.figureError * 3 < h.mixError, "laid over a broken frame the figure is the true one to a few of 255");
        check(h.outside == 0, "and nothing is touched where nothing is laid over");
        // A third and two thirds of the way: the nearer real frame is the source.
        const Scene third = draw(30, 4.0f / 3, {moved(1.0f / 3, 6, 2)}), twoThirds = draw(60, 8.0f / 3, {moved(2.0f / 3, 6, 2)});
        const Held a = hold(older, newer, third, f, 1.0f / 3), b = hold(older, newer, twoThirds, f, 2.0f / 3);
        std::printf("  at a third %.2f, at two thirds %.2f of 255\n", a.figureError, b.figureError);
        check(!layAt(f, 900, 500, kMatteW, kMatteH, 1.0f / 3).newer && layAt(f, 900, 500, kMatteW, kMatteH, 2.0f / 3).newer && layAt(f, 900, 500, kMatteW, kMatteH, 0.5f).newer &&
              a.figureError < 6.0 && b.figureError < 6.0, "at other times the nearer real frame is moved by its share of the motion");
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
        int active = 0; float weight = 0.0f;
        for (const Block& b : f.blocks) active += b.active ? 1 : 0;
        for (int y = 0; y < kMatteH; y += 9)
            for (int x = 0; x < kMatteW; x += 11) { const Lay l = layAt(f, x, y, kMatteW, kMatteH, 0.5f); weight = std::max(weight, weightOf(matteAt(newer.matte8.data(), l.x, l.y, kMatteW, kMatteH), l.trust)); }
        check(active == 0 && weight == 0.0f, "a picture without a person is left alone");
    }
    // A figure that crosses the screen further than the search reaches: it is not laid over.
    {
        const Scene older = draw(0, 0, {moved(0, 260, 0)}), newer = draw(20, 0, {moved(1, 260, 0)});
        const Field f = search(older.p0(), newer.p0(), older.p1(), newer.p1());
        double most = 0; size_t over = 0, pixels = 0;
        for (int y = 0; y < kMatteH; y += 3)
            for (int x = 0; x < kMatteW; x += 3) {
                const Lay l = layAt(f, x, y, kMatteW, kMatteH, 0.5f);
                const float w = weightOf(matteAt(newer.matte8.data(), l.x, l.y, kMatteW, kMatteH), l.trust);
                if (matteAt(newer.matte8.data(), l.x, l.y, kMatteW, kMatteH) > 0.5f) { ++pixels; if (w > 0.5f) ++over; }
                most = std::max<double>(most, w);
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
