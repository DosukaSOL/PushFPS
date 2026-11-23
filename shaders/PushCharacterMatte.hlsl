// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PushFPS Character Hold: the network's answer becomes the matte (8 bits, 1920 x 1080) and the matted picture
// at 960 x 540 (the frame where the matte is, mid grey elsewhere, the matte as the fourth channel; 8 bits).
// The same statements as pipeline/PushCharacter.h, matted(): change both together.
RWStructuredBuffer<float> picture : register(u0);   // what the network was given: [1, 3, 1080, 1920]
RWStructuredBuffer<float> answer : register(u1);    // what it said: [1, 1, 1080, 1920]
RWTexture2D<float> matteOut : register(u2);
RWTexture2D<float4> fineOut : register(u3);
cbuffer Constants : register(b0) {
    uint unused0, unused1, unused2, unused3;
};
static const uint kW = 1920, kH = 1080;
static const float kGrey = 0.46;
float byteOf(float v) { return float(uint(saturate(v) * 255.0 + 0.5)) / 255.0; }
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= kW / 2 || id.y >= kH / 2) return;
    const uint plane = kW * kH;
    float4 sum = 0.0;
    [unroll] for (uint k = 0; k < 4; ++k) {
        const uint2 p = uint2(2 * id.x + (k & 1), 2 * id.y + (k >> 1));
        const uint i = p.y * kW + p.x;
        const float raw = answer[i];
        matteOut[p] = byteOf(raw);
        const float a = saturate(raw);
        sum.r += picture[i] * a + kGrey * (1.0 - a);
        sum.g += picture[plane + i] * a + kGrey * (1.0 - a);
        sum.b += picture[2 * plane + i] * a + kGrey * (1.0 - a);
        sum.a += a;
    }
    fineOut[id.xy] = float4(byteOf(sum.r * 0.25), byteOf(sum.g * 0.25), byteOf(sum.b * 0.25), byteOf(sum.a * 0.25));
}
