#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// NVIDIA Image Scaling (NIS) controls shared by the interface and the graph.
// NIS is the super-resolution choice engine::kVideoSrNis; its sharpness is a
// live value (no graph rebuild), kept here rather than in EnhancementSettings so
// the preset schema Veyra shares with its presets stays unchanged.
#include <algorithm>
#include <atomic>
#include <cmath>

namespace veyra::pipeline {
// NVIDIA's own default (NVIDIA Control Panel / NIS samples): 50 %.
inline constexpr float kNisDefaultSharpness = 0.5f;
// NIS upscales 1x..2x per axis in one pass; larger factors finish with a bilinear resize.
inline constexpr float kNisMaxScale = 2.0f;

inline std::atomic<float>& nisSharpnessStore() {
    static std::atomic<float> value{kNisDefaultSharpness};
    return value;
}
// 0 = no sharpening, 1 = strongest. Out-of-range and NaN values are clamped / reset.
inline void setNisSharpness(float value) {
    nisSharpnessStore().store(std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : kNisDefaultSharpness);
}
inline float nisSharpness() { return nisSharpnessStore().load(); }
}
