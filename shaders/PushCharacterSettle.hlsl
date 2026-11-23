// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PushFPS Character Hold: the place a block ends with (pipeline/PushCharacter.h, fitOf() and settleAt()). A
// block whose neighbourhood the method broke has no place of its own, since nothing of the character fits
// there: it takes what the believed blocks of the 7 x 7 round it found, as a correction to the search's answer.
// Written: (x, y) in pixels of 960 x 540, the region's trust, and how far the block's own place was believed.
Texture2D<float4> placedIn : register(t0);  // (x, y, left apart, found)
Texture2D<float4> field : register(t1);     // (x, y, trust, 0)
RWTexture2D<float4> placeOut : register(u0);
cbuffer Constants : register(b0) {
    float share;
    uint unused0, unused1, unused2;
};
static const float kTent[7] = {1.0, 2.0, 3.0, 4.0, 3.0, 2.0, 1.0};
static const int2 kBlocks = int2(60, 34);
float stepOf(float x, float lo, float hi) {
    const float t = saturate((x - lo) / (hi - lo));
    return t * t * (3.0 - 2.0 * t);
}
float fitOf(float4 placed) { return placed.w >= 0.5 ? 1.0 - stepOf(placed.z, 32.0, 64.0) : 0.0; }
[numthreads(4, 4, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= 60 || id.y >= 34) return;
    const int2 b = int2(id.xy);
    float weight = 0.0, cx = 0.0, cy = 0.0;
    [loop] for (int j = -3; j <= 3; ++j)
        [loop] for (int i = -3; i <= 3; ++i) {
            const int2 q = b + int2(i, j);
            if (q.x < 0 || q.x >= kBlocks.x || q.y < 0 || q.y >= kBlocks.y) continue;
            const float4 v = placedIn[q];
            const float2 from = share * field[q].xy;
            const float w = kTent[i + 3] * kTent[j + 3] * fitOf(v);
            weight += w; cx += w * (v.x - from.x); cy += w * (v.y - from.y);
        }
    if (weight > 0.0) { cx /= weight; cy /= weight; } else { cx = 0.0; cy = 0.0; }
    const float4 own = placedIn[b];
    const float4 f = field[b];
    const float2 start = share * f.xy;
    const float fit = fitOf(own);
    placeOut[id.xy] = float4(start.x + fit * (own.x - start.x) + (1.0 - fit) * cx, start.y + fit * (own.y - start.y) + (1.0 - fit) * cy, f.z, fit);
}
