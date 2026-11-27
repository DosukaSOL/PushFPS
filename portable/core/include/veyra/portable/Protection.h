// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once
// HUD protection boxes (the Windows build's "Exclude all effects" regions, shaders/NrProtection.hlsli and
// EffectExclusion.hlsl): up to four boxes, rectangles or the ellipse inscribed in one, as shares of the
// picture. A generated frame shows the older real frame (A) inside them, so a minimap or a health bar is
// never moved by frame generation. The edge is softened over `feather` picture pixels inwards.
//
// The rule is the Windows one, statement for statement: `weight` is NrProtection, the strongest box wins.
// portable/app/shaders/frame.frag repeats it: change both together.
#include <algorithm>
#include <array>
#include <cmath>

namespace veyra::portable {

struct ProtectBox {
    float left = 0, top = 0, right = 0, bottom = 0;  // 0..1 of the picture
    bool ellipse = false;
};

struct Protection {
    static constexpr int kMaxBoxes = 4;
    std::array<ProtectBox, kMaxBoxes> boxes{};
    int count = 0;
    float feather = 8.0f;  // picture pixels, 0..64

    bool any() const { return count > 0; }

    // Boxes put in order and in range, smaller than half a per cent dropped (Windows pushSetExclusionRegions).
    static Protection clean(const ProtectBox* list, int n, float feather) {
        Protection p;
        const auto unit = [](float v) { return std::isfinite(v) ? std::clamp(v, 0.f, 1.f) : 0.f; };
        for (int i = 0; i < n && p.count < kMaxBoxes; ++i) {
            ProtectBox q{std::min(unit(list[i].left), unit(list[i].right)), std::min(unit(list[i].top), unit(list[i].bottom)),
                         std::max(unit(list[i].left), unit(list[i].right)), std::max(unit(list[i].top), unit(list[i].bottom)),
                         list[i].ellipse};
            if (q.right - q.left < 0.005f || q.bottom - q.top < 0.005f) continue;
            p.boxes[size_t(p.count++)] = q;
        }
        p.feather = std::isfinite(feather) ? std::clamp(feather, 0.f, 64.f) : 8.f;
        return p;
    }

    // 0..1: how much of the real frame a pixel centre (px, py) of a width x height picture takes.
    float weight(float px, float py, int width, int height) const {
        float protection = 0.f;
        const auto smoothstep = [](float e0, float e1, float x) {
            const float t = std::clamp((x - e0) / (e1 - e0), 0.f, 1.f);
            return t * t * (3.f - 2.f * t);
        };
        for (int i = 0; i < count; ++i) {
            const ProtectBox& b = boxes[size_t(i)];
            const float l = b.left * float(width), t = b.top * float(height), r = b.right * float(width), d = b.bottom * float(height);
            if (r <= l || d <= t) continue;
            float edge;
            if (b.ellipse) {
                // Distance inside the ellipse along the normalised radius, scaled by the shorter semi-axis
                // (exact on circles).
                const float cx = (l + r) * 0.5f, cy = (t + d) * 0.5f;
                const float ax = std::max((r - l) * 0.5f, 1e-3f), ay = std::max((d - t) * 0.5f, 1e-3f);
                edge = (1.f - std::hypot((px - cx) / ax, (py - cy) / ay)) * std::min(ax, ay);
            } else {
                edge = std::min(std::min(px - l, r - px), std::min(py - t, d - py));
            }
            protection = std::max(protection, feather > 0.f ? smoothstep(0.f, feather, edge) : (edge >= 0.f ? 1.f : 0.f));
        }
        return protection;
    }
};

}  // namespace veyra::portable
