// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PushFPS Character Hold (pipeline/PushCharacter.h; src/pipeline/PushCharacterPass.cpp): a real frame as the
// matting network is given it: [1, 3, 1080, 1920], planar, values 0..1, encoded the way such networks are
// trained (sRGB-like): encoding 0 = already encoded, 1 = linear SDR -> sRGB, 2 = scRGB -> PQ (as PushFlowPack).
// A 4K frame lands exactly between four pixels, so each value is their mean.
Texture2D<float4> frame : register(t0);
RWStructuredBuffer<float> tensor : register(u0);
cbuffer Constants : register(b0) {
    uint srcW, srcH, encoding, unused0;
};
static const uint kW = 1920, kH = 1080;
float3 encodeColor(float3 c) {
    if (encoding == 1) {
        c = saturate(c);
        return lerp(c * 12.92, 1.055 * pow(c, 1.0 / 2.4) - 0.055, step(0.0031308, c));
    }
    if (encoding == 2) {
        const float m1 = 0.1593017578125, m2 = 78.84375, c1 = 0.8359375, c2 = 18.8515625, c3 = 18.6875;
        const float3 y = pow(saturate(max(c, 0.0) * (80.0 / 10000.0)), m1);
        return pow((c1 + c2 * y) / (1.0 + c3 * y), m2);
    }
    return saturate(c);
}
float3 tap(float2 p) {
    const float2 q = clamp(p - 0.5, 0.0, float2(srcW - 1, srcH - 1));
    const uint2 a = uint2(q);
    const uint2 b = min(a + 1, uint2(srcW - 1, srcH - 1));
    const float2 f = q - float2(a);
    return lerp(lerp(frame[a].rgb, frame[uint2(b.x, a.y)].rgb, f.x), lerp(frame[uint2(a.x, b.y)].rgb, frame[b].rgb, f.x), f.y);
}
[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= kW || id.y >= kH) return;
    const float2 s = (float2(id.xy) + 0.5) * float2(srcW, srcH) / float2(kW, kH);
    const float3 c = encodeColor(tap(s));
    const uint plane = kW * kH, i = id.y * kW + id.x;
    tensor[i] = c.r; tensor[plane + i] = c.g; tensor[2 * plane + i] = c.b;
}
