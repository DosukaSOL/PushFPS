// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once
// Motion-compensated interpolation between two real frames: the reference the GPU shader
// (portable/app/shaders/frame.frag) follows line by line, used for tests, screenshots and the
// software fallback.
//
// For an output pixel p at phase t (0 = frame A, 1 = frame B):
//   two candidates: the forward field F (A->B) and the reversed backward field -K (K: B->A),
//   each samples A at p - t v and B at p + (1-t) v;
//   each candidate's error is how differently A and B look along its path (a correct vector
//   sees the same thing in both);
//   the candidates blend weighted by 1/(error+4)^2;
//   where even the better one disagrees (something appears or disappears: occlusion), the result
//   moves toward the nearer real frame's sample instead of a double image;
//   Live HUD Lock: masked pixels take the nearer real frame directly;
//   HUD protection boxes (Protection.h): inside them the older real frame (A), after everything else;
//   scene cut: no blending at all, the nearer real frame.
#include "veyra/portable/CrosshairHold.h"
#include "veyra/portable/Image.h"
#include "veyra/portable/Motion.h"
#include "veyra/portable/Protection.h"

namespace veyra::portable {

struct InterpolateSettings {
    float errorLow = 24.0f;   // mean channel error (0..255) where the occlusion fallback starts
    float errorHigh = 48.0f;  // ... and where it has fully taken over
};

// out gets the size of a. Returns false if a and b differ in size or are empty. Missing fields
// (empty) or a scene cut give the nearer real frame. mask: Live HUD Lock weights 0..255 of any
// size (sampled bilinearly), or null.
// hold / subtitles: Crosshair Hold's and Subtitle Hold's parts for this pair (CrosshairHold.h), or null:
// then nothing is different.
// protect: the HUD protection boxes, or null (or none): then nothing is different.
bool interpolate(const Bgra& a, const Bgra& b, const MotionField& forward, const MotionField& backward,
                 const Gray* mask, float t, Bgra& out, int threads = 1, InterpolateSettings settings = {},
                 const HoldPair* hold = nullptr, const HoldPair* subtitles = nullptr, const Protection* protect = nullptr);

// Bilinear BGRA sample at continuous coordinates (pixel centres at +0.5), clamped to the edge.
void sampleBilinear(const Bgra& image, float x, float y, float out[4]);

}  // namespace veyra::portable
