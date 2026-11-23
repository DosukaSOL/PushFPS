// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PushFPS Character Hold: the matted picture at 480 x 270, the rounded mean of four pixels of the one at
// 960 x 540, in whole numbers (pipeline/PushCharacter.h, matted()).
Texture2D<float4> fineIn : register(t0);
RWTexture2D<float4> coarseOut : register(u0);
cbuffer Constants : register(b0) {
    uint unused0, unused1, unused2, unused3;
};
uint4 bytes(uint2 p) { return uint4(fineIn[p] * 255.0 + 0.5); }
[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= 480 || id.y >= 270) return;
    const uint2 p = 2 * id.xy;
    const uint4 s = bytes(p) + bytes(p + uint2(1, 0)) + bytes(p + uint2(0, 1)) + bytes(p + uint2(1, 1));
    coarseOut[id.xy] = float4((s + 2) >> 2) / 255.0;
}
