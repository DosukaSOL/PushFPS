// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PushFPS Character Hold: how far a region's answer is trusted (pipeline/PushCharacter.h, trustOf()): over
// 3 x 3 blocks, do the two mattes agree where they were brought together, do the insides agree in colour, and
// is the motion away from the search's reach. Written per block: the half-motion in pixels of 960 x 540, the trust.
Texture2D<float4> smoothIn : register(t0);
Texture2D<uint4> sumsIn : register(t1);
RWTexture2D<float4> fieldOut : register(u0);
cbuffer Constants : register(b0) {
    uint unused0, unused1, unused2, unused3;
};
static const int2 kBlocks = int2(60, 34);
static const float kMass = 1020.0;
float stepOf(float x, float lo, float hi) {
    const float t = saturate((x - lo) / (hi - lo));
    return t * t * (3.0 - 2.0 * t);
}
[numthreads(4, 4, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= 60 || id.y >= 34) return;
    const int2 b = int2(id.xy);
    uint4 around = uint4(0, 0, 0, 0);
    [loop] for (int j = -1; j <= 1; ++j)
        [loop] for (int i = -1; i <= 1; ++i) {
            const int2 q = b + int2(i, j);
            if (q.x < 0 || q.x >= kBlocks.x || q.y < 0 || q.y >= kBlocks.y) continue;
            around += sumsIn[q];
        }
    const float2 m = smoothIn[b].xy;
    const float mattes = float(around.x) / (float(around.y) + kMass);
    const float inside = float(around.w) / (765.0 * (float(around.z) + kMass));
    const float reach = max(abs(m.x) / 16.0, abs(m.y) / 8.0);
    const float trust = (1.0 - stepOf(mattes, 0.25, 0.50)) * (1.0 - stepOf(inside, 0.10, 0.18)) * (1.0 - stepOf(reach, 0.60, 0.85));
    fieldOut[id.xy] = float4(m, trust, 0.0);
}
