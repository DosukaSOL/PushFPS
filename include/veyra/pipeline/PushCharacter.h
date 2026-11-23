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
//   4. For a generated frame at time t the character comes from ONE real frame, the nearer (the newer at a
//      half). It is placed where the method itself put the character (the search's answer is only where the
//      looking starts), and laid over only where the method's frame differs from it there, with the matte
//      times the trust. Where frame generation got the character right, its own pixels stay.
// What it does not do: repair the world beside the character, follow anyone moving further than the search
// reaches (128 pixels a real frame sideways and 64 up or down at 4K: those are left as the method made them),
// or see anything that is not a person (a car, an animal).
//
// What failed on real frames, so it is not rebuilt: laying the character over wherever a person is, at the
// place the search gives (the first form: a jitter of 3 pixels on average, section 4), a motion per pixel (a noisy field makes hair wavy), mixing
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

// --- 4. where the character stands in the frame the method made ------------------------------------------
// The first form of this rule laid the character over wherever a person was, at the place the search above
// gives. On real play that place was 3 pixels off on average and up to 15 at 4K (measured frame by frame on
// the owner's clips of 23 November 2026): a head that turns keeps its outline almost where it was while its
// surface moves, the search follows the outline, and one smooth motion fits neither. The character then stood
// still for two frames out of every four: a jitter. The method's own frame has the character in the right place
// wherever the method did not break it (0.2 pixels off the middle on a clip without Character Hold). So:
//   - the source frame's character is PLACED where the method put it: per block, round the search's answer,
//     the whole-pixel offset at which the character's own pixels (matte from kSolid on) are nearest the
//     generated frame, found for a neighbourhood of 3 x 3 blocks together so that both edges of a head count
//     (an edge alone says nothing about a shift inwards), then brought under a pixel; a block the method
//     broke has no such place and takes what the blocks round it found;
//   - and it is LAID OVER only where the method's frame still differs from it there: where frame generation
//     got the character right its own pixels stay, and only what it broke is taken from the real frame.
// Measured against the method's own frames (the same clip): the head's inside 0.5 pixels from the method's
// where the first form was 3.4; against a frame in which a turning head is right, 0.5 where it was 4.6.
inline constexpr int kGreyByte = 117;                     // kGrey as a byte
inline constexpr int kSolid = 230;                        // from this matte on a pixel is the character's own
inline constexpr int kPlaceReach = 4;                     // offsets looked at round the search's answer, each way, in fine pixels
inline constexpr int kPlaceAcross = 2 * kPlaceReach + 1;
inline constexpr int kPlaceCandidates = kPlaceAcross * kPlaceAcross;
inline constexpr uint32_t kPlaceLeast = 576;              // solid pixels a neighbourhood needs before an offset counts
inline constexpr uint32_t kPlaceScale = 32;               // a mean difference is kept in 32nds
inline constexpr uint32_t kPlaceStep = 48;                // a step away from the search's answer: 1.5 a pixel, in 32nds
inline constexpr uint32_t kNoAnswer = 0xFFFFFFFFu;
inline constexpr int kDifferLo = 24, kDifferHi = 48;      // a neighbourhood's mean difference: from nothing laid over to everything

// The generated frame as the network would be given it (planar, kMatteW x kMatteH, 0..1), at the fine size:
// the mean of four pixels, 8 bits, RGBA with the fourth byte full.
inline void madeFine(const float* rgb, std::vector<uint8_t>& fine) {
    const size_t plane = size_t(kMatteW) * kMatteH;
    fine.resize(size_t(kFineW) * kFineH * 4);
    for (int y = 0; y < kFineH; ++y)
        for (int x = 0; x < kFineW; ++x) {
            uint8_t* out = &fine[(size_t(y) * kFineW + x) * 4];
            for (int c = 0; c < 3; ++c) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) sum += rgb[size_t(c) * plane + size_t(2 * y + (k >> 1)) * kMatteW + size_t(2 * x + (k & 1))];
                out[c] = toByte(sum * 0.25f);
            }
            out[3] = 255;
        }
}
// A pixel of the generated frame as a matted pixel with the source's matte, in whole numbers.
inline int blended(int value, int matte) { return (value * matte + kGreyByte * (255 - matte) + 127) / 255; }
// How far a pixel of the generated frame is from a pixel of the source's matted picture (0..kCap).
inline int apartFrom(const uint8_t* made, const uint8_t* source) {
    const int a = source[3];
    const int d = std::abs(blended(made[0], a) - int(source[0])) + std::abs(blended(made[1], a) - int(source[1])) + std::abs(blended(made[2], a) - int(source[2]));
    return std::min(2 * d, kCap);
}
// The source of a generated frame `t` of the way from the older real frame to the newer: the nearer one (the
// newer at a half), and the share of the half-motion that leads from a generated pixel to it.
inline bool sourceIsNewer(float t) { return t >= 0.5f; }
inline float shareOf(float t) { return sourceIsNewer(t) ? 2.0f * (1.0f - t) : -2.0f * t; }
// One block's table: for every offset e (kPlaceReach each way) round its centre, over the block's window at
// the fine size, the summed difference between the generated frame at p and the source at p + centre + e where
// the source is solid, and how many pixels that was.
struct Table {
    int cx = 0, cy = 0;
    bool judged = false;
    uint32_t apart[kPlaceCandidates]{}, solid[kPlaceCandidates]{};
};
inline Table tableOf(const Picture& source1, const Picture& made1, int bx, int by, Motion m, float share) {
    Table t;
    t.judged = true;
    t.cx = nearest(share * m.x); t.cy = nearest(share * m.y);
    const Span w = fineWindow(bx, by);
    for (int ey = -kPlaceReach; ey <= kPlaceReach; ++ey)
        for (int ex = -kPlaceReach; ex <= kPlaceReach; ++ex) {
            uint32_t apart = 0, solid = 0;
            for (int y = w.y0; y < w.y1; ++y)
                for (int x = w.x0; x < w.x1; ++x) {
                    const uint8_t* s = source1.at(x + t.cx + ex, y + t.cy + ey);
                    if (s[3] < kSolid) continue;
                    apart += uint32_t(apartFrom(made1.at(x, y), s)); ++solid;
                }
            const int i = (ey + kPlaceReach) * kPlaceAcross + ex + kPlaceReach;
            t.apart[i] = apart; t.solid[i] = solid;
        }
    return t;
}
struct Placed {
    float x = 0.0f, y = 0.0f;     // from a generated pixel to its place in the source frame, in fine pixels
    float left = 0.0f;            // what is still apart there, a pixel (0..kCap)
    bool found = false;
};
// The place of one block: the tables of its 3 x 3 neighbourhood added up offset by offset (each neighbour's
// table stands round its own centre), the cheapest offset within kPlaceReach - 1 with a step's cost for
// leaving the search's answer, and a parabola. Whole numbers decide.
inline Placed placeBlock(const Table* tables, int bx, int by, Motion m, float share) {
    Placed out;
    out.x = share * m.x; out.y = share * m.y;
    const Table& own = tables[by * kBlocksX + bx];
    if (!own.judged) return out;
    uint32_t apart[kPlaceCandidates]{}, solid[kPlaceCandidates]{};
    for (int j = -1; j <= 1; ++j)
        for (int i = -1; i <= 1; ++i) {
            const int x = bx + i, y = by + j;
            if (x < 0 || x >= kBlocksX || y < 0 || y >= kBlocksY) continue;
            const Table& n = tables[y * kBlocksX + x];
            if (!n.judged) continue;
            const int sx = own.cx - n.cx, sy = own.cy - n.cy;          // this block's offset e is the neighbour's e + s
            for (int ey = -kPlaceReach; ey <= kPlaceReach; ++ey) {
                const int ny = ey + sy;
                if (ny < -kPlaceReach || ny > kPlaceReach) continue;
                for (int ex = -kPlaceReach; ex <= kPlaceReach; ++ex) {
                    const int nx = ex + sx;
                    if (nx < -kPlaceReach || nx > kPlaceReach) continue;
                    const int to = (ey + kPlaceReach) * kPlaceAcross + ex + kPlaceReach, from = (ny + kPlaceReach) * kPlaceAcross + nx + kPlaceReach;
                    apart[to] += n.apart[from]; solid[to] += n.solid[from];
                }
            }
        }
    const auto mean = [&](int ex, int ey) {                              // in 32nds; kNoAnswer where too little is solid
        const int i = (ey + kPlaceReach) * kPlaceAcross + ex + kPlaceReach;
        return solid[i] >= kPlaceLeast ? (kPlaceScale * apart[i]) / solid[i] : kNoAnswer;
    };
    uint32_t best = kNoAnswer;
    int bex = 0, bey = 0;
    for (int ey = 1 - kPlaceReach; ey <= kPlaceReach - 1; ++ey)
        for (int ex = 1 - kPlaceReach; ex <= kPlaceReach - 1; ++ex) {
            const uint32_t q = mean(ex, ey);
            if (q == kNoAnswer) continue;
            const uint32_t cost = q + kPlaceStep * uint32_t(std::abs(ex) + std::abs(ey));
            if (cost < best) { best = cost; bex = ex; bey = ey; }
        }
    if (best == kNoAnswer) return out;
    const uint32_t q = mean(bex, bey), l = mean(bex - 1, bey), r = mean(bex + 1, bey), u = mean(bex, bey - 1), d = mean(bex, bey + 1);
    out.found = true;
    out.x = float(own.cx + bex) + (l != kNoAnswer && r != kNoAnswer ? parabola(l, q, r) : 0.0f);
    out.y = float(own.cy + bey) + (u != kNoAnswer && d != kNoAnswer ? parabola(u, q, d) : 0.0f);
    out.left = float(q) / float(kPlaceScale);
    return out;
}
// A block whose neighbourhood the method broke has no place of its own: nothing of the character fits there.
// How far a block's own place is believed: it was found, and little is left apart there.
inline constexpr float kFitGood = 32.0f, kFitBad = 64.0f;
inline float fitOf(float left, bool found) { return found ? 1.0f - smoothStep(left, kFitGood, kFitBad) : 0.0f; }
// The place a block ends with: its own as far as it is believed, and for the rest what the believed blocks of
// the 7 x 7 round it found (a head the method broke whole is some five blocks across at 4K), as a correction
// to the search's answer (nothing believed near: the search's answer).
inline constexpr float kSettleTent[7] = {1.0f, 2.0f, 3.0f, 4.0f, 3.0f, 2.0f, 1.0f};
// placedX/Y, fit: per block as found; startX/Y: the search's answer times the share.
inline Motion settleAt(const float* placedX, const float* placedY, const float* fit, const float* startX, const float* startY, int bx, int by) {
    float weight = 0.0f, cx = 0.0f, cy = 0.0f;
    for (int j = -3; j <= 3; ++j)
        for (int i = -3; i <= 3; ++i) {
            const int x = bx + i, y = by + j;
            if (x < 0 || x >= kBlocksX || y < 0 || y >= kBlocksY) continue;
            const int n = y * kBlocksX + x;
            const float w = kSettleTent[i + 3] * kSettleTent[j + 3] * fit[n];
            weight += w; cx += w * (placedX[n] - startX[n]); cy += w * (placedY[n] - startY[n]);
        }
    if (weight > 0.0f) { cx /= weight; cy /= weight; } else { cx = 0.0f; cy = 0.0f; }
    const int own = by * kBlocksX + bx;
    const float f = fit[own];
    return {startX[own] + f * (placedX[own] - startX[own]) + (1.0f - f) * cx, startY[own] + f * (placedY[own] - startY[own]) + (1.0f - f) * cy};
}
// Everything one generated frame needs of its pair: per block the place (fine pixels) and the trust.
struct Placing {
    std::vector<float> x, y, trust;
    std::vector<float> placedX, placedY, left, fit;       // as each block found it (tests)
    std::vector<uint8_t> found;
    std::vector<Table> tables;                            // (tests)
};
// source1: the matted fine picture of the source frame (the newer real frame when sourceIsNewer(t)); made1: madeFine().
inline Placing place(const Field& f, const Picture& source1, const Picture& made1, float t) {
    Placing p;
    const float share = shareOf(t);
    p.x.resize(kBlockCount); p.y.resize(kBlockCount); p.trust = f.trust;
    p.placedX.resize(kBlockCount); p.placedY.resize(kBlockCount); p.left.assign(kBlockCount, 0.0f); p.fit.assign(kBlockCount, 0.0f); p.found.assign(kBlockCount, 0);
    p.tables.resize(kBlockCount);
    std::vector<float> startX(kBlockCount), startY(kBlockCount);
    for (int by = 0; by < kBlocksY; ++by)
        for (int bx = 0; bx < kBlocksX; ++bx) {
            const int i = by * kBlocksX + bx;
            startX[i] = share * f.x[i]; startY[i] = share * f.y[i];
            if (f.blocks[i].active) p.tables[i] = tableOf(source1, made1, bx, by, {f.x[i], f.y[i]}, share);
        }
    for (int by = 0; by < kBlocksY; ++by)
        for (int bx = 0; bx < kBlocksX; ++bx) {
            const int i = by * kBlocksX + bx;
            const Placed at = placeBlock(p.tables.data(), bx, by, {f.x[i], f.y[i]}, share);
            p.placedX[i] = at.x; p.placedY[i] = at.y; p.left[i] = at.left; p.found[i] = at.found ? 1 : 0; p.fit[i] = fitOf(at.left, at.found);
        }
    for (int by = 0; by < kBlocksY; ++by)
        for (int bx = 0; bx < kBlocksX; ++bx) {
            const Motion m = settleAt(p.placedX.data(), p.placedY.data(), p.fit.data(), startX.data(), startY.data(), bx, by);
            p.x[by * kBlocksX + bx] = m.x; p.y[by * kBlocksX + bx] = m.y;
        }
    return p;
}

// --- 5. where the method's frame differs from the placed character ------------------------------------------
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
// Where pixel (x, y) of a picture of width x height lies among the blocks.
inline float blockX(int x, int width) { return (float(x) + 0.5f) * float(kBlocksX) / float(width) - 0.5f; }
inline float blockY(int y, int height) { return (float(y) + 0.5f) * (float(kFineH) / float(2 * kBlock)) / float(height) - 0.5f; }
// For a pixel of the fine size: 0 where the placed source is not solid (or the region is not trusted at all),
// else 1 + how far the generated frame is from it (the source read at the nearest whole pixel). A place often
// lies exactly half-way between two pixels (the parabola stops at a half): kTie decides those the same way
// whatever the last bit of the arithmetic says, so shader and this file read the same pixel.
inline constexpr float kTie = 1.0f / 512.0f;
inline int nearestTied(float v) { return int(std::floor(v + (0.5f + kTie))); }
inline uint16_t apartAt(const Placing& p, const Picture& source1, const Picture& made1, int x, int y) {
    const float bx = blockX(x, kFineW), by = blockY(y, kFineH);
    if (between(p.trust.data(), kBlocksX, kBlocksY, bx, by) < kLeast) return 0;
    const int sx = x + nearestTied(between(p.x.data(), kBlocksX, kBlocksY, bx, by)), sy = y + nearestTied(between(p.y.data(), kBlocksX, kBlocksY, bx, by));
    const uint8_t* s = source1.at(sx, sy);
    if (s[3] < kSolid) return 0;
    return uint16_t(1 + apartFrom(made1.at(x, y), s));
}
// For a pixel of the fine size, 0..255: the largest, over the 5 x 5 pixels round it, of how far above kDifferLo
// (255 at kDifferHi) the mean difference of that pixel's 3 x 3 solid neighbours is. A single pixel says little,
// and what differs is laid over with a margin.
inline uint8_t differAt(const Placing& p, const uint16_t* apart, int x, int y) {
    if (between(p.trust.data(), kBlocksX, kBlocksY, blockX(x, kFineW), blockY(y, kFineH)) < kLeast) return 0;
    uint32_t most = 0;
    for (int ny = y - 2; ny <= y + 2; ++ny)
        for (int nx = x - 2; nx <= x + 2; ++nx) {
            if (nx < 0 || nx >= kFineW || ny < 0 || ny >= kFineH) continue;
            uint32_t sum = 0, count = 0;
            for (int my = ny - 1; my <= ny + 1; ++my)
                for (int mx = nx - 1; mx <= nx + 1; ++mx) {
                    if (mx < 0 || mx >= kFineW || my < 0 || my >= kFineH) continue;
                    const uint32_t v = apart[size_t(my) * kFineW + mx];
                    if (v) { sum += v - 1; ++count; }
                }
            if (count < 3 || sum <= uint32_t(kDifferLo) * count) continue;
            most = std::max(most, std::min<uint32_t>(255u, (255u * (sum - uint32_t(kDifferLo) * count)) / (uint32_t(kDifferHi - kDifferLo) * count)));
        }
    return uint8_t(most);
}
inline void differMap(const Placing& p, const Picture& source1, const Picture& made1, std::vector<uint16_t>& apart, std::vector<uint8_t>& differ) {
    apart.resize(size_t(kFineW) * kFineH); differ.resize(size_t(kFineW) * kFineH);
    for (int y = 0; y < kFineH; ++y)
        for (int x = 0; x < kFineW; ++x) apart[size_t(y) * kFineW + x] = apartAt(p, source1, made1, x, y);
    for (int y = 0; y < kFineH; ++y)
        for (int x = 0; x < kFineW; ++x) differ[size_t(y) * kFineW + x] = differAt(p, apart.data(), x, y);
}

// --- 6. laying the character over a generated frame -------------------------------------------------------
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
// The differ map (kFineW x kFineH, bytes) at a pixel of a frame of width x height, between its four
// neighbours, as a smooth step 0..1.
inline float differsAt(const uint8_t* differ, int x, int y, int width, int height) {
    float u = (float(x) + 0.5f) * float(kFineW) / float(width) - 0.5f, v = (float(y) + 0.5f) * float(kFineH) / float(height) - 0.5f;
    u = u < 0.0f ? 0.0f : (u > float(kFineW - 1) ? float(kFineW - 1) : u);
    v = v < 0.0f ? 0.0f : (v > float(kFineH - 1) ? float(kFineH - 1) : v);
    const int x0 = int(u), y0 = int(v), x1 = std::min(x0 + 1, kFineW - 1), y1 = std::min(y0 + 1, kFineH - 1);
    const float fx = u - float(x0), fy = v - float(y0);
    const auto at = [&](int px, int py) { return float(differ[size_t(py) * kFineW + px]) * (1.0f / 255.0f); };
    const float top = at(x0, y0) + (at(x1, y0) - at(x0, y0)) * fx, bottom = at(x0, y1) + (at(x1, y1) - at(x0, y1)) * fx;
    const float d = top + (bottom - top) * fy;
    return d * d * (3.0f - 2.0f * d);
}
struct Laid {
    float x = 0.0f, y = 0.0f;     // where in the source frame this pixel's character comes from (pixel centres at whole numbers)
    float weight = 0.0f;          // how much of it: the matte there, times the region's trust, times how far the method's frame differs
};
// Pixel (x, y) of a generated frame of width x height. sourceMatte8: the 8-bit matte of the source frame.
// Nothing is laid over where the weight is under kLeast.
inline Laid laidAt(const Placing& p, const uint8_t* differ, const uint8_t* sourceMatte8, int x, int y, int width, int height) {
    Laid out;
    const float bx = blockX(x, width), by = blockY(y, height);
    const float trust = between(p.trust.data(), kBlocksX, kBlocksY, bx, by);
    if (trust < kLeast) return out;
    out.x = float(x) + between(p.x.data(), kBlocksX, kBlocksY, bx, by) * float(width) / float(kFineW);
    out.y = float(y) + between(p.y.data(), kBlocksX, kBlocksY, bx, by) * float(height) / float(kFineH);
    out.weight = matteAt(sourceMatte8, out.x, out.y, width, height) * trust * differsAt(differ, x, y, width, height);
    return out;
}

// Character Hold is a choice of the user's (preference pushCharacterHold, process-wide like the other PushFPS
// switches, so the settings record shared with the engine release keeps its shape). Off unless switched on.
inline bool& wantedStore() { static bool wanted = false; return wanted; }
inline void setWanted(bool on) { wantedStore() = on; }
inline bool wanted() { return wantedStore(); }
}
