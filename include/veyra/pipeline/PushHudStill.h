#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// PushFPS 4.0: what stands still in front of a moving picture stays as the console drew it.
//
// Frame generation works out one motion for every place. Where a HUD stands still in front of a
// world that moves, it takes the world's motion for the HUD's pixels too: text, bars and a
// crosshair smear or vanish in the generated frames. That is the ghosting people see in a HUD,
// with every method. Measured on the shipped PushFlow models with drawn pictures (the ghost study
// in scripts/pushflow/prepare_runtime.py, 20 November 2026): a HUD's pixels were off by 14 to 40
// of 255 in the generated frame, and by about 1 with this rule; the outline of a character who
// stands still while the world moves behind him was off by 3 to 10 and by about 2 to 3 with it;
// a plain pan and a ball crossing a still world were left as they were.
//
// The rule, for one pair of real frames and every generated frame between them:
//   a pixel is given its real value when
//   1. it is the same in both real frames (within kPairStill),
//   2. it has company: kNear of the 9 pixels around it and kAround of the 25 are the same too
//      (a thin stroke has, a chance match in a moving texture has not), and
//   3. it stands in a surrounding that mostly moves: under kSurround of the pixels in the 5 x 5
//      blocks of kBlock pixels around it are the same in both frames (counted, not averaged, so
//      the shaders and this header decide in whole numbers and can never differ by a rounding).
// Point 3 is what makes it safe. Where most of the surrounding stands still (a still camera, a
// menu) nothing is touched, so a small thing that crosses a still world between two real frames,
// and is at a place both real frames show empty, stays what frame generation made of it.
// It needs no warm-up and keeps no memory: every pair is judged by itself. Live HUD Lock
// (PushHudLock.h) is the other tool: it remembers, and also holds a HUD whose surrounding does not move.
// A source with noise (an analogue link, a hard-compressed stream) has no pixels that are the
// same twice; the rule then finds nothing and frame generation is what it was.
//
// This header is the rule in plain C++: the shaders (PushHudLockUpdate, PushHudStill,
// PushHudLockCompose) do the same, and the tests compare them with it. Change both together, and
// measure a threshold with the study before changing it.
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <vector>
namespace veyra::pipeline::hudstill {
inline constexpr float kPairStill = 2.0f / 255.0f + 1e-4f;   // largest channel change still "the same"
inline constexpr unsigned kNear = 3, kAround = 5;            // of 9, of 25 (the pixel itself counts)
inline constexpr unsigned kBlock = 16;                       // pixels a side
inline constexpr float kSurround = 0.5f;

// The process-wide switch (preference pushHudStill). Off unless the user turned it on, since 4.0.1:
// 4.0 had it on for everyone, and with it on the owner saw more artifacts around the edges of moving
// things with NVIDIA's frame generation than before 4.0 (20 November 2026). The study that set the
// thresholds used drawn pictures; a real game has far more pixels that are the same in two frames by
// chance, and where one of them belongs to something passing in between, a real pixel is written
// into the generated frame where the moving thing should be. With the switch off the graph has no
// pass for it at all: every generated frame is what it was before 4.0. Never make it the default
// again before it has been seen on real games on a real card.
inline std::atomic<bool>& wantedStore() { static std::atomic<bool> value{false}; return value; }
inline void setWanted(bool on) { wantedStore().store(on, std::memory_order_relaxed); }
inline bool wanted() { return wantedStore().load(std::memory_order_relaxed); }

// same[y * width + x]: is the pixel the same in both real frames. Returns which pixels keep their real value.
inline std::vector<uint8_t> decide(const std::vector<uint8_t>& same, unsigned width, unsigned height) {
    std::vector<uint8_t> keep(same.size(), 0);
    if (!width || !height || same.size() != size_t(width) * height) return keep;
    const unsigned bw = (width + kBlock - 1) / kBlock, bh = (height + kBlock - 1) / kBlock;
    // Per block: how many of its pixels are the same, and how many pixels it has (the last column and row may be cut).
    std::vector<unsigned> stillIn(size_t(bw) * bh, 0), pixelsIn(size_t(bw) * bh, 0);
    for (unsigned by = 0; by < bh; ++by)
        for (unsigned bx = 0; bx < bw; ++bx) {
            unsigned still = 0, total = 0;
            for (unsigned y = by * kBlock; y < std::min(height, (by + 1) * kBlock); ++y)
                for (unsigned x = bx * kBlock; x < std::min(width, (bx + 1) * kBlock); ++x) { ++total; still += same[size_t(y) * width + x] ? 1u : 0u; }
            stillIn[size_t(by) * bw + bx] = still; pixelsIn[size_t(by) * bw + bx] = total;
        }
    // Per block: does the surrounding (the 5 x 5 blocks around it, as far as the picture goes) mostly move?
    std::vector<uint8_t> moves(size_t(bw) * bh, 0);
    for (int by = 0; by < int(bh); ++by)
        for (int bx = 0; bx < int(bw); ++bx) {
            unsigned still = 0, total = 0;
            for (int dy = -2; dy <= 2; ++dy)
                for (int dx = -2; dx <= 2; ++dx) {
                    const int qx = bx + dx, qy = by + dy;
                    if (qx < 0 || qy < 0 || qx >= int(bw) || qy >= int(bh)) continue;
                    still += stillIn[size_t(qy) * bw + qx]; total += pixelsIn[size_t(qy) * bw + qx];
                }
            moves[size_t(by) * bw + bx] = float(still) < kSurround * float(total) ? 1 : 0;
        }
    const auto at = [&](int x, int y) { return same[size_t(std::clamp(y, 0, int(height) - 1)) * width + size_t(std::clamp(x, 0, int(width) - 1))] != 0; };
    for (int y = 0; y < int(height); ++y)
        for (int x = 0; x < int(width); ++x) {
            if (!same[size_t(y) * width + x]) continue;
            if (!moves[size_t(unsigned(y) / kBlock) * bw + unsigned(x) / kBlock]) continue;
            // Not "near": the Windows headers define that word away, and this header is read after them.
            unsigned beside = 0, around = 0;
            for (int dy = -2; dy <= 2; ++dy)
                for (int dx = -2; dx <= 2; ++dx) {
                    const bool s = at(x + dx, y + dy);
                    around += s ? 1u : 0u;
                    if (dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1) beside += s ? 1u : 0u;
                }
            keep[size_t(y) * width + x] = beside >= kNear && around >= kAround ? 1 : 0;
        }
    return keep;
}
} // namespace veyra::pipeline::hudstill
