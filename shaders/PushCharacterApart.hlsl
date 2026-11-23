// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PushFPS Character Hold: for every pixel of 960 x 540, how far the frame the method made is from the placed
// character (pipeline/PushCharacter.h, apartAt()): 0 where the placed source is not solid or the region is
// not trusted at all, else 1 + the difference (the source read at the nearest whole pixel).
Texture2D<float4> made1 : register(t0);
Texture2D<float4> older1 : register(t1);
Texture2D<float4> newer1 : register(t2);
Texture2D<float4> place : register(t3);     // 60 x 34: (x, y, trust, fit)
RWTexture2D<uint> apartOut : register(u0);
cbuffer Constants : register(b0) {
    uint newer, unused0, unused1, unused2;
};
static const int kSolid = 230, kCap = 382, kGrey = 117;
static const int2 kFine = int2(960, 540);
static const float2 kBlocks = float2(60.0, 34.0);
static const float kLeast = 1.0 / 255.0;
float3 placeAt(float2 p) {
    p = clamp(p, float2(0.0, 0.0), kBlocks - 1.0);
    const int2 a = int2(p), b = min(a + 1, int2(kBlocks) - 1);
    const float2 f = p - float2(a);
    const float3 top = place[a].xyz + (place[int2(b.x, a.y)].xyz - place[a].xyz) * f.x;
    const float3 bottom = place[int2(a.x, b.y)].xyz + (place[b].xyz - place[int2(a.x, b.y)].xyz) * f.x;
    return top + (bottom - top) * f.y;
}
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= 960 || id.y >= 540) return;
    const float3 f = placeAt(float2((float(id.x) + 0.5) * 60.0 / 960.0 - 0.5, (float(id.y) + 0.5) * 33.75 / 540.0 - 0.5));
    if (f.z < kLeast) { apartOut[id.xy] = 0u; return; }
    // A place half-way between two pixels is read the same way whatever the last bit says (the header's kTie).
    const int2 p = clamp(int2(id.xy) + int2(floor(f.xy + (0.5 + 1.0 / 512.0))), int2(0, 0), kFine - 1);
    int4 s;
    if (newer != 0) s = int4(newer1[p] * 255.0 + 0.5); else s = int4(older1[p] * 255.0 + 0.5);
    if (s.w < kSolid) { apartOut[id.xy] = 0u; return; }
    const int3 g = int3(made1[id.xy].rgb * 255.0 + 0.5);
    const int3 d = abs((g * s.w + kGrey * (255 - s.w) + 127) / 255 - s.xyz);
    apartOut[id.xy] = uint(1 + min(2 * (d.x + d.y + d.z), kCap));
}
