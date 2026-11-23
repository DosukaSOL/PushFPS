// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PushFPS Character Hold, the last step of one generated frame (pipeline/PushCharacter.h, laidAt(), matteAt()
// and differsAt(): the same statements). A pixel's character comes from ONE real frame, the nearer in time
// (the newer at a half), at the place found for it in the frame the method made; it is laid over that frame
// with the matte there, times the region's trust, times how far the method's frame differs from it. Where
// frame generation got the character right nothing is written; the rest of the generated frame is never
// touched. Every value between pixels is worked out here from four loads (no sampler), as the header does.
Texture2D<float4> made : register(t0);          // the generated frame as it was made (a copy)
Texture2D<float4> olderFrame : register(t1);
Texture2D<float4> newerFrame : register(t2);
Texture2D<float> olderMatte : register(t3);     // 1920 x 1080
Texture2D<float> newerMatte : register(t4);
Texture2D<float4> place : register(t5);         // 60 x 34: from a pixel to its place in the source, in pixels of 960 x 540; the trust
Texture2D<uint> differ : register(t6);          // 960 x 540: 0..255
RWTexture2D<float4> outTex : register(u0);      // the generated frame
cbuffer Constants : register(b0) {
    uint width, height, newer, unused0;
};
static const float2 kBlocks = float2(60.0, 34.0), kMatte = float2(1920.0, 1080.0), kFine = float2(960.0, 540.0);
static const float kLeast = 1.0 / 255.0;
float3 placeAt(float2 p) {
    p = clamp(p, float2(0.0, 0.0), kBlocks - 1.0);
    const int2 a = int2(p), b = min(a + 1, int2(kBlocks) - 1);
    const float2 f = p - float2(a);
    const float3 top = place[a].xyz + (place[int2(b.x, a.y)].xyz - place[a].xyz) * f.x;
    const float3 bottom = place[int2(a.x, b.y)].xyz + (place[b].xyz - place[int2(a.x, b.y)].xyz) * f.x;
    return top + (bottom - top) * f.y;
}
float differsAt(uint2 id) {
    float2 u = (float2(id) + 0.5) * kFine / float2(width, height) - 0.5;
    u = clamp(u, float2(0.0, 0.0), kFine - 1.0);
    const int2 a = int2(u), b = min(a + 1, int2(kFine) - 1);
    const float2 f = u - float2(a);
    const float k = 1.0 / 255.0;
    const float aa = float(differ[a]) * k, ba = float(differ[int2(b.x, a.y)]) * k, ab = float(differ[int2(a.x, b.y)]) * k, bb = float(differ[b]) * k;
    const float top = aa + (ba - aa) * f.x;
    const float bottom = ab + (bb - ab) * f.x;
    const float d = top + (bottom - top) * f.y;
    return d * d * (3.0 - 2.0 * d);
}
float matteAt(Texture2D<float> matte, float2 p) {
    float2 u = (p + 0.5) * kMatte / float2(width, height) - 0.5;
    u = clamp(u, float2(0.0, 0.0), kMatte - 1.0);
    const int2 a = int2(u), b = min(a + 1, int2(kMatte) - 1);
    const float2 f = u - float2(a);
    const float top = matte[a] + (matte[int2(b.x, a.y)] - matte[a]) * f.x;
    const float bottom = matte[int2(a.x, b.y)] + (matte[b] - matte[int2(a.x, b.y)]) * f.x;
    return top + (bottom - top) * f.y;
}
float3 frameAt(Texture2D<float4> frame, float2 p) {
    p = clamp(p, float2(0.0, 0.0), float2(width - 1, height - 1));
    const int2 a = int2(p), b = min(a + 1, int2(width - 1, height - 1));
    const float2 f = p - float2(a);
    const float3 top = frame[a].rgb + (frame[int2(b.x, a.y)].rgb - frame[a].rgb) * f.x;
    const float3 bottom = frame[int2(a.x, b.y)].rgb + (frame[b].rgb - frame[int2(a.x, b.y)].rgb) * f.x;
    return top + (bottom - top) * f.y;
}
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= width || id.y >= height) return;
    const float2 block = float2((float(id.x) + 0.5) * kBlocks.x / float(width) - 0.5, (float(id.y) + 0.5) * 33.75 / float(height) - 0.5);
    const float3 f = placeAt(block);
    if (f.z < kLeast) return;
    const float2 p = float2(float(id.x) + f.x * float(width) / 960.0, float(id.y) + f.y * float(height) / 540.0);
    float matte;
    if (newer != 0) matte = matteAt(newerMatte, p); else matte = matteAt(olderMatte, p);
    const float weight = matte * f.z * differsAt(id.xy);
    if (weight < kLeast) return;
    float3 source;
    if (newer != 0) source = frameAt(newerFrame, p); else source = frameAt(olderFrame, p);
    const float4 g = made[id.xy];
    outTex[id.xy] = float4(g.rgb * (1.0 - weight) + source * weight, g.a);
}
