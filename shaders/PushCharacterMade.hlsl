// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PushFPS Character Hold: a generated frame at 960 x 540, 8 bits, as the matting network would be given it
// (the same encoding and the same taps as PushCharacterPack, then the mean of four of its pixels):
// pipeline/PushCharacter.h, madeFine(). The matted pictures of the real frames are compared with this one.
Texture2D<float4> made : register(t0);          // the generated frame as it was made (a copy)
RWTexture2D<float4> fineOut : register(u0);
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
    return lerp(lerp(made[a].rgb, made[uint2(b.x, a.y)].rgb, f.x), lerp(made[uint2(a.x, b.y)].rgb, made[b].rgb, f.x), f.y);
}
float byteOf(float v) { return float(uint(saturate(v) * 255.0 + 0.5)) / 255.0; }
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= kW / 2 || id.y >= kH / 2) return;
    float3 sum = 0.0;
    [unroll] for (uint k = 0; k < 4; ++k) {
        const uint2 p = uint2(2 * id.x + (k & 1), 2 * id.y + (k >> 1));
        sum += encodeColor(tap((float2(p) + 0.5) * float2(srcW, srcH) / float2(kW, kH)));
    }
    fineOut[id.xy] = float4(byteOf(sum.r * 0.25), byteOf(sum.g * 0.25), byteOf(sum.b * 0.25), 1.0);
}
