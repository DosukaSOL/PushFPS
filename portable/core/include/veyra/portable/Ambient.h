#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Ambient fill for the preview: what the window shows beside the picture (the Windows build's
// engine/PushAmbient.h, whose bars and cover rules and gain are used as they are).
//
// A console sends a 16:9 picture; on a wider window Fit leaves bars. Ambient fill puts a soft, dimmed
// copy of the same picture behind it, scaled until it covers the window. The Windows build makes that
// copy on the graphics card (PushAmbientDown.hlsl, PushAmbientBlur.hlsl); the preview makes it here,
// once a real frame, with the same numbers:
//   1. the frame averaged down to kWidth x kHeight: every small pixel is the mean of up to kTaps x kTaps
//      samples spread evenly over its own share of the frame (the nearest pixel to each, where Windows
//      takes a bilinear sample; fewer when the share is smaller than kTaps pixels);
//   2. a Gaussian blur of sigma kSigma small pixels, radius kRadius, edges repeated (done in two passes,
//      which is the same sum as the shader's square, edges included, since every tap keeps its weight);
//   3. the result times the gain (kPushAmbientEncodedGain: the preview's output is SDR and display
//      encoded), rounded to 8 bits.
// The app enlarges that small picture over the window with linear filtering and draws the picture over it.
#include "veyra/portable/Image.h"

namespace veyra::portable::ambient {

inline constexpr int kWidth = 96;
inline constexpr int kHeight = 54;
inline constexpr int kTaps = 16;
inline constexpr int kRadius = 6;
inline constexpr float kSigma = 3.0f;

// The averaged picture (step 1), in 0..255 floats, three per small pixel (B, G, R). Empty for an empty frame.
struct Small {
    int width = 0, height = 0;
    std::vector<float> bgr;
};
Small average(const Bgra& frame);
// Step 2 on a small picture.
Small blur(const Small& small);
// The whole: steps 1 to 3. An empty frame gives an empty picture.
Bgra make(const Bgra& frame, float gain);

}  // namespace veyra::portable::ambient
