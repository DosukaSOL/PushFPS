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

float3 SrgbEncode(float3 c) {
    c = saturate(c);
    return c <= 0.0031308 ? 12.92 * c : 1.055 * pow(c, 1.0 / 2.4) - 0.055;
}
float3 SrgbDecode(float3 c) {
    c = saturate(c);
    return c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

[numthreads(8, 8, 1)]
void main(uint3 p : SV_DispatchThreadID) {
    if (p.x >= outWidth || p.y >= outHeight) return;
    const float2 uv = (float2(p.xy) + 0.5) * float2(invOutWidth, invOutHeight);
    const float4 c = source.SampleLevel(decode ? linearClamp : pointClamp, uv, 0);
    float3 rgb;
    if (decode) rgb = hdr ? HdrDecodePq(c.rgb) : SrgbDecode(c.rgb);
    else rgb = hdr ? HdrEncodePq(c.rgb) : SrgbEncode(c.rgb);
    target[p.xy] = float4(rgb, 1.0);
}
