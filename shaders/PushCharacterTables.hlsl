// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PushFPS Character Hold: where the source frame's character stands in the frame the method made, one block's
// table a thread group and one offset a thread (pipeline/PushCharacter.h, tableOf(): the same statements, in
// whole numbers). For an offset e (4 each way) round the block's centre (the search's answer times the share,
// to the nearest pixel): over the block's 32 x 32 window at 960 x 540, the summed difference between the
// generated frame at p and the source's matted picture at p + centre + e where the source is solid, and how
// many pixels that was. Written as one number: the sum times 2048, plus the count.
Texture2D<float4> made1 : register(t0);     // the generated frame, 960 x 540
Texture2D<float4> older1 : register(t1);    // the real frames' matted pictures, 960 x 540
Texture2D<float4> newer1 : register(t2);
Texture2D<float4> blocksIn : register(t3);  // (x, y, how distinct, judged)
Texture2D<float4> field : register(t4);     // (x, y, trust, 0): the half-motion in pixels of 960 x 540
RWTexture2D<uint> tablesOut : register(u0); // 540 x 306: nine by nine a block
cbuffer Constants : register(b0) {
    float share;
    uint newer, unused0, unused1;
};
static const int kReach = 4, kSolid = 230, kCap = 382, kGrey = 117;
static const int2 kFine = int2(960, 540);
int4 sourceAt(int2 p) {
    p = clamp(p, int2(0, 0), kFine - 1);
    int4 s;
    if (newer != 0) s = int4(newer1[p] * 255.0 + 0.5); else s = int4(older1[p] * 255.0 + 0.5);
    return s;
}
// The whole-number offset a block's offsets stand round. The product is kept apart from the sum, so that this
// shader and PushCharacterPlace can never round it differently.
int2 centreOf(float2 motion) {
    precise float2 v = share * motion;
    return int2(floor(v + 0.5));
}
[numthreads(9, 9, 1)]
void main(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID, uint3 id : SV_DispatchThreadID) {
    if (gid.x >= 60 || gid.y >= 34) return;
    const int2 b = int2(gid.xy);
    if (blocksIn[b].w < 0.5) { tablesOut[id.xy] = 0u; return; }
    const int2 centre = centreOf(field[b].xy);
    const int2 e = int2(tid.xy) - kReach;
    const int2 lo = max(16 * b - 8, int2(0, 0)), hi = min(16 * b - 8 + 32, kFine);
    uint apart = 0, solid = 0;
    [loop] for (int y = lo.y; y < hi.y; ++y)
        [loop] for (int x = lo.x; x < hi.x; ++x) {
            const int4 s = sourceAt(int2(x, y) + centre + e);
            if (s.w < kSolid) continue;
            const int3 g = int3(made1[int2(x, y)].rgb * 255.0 + 0.5);
            const int3 d = abs((g * s.w + kGrey * (255 - s.w) + 127) / 255 - s.xyz);
            apart += uint(min(2 * (d.x + d.y + d.z), kCap));
            ++solid;
        }
    tablesOut[id.xy] = apart * 2048u + solid;
}
