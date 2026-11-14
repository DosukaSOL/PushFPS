// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Colour boundary of the NVIDIA Image Scaling stage (NisPass). NIS works on
// display-referred colour in [0, 1]; the graph works on linear FP16 (SDR:
// BT.709 with white = 1; HDR: scRGB, 1 = 80 cd/m2, out-of-709 colours negative).
//   encode: linear working -> sRGB (SDR) or BT.2020 PQ (HDR), same size
//   decode: the NIS result -> linear working, resampled bilinearly when the
//           output size differs (NIS covers 1x..2x per axis; the rest is bilinear)
// BT.2020 PQ holds every colour a console sends in HDR up to 10 000 cd/m2, so
// nothing is clipped on the HDR path.
#include "HdrColor.hlsli"

cbuffer NisColorParams : register(b0) {
    uint outWidth; uint outHeight; uint hdr; uint decode;
    float invOutWidth; float invOutHeight; float reserved0; float reserved1;
};
SamplerState pointClamp : register(s0);
SamplerState linearClamp : register(s1);
Texture2D<float4> source : register(t0);
RWTexture2D<float4> target : register(u0);

// Per-component branch without a vector ternary (HLSL 2021 rejects those).
float3 SrgbEncode(float3 c) {
    c = saturate(c);
    return lerp(1.055 * pow(c, 1.0 / 2.4) - 0.055, 12.92 * c, step(c, 0.0031308));
}
float3 SrgbDecode(float3 c) {
    c = saturate(c);
    return lerp(pow((c + 0.055) / 1.055, 2.4), c / 12.92, step(c, 0.04045));
}

[numthreads(8, 8, 1)]
void main(uint3 p : SV_DispatchThreadID) {
    if (p.x >= outWidth || p.y >= outHeight) return;
    const float2 uv = (float2(p.xy) + 0.5) * float2(invOutWidth, invOutHeight);
    const float4 c = decode != 0 ? source.SampleLevel(linearClamp, uv, 0) : source.SampleLevel(pointClamp, uv, 0);
    float3 rgb;
    if (decode != 0) rgb = hdr != 0 ? HdrDecodePq(c.rgb) : SrgbDecode(c.rgb);
    else rgb = hdr != 0 ? HdrEncodePq(c.rgb) : SrgbEncode(c.rgb);
    target[p.xy] = float4(rgb, 1.0);
}
