#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// PushFPS Character Hold: the people in a generated frame stay whole.
//
// A character the camera follows moves a few pixels a frame while the world behind it passes at hundreds.
// Frame generation works out one motion per place, so where the two meet it gives parts of the character the
// world's motion: hair and shoulders break up, and what stands behind bends into them. No rule on the pixels
// alone tells a character from the world (several were tried on real frames and are in the project notes).
// A matting network does: for every real frame it says which pixels are a person (pipeline/PushCharacterPass.h
// runs Robust Video Matting, GPL-3.0, once a real frame).
//
// With the matte the rest is this file, pure and the same statements as the shaders (change both together):
//   1. Each real frame becomes a "matted picture": the frame where the matte is, mid grey elsewhere, with the
//      matte as a fourth channel; 8 bits, at 960 x 540 and 480 x 270. The world is not in it.
//   2. The character's own motion between two real frames, per block: a block of 8 x 8 at 480 x 270 looks, over
//      a 16 x 16 window, for the half-motion d that makes the older picture at p - d and the newer at p + d
//      alike; refined at 960 x 540 and brought under a pixel with a parabola. Whole numbers decide, so shader and
//      this file cannot differ by a rounding. The field is then smoothed by how distinct each block's answer is.
//   3. Three checks per region say whether it is laid over at all: do the two mattes agree where they were
//      brought together, do the insides agree in colour, is the motion near what the search can reach.
//   4. For a generated frame at time t the character is taken from ONE real frame, the nearer (the newer at a
//      half), moved by that field, and laid over the method's frame with the matte times the trust.
// What it does not do: repair the world beside the character, follow anyone moving further than the search
// reaches (128 pixels a real frame sideways and 64 up or down at 4K: those are left as the method made them),
// or see anything that is not a person (a car, an animal).
//
// What failed on real frames, so it is not rebuilt: a motion per pixel (a noisy field makes hair wavy), mixing
// both real frames (double edges wherever the motion is a little off), a pixel's own match cost as its trust,
// a weighted mean of per-pixel answers (the matte's flicker and straight edges outvote the rest), and "the
// answer is at the search's limit" per pixel as the sign of a miss.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace veyra::pipeline::character {

inline constexpr int kMatteW = 1920, kMatteH = 1080;     // the matte, and the picture the network is given
inline constexpr int kFineW = 960, kFineH = 540;
inline constexpr int kCoarseW = 480, kCoarseH = 270;
inline constexpr int kBlock = 8;                          // a block at the coarse size (16 at the fine size)
inline constexpr int kWindow = 16;                        // the window a block is judged over, at the coarse size
inline constexpr int kBlocksX = 60, kBlocksY = 34;        // 480 / 8 and 270 / 8 rounded up
inline constexpr int kBlockCount = kBlocksX * kBlocksY;
inline constexpr int kReachX = 8, kReachY = 4;            // half-motion searched, in coarse pixels
inline constexpr int kCandidates = (2 * kReachX + 1) * (2 * kReachY + 1);
inline constexpr float kGrey = 0.46f;                     // what stands where the matte is not
// A pixel's difference is 2 (|r| + |g| + |b|) + 3 |matte| of the 8-bit values: 1530 = wholly different.
inline constexpr int kWhole = 1530;
inline constexpr int kCap = 382;                          // one pixel counts for a quarter at most
inline constexpr int kStepCost = 3;                       // a step of half-motion, per window pixel, against twice the sum
inline constexpr int kMass = 4 * 255;                     // matte in a window (four whole pixels) for a block to be judged
// The checks (each a smooth step from the first number to the second).
inline constexpr float kMattesApart0 = 0.25f, kMattesApart1 = 0.50f;
inline constexpr float kInsideApart0 = 0.10f, kInsideApart1 = 0.18f;
inline constexpr float kAtReach0 = 0.60f, kAtReach1 = 0.85f;
inline constexpr float kLeast = 1.0f / 255.0f;            // under this nothing is laid over

inline uint8_t toByte(float v) {
    v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    return uint8_t(v * 255.0f + 0.5f);
}
inline float smoothStep(float x, float lo, float hi) {
    float t = (x - lo) / (hi - lo);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return t * t * (3.0f - 2.0f * t);
}

// --- 1. the matted pictures -----------------------------------------------------------------------------
// rgb: the picture the network was given, planar (three planes of kMatteW x kMatteH, values 0..1).
// matte: the network's answer, kMatteW x kMatteH. fine / coarse: RGBA, 8 bits. matte8: the matte, 8 bits.
inline void matted(const float* rgb, const float* matte, std::vector<uint8_t>& matte8, std::vector<uint8_t>& fine, std::vector<uint8_t>& coarse) {
    const size_t plane = size_t(kMatteW) * kMatteH;
    matte8.resize(plane);
    for (size_t i = 0; i < plane; ++i) matte8[i] = toByte(matte[i]);
    fine.resize(size_t(kFineW) * kFineH * 4);
    for (int y = 0; y < kFineH; ++y)
        for (int x = 0; x < kFineW; ++x) {
            float sum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            for (int k = 0; k < 4; ++k) {
                const size_t i = size_t(2 * y + (k >> 1)) * kMatteW + size_t(2 * x + (k & 1));
                float a = matte[i];
                a = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a);
                for (int c = 0; c < 3; ++c) sum[c] += rgb[size_t(c) * plane + i] * a + kGrey * (1.0f - a);
                sum[3] += a;
            }
            uint8_t* out = &fine[(size_t(y) * kFineW + x) * 4];
            for (int c = 0; c < 4; ++c) out[c] = toByte(sum[c] * 0.25f);
        }
    coarse.resize(size_t(kCoarseW) * kCoarseH * 4);
    for (int y = 0; y < kCoarseH; ++y)
        for (int x = 0; x < kCoarseW; ++x)
            for (int c = 0; c < 4; ++c) {
                const auto at = [&](int dx, int dy) { return int(fine[(size_t(2 * y + dy) * kFineW + size_t(2 * x + dx)) * 4 + c]); };
                coarse[(size_t(y) * kCoarseW + x) * 4 + c] = uint8_t((at(0, 0) + at(1, 0) + at(0, 1) + at(1, 1) + 2) >> 2);
            }
}

// --- 2. the motion, per block ---------------------------------------------------------------------------
struct Picture {            // RGBA, 8 bits; outside it the edge repeats
    int w = 0, h = 0;
    const uint8_t* p = nullptr;
    const uint8_t* at(int x, int y) const {
        x = x < 0 ? 0 : (x >= w ? w - 1 : x);
        y = y < 0 ? 0 : (y >= h ? h - 1 : y);
        return p + (size_t(y) * size_t(w) + size_t(x)) * 4;
    }
};
inline int difference(const uint8_t* a, const uint8_t* b) {
    return 2 * (std::abs(int(a[0]) - int(b[0])) + std::abs(int(a[1]) - int(b[1])) + std::abs(int(a[2]) - int(b[2]))) + 3 * std::abs(int(a[3]) - int(b[3]));
}
struct Span {
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    int count() const { return (x1 - x0) * (y1 - y0); }
};
inline Span coarseWindow(int bx, int by) {
    const int edge = (kWindow - kBlock) / 2;
    return {std::max(0, kBlock * bx - edge), std::max(0, kBlock * by - edge), std::min(kCoarseW, kBlock * bx - edge + kWindow), std::min(kCoarseH, kBlock * by - edge + kWindow)};
}
inline Span fineWindow(int bx, int by) {
    const int edge = kWindow - kBlock;
    return {std::max(0, 2 * kBlock * bx - edge), std::max(0, 2 * kBlock * by - edge), std::min(kFineW, 2 * kBlock * bx - edge + 2 * kWindow), std::min(kFineH, 2 * kBlock * by - edge + 2 * kWindow)};
}
inline Span fineBlock(int bx, int by) {
    return {2 * kBlock * bx, 2 * kBlock * by, std::min(kFineW, 2 * kBlock * bx + 2 * kBlock), std::min(kFineH, 2 * kBlock * by + 2 * kBlock)};
}
// The capped differences of the older picture at p - (dx, dy) and the newer at p + (dx, dy), summed over a span.
inline uint32_t apart(const Picture& older, const Picture& newer, const Span& s, int dx, int dy) {
    uint32_t sum = 0;
    for (int y = s.y0; y < s.y1; ++y)
        for (int x = s.x0; x < s.x1; ++x) sum += uint32_t(std::min(difference(older.at(x - dx, y - dy), newer.at(x + dx, y + dy)), kCap));
    return sum;
}
struct Block {
    float fx = 0.0f, fy = 0.0f;   // the half-motion, in fine pixels
    float sure = 0.0f;            // how distinct the answer is (0: a flat block says nothing)
    bool active = false;          // there is matte in its window
};
inline float parabola(uint32_t left, uint32_t middle, uint32_t right) {
    const int64_t den = int64_t(left) - 2 * int64_t(middle) + int64_t(right);
    if (den <= 0) return 0.0f;
    const float off = 0.5f * float(int64_t(left) - int64_t(right)) / float(den);
    return off < -0.5f ? -0.5f : (off > 0.5f ? 0.5f : off);
}
inline Block searchBlock(const Picture& older0, const Picture& newer0, const Picture& older1, const Picture& newer1, int bx, int by) {
    Block out;
    const Span w = coarseWindow(bx, by);
    uint32_t mass = 0;
    for (int y = w.y0; y < w.y1; ++y)
        for (int x = w.x0; x < w.x1; ++x) mass += uint32_t(std::max(older0.at(x, y)[3], newer0.at(x, y)[3]));
    if (mass < uint32_t(kMass)) return out;
    out.active = true;
    const uint32_t pixels = uint32_t(w.count());
    uint32_t sums[kCandidates];
    uint32_t best = 0xFFFFFFFFu;
    int bdx = 0, bdy = 0;
    for (int dy = -kReachY; dy <= kReachY; ++dy)
        for (int dx = -kReachX; dx <= kReachX; ++dx) {
            const uint32_t s = apart(older0, newer0, w, dx, dy);
            sums[(dy + kReachY) * (2 * kReachX + 1) + dx + kReachX] = s;
            const uint32_t cost = 2u * s + uint32_t(kStepCost) * pixels * uint32_t(std::abs(dx) + std::abs(dy));
            if (cost < best) { best = cost; bdx = dx; bdy = dy; }
        }
    // How distinct: the answers one step away, on average, against its own.
    uint32_t ring = 0, count = 0;
    for (int ey = -1; ey <= 1; ++ey)
        for (int ex = -1; ex <= 1; ++ex) {
            if (!ex && !ey) continue;
            const int dx = bdx + ex, dy = bdy + ey;
            if (dx < -kReachX || dx > kReachX || dy < -kReachY || dy > kReachY) continue;
            ring += sums[(dy + kReachY) * (2 * kReachX + 1) + dx + kReachX];
            ++count;
        }
    const uint32_t own = sums[(bdy + kReachY) * (2 * kReachX + 1) + bdx + kReachX];
    const float gain = float(ring) / float(count) - float(own);
    out.sure = (gain > 0.0f ? gain : 0.0f) / (float(pixels) * float(kWhole));
    // At the fine size: twenty-five answers round twice the coarse one, the best of the inner nine, a parabola.
    const Span f = fineWindow(bx, by);
    uint32_t fineSums[25];
    for (int ey = -2; ey <= 2; ++ey)
        for (int ex = -2; ex <= 2; ++ex) fineSums[(ey + 2) * 5 + ex + 2] = apart(older1, newer1, f, 2 * bdx + ex, 2 * bdy + ey);
    uint32_t fineBest = 0xFFFFFFFFu;
    int fex = 0, fey = 0;
    for (int ey = -1; ey <= 1; ++ey)
        for (int ex = -1; ex <= 1; ++ex) {
            const uint32_t s = fineSums[(ey + 2) * 5 + ex + 2];
            if (s < fineBest) { fineBest = s; fex = ex; fey = ey; }
        }
    const int at = (fey + 2) * 5 + fex + 2;
    out.fx = float(2 * bdx + fex) + parabola(fineSums[at - 1], fineSums[at], fineSums[at + 1]);
    out.fy = float(2 * bdy + fey) + parabola(fineSums[at - 5], fineSums[at], fineSums[at + 5]);
    return out;
}

// --- 3. the smooth field and the checks ------------------------------------------------------------------
inline constexpr float kTent[5] = {1.0f, 2.0f, 3.0f, 2.0f, 1.0f};
inline constexpr float kFloor = 1e-5f;                  // a judged block that says nothing still counts a little
struct Motion { float x = 0.0f, y = 0.0f; };
inline Motion smoothAt(const Block* blocks, int bx, int by) {
    float weight = 0.0f, sx = 0.0f, sy = 0.0f;
    for (int j = -2; j <= 2; ++j)
        for (int i = -2; i <= 2; ++i) {
            const int x = bx + i, y = by + j;
            if (x < 0 || x >= kBlocksX || y < 0 || y >= kBlocksY) continue;
            const Block& b = blocks[y * kBlocksX + x];
            if (!b.active) continue;
            const float w = kTent[i + 2] * kTent[j + 2] * (b.sure + kFloor);
            weight += w; sx += w * b.fx; sy += w * b.fy;
        }
    if (!(weight > 0.0f)) return {};
    return {sx / weight, sy / weight};
}
inline int nearest(float v) { return int(std::floor(v + 0.5f)); }
struct Sums { uint32_t apart = 0, either = 0, both = 0, inside = 0; };
// Over the block's own pixels at the fine size, with its motion to the nearest pixel: how far the two mattes
// are apart, how much matte either has, how much both have, and how far the colours are apart where both have it.
inline Sums checkBlock(const Picture& older1, const Picture& newer1, int bx, int by, Motion m) {
    Sums s;
    const Span f = fineBlock(bx, by);
    const int dx = nearest(m.x), dy = nearest(m.y);
    for (int y = f.y0; y < f.y1; ++y)
        for (int x = f.x0; x < f.x1; ++x) {
            const uint8_t* a = older1.at(x - dx, y - dy);
            const uint8_t* b = newer1.at(x + dx, y + dy);
            const uint32_t lo = std::min(a[3], b[3]), hi = std::max(a[3], b[3]);
            s.apart += hi - lo; s.either += hi; s.both += lo;
            s.inside += lo * uint32_t(std::abs(int(a[0]) - int(b[0])) + std::abs(int(a[1]) - int(b[1])) + std::abs(int(a[2]) - int(b[2])));
        }
    return s;
}
inline float trustOf(const Sums& around, Motion m) {
    const float mattes = float(around.apart) / (float(around.either) + float(kMass));
    const float inside = float(around.inside) / (765.0f * (float(around.both) + float(kMass)));
    const float reach = std::max(std::fabs(m.x) / float(2 * kReachX), std::fabs(m.y) / float(2 * kReachY));
    return (1.0f - smoothStep(mattes, kMattesApart0, kMattesApart1)) * (1.0f - smoothStep(inside, kInsideApart0, kInsideApart1)) *
           (1.0f - smoothStep(reach, kAtReach0, kAtReach1));
}
struct Field {                                            // per block: the half-motion in fine pixels, and the trust
    std::vector<float> x, y, trust;
    std::vector<Block> blocks;                            // as found, before smoothing (tests)
    std::vector<Sums> sums;                               // each block's own (tests)
};
inline Field search(const Picture& older0, const Picture& newer0, const Picture& older1, const Picture& newer1) {
    Field f;
    f.blocks.resize(kBlockCount); f.sums.resize(kBlockCount);
    f.x.assign(kBlockCount, 0.0f); f.y.assign(kBlockCount, 0.0f); f.trust.assign(kBlockCount, 0.0f);
    for (int by = 0; by < kBlocksY; ++by)
        for (int bx = 0; bx < kBlocksX; ++bx) f.blocks[by * kBlocksX + bx] = searchBlock(older0, newer0, older1, newer1, bx, by);
    for (int by = 0; by < kBlocksY; ++by)
        for (int bx = 0; bx < kBlocksX; ++bx) {
            const int i = by * kBlocksX + bx;
            const Motion m = smoothAt(f.blocks.data(), bx, by);
            f.x[i] = m.x; f.y[i] = m.y;
            if (f.blocks[i].active) f.sums[i] = checkBlock(older1, newer1, bx, by, m);
        }
    for (int by = 0; by < kBlocksY; ++by)
        for (int bx = 0; bx < kBlocksX; ++bx) {
            Sums around;
            for (int j = -1; j <= 1; ++j)
                for (int i = -1; i <= 1; ++i) {
                    const int x = bx + i, y = by + j;
                    if (x < 0 || x >= kBlocksX || y < 0 || y >= kBlocksY) continue;
                    const Sums& s = f.sums[y * kBlocksX + x];
                    around.apart += s.apart; around.either += s.either; around.both += s.both; around.inside += s.inside;
                }
            const int i = by * kBlocksX + bx;
            f.trust[i] = trustOf(around, {f.x[i], f.y[i]});
        }
    return f;
}

// --- 4. laying the character over a generated frame -------------------------------------------------------
// A value of a grid of `w` x `h` at (x, y), between its four neighbours, the edge repeated.
inline float between(const float* grid, int w, int h, float x, float y) {
    x = x < 0.0f ? 0.0f : (x > float(w - 1) ? float(w - 1) : x);
    y = y < 0.0f ? 0.0f : (y > float(h - 1) ? float(h - 1) : y);
    const int x0 = int(x), y0 = int(y), x1 = std::min(x0 + 1, w - 1), y1 = std::min(y0 + 1, h - 1);
    const float fx = x - float(x0), fy = y - float(y0);
    const float top = grid[y0 * w + x0] + (grid[y0 * w + x1] - grid[y0 * w + x0]) * fx;
    const float bottom = grid[y1 * w + x0] + (grid[y1 * w + x1] - grid[y1 * w + x0]) * fx;
    return top + (bottom - top) * fy;
}
struct Lay {
    float x = 0.0f, y = 0.0f;     // where in the source frame this pixel's character comes from (pixel centres at whole numbers)
    float trust = 0.0f;
    bool newer = true;            // the source: the newer real frame, or the older
};
// Pixel (x, y) of a generated frame of width x height, `t` of the way from the older real frame to the newer.
inline Lay layAt(const Field& f, int x, int y, int width, int height, float t) {
    Lay out;
    const float bx = (float(x) + 0.5f) * float(kBlocksX) / float(width) - 0.5f;
    const float by = (float(y) + 0.5f) * (float(kFineH) / float(2 * kBlock)) / float(height) - 0.5f;
    const float mx = between(f.x.data(), kBlocksX, kBlocksY, bx, by) * float(width) / float(kFineW);
    const float my = between(f.y.data(), kBlocksX, kBlocksY, bx, by) * float(height) / float(kFineH);
    out.trust = between(f.trust.data(), kBlocksX, kBlocksY, bx, by);
    out.newer = t >= 0.5f;
    const float k = out.newer ? 2.0f * (1.0f - t) : -2.0f * t;
    out.x = float(x) + k * mx;
    out.y = float(y) + k * my;
    return out;
}
// The 8-bit matte of the source frame at a point of a frame of width x height (0..1).
inline float matteAt(const uint8_t* matte8, float x, float y, int width, int height) {
    float u = (x + 0.5f) * float(kMatteW) / float(width) - 0.5f, v = (y + 0.5f) * float(kMatteH) / float(height) - 0.5f;
    u = u < 0.0f ? 0.0f : (u > float(kMatteW - 1) ? float(kMatteW - 1) : u);
    v = v < 0.0f ? 0.0f : (v > float(kMatteH - 1) ? float(kMatteH - 1) : v);
    const int x0 = int(u), y0 = int(v), x1 = std::min(x0 + 1, kMatteW - 1), y1 = std::min(y0 + 1, kMatteH - 1);
    const float fx = u - float(x0), fy = v - float(y0);
    const auto at = [&](int px, int py) { return float(matte8[size_t(py) * kMatteW + px]) * (1.0f / 255.0f); };
    const float top = at(x0, y0) + (at(x1, y0) - at(x0, y0)) * fx, bottom = at(x0, y1) + (at(x1, y1) - at(x0, y1)) * fx;
    return top + (bottom - top) * fy;
}
// What the pass keeps of the search for one frame: nothing is laid over where this is under kLeast.
inline float weightOf(float matte, float trust) { return matte * trust; }

// Character Hold is a choice of the user's (preference pushCharacterHold, process-wide like the other PushFPS
// switches, so the settings record shared with the engine release keeps its shape). Off unless switched on.
inline bool& wantedStore() { static bool wanted = false; return wanted; }
inline void setWanted(bool on) { wantedStore() = on; }
inline bool wanted() { return wantedStore(); }
}
