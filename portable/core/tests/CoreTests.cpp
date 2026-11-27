// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// Tests for the Linux / macOS core: conversion, motion, interpolation, duplicate removal, HUD lock and
// the engine's timing, plus abuse (odd sizes, clocks going back, settings changed mid-stream).
#include "veyra/portable/Ambient.h"
#include "veyra/portable/Convert.h"
#include "veyra/portable/Dedup.h"
#include "veyra/portable/Engine.h"
#include "veyra/portable/HudLock.h"
#include "veyra/portable/Interpolate.h"
#include "veyra/portable/Motion.h"
#include "veyra/engine/PushAmbient.h"  // the Windows build's bars and cover rules and gain

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <thread>
#include <vector>

using namespace veyra::portable;

static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (!(cond)) { ++failures; std::printf("FAIL %s:%d: %s ", __FILE__, __LINE__, #cond); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

// ---- helpers ---------------------------------------------------------------------------------

// A textured scene: smooth noise, so motion search has something to lock onto (like a game picture).
struct Scene {
    int w, h;
    std::vector<float> base;  // larger than the picture so shifted views stay inside
    int margin;
    Scene(int w_, int h_, int margin_, unsigned seed) : w(w_), h(h_), margin(margin_) {
        const int W = w + 2 * margin, H = h + 2 * margin;
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> u(0.f, 255.f);
        std::vector<float> n(size_t(W) * H);
        for (auto& v : n) v = u(rng);
        // Two box blurs: features of a few pixels.
        base = n;
        for (int pass = 0; pass < 2; ++pass) {
            std::vector<float> t(base.size());
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x) {
                    float s = 0; int c = 0;
                    for (int d = -2; d <= 2; ++d) { const int xx = std::clamp(x + d, 0, W - 1); s += base[size_t(y) * W + xx]; ++c; }
                    t[size_t(y) * W + x] = s / c;
                }
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x) {
                    float s = 0; int c = 0;
                    for (int d = -2; d <= 2; ++d) { const int yy = std::clamp(y + d, 0, H - 1); s += t[size_t(yy) * W + x]; ++c; }
                    base[size_t(y) * W + x] = s / c;
                }
        }
        // Stretch contrast back up.
        float lo = 1e9f, hi = -1e9f;
        for (float v : base) { lo = std::min(lo, v); hi = std::max(hi, v); }
        for (float& v : base) v = (v - lo) / (hi - lo) * 255.f;
    }
    float at(float x, float y) const {  // bilinear
        const int W = w + 2 * margin, H = h + 2 * margin;
        x += margin; y += margin;
        x = std::clamp(x, 0.f, float(W - 1)); y = std::clamp(y, 0.f, float(H - 1));
        const int x0 = int(x), y0 = int(y), x1 = std::min(x0 + 1, W - 1), y1 = std::min(y0 + 1, H - 1);
        const float fx = x - x0, fy = y - y0;
        const float a = base[size_t(y0) * W + x0], b = base[size_t(y0) * W + x1], c = base[size_t(y1) * W + x0], d = base[size_t(y1) * W + x1];
        return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
    }
};

// Background shifted by (bx, by), a textured square of size s at (ox, oy) on top.
Bgra render(const Scene& bg, const Scene& fg, float bx, float by, float ox, float oy, int s, bool hud = false) {
    Bgra img(bg.w, bg.h);
    for (int y = 0; y < bg.h; ++y)
        for (int x = 0; x < bg.w; ++x) {
            float v = bg.at(float(x) - bx, float(y) - by);
            float g = v * 0.8f + 20.f, b = 255.f - v;
            if (x >= ox && x < ox + s && y >= oy && y < oy + s) {
                const float f = fg.at(float(x) - ox, float(y) - oy);
                v = f; g = 255.f - f * 0.5f; b = f * 0.3f;
            }
            if (hud && y >= 8 && y < 24 && x >= 8 && x < 104 && ((x / 8 + y / 8) % 2 == 0)) { v = 250; g = 250; b = 250; }
            uint8_t* p = img.pixel(x, y);
            p[0] = uint8_t(std::clamp(b, 0.f, 255.f)); p[1] = uint8_t(std::clamp(g, 0.f, 255.f)); p[2] = uint8_t(std::clamp(v, 0.f, 255.f)); p[3] = 255;
        }
    return img;
}

double psnr(const Bgra& a, const Bgra& b, int border = 0) {
    double se = 0; size_t n = 0;
    for (int y = border; y < a.height - border; ++y)
        for (int x = border; x < a.width - border; ++x)
            for (int c = 0; c < 3; ++c) { const double d = double(a.pixel(x, y)[c]) - double(b.pixel(x, y)[c]); se += d * d; ++n; }
    if (n == 0) return 0;
    const double mse = se / double(n);
    return mse <= 1e-9 ? 99.0 : 10.0 * std::log10(255.0 * 255.0 / mse);
}

Bgra blend(const Bgra& a, const Bgra& b, float t) {
    Bgra o(a.width, a.height);
    for (size_t i = 0; i < a.px.size(); ++i) o.px[i] = uint8_t(std::lround(a.px[i] + (b.px[i] - a.px[i]) * t));
    return o;
}

float medianOf(std::vector<float> v) { std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end()); return v[v.size() / 2]; }

// ---- tests -----------------------------------------------------------------------------------

void testConvert() {
    // NV12 limited range: Y=235, U=V=128 is white; Y=16 black.
    for (auto m : {Matrix::Bt709, Matrix::Bt601}) {
        uint8_t y[4] = {235, 16, 235, 16}, uv[2] = {128, 128};
        SourceFrame f; f.format = PixelFormat::Nv12; f.width = 2; f.height = 2; f.matrix = m;
        f.planes[0] = {y, 2}; f.planes[1] = {uv, 2};
        Bgra out;
        CHECK(convertToBgra(f, out), "nv12");
        CHECK(out.pixel(0, 0)[0] == 255 && out.pixel(0, 0)[1] == 255 && out.pixel(0, 0)[2] == 255, "white %d %d %d", out.pixel(0, 0)[0], out.pixel(0, 0)[1], out.pixel(0, 0)[2]);
        CHECK(out.pixel(1, 0)[0] == 0 && out.pixel(1, 0)[2] == 0, "black");
    }
    // BT.709 limited red: Y=63 U=102 V=240.
    {
        uint8_t yuyv[4] = {63, 102, 63, 240};
        SourceFrame f; f.format = PixelFormat::Yuyv; f.width = 2; f.height = 1; f.planes[0] = {yuyv, 4};
        Bgra out;
        CHECK(convertToBgra(f, out), "yuyv");
        const uint8_t* p = out.pixel(0, 0);
        CHECK(p[2] > 245 && p[1] < 12 && p[0] < 12, "red %d %d %d", p[2], p[1], p[0]);
        uint8_t uyvy[4] = {102, 63, 240, 63};
        f.format = PixelFormat::Uyvy; f.planes[0] = {uyvy, 4};
        Bgra out2;
        CHECK(convertToBgra(f, out2) && std::memcmp(out.px.data(), out2.px.data(), out.px.size()) == 0, "uyvy == yuyv");
    }
    // I420 and P010 agree with NV12 on a gradient, odd size 5x3.
    {
        const int w = 5, h = 3, cw = 3, ch = 2;
        std::vector<uint8_t> Y(w * h), U(cw * ch), V(cw * ch), UV(cw * ch * 2);
        std::vector<uint8_t> Y16(w * h * 2), UV16(cw * ch * 4);
        for (int i = 0; i < w * h; ++i) { Y[i] = uint8_t(16 + i * 13); Y16[i * 2] = 0; Y16[i * 2 + 1] = Y[i]; }
        for (int i = 0; i < cw * ch; ++i) {
            U[i] = uint8_t(60 + i * 20); V[i] = uint8_t(200 - i * 17);
            UV[i * 2] = U[i]; UV[i * 2 + 1] = V[i];
            UV16[i * 4 + 1] = U[i]; UV16[i * 4 + 3] = V[i];
        }
        SourceFrame a; a.format = PixelFormat::Nv12; a.width = w; a.height = h; a.planes[0] = {Y.data(), w}; a.planes[1] = {UV.data(), cw * 2};
        SourceFrame b; b.format = PixelFormat::I420; b.width = w; b.height = h; b.planes[0] = {Y.data(), w}; b.planes[1] = {U.data(), cw}; b.planes[2] = {V.data(), cw};
        SourceFrame c; c.format = PixelFormat::P010; c.width = w; c.height = h; c.planes[0] = {Y16.data(), w * 2}; c.planes[1] = {UV16.data(), cw * 4};
        Bgra oa, ob, oc;
        CHECK(convertToBgra(a, oa) && convertToBgra(b, ob) && convertToBgra(c, oc), "odd convert");
        CHECK(oa.px == ob.px, "i420 == nv12");
        CHECK(oa.px == oc.px, "p010 == nv12");
    }
    // Luma straight from the source matches the luma of the converted picture (within rounding).
    {
        const int w = 6, h = 4, cw = 3;
        std::vector<uint8_t> Y(w * h), UV(cw * 2 * 2, 128), yuyv(cw * 4 * h);
        for (int i = 0; i < w * h; ++i) Y[i] = uint8_t(16 + i * 9);
        for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) { yuyv[y * cw * 4 + x * 2] = Y[y * w + x]; yuyv[y * cw * 4 + (x / 2) * 4 + 1 + (x & 1) * 2] = 128; }
        SourceFrame a; a.format = PixelFormat::Nv12; a.width = w; a.height = h; a.planes[0] = {Y.data(), w}; a.planes[1] = {UV.data(), cw * 2};
        SourceFrame b; b.format = PixelFormat::Yuyv; b.width = w; b.height = h; b.planes[0] = {yuyv.data(), cw * 4};
        Bgra rgb;
        convertToBgra(a, rgb);
        const Gray la = lumaFromSource(a), lb = lumaFromSource(b), lc = lumaOf(rgb);
        CHECK(la.px == lb.px, "nv12 luma == yuyv luma");
        int worst = 0;
        for (size_t i = 0; i < la.px.size(); ++i) worst = std::max(worst, std::abs(int(la.px[i]) - int(lc.px[i])));
        CHECK(worst <= 2, "source luma vs converted luma %d", worst);
        a.planes[0].stride = 2;
        CHECK(lumaFromSource(a).empty(), "luma short stride");
    }
    // Rejected: zero size, short stride, missing plane, absurd size.
    {
        uint8_t buf[64] = {};
        SourceFrame f; f.format = PixelFormat::Nv12; f.width = 4; f.height = 2; f.planes[0] = {buf, 3}; f.planes[1] = {buf, 4};
        Bgra out;
        CHECK(!convertToBgra(f, out), "short stride");
        f.planes[0] = {buf, 4}; f.planes[1] = {nullptr, 4};
        CHECK(!convertToBgra(f, out), "missing plane");
        f.width = 0;
        CHECK(!convertToBgra(f, out), "zero size");
        f.width = 100000; f.planes[1] = {buf, 4};
        CHECK(!convertToBgra(f, out), "huge");
        CHECK(out.empty(), "out untouched");
    }
}

void testPyramid() {
    Gray g(17, 9, 100);
    const Pyramid p = Pyramid::of(g, 4);
    CHECK(p.levels.size() == 2, "levels %zu", p.levels.size());  // 17x9 -> 9x5 (stops at <= 8 height)
    CHECK(p.levels[1].width == 9 && p.levels[1].height == 5, "halve odd");
    CHECK(Pyramid::of(Gray(), 4).levels.empty(), "empty");
    CHECK(Pyramid::of(Gray(1, 1, 7), 4).levels.size() == 1, "1x1");
}

void testMotion() {
    const Scene bg(320, 192, 80, 1), fg(64, 64, 8, 2);
    const MotionEstimator est;
    struct Case { float dx, dy; } cases[] = {{0, 0}, {6, -3}, {2.5f, 1.5f}, {-17, 9}, {41, -22}, {-60, 30}};
    for (const auto& c : cases) {
        const Bgra a = render(bg, fg, 0, 0, -1000, -1000, 0), b = render(bg, fg, c.dx, c.dy, -1000, -1000, 0);
        const Pyramid pa = Pyramid::of(lumaOf(a), 4), pb = Pyramid::of(lumaOf(b), 4);
        const MotionField f = est.estimate(pa, pb);
        CHECK(!f.empty() && f.blockSize == 16, "field");
        // Interior blocks (edges see the picture border).
        // Interior blocks: the picture edge (where new content enters) has no right answer.
        std::vector<float> xs, ys;
        const int mx0 = 2 + int(std::ceil(std::abs(c.dx) / 16.f)), my0 = 2 + int(std::ceil(std::abs(c.dy) / 16.f));
        for (int j = my0; j < f.blocksY - my0; ++j)
            for (int i = mx0; i < f.blocksX - mx0; ++i) { xs.push_back(f.vx[j * f.blocksX + i]); ys.push_back(f.vy[j * f.blocksX + i]); }
        const float mx = medianOf(xs), my = medianOf(ys);
        CHECK(std::abs(mx - c.dx) < 0.75f && std::abs(my - c.dy) < 0.75f, "shift (%.1f,%.1f) found (%.2f,%.2f)", c.dx, c.dy, mx, my);
        int good = 0;
        for (size_t k = 0; k < xs.size(); ++k) good += std::abs(xs[k] - c.dx) < 1.5f && std::abs(ys[k] - c.dy) < 1.5f;
        CHECK(good > int(xs.size() * 0.9), "shift (%.1f,%.1f): %d/%zu blocks right", c.dx, c.dy, good, xs.size());
        CHECK(!f.sceneCut, "no cut for a pan");
    }
    // Flat picture: vectors stay small (smoothness), no NaN.
    {
        Bgra a(128, 96), b(128, 96);
        for (size_t i = 0; i < a.px.size(); ++i) if (i % 4 != 3) a.px[i] = b.px[i] = 90;
        const MotionField f = est.estimate(Pyramid::of(lumaOf(a), 4), Pyramid::of(lumaOf(b), 4));
        float worst = 0;
        for (size_t k = 0; k < f.vx.size(); ++k) { CHECK(std::isfinite(f.vx[k]) && std::isfinite(f.vy[k]), "finite"); worst = std::max({worst, std::abs(f.vx[k]), std::abs(f.vy[k])}); }
        CHECK(worst < 1.0f, "flat picture vectors %.2f", worst);
    }
    // Scene cut: two unrelated pictures.
    {
        const Scene other(320, 192, 80, 99);
        const Bgra a = render(bg, fg, 0, 0, -1000, -1000, 0), b = render(other, fg, 0, 0, -1000, -1000, 0);
        const MotionField f = est.estimate(Pyramid::of(lumaOf(a), 4), Pyramid::of(lumaOf(b), 4));
        CHECK(f.sceneCut, "scene cut (mean %.1f)", f.meanCost);
    }
    // Size mismatch gives an empty field.
    CHECK(est.estimate(Pyramid::of(Gray(64, 64, 1), 4), Pyramid::of(Gray(64, 32, 1), 4)).empty(), "size mismatch");
}

void testInterpolate() {
    const Scene bg(320, 192, 80, 3), fg(64, 64, 8, 4);
    const MotionEstimator est;
    // Background pans 8 px, the square moves 24 px the other way: the hard case for a plain blend.
    const Bgra a = render(bg, fg, 0, 0, 100, 60, 64), b = render(bg, fg, 8, 2, 76, 60, 64);
    const Bgra truth = render(bg, fg, 4, 1, 88, 60, 64);
    const Pyramid pa = Pyramid::of(lumaOf(a), 4), pb = Pyramid::of(lumaOf(b), 4);
    const MotionField f = est.estimate(pa, pb), k = est.estimate(pb, pa);
    Bgra out;
    CHECK(interpolate(a, b, f, k, nullptr, 0.5f, out, 3), "interpolate");
    const double mc = psnr(out, truth, 8), lin = psnr(blend(a, b, 0.5f), truth, 8), hold = psnr(a, truth, 8);
    std::printf("  interpolation PSNR at t=0.5: motion %.1f dB, blend %.1f dB, repeat %.1f dB\n", mc, lin, hold);
    CHECK(mc > lin + 6.0 && mc > hold + 6.0, "motion-compensated beats blend/repeat");
    CHECK(mc > 26.0, "motion-compensated quality %.1f", mc);
    // Same result with 1 and many threads.
    Bgra out1;
    interpolate(a, b, f, k, nullptr, 0.5f, out1, 1);
    CHECK(out1.px == out.px, "threads give the same picture");
    // t = 0 and 1 are the real frames exactly.
    interpolate(a, b, f, k, nullptr, 0.f, out1, 2);
    CHECK(out1.px == a.px, "t=0 is A");
    interpolate(a, b, f, k, nullptr, 1.f, out1, 2);
    CHECK(out1.px == b.px, "t=1 is B");
    // NaN phase and missing fields fall back to a real frame.
    interpolate(a, b, f, k, nullptr, std::nanf(""), out1, 2);
    CHECK(out1.px == a.px, "NaN t");
    interpolate(a, b, MotionField{}, k, nullptr, 0.7f, out1, 2);
    CHECK(out1.px == b.px, "no field: nearer frame");
    // A full mask locks everything to the nearer real frame.
    Gray full(10, 10, 255);
    interpolate(a, b, f, k, &full, 0.25f, out1, 2);
    CHECK(psnr(out1, a) > 45.0, "full HUD mask = A");
    // Different sizes are refused.
    CHECK(!interpolate(a, Bgra(10, 10), f, k, nullptr, 0.5f, out1), "size mismatch");
}

// HUD protection boxes (Protection.h): inside a box a generated frame is the older real frame, outside it is
// what it was; the feather softens the edge inwards, an ellipse leaves its box's corners alone; the weight is the
// Windows NrProtection's.
void testProtection() {
    const Scene bg(320, 192, 80, 3), fg(64, 64, 8, 4);
    const MotionEstimator est;
    const Bgra a = render(bg, fg, 0, 0, 100, 60, 64), b = render(bg, fg, 8, 2, 76, 60, 64);
    const Pyramid pa = Pyramid::of(lumaOf(a), 4), pb = Pyramid::of(lumaOf(b), 4);
    const MotionField f = est.estimate(pa, pb), k = est.estimate(pb, pa);
    Bgra plain, boxed;
    interpolate(a, b, f, k, nullptr, 0.5f, plain, 2);
    // A box over the top left quarter (0..160 x 0..96 px), hard edge.
    const ProtectBox list[] = {{0.5f, 0.5f, 0.f, 0.f, false}, {0.2f, 0.2f, 0.201f, 0.9f, false}};  // swapped, and one too thin
    Protection p = Protection::clean(list, 2, 0.f);
    CHECK(p.count == 1 && p.boxes[0].left == 0.f && p.boxes[0].right == 0.5f, "boxes are put in order, a too thin one dropped");
    interpolate(a, b, f, k, nullptr, 0.5f, boxed, 2, {}, nullptr, nullptr, &p);
    int inside = 0, outside = 0;
    for (int y = 0; y < 192; ++y)
        for (int x = 0; x < 320; ++x) {
            const bool in = x < 160 && y < 96;
            if (in && std::memcmp(boxed.pixel(x, y), a.pixel(x, y), 3) != 0) ++inside;
            if (!in && std::memcmp(boxed.pixel(x, y), plain.pixel(x, y), 4) != 0) ++outside;
        }
    CHECK(inside == 0, "inside the box the older real frame (%d px differ)", inside);
    CHECK(outside == 0, "outside it the generated frame as it was (%d px differ)", outside);
    // Real frames are never touched.
    Bgra real;
    interpolate(a, b, f, k, nullptr, 1.f, real, 2, {}, nullptr, nullptr, &p);
    CHECK(real.px == b.px, "a real frame is shown as it is");
    // Feather: full weight 8 px inside the edge, half at about 4, none at the edge.
    p.feather = 8.f;
    CHECK(p.weight(80.5f, 48.5f, 320, 192) == 1.f && p.weight(159.5f, 48.5f, 320, 192) < 0.02f &&
          std::abs(p.weight(156.f, 48.5f, 320, 192) - 0.5f) < 0.01f && p.weight(170.f, 48.5f, 320, 192) == 0.f, "the feather");
    // Ellipse: the centre is held, the box's corner is not.
    const ProtectBox round[] = {{0.25f, 0.25f, 0.75f, 0.75f, true}};
    const Protection e = Protection::clean(round, 1, 0.f);
    CHECK(e.weight(160.f, 96.f, 320, 192) == 1.f && e.weight(81.f, 49.f, 320, 192) == 0.f, "the ellipse leaves its box's corners");
    // No boxes: nothing is different.
    Protection none;
    interpolate(a, b, f, k, nullptr, 0.5f, boxed, 2, {}, nullptr, nullptr, &none);
    CHECK(boxed.px == plain.px, "no boxes, no change");
    const ProtectBox bad[] = {{std::nanf(""), -3.f, 7.f, 0.5f, false}};
    const Protection q = Protection::clean(bad, 1, std::nanf(""));
    CHECK(q.count == 1 && q.boxes[0].left == 0.f && q.boxes[0].right == 1.f && q.boxes[0].top == 0.f && q.feather == 8.f, "values out of range are put in range");
}

void testDedup() {
    Gray a(64, 36, 50);
    for (int i = 0; i < 64 * 36; ++i) a.px[i] = uint8_t((i * 37) % 200);
    Gray b = a;
    CHECK(compareForDuplicate(a, b).duplicate, "identical");
    std::mt19937 rng(5);
    for (auto& v : b.px) v = uint8_t(std::clamp(int(v) + int(rng() % 3) - 1, 0, 255));
    CHECK(compareForDuplicate(a, b).duplicate, "capture noise +-1");
    Gray c = a;
    for (int y = 10; y < 13; ++y) for (int x = 20; x < 23; ++x) c.at(x, y) = 255;  // a 3x3 cursor at quarter size
    CHECK(!compareForDuplicate(a, c).duplicate, "small change is new");
    Gray d = a;
    for (auto& v : d.px) v = uint8_t(std::min(255, v + 2));  // slow fade
    CHECK(!compareForDuplicate(a, d).duplicate, "fade is new");
    CHECK(!compareForDuplicate(a, Gray(32, 18, 50)).duplicate, "other size");
    CHECK(!compareForDuplicate(Gray(), a).duplicate, "empty");
}

void testHudLock() {
    const Scene bg(320, 192, 80, 7), fg(32, 32, 4, 8);
    HudLock hud;
    Gray mask;
    for (int i = 0; i < 40; ++i) {
        const Bgra f = render(bg, fg, float(i * 3), 0, -1000, -1000, 0, true);
        mask = hud.update(halve(lumaOf(f)));
    }
    // HUD cells: 8x8 at full size (4x4 at half size) in a checkerboard from (8, 8); the inner 2x2 of
    // each white cell is fully HUD.
    int hudOn = 0, hudTotal = 0, bgOn = 0, bgTotal = 0;
    for (int y = 0; y < mask.height; ++y)
        for (int x = 0; x < mask.width; ++x) {
            const int fx = x * 2, fy = y * 2;
            const bool inHud = fx >= 8 && fx < 104 && fy >= 8 && fy < 24 && ((fx / 8 + fy / 8) % 2 == 0) && (x % 4 == 1 || x % 4 == 2) && (y % 4 == 1 || y % 4 == 2);
            const bool farFromHud = y > 20;
            if (inHud) { ++hudTotal; hudOn += mask.at(x, y) > 128; }
            if (farFromHud) { ++bgTotal; bgOn += mask.at(x, y) > 0; }
        }
    CHECK(hudOn > hudTotal * 0.9, "HUD locked %d/%d", hudOn, hudTotal);
    CHECK(bgOn < bgTotal / 100, "moving background not locked %d/%d", bgOn, bgTotal);
    // HUD disappears: released on the next frame.
    mask = hud.update(halve(lumaOf(render(bg, fg, 200, 0, -1000, -1000, 0, false))));
    int still = 0;
    for (int y = 4; y < 12; ++y) for (int x = 4; x < 52; ++x) still += mask.at(x, y) > 0;
    CHECK(still == 0, "released at once (%d)", still);
    // Size change resets.
    CHECK(hud.update(Gray(10, 10, 3)).px == std::vector<uint8_t>(100, 0), "size change resets");
}

// Simulated capture: 60 Hz transport, 30 FPS content, with optional 17/50 ms (1 then 3 refresh) pacing.
// ---- Crosshair Hold ----------------------------------------------------------------------------
// A panning picture of moderate brightness with a white crosshair standing at the screen centre: four
// strokes two pixels thick (what a 4K reticle's four-pixel strokes are at 720 lines).
bool onReticle(int x, int y, int w, int h) {
    const int cx = w / 2, cy = h / 2, dx = x - cx, dy = y - cy;
    const bool across = dy >= -1 && dy <= 0 && std::abs(dx) >= 5 && std::abs(dx) <= 13;
    const bool down = dx >= -1 && dx <= 0 && std::abs(dy) >= 5 && std::abs(dy) <= 13;
    return across || down;
}
Bgra renderAim(const Scene& bg, float shift, bool reticle) {
    Bgra img(bg.w, bg.h);
    for (int y = 0; y < bg.h; ++y)
        for (int x = 0; x < bg.w; ++x) {
            const float v = 40.f + bg.at(float(x) - shift, float(y)) * 0.45f;   // 40..155: the crosshair always stands out
            float r = v, g = v * 0.9f + 8.f, b = 170.f - v * 0.5f;
            if (reticle && onReticle(x, y, bg.w, bg.h)) { r = 250; g = 250; b = 250; }
            uint8_t* p = img.pixel(x, y);
            p[0] = uint8_t(std::clamp(b, 0.f, 255.f)); p[1] = uint8_t(std::clamp(g, 0.f, 255.f)); p[2] = uint8_t(std::clamp(r, 0.f, 255.f)); p[3] = 255;
        }
    return img;
}
// Mean and worst channel difference from the truth over the crosshair's own pixels, and over the two
// places beside it where frame generation alone leaves its ghosts (half the motion to either side).
struct AimError { double reticle = 0, ghost = 0; };
AimError aimError(const Bgra& out, const Bgra& truth, int motion) {
    AimError e; double rs = 0, gs = 0; int rn = 0, gn = 0;
    for (int y = 0; y < out.height; ++y)
        for (int x = 0; x < out.width; ++x) {
            double d = 0;
            for (int c = 0; c < 3; ++c) d = std::max(d, std::abs(double(out.pixel(x, y)[c]) - double(truth.pixel(x, y)[c])));
            if (onReticle(x, y, out.width, out.height)) { rs += d; ++rn; }
            else if (onReticle(x - motion / 2, y, out.width, out.height) || onReticle(x + motion / 2, y, out.width, out.height)) { gs += d; ++gn; }
        }
    e.reticle = rn ? rs / rn : 0; e.ghost = gn ? gs / gn : 0;
    return e;
}

void testCrosshairHold() {
    const int W = 1280, H = 720, motion = 12;
    const Scene bg(W, H, 200, 31);
    const MotionEstimator est;
    {   // too small a picture has no zone and nothing is done
        CrosshairHold tiny;
        CHECK(tiny.frame(Bgra(160, 90)) == nullptr, "no zone in a tiny picture");
        CHECK(tiny.frame(Bgra()) == nullptr, "an empty picture");
    }
    // The crosshair over a pan: found after it has stood while the picture moved, then held.
    CrosshairHold hold;
    std::vector<Bgra> frames;
    std::vector<std::shared_ptr<const HoldFrame>> parts;
    int firstHeld = -1;
    for (int i = 0; i < 10; ++i) {
        frames.push_back(renderAim(bg, float(i * motion), true));
        parts.push_back(hold.frame(frames.back()));
        CHECK(parts.back() != nullptr && parts.back()->size == 128, "the zone at 720 lines is 128 (got %d)", parts.back() ? parts.back()->size : -1);
        if (parts.back() && parts.back()->masked > 0 && firstHeld < 0) firstHeld = i;
    }
    std::printf("  crosshair hold: held from frame %d, %d px in the mask\n", firstHeld, parts.back()->masked);
    CHECK(firstHeld >= 3 && firstHeld <= 6, "held after it has stood four moving frames (frame %d)", firstHeld);
    CHECK(parts.back()->masked >= 60 && parts.back()->masked < 1500, "the mask is the crosshair and little more (%d px)", parts.back()->masked);
    CHECK(parts[0]->masked == 0 && CrosshairHold::pair(*parts[0], *parts[1]) == nullptr, "nothing is held before it is found, and no pair is made");
    // The pair between the last two frames.
    const Bgra &a = frames[8], &b = frames[9];
    const auto pair = CrosshairHold::pair(*parts[8], *parts[9]);
    CHECK(pair != nullptr && pair->held > 0 && pair->size == 128 && pair->x == (W - 128) / 2 && pair->y == (H - 128) / 2, "a pair for the generated frames");
    if (!pair) return;
    const Pyramid pa = Pyramid::of(lumaOf(a), 4), pb = Pyramid::of(lumaOf(b), 4);
    const MotionField f = est.estimate(pa, pb), k = est.estimate(pb, pa);
    const Bgra truth = renderAim(bg, 8.5f * float(motion), true);
    Bgra plain, held, held1;
    CHECK(interpolate(a, b, f, k, nullptr, 0.5f, plain, 3), "plain");
    CHECK(interpolate(a, b, f, k, nullptr, 0.5f, held, 3, {}, pair.get()), "held");
    const AimError before = aimError(plain, truth, motion), after = aimError(held, truth, motion);
    std::printf("  crosshair's own pixels off by %.1f -> %.1f of 255, its ghosts beside it %.1f -> %.1f\n", before.reticle, after.reticle, before.ghost, after.ghost);
    CHECK(before.reticle > 30.0, "without it the crosshair is gone from the middle (%.1f)", before.reticle);
    CHECK(after.reticle < 8.0 && after.reticle * 6 < before.reticle, "with it the crosshair stands where it is (%.1f)", after.reticle);
    CHECK(before.ghost > 20.0 && after.ghost < 8.0 && after.ghost * 3 < before.ghost, "and its ghosts beside it are gone (%.1f -> %.1f)", before.ghost, after.ghost);
    // Far from the zone nothing is different.
    int farDifferent = 0;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const bool nearZone = x >= pair->x - 48 && x < pair->x + pair->size + 48 && y >= pair->y - 48 && y < pair->y + pair->size + 48;
            if (!nearZone && std::memcmp(plain.pixel(x, y), held.pixel(x, y), 4) != 0) ++farDifferent;
        }
    CHECK(farDifferent == 0, "pixels away from the zone are what they were (%d differ)", farDifferent);
    interpolate(a, b, f, k, nullptr, 0.5f, held1, 1, {}, pair.get());
    CHECK(held1.px == held.px, "threads give the same picture");
    Bgra real;
    interpolate(a, b, f, k, nullptr, 0.f, real, 2, {}, pair.get());
    CHECK(real.px == a.px, "a real frame is shown as it is");
    interpolate(a, b, f, k, nullptr, 1.f, real, 2, {}, pair.get());
    CHECK(real.px == b.px, "and so is the other");
    {   // a pair that does not fit the picture is ignored, not read out of bounds
        HoldPair bad = *pair; bad.x = W - 20;
        Bgra o; interpolate(a, b, f, k, nullptr, 0.5f, o, 2, {}, &bad);
        CHECK(o.px == plain.px, "a pair outside the picture changes nothing");
        HoldPair cut = *pair; cut.filledA = Bgra(8, 8);
        interpolate(a, b, f, k, nullptr, 0.5f, o, 2, {}, &cut);
        CHECK(o.px == plain.px, "nor one whose pictures are not the zone's size");
    }
    {   // the faster pass decides exactly what the rule's own step() decides: the mask, every count, every memory
        using namespace veyra::pipeline::overlay;
        const Zone zone = zoneFor(uint32_t(W), uint32_t(H));
        const Rule rule = ruleFor(uint32_t(H));
        const int n = int(zone.size);
        int different = 0, maskedSeen = 0;
        for (int shape = 0; shape < 3; ++shape) {     // the crosshair, a ring (its inside is closed in), plain noise
            State slow(n), fast(n);
            std::mt19937 rng(77);
            for (int i = 0; i < 12; ++i) {
                Bgra frame = renderAim(bg, float(i * motion), shape == 0);
                if (shape == 1)
                    for (int y = H / 2 - 9; y <= H / 2 + 9; ++y)
                        for (int x = W / 2 - 9; x <= W / 2 + 9; ++x) {
                            const double d = std::hypot(double(x - W / 2), double(y - H / 2));
                            if (d >= 5.0 && d <= 7.0) { uint8_t* p = frame.pixel(x, y); p[0] = p[1] = p[2] = 250; }
                        }
                Plane luma(n);
                for (int y = 0; y < n; ++y)
                    for (int x = 0; x < n; ++x) {
                        const uint8_t* p = frame.pixel(int(zone.x) + x, int(zone.y) + y);
                        float v = lumaOf(float(p[2]) / 255.0f, float(p[1]) / 255.0f, float(p[0]) / 255.0f, 0);
                        if (shape == 2) v = float(rng() % 1000) / 999.0f;   // every pixel a bar candidate somewhere, edges included
                        luma.at(x, y) = v;
                    }
                const Plane a = step(slow, luma, rule), b = stepFast(fast, luma, rule);
                if (a.v != b.v || slow.moving != fast.moving) ++different;
                for (int k = 0; k < kKinds; ++k) if (slow.count[k].v != fast.count[k].v || slow.memory[k].v != fast.memory[k].v) ++different;
                for (const float v : a.v) if (v > 0) ++maskedSeen;
            }
        }
        CHECK(different == 0, "the faster pass is the rule's own, to the pixel (%d differences)", different);
        CHECK(maskedSeen > 500, "and the comparison saw masks (%d px)", maskedSeen);
    }
    {   // a plain pan with no crosshair: nothing is ever held
        CrosshairHold none; int marked = 0;
        for (int i = 0; i < 12; ++i) { const auto part = none.frame(renderAim(bg, float(i * motion), false)); marked += part ? part->masked : 0; }
        CHECK(marked == 0, "a pan without a crosshair marks nothing (%d px)", marked);
        // and a still picture with one: it stands, but nothing moves, so it is not taken for an overlay
        CrosshairHold still; marked = 0;
        for (int i = 0; i < 12; ++i) { const auto part = still.frame(renderAim(bg, 0, true)); marked += part ? part->masked : 0; }
        CHECK(marked == 0, "a still picture marks nothing (%d px)", marked);
    }
    {   // through the engine: the switch off carries nothing, on it carries the pair with the generated frame
        for (int on = 0; on < 2; ++on) {
            EngineSettings s;
            s.crosshairHold = on != 0;
            s.removeDuplicates = false;
            s.estimateBudgetMs = 6;
            Engine e(s);
            const int64_t T = 33'333'333;
            int64_t now = 1'000'000'000;
            int withHold = 0, generated = 0;
            for (int i = 0; i < 12; ++i) {
                e.push(renderAim(bg, float(i * motion), true), now);
                e.waitIdle();
                for (int step = 1; step <= 3; ++step) {
                    const Pick p = e.pick(now + step * (T / 4));
                    if (p.generated()) { ++generated; if (p.hold) ++withHold; CHECK(!p.hold || (p.hold->size == 128 && p.hold->held > 0), "a carried pair is whole"); }
                    else CHECK(p.hold == nullptr, "a real frame carries no pair");
                }
                now += T;
            }
            const EngineStats st = e.stats();
            std::printf("  engine, switch %s: %d generated picks, %d with the crosshair held, %d px held, state %d\n", on ? "on" : "off", generated, withHold, st.holdPixels, st.holdState);
            if (on) { CHECK(st.holdState == 1 && st.holdPixels > 0, "the engine says it holds a crosshair"); CHECK(generated == 0 || withHold > 0, "generated frames carry the pair"); }
            else CHECK(st.holdState == 0 && st.holdPixels == 0 && withHold == 0, "switched off nothing is held");
        }
    }
}

// Subtitle Hold: a line of "text" (thin strokes of letters) in the subtitle band over a panning picture.
bool onText(int x, int y, int W, int H) {
    const auto zone = veyra::pipeline::overlay::subtitleZoneFor(uint32_t(W), uint32_t(H));
    if (zone.size == 0) return false;
    const int top = int(zone.y + zone.h / 2) - 8, left = int(zone.x) + 40;
    const int lx = x - left, ly = y - top;
    if (ly < 0 || ly >= 16 || lx < 0 || lx >= 12 * 14) return false;
    const int letter = lx / 14, cx = lx % 14;
    if (cx >= 10) return false;                       // the gap between letters
    const bool vertical = (cx < 2) || (letter % 3 == 0 && cx >= 8);
    const bool horizontal = (ly < 2) || (letter % 2 == 0 && ly >= 7 && ly < 9) || (letter % 3 != 1 && ly >= 14);
    return vertical || horizontal;
}
Bgra renderText(const Scene& bg, float shift, bool text) {
    Bgra img = renderAim(bg, shift, false);
    if (text)
        for (int y = 0; y < bg.h; ++y)
            for (int x = 0; x < bg.w; ++x)
                if (onText(x, y, bg.w, bg.h)) { uint8_t* p = img.pixel(x, y); p[0] = p[1] = p[2] = 250; }
    return img;
}
void testSubtitleHold() {
    using namespace veyra::pipeline::overlay;
    const int W = 1280, H = 720, motion = 12;
    const Scene bg(W, H, 200, 37);
    const Zone zone = subtitleZoneFor(uint32_t(W), uint32_t(H));
    CHECK(zone.size > 0 && zone.h < zone.w, "a band at 720 lines");
    CrosshairHold hold(HoldKind::Subtitles);
    std::vector<Bgra> frames;
    std::vector<std::shared_ptr<const HoldFrame>> parts;
    for (int i = 0; i < 10; ++i) {
        frames.push_back(renderText(bg, float(i * motion), true));
        parts.push_back(hold.frame(frames.back()));
        CHECK(parts.back() && parts.back()->w == int(zone.w) && parts.back()->h == int(zone.h) && parts.back()->x == int(zone.x) && parts.back()->y == int(zone.y),
              "the band's place and size");
    }
    int textPixels = 0, maskedText = 0;
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) if (onText(x, y, W, H)) {
        ++textPixels;
        const int zx = x - int(zone.x), zy = y - int(zone.y);
        if (parts.back()->mask.at(zx, zy) > 0) ++maskedText;
    }
    std::printf("  subtitle hold: %d of %d text pixels masked, %d px in the mask\n", maskedText, textPixels, parts.back()->masked);
    CHECK(maskedText * 10 >= textPixels * 9, "the line is held (%d of %d)", maskedText, textPixels);
    const auto pair = CrosshairHold::pair(*parts[8], *parts[9]);
    CHECK(pair && pair->held > 0 && pair->w == int(zone.w) && pair->h == int(zone.h), "a pair for the band");
    if (!pair) return;
    const Bgra &a = frames[8], &b = frames[9];
    const MotionEstimator est;
    const Pyramid pa = Pyramid::of(lumaOf(a), 4), pb = Pyramid::of(lumaOf(b), 4);
    const MotionField f = est.estimate(pa, pb), k = est.estimate(pb, pa);
    const Bgra truth = renderText(bg, 8.5f * float(motion), true);
    Bgra plain, held;
    interpolate(a, b, f, k, nullptr, 0.5f, plain, 3);
    interpolate(a, b, f, k, nullptr, 0.5f, held, 3, {}, nullptr, pair.get());
    double before = 0, after = 0; int n = 0;
    for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) if (onText(x, y, W, H)) {
        double d0 = 0, d1 = 0;
        for (int c = 0; c < 3; ++c) {
            d0 = std::max(d0, std::abs(double(plain.pixel(x, y)[c]) - truth.pixel(x, y)[c]));
            d1 = std::max(d1, std::abs(double(held.pixel(x, y)[c]) - truth.pixel(x, y)[c]));
        }
        before += d0; after += d1; ++n;
    }
    before /= std::max(1, n); after /= std::max(1, n);
    std::printf("  subtitle text off by %.1f -> %.1f of 255\n", before, after);
    CHECK(before > 10.0 && after < 6.0 && after * 3 < before, "the text stands where it is (%.1f -> %.1f)", before, after);
    // Both zones at once: a crosshair and a line of text; each held in its own zone, nothing else touched.
    {
        CrosshairHold aim, sub(HoldKind::Subtitles);
        std::shared_ptr<const HoldFrame> pa0, pa1, ps0, ps1;
        Bgra f0, f1;
        for (int i = 0; i < 10; ++i) {
            Bgra fr = renderAim(bg, float(i * motion), true);
            for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) if (onText(x, y, W, H)) { uint8_t* p = fr.pixel(x, y); p[0] = p[1] = p[2] = 250; }
            pa0 = pa1; ps0 = ps1; f0 = f1;
            pa1 = aim.frame(fr); ps1 = sub.frame(fr); f1 = fr;
        }
        const auto ha = CrosshairHold::pair(*pa0, *pa1), hs = CrosshairHold::pair(*ps0, *ps1);
        CHECK(ha && hs, "both zones make a pair");
        Bgra one, both;
        const Pyramid q0 = Pyramid::of(lumaOf(f0), 4), q1 = Pyramid::of(lumaOf(f1), 4);
        const MotionField g = est.estimate(q0, q1), gk = est.estimate(q1, q0);
        interpolate(f0, f1, g, gk, nullptr, 0.5f, one, 2, {}, ha.get());
        interpolate(f0, f1, g, gk, nullptr, 0.5f, both, 2, {}, ha.get(), hs.get());
        int outsideBand = 0;
        for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
            const bool inBand = x >= hs->x && y >= hs->y && x < hs->x + hs->w && y < hs->y + hs->h;
            if (!inBand && std::memcmp(one.pixel(x, y), both.pixel(x, y), 3) != 0) ++outsideBand;
        }
        CHECK(outsideBand < 200, "the band changes nothing far from itself (%d px)", outsideBand);
    }
    {   // the faster pass in the band is the rule's own step(), to the pixel
        const Rule rule = ruleFor(uint32_t(H));
        State slow(int(zone.w), int(zone.h)), fast(int(zone.w), int(zone.h));
        int different = 0, seen = 0;
        for (int i = 0; i < 10; ++i) {
            const Bgra fr = renderText(bg, float(i * motion), true);
            Plane luma(int(zone.w), int(zone.h), 0.0f);
            for (int y = 0; y < int(zone.h); ++y)
                for (int x = 0; x < int(zone.w); ++x) {
                    const uint8_t* p = fr.pixel(int(zone.x) + x, int(zone.y) + y);
                    luma.at(x, y) = lumaOf(float(p[2]) / 255.0f, float(p[1]) / 255.0f, float(p[0]) / 255.0f, 0);
                }
            const Plane m0 = step(slow, luma, rule), m1 = stepFast(fast, luma, rule);
            if (m0.v != m1.v || slow.moving != fast.moving) ++different;
            for (int kk = 0; kk < kKinds; ++kk) if (slow.count[kk].v != fast.count[kk].v || slow.memory[kk].v != fast.memory[kk].v) ++different;
            for (const float v : m0.v) if (v > 0) ++seen;
        }
        CHECK(different == 0 && seen > 500, "the faster pass in the band is the rule's own (%d differences, %d px seen)", different, seen);
    }
    {   // through the engine, its own switch
        for (int on = 0; on < 2; ++on) {
            EngineSettings s;
            s.subtitleHold = on != 0;
            s.removeDuplicates = false;
            s.estimateBudgetMs = 6;
            Engine e(s);
            const int64_t T = 33'333'333;
            int64_t now = 1'000'000'000;
            int withSub = 0, withAim = 0;
            for (int i = 0; i < 12; ++i) {
                e.push(renderText(bg, float(i * motion), true), now);
                e.waitIdle();
                for (int st = 1; st <= 3; ++st) {
                    const Pick p = e.pick(now + st * (T / 4));
                    if (p.generated() && p.subHold) ++withSub;
                    if (p.hold) ++withAim;
                }
                now += T;
            }
            const EngineStats st = e.stats();
            if (on) CHECK(st.subHoldState == 1 && st.subHoldPixels > 0 && withSub > 0, "the engine holds the line (%d picks)", withSub);
            else CHECK(st.subHoldState == 0 && withSub == 0, "switched off nothing is held");
            CHECK(withAim == 0 && st.holdState == 0, "Crosshair Hold's own switch is not touched");
        }
    }
}

void testEngineTiming(bool uneven, bool repair, double target, int64_t refreshNs, double expectOut) {
    const Scene bg(160, 96, 420, 11), fg(16, 16, 2, 12);
    EngineSettings s;
    s.pacingRepair = repair;
    s.targetFps = target;
    s.estimateBudgetMs = 6;  // simulated clock: real motion estimation time (slow under sanitizers) must not move the delay
    Engine e(s);
    const int64_t T = 16666667;  // 60 Hz
    int64_t now = 1'000'000'000;
    // Content index per 60 Hz slot: even = each picture twice, uneven = 1, 3, 1, 3 ... repeats.
    std::vector<int> slots;
    for (int c = 1; slots.size() < 360; ++c)
        for (int r = 0; r < (uneven ? ((c % 2) ? 3 : 1) : 2); ++r) slots.push_back(c);
    std::vector<float> ts;
    int64_t display = now + 300 * 1000000LL;
    for (int slot = 0; slot < 360; ++slot) {
        const int contentIndex = slots[size_t(slot)];
        e.push(render(bg, fg, float(contentIndex * 2), 0, 50.f + float(contentIndex), 40, 16), now);
        e.waitIdle();
        now += T;
        while (display <= now - 100 * 1000000LL) {
            const Pick p = e.pick(display);
            if (slot > 120 && p.valid()) ts.push_back(float(p.aId) + p.t);
            display += refreshNs;
        }
    }
    const EngineStats st = e.stats();
    std::printf("  uneven=%d repair=%d target=%.0f: transport %.1f content %.1f output %.1f fps, delay %.1f ms, latency %.1f ms, late %llu\n",
                uneven, repair, target, st.transportFps, st.contentFps, st.outputFps, st.delayMs, st.latencyMs, (unsigned long long)st.lateFields);
    CHECK(std::abs(st.transportFps - 60.0) < 1.0, "transport");
    CHECK(std::abs(st.contentFps - 30.0) < 1.5, "content %.1f", st.contentFps);
    CHECK(std::abs(st.outputFps - expectOut) < expectOut * 0.06, "output %.1f expected %.1f", st.outputFps, expectOut);
    CHECK(st.duplicates > 100, "duplicates removed");
    CHECK(st.lateFields == 0, "fields ready in time");
    // Smoothness: steps of content position between consecutive pictures.
    std::vector<float> steps;
    for (size_t i = 1; i < ts.size(); ++i) if (ts[i] != ts[i - 1]) steps.push_back(ts[i] - ts[i - 1]);
    double mean = 0, var = 0;
    for (float d : steps) mean += d;
    mean /= double(steps.size());
    for (float d : steps) var += (d - mean) * (d - mean);
    const double sd = std::sqrt(var / double(steps.size()));
    std::printf("    picture step %.3f +- %.3f content frames\n", mean, sd);
    if (!uneven || repair) CHECK(sd < mean * 0.15, "even motion (sd %.3f of %.3f)", sd, mean);
    else CHECK(sd > mean * 0.3, "uneven input stays uneven without repair (sd %.3f of %.3f)", sd, mean);
}

// A machine that runs late for a while gets a margin, and gives it back once its motion is ready in
// good time again, without running late to find that out. The clock is simulated: frames arrive every
// 60th of a second (each picture twice: a 30 FPS game), the screen refreshes 120 times a second and
// asks, as the app does, for the picture of the next refresh.
void testEngineMargin() {
    const Scene bg(160, 96, 420, 11), fg(16, 16, 2, 12);
    EngineSettings s;
    s.estimateBudgetMs = 6;
    Engine e(s);
    std::atomic<int64_t> sim{1'000'000'000};
    e.setClock([&] { return sim.load(); });
    const int64_t T = 16666667, R = 8333333;
    int64_t nextCapture = sim.load(), nextRefresh = sim.load() + 1'000'000;
    int slot = 0;
    bool pending = false;
    int64_t releaseAt = 0;
    // slowNs: how long after a new picture arrives its pair's motion is ready (0: at once).
    const auto run = [&](int slots, int64_t slowNs) {
        e.holdWorker(slowNs > 0);
        for (const int until = slot + slots; slot < until;) {
            const bool capture = nextCapture <= nextRefresh;
            const int64_t t = capture ? nextCapture : nextRefresh;
            sim = t;
            if (pending && t >= releaseAt) { e.holdWorker(false); e.waitIdle(); e.holdWorker(true); pending = false; }
            if (capture) {
                // The background goes two pixels a picture one way and then back (the scene is only so
                // wide, and the run is long); no two pictures in a row are the same.
                const int content = slot / 2 + 1, k = content % 400;
                e.push(render(bg, fg, float(k < 200 ? k * 2 : (400 - k) * 2), 0, 50.f + float(content % 60), 40, 16), t);
                if (slowNs > 0) { if (slot % 2 == 0) { pending = true; releaseAt = t + slowNs; } }
                else e.waitIdle();
                ++slot;
                nextCapture += T;
            } else {
                (void)e.pick(t + R);
                nextRefresh += R;
            }
        }
        if (pending) { e.holdWorker(false); e.waitIdle(); pending = false; }
        e.holdWorker(false);
    };
    run(240, 0);
    EngineStats st = e.stats();
    std::printf("  margin: steady          delay %.1f ms, margin %.1f ms, least slack %.1f ms, late %llu\n", st.delayMs, st.marginMs, st.slackMs, (unsigned long long)st.lateFields);
    CHECK(st.lateFields == 0 && st.marginMs == 0.0, "a machine that keeps up needs no margin (late %llu, margin %.2f)", (unsigned long long)st.lateFields, st.marginMs);
    CHECK(std::abs(st.slackMs - 6.0) < 1.0, "its motion is ready the whole allowance early (%.2f ms)", st.slackMs);
    const double steadyDelay = st.delayMs;
    run(120, 30'000'000);
    st = e.stats();
    std::printf("  margin: 30 ms motion    delay %.1f ms, margin %.1f ms, least slack %.1f ms, late %llu\n", st.delayMs, st.marginMs, st.slackMs, (unsigned long long)st.lateFields);
    CHECK(st.lateFields > 0 && st.marginMs > 15.0 && st.marginMs <= 40.0, "running late builds a margin (%.1f ms after %llu late refreshes)", st.marginMs, (unsigned long long)st.lateFields);
    const double slowMargin = st.marginMs;
    const uint64_t lateAfterSlow = st.lateFields;
    // Fast again. For as long as the slow pairs are among those looked back on, their margin is kept.
    run(240, 0);
    st = e.stats();
    std::printf("  margin: fast again, 4 s delay %.1f ms, margin %.1f ms, least slack %.1f ms\n", st.delayMs, st.marginMs, st.slackMs);
    CHECK(st.marginMs > slowMargin - 4.0, "what was needed four seconds ago is not given away yet (%.1f of %.1f ms)", st.marginMs, slowMargin);
    run(660, 0);
    st = e.stats();
    std::printf("  margin: fast again, 15 s delay %.1f ms, margin %.1f ms, least slack %.1f ms, late %llu\n", st.delayMs, st.marginMs, st.slackMs, (unsigned long long)st.lateFields);
    CHECK(st.marginMs < 0.5, "the margin is given back (%.2f ms left)", st.marginMs);
    CHECK(st.lateFields == lateAfterSlow, "and never by running late to find out (%llu more)", (unsigned long long)(st.lateFields - lateAfterSlow));
    CHECK(std::abs(st.delayMs - steadyDelay) < 1.0, "the delay is what it was before (%.1f, was %.1f ms)", st.delayMs, steadyDelay);
    CHECK(st.slackMs > Engine::kSlackGuardMs - 0.01, "with the guard's slack to spare (%.2f ms)", st.slackMs);
}

// The allowance before any pair has been measured is not the floor (a session's first seconds ran
// late with it and filled the margin).
void testEngineStartBudget() {
    const Scene bg(160, 96, 420, 11), fg(16, 16, 2, 12);
    Engine e;  // measured allowance, as the app runs
    e.holdWorker(true);  // nothing measured yet
    int64_t now = 1'000'000'000;
    for (int i = 1; i <= 4; ++i) { e.push(render(bg, fg, float(i * 2), 0, 50.f + float(i), 40, 16), now); now += 33'333'333; }
    for (int k = 0; k < 8; ++k) (void)e.pick(now + k * 8'333'333);
    const EngineStats st = e.stats();
    // One interval of 33.3 ms and the allowance (the screen's own refresh is added once it is known;
    // with the 6 ms floor this was 39 ms, and every pair of a session's first seconds came late).
    CHECK(st.delayMs > 33.3 + Engine::kStartBudgetMs - 1.0, "the first delay leaves room for motion that is not measured yet (%.1f ms)", st.delayMs);
    e.holdWorker(false);
    e.waitIdle();
}

// The readout's three rates (the footer's "game -> shown FPS"): they follow the picture, and fall off
// when it stands still or stops instead of keeping the last rate they had (they did: a menu showed the
// rate of the last moving second for as long as it stood).
void testEngineLiveRates() {
    const Scene bg(64, 48, 40, 31), fg(8, 8, 1, 32);
    EngineSettings s;
    s.frameGeneration = false;
    Engine e(s);
    int64_t now = 1'000'000'000;
    const int64_t frame = 33'333'333;
    for (int i = 0; i < 30; ++i) {
        e.push(render(bg, fg, float(i * 4), 0, 10, 10, 8), now);
        (void)e.pick(now);
        now += frame;
    }
    EngineStats st = e.stats(now);
    CHECK(std::abs(st.transportFps - 30) < 1.5 && std::abs(st.contentFps - 30) < 1.5 && std::abs(st.outputFps - 30) < 1.5,
          "a moving picture at 30: signal %.1f game %.1f shown %.1f", st.transportFps, st.contentFps, st.outputFps);
    // The same picture again and again (a paused game): the signal goes on, the game and what is shown
    // fall off, and read 0 a second after the last new picture.
    const Bgra still = render(bg, fg, 200, 0, 10, 10, 8);
    for (int i = 0; i < 18; ++i) { e.push(Bgra(still), now); (void)e.pick(now); now += frame; }
    st = e.stats(now);
    CHECK(std::abs(st.transportFps - 30) < 1.5, "a still picture: the signal still arrives (%.1f)", st.transportFps);
    CHECK(st.contentFps < 20 && st.outputFps < 20, "a still picture for 0.6 s: game %.1f shown %.1f fall off (they kept 30 before)", st.contentFps, st.outputFps);
    CHECK(e.stats().contentFps > 29, "the rate pacing reads is unchanged (%.1f)", e.stats().contentFps);
    for (int i = 0; i < 15; ++i) { e.push(Bgra(still), now); (void)e.pick(now); now += frame; }
    st = e.stats(now);
    CHECK(st.contentFps == 0 && st.outputFps == 0 && std::abs(st.transportFps - 30) < 1.5, "a still picture for 1.1 s: game %.1f shown %.1f signal %.1f",
          st.contentFps, st.outputFps, st.transportFps);
    // A card whose frames come over USB in bunches: 60 a second, each handed over up to 30 ms late (in order).
    // The signal reads 60, where a rate over the last half second read up to 64.
    {
        Engine b(s);
        std::mt19937 rng(77);
        std::uniform_int_distribution<int64_t> held(0, 30'000'000);
        int64_t t = 1'000'000'000, last = 0;
        double worst = 0, worstBefore = 0;
        for (int i = 0; i < 600; ++i) {
            const int64_t at = std::max(last + 1, t + held(rng));
            last = at;
            b.push(render(bg, fg, float(i % 40), 0, 10, 10, 8), at);
            t += 16'666'667;
            if (i > 150 && i % 7 == 0) {
                worst = std::max(worst, std::abs(b.stats(at).transportFps - 60));
                worstBefore = std::max(worstBefore, std::abs(b.stats().transportFps - 60));
            }
        }
        std::printf("  bunched 60 Hz card: signal off by at most %.2f FPS (over half a second: %.2f)\n", worst, worstBefore);
        CHECK(worst < 1.0 && worstBefore > worst, "bunched 60 Hz card: signal off by %.2f FPS (half a second: %.2f)", worst, worstBefore);
    }
    // Nothing arrives at all: everything falls off, then reads 0.
    for (int i = 0; i < 25; ++i) { e.push(render(bg, fg, float(300 + i * 4), 0, 10, 10, 8), now); (void)e.pick(now); now += frame; }
    now += 400'000'000;
    st = e.stats(now);
    CHECK(st.transportFps > 0 && st.transportFps < 25, "0.4 s without frames: the signal falls off (%.1f)", st.transportFps);
    now += 700'000'000;
    st = e.stats(now);
    CHECK(st.transportFps == 0 && st.contentFps == 0 && st.outputFps == 0, "1.1 s without frames: %.1f %.1f %.1f", st.transportFps, st.contentFps, st.outputFps);
}

void testEngineOff() {
    const Scene bg(64, 48, 40, 21), fg(8, 8, 1, 22);
    EngineSettings s;
    s.frameGeneration = false;
    Engine e(s);
    CHECK(!e.pick(0).valid(), "nothing before the first frame");
    int64_t now = 1'000'000;
    for (int i = 0; i < 10; ++i) {
        e.push(render(bg, fg, float(i * 4), 0, 10, 10, 8), now);
        const Pick p = e.pick(now);
        CHECK(p.valid() && p.a == p.b && !p.generated() && p.aId == uint64_t(i + 1), "newest frame at once");
        now += 33'000'000;
    }
}

// Abuse: random sizes, clocks going back, resolution changes, settings flipped from another thread,
// picks from a third thread. Run under the sanitizers in CI.
void testEngineAbuse() {
    Engine e;
    std::atomic<bool> done{false};
    std::thread picker([&] {
        int64_t t = 0;
        while (!done) { const Pick p = e.pick(t); if (p.valid()) { (void)p.a->px.size(); } t += 3'000'000; std::this_thread::yield(); }
    });
    std::thread flipper([&] {
        std::mt19937 rng(9);
        while (!done) {
            EngineSettings s;
            s.frameGeneration = rng() % 4 != 0;
            s.targetFps = double(rng() % 3) * 60.0;
            s.pacingRepair = rng() % 2;
            s.hudLock = rng() % 2;
            s.ambient = rng() % 2;
            s.quality = int(rng() % 2);
            s.removeDuplicates = rng() % 3 != 0;
            e.setSettings(s);
            if (rng() % 50 == 0) e.reset();
            std::this_thread::sleep_for(std::chrono::microseconds(300));
        }
    });
    std::mt19937 rng(77);
    const int sizes[][2] = {{1, 1}, {2, 3}, {17, 9}, {64, 36}, {160, 90}, {7, 200}, {300, 2}, {33, 33}};
    int64_t clock = 0;
    for (int i = 0; i < 600; ++i) {
        const auto& sz = sizes[(i / 40) % 8];
        Bgra img(sz[0], sz[1]);
        for (auto& v : img.px) v = uint8_t(rng());
        if (rng() % 5 == 0 && i > 0) { /* duplicate content */ }
        const int jump = int(rng() % 10);
        clock += jump == 0 ? -50'000'000 : (jump == 1 ? 0 : 16'666'667);  // backwards, equal, normal
        e.push(std::move(img), clock);
        if (i % 7 == 0) e.push(Bgra(), clock);  // empty frames are ignored
    }
    e.waitIdle();
    done = true;
    picker.join();
    flipper.join();
    const EngineStats st = e.stats();
    CHECK(st.framesIn == 600, "frames counted %llu", (unsigned long long)st.framesIn);
    CHECK(std::isfinite(st.contentFps) && std::isfinite(st.latencyMs) && std::isfinite(st.delayMs), "stats finite");
}

// Big motion, flash frames, black frames: no crash, sane output.
void testEngineHardContent() {
    const Scene bg(200, 120, 400, 31), fg(16, 16, 2, 32);
    Engine e;
    int64_t now = 0;
    for (int i = 0; i < 40; ++i) {
        Bgra img;
        if (i % 13 == 5) img = Bgra(200, 120);                        // black frame
        else if (i % 11 == 7) { img = Bgra(200, 120); for (size_t k = 0; k < img.px.size(); ++k) if (k % 4 != 3) img.px[k] = 255; }  // flash
        else img = render(bg, fg, float(i * 150 % 700) - 350.f, float(i * 37 % 200) - 100.f, 20, 20, 16);  // huge jumps
        e.push(std::move(img), now);
        e.waitIdle();
        for (int k = 0; k < 4; ++k) {
            const Pick p = e.pick(now + k * 8'333'333);
            if (p.generated()) {
                Bgra out;
                CHECK(interpolate(*p.a, *p.b, p.fields->forward, p.fields->backward, p.fields->hudMask.get(), p.t, out, 2), "interp");
            }
        }
        now += 33'333'333;
    }
    CHECK(e.stats().sceneCuts > 0, "cuts seen in random jumps");
}

void benchmark() {
    const Scene bg(1920, 1080, 64, 41), fg(200, 200, 8, 42);
    const Bgra a = render(bg, fg, 0, 0, 800, 400, 200), b = render(bg, fg, 12, 4, 760, 400, 200);
    {   // Ambient fill's copy, once a real frame, at 1080p and 4K.
        const Bgra big(3840, 2160);
        const auto s0 = std::chrono::steady_clock::now();
        for (int i = 0; i < 20; ++i) (void)veyra::portable::ambient::make(a, 0.6f);
        const auto s1 = std::chrono::steady_clock::now();
        for (int i = 0; i < 20; ++i) (void)veyra::portable::ambient::make(big, 0.6f);
        const auto s2 = std::chrono::steady_clock::now();
        std::printf("ambient: 1080p %.2f ms, 4K %.2f ms\n", std::chrono::duration<double, std::milli>(s1 - s0).count() / 20,
                    std::chrono::duration<double, std::milli>(s2 - s1).count() / 20);
    }
    auto t0 = std::chrono::steady_clock::now();
    const Pyramid pa = Pyramid::of(lumaOf(a), 4), pb = Pyramid::of(lumaOf(b), 4);
    auto t1 = std::chrono::steady_clock::now();
    const MotionEstimator est;
    const MotionField f = est.estimate(pa, pb), k = est.estimate(pb, pa);
    auto t2 = std::chrono::steady_clock::now();
    MotionSettings fast; fast.finestLevel = 2;
    const MotionEstimator estFast(fast);
    (void)estFast.estimate(pa, pb);
    auto t3 = std::chrono::steady_clock::now();
    Bgra out;
    interpolate(a, b, f, k, nullptr, 0.5f, out, int(std::max(1u, std::thread::hardware_concurrency())));
    auto t4 = std::chrono::steady_clock::now();
    auto ms = [](auto x, auto y) { return std::chrono::duration<double, std::milli>(y - x).count(); };
    std::printf("  1080p: pyramids %.1f ms, motion both ways %.1f ms (fast one way %.1f ms), CPU interpolation %.1f ms on %u threads\n",
                ms(t0, t1), ms(t1, t2), ms(t2, t3), ms(t3, t4), std::thread::hardware_concurrency());
}

// Ambient fill (Ambient.h): the small, blurred, dimmed copy of a frame behind the picture.
void testAmbient() {
    using namespace veyra::portable::ambient;
    const float gain = veyra::engine::kPushAmbientEncodedGain;
    {   // One colour stays that colour, dimmed.
        Bgra f(1920, 1080);
        for (size_t i = 0; i < f.px.size(); i += 4) { f.px[i] = 200; f.px[i + 1] = 100; f.px[i + 2] = 50; }
        const Bgra a = make(f, gain);
        CHECK(a.width == kWidth && a.height == kHeight, "size %dx%d", a.width, a.height);
        bool same = true;
        for (int y = 0; y < a.height; ++y)
            for (int x = 0; x < a.width; ++x) {
                const uint8_t* p = a.pixel(x, y);
                same = same && p[0] == 120 && p[1] == 60 && p[2] == 30 && p[3] == 255;
            }
        CHECK(same, "uniform frame %d %d %d", a.pixel(0, 0)[0], a.pixel(0, 0)[1], a.pixel(0, 0)[2]);
    }
    {   // Every small pixel is the mean of its own share: a frame coloured by share comes out exactly so,
        // at 1080p (20 x 20 pixels a share) and at 4K (40 x 40, 16 x 16 samples of it).
        for (int scale : {1, 2}) {
            Bgra f(1920 * scale, 1080 * scale);
            for (int y = 0; y < f.height; ++y)
                for (int x = 0; x < f.width; ++x) {
                    uint8_t* p = f.pixel(x, y);
                    const int cx = x / (20 * scale), cy = y / (20 * scale);
                    p[0] = uint8_t(cx * 2); p[1] = uint8_t(cy * 4); p[2] = uint8_t((cx + cy) & 255);
                }
            const Small s = average(f);
            bool exact = s.width == kWidth && s.height == kHeight;
            for (int cy = 0; exact && cy < kHeight; ++cy)
                for (int cx = 0; cx < kWidth; ++cx) {
                    const float* v = s.bgr.data() + (size_t(cy) * kWidth + size_t(cx)) * 3;
                    exact = exact && v[0] == float(cx * 2) && v[1] == float(cy * 4) && v[2] == float((cx + cy) & 255);
                }
            CHECK(exact, "shares at scale %d", scale);
        }
    }
    {   // The blur in two passes is the shader's square kernel (PushAmbientBlur.hlsl), edges included.
        std::mt19937 rng(7);
        Small s{kWidth, kHeight, std::vector<float>(size_t(kWidth) * kHeight * 3)};
        for (float& v : s.bgr) v = float(rng() % 256);
        const Small b = blur(s);
        double worst = 0;
        for (int y = 0; y < kHeight; ++y)
            for (int x = 0; x < kWidth; ++x) {
                double sum[3] = {0, 0, 0}, weight = 0;
                for (int dy = -kRadius; dy <= kRadius; ++dy)
                    for (int dx = -kRadius; dx <= kRadius; ++dx) {
                        const double w = std::exp(-double(dx * dx + dy * dy) / (2.0 * kSigma * kSigma));
                        const int sx = std::clamp(x + dx, 0, kWidth - 1), sy = std::clamp(y + dy, 0, kHeight - 1);
                        for (int c = 0; c < 3; ++c) sum[c] += s.bgr[(size_t(sy) * kWidth + size_t(sx)) * 3 + size_t(c)] * w;
                        weight += w;
                    }
                for (int c = 0; c < 3; ++c)
                    worst = std::max(worst, std::abs(sum[c] / weight - b.bgr[(size_t(y) * kWidth + size_t(x)) * 3 + size_t(c)]));
            }
        CHECK(worst < 1e-3, "blur against the square kernel: %.6f", worst);
    }
    {   // A hard edge becomes a soft one: bright left, dark right, no step larger than a sixth of it.
        Bgra f(1280, 720);
        for (int y = 0; y < f.height; ++y)
            for (int x = 0; x < f.width; ++x) { uint8_t* p = f.pixel(x, y); p[0] = p[1] = p[2] = x < 640 ? 250 : 0; }
        const Bgra a = make(f, 1.f);
        int biggest = 0;
        for (int x = 1; x < a.width; ++x) biggest = std::max(biggest, std::abs(int(a.pixel(x, 27)[1]) - int(a.pixel(x - 1, 27)[1])));
        CHECK(a.pixel(0, 27)[1] > 240 && a.pixel(kWidth - 1, 27)[1] < 10, "ends %d %d", a.pixel(0, 27)[1], a.pixel(kWidth - 1, 27)[1]);
        CHECK(biggest < 250 / 6, "softened edge: step %d", biggest);
    }
    {   // A frame smaller than the small picture, odd sizes, no gain, a broken gain, an empty frame.
        Bgra f(51, 29);
        for (size_t i = 0; i < f.px.size(); i += 4) f.px[i] = f.px[i + 1] = f.px[i + 2] = 90;
        const Bgra a = make(f, 1.f);
        CHECK(a.width == kWidth && a.pixel(kWidth - 1, kHeight - 1)[0] == 90 && a.pixel(0, 0)[2] == 90, "small frame");
        CHECK(make(f, 0.f).pixel(5, 5)[1] == 0, "no gain");
        CHECK(make(f, std::nanf("")).pixel(5, 5)[1] == 0, "a gain that is not a number");
        CHECK(make(Bgra{}, gain).empty() && average(Bgra{}).width == 0 && blur(Small{}).width == 0, "empty");
        Bgra cut(64, 64);
        cut.px.resize(10);
        CHECK(make(cut, gain).empty(), "a frame shorter than its size");
    }
    {   // The Windows rules the app uses: a 16:9 picture fitted in a 21:9 window leaves bars; covering takes
        // the window's width; a picture that fills the window leaves none.
        CHECK(veyra::engine::pushAmbientBars(2560, 1080, 1920, 1080), "21:9 has bars");
        CHECK(!veyra::engine::pushAmbientBars(1920, 1080, 1920, 1080), "16:9 has none");
        const auto [w, h] = veyra::engine::pushAmbientCover(2560, 1080, 1920, 1080);
        CHECK(std::abs(w - 2560) < 0.01f && std::abs(h - 1440) < 0.01f, "cover %.1f x %.1f", double(w), double(h));
    }
    {   // Through the engine: off, no copy; on, every pick carries the copies of the frames it shows.
        const Scene bg(160, 90, 40, 31), fg(16, 16, 2, 32);
        EngineSettings s;
        s.frameGeneration = false;
        Engine e(s);
        int64_t now = 1'000'000;
        e.push(render(bg, fg, 0, 0, 10, 10, 8), now);
        CHECK(e.pick(now).valid() && !e.pick(now).ambientA, "no copy with ambient fill off");
        s.ambient = true;
        e.setSettings(s);
        now += 33'000'000;
        e.push(render(bg, fg, 4, 0, 10, 10, 8), now);
        const Pick p = e.pick(now);
        CHECK(p.valid() && p.ambientA && p.ambientA == p.ambientB, "a copy with it on");
        CHECK(p.ambientA && p.ambientA->px == make(*p.a, gain).px, "the copy is make() of the frame shown");
    }
}

int main(int argc, char** argv) {
    const bool bench = argc > 1 && std::string(argv[1]) == "--bench";
    testConvert();
    testPyramid();
    testMotion();
    testInterpolate();
    testProtection();
    testAmbient();
    testDedup();
    testHudLock();
    testCrosshairHold();
    testSubtitleHold();
    testEngineTiming(false, false, 0, 8333333, 120.0);   // 30 -> 120 Hz display, match display
    testEngineTiming(false, false, 60, 8333333, 60.0);   // capped at 60
    testEngineTiming(true, false, 0, 8333333, 120.0);    // uneven 17/50 ms, no repair
    testEngineTiming(true, true, 0, 8333333, 120.0);     // uneven, repaired
    testEngineTiming(false, false, 0, 16666667, 60.0);   // 60 Hz display
    testEngineMargin();
    testEngineStartBudget();
    testEngineOff();
    testEngineLiveRates();
    testEngineHardContent();
    testEngineAbuse();
    if (bench) benchmark();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
