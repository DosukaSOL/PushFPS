// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PushFPS Character Hold: where the frame the method made differs from the placed character, for every pixel
// of 960 x 540, 0..255 (pipeline/PushCharacter.h, differAt(): the same statements, whole numbers). The largest,
// over the 5 x 5 pixels round it, of how far above 24 (255 at 48) the mean difference of that pixel's 3 x 3
// solid neighbours is: a single pixel says little, and what differs is laid over with a margin.
Texture2D<uint> apartIn : register(t0);     // 0, or 1 + the difference
Texture2D<float4> place : register(t1);     // 60 x 34: (x, y, trust, fit)
RWTexture2D<uint> differOut : register(u0);
cbuffer Constants : register(b0) {
    uint unused0, unused1, unused2, unused3;
};
static const uint kLo = 24u, kHi = 48u;
static const int2 kFine = int2(960, 540);
static const float2 kBlocks = float2(60.0, 34.0);
static const float kLeast = 1.0 / 255.0;
float trustAt(float2 p) {
    p = clamp(p, float2(0.0, 0.0), kBlocks - 1.0);
    const int2 a = int2(p), b = min(a + 1, int2(kBlocks) - 1);
    const float2 f = p - float2(a);
    const float top = place[a].z + (place[int2(b.x, a.y)].z - place[a].z) * f.x;
    const float bottom = place[int2(a.x, b.y)].z + (place[b].z - place[int2(a.x, b.y)].z) * f.x;
    return top + (bottom - top) * f.y;
}
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= 960 || id.y >= 540) return;
    if (trustAt(float2((float(id.x) + 0.5) * 60.0 / 960.0 - 0.5, (float(id.y) + 0.5) * 33.75 / 540.0 - 0.5)) < kLeast) { differOut[id.xy] = 0u; return; }
    const int2 p = int2(id.xy);
    uint most = 0u;
    [loop] for (int ny = p.y - 2; ny <= p.y + 2; ++ny)
        [loop] for (int nx = p.x - 2; nx <= p.x + 2; ++nx) {
            if (nx < 0 || nx >= kFine.x || ny < 0 || ny >= kFine.y) continue;
            uint sum = 0u, count = 0u;
            [loop] for (int my = ny - 1; my <= ny + 1; ++my)
                [loop] for (int mx = nx - 1; mx <= nx + 1; ++mx) {
                    if (mx < 0 || mx >= kFine.x || my < 0 || my >= kFine.y) continue;
                    const uint v = apartIn[int2(mx, my)];
                    if (v != 0u) { sum += v - 1u; ++count; }
                }
            if (count < 3u || sum <= kLo * count) continue;
            most = max(most, min(255u, (255u * (sum - kLo * count)) / ((kHi - kLo) * count)));
        }
    differOut[id.xy] = most;
}
