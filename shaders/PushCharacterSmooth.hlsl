// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PushFPS Character Hold: each block's motion smoothed over 5 x 5 blocks by how distinct the answers are, and
// then, with that motion to the nearest pixel, what the block's own 16 x 16 pixels at 960 x 540 say about the
// two mattes and the colours inside them (pipeline/PushCharacter.h, smoothAt() and checkBlock()).
Texture2D<float4> blocksIn : register(t0);   // (x, y, how distinct, judged)
Texture2D<float4> older1 : register(t1);
Texture2D<float4> newer1 : register(t2);
RWTexture2D<float4> smoothOut : register(u0);   // (x, y, 0, 0)
RWTexture2D<uint4> sumsOut : register(u1);      // mattes apart, matte of either, of both, colours apart inside
cbuffer Constants : register(b0) {
    uint unused0, unused1, unused2, unused3;
};
static const float kTent[5] = {1.0, 2.0, 3.0, 2.0, 1.0};
static const float kFloor = 1e-5;
static const int2 kBlocks = int2(60, 34), kFine = int2(960, 540);
int4 at(Texture2D<float4> t, int2 p) { return int4(t[clamp(p, int2(0, 0), kFine - 1)] * 255.0 + 0.5); }
[numthreads(4, 4, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= 60 || id.y >= 34) return;
    const int2 b = int2(id.xy);
    float weight = 0.0, sx = 0.0, sy = 0.0;
    [loop] for (int j = -2; j <= 2; ++j)
        [loop] for (int i = -2; i <= 2; ++i) {
            const int2 q = b + int2(i, j);
            if (q.x < 0 || q.x >= kBlocks.x || q.y < 0 || q.y >= kBlocks.y) continue;
            const float4 v = blocksIn[q];
            if (v.w < 0.5) continue;
            const float w = kTent[i + 2] * kTent[j + 2] * (v.z + kFloor);
            weight += w; sx += w * v.x; sy += w * v.y;
        }
    float2 m = float2(0.0, 0.0);
    if (weight > 0.0) m = float2(sx / weight, sy / weight);
    smoothOut[id.xy] = float4(m, 0.0, 0.0);
    uint4 s = uint4(0, 0, 0, 0);
    if (blocksIn[b].w >= 0.5) {
        const int2 d = int2(floor(m + 0.5));
        const int2 lo = 16 * b, hi = min(16 * b + 16, kFine);
        [loop] for (int y = lo.y; y < hi.y; ++y)
            [loop] for (int x = lo.x; x < hi.x; ++x) {
                const int4 a = at(older1, int2(x, y) - d), c = at(newer1, int2(x, y) + d);
                const uint low = uint(min(a.w, c.w)), high = uint(max(a.w, c.w));
                const int3 e = abs(a.xyz - c.xyz);
                s.x += high - low; s.y += high; s.z += low; s.w += low * uint(e.x + e.y + e.z);
            }
    }
    sumsOut[id.xy] = s;
}
