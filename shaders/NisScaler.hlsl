// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// NVIDIA Image Scaling (NIS) upscaler: the unchanged MIT-licensed NIS_Scaler.h
// (third_party/nis, NVIDIA Image Scaling SDK 1.0.3) with the bindings of
// PushFPS's ComputePass helper: 28 root constants at b0 (the NISConfig layout),
// SRVs t0 input / t1 scaler coefficients / t2 USM coefficients, UAV u0 output,
// and the helper's static bilinear clamp sampler at s1.
//
// The input is the display-referred colour NisColor.hlsl encodes: sRGB for SDR
// working, BT.2020 PQ for HDR working (NIS_HDR_MODE 2), both in [0, 1] as NIS
// requires. Variants: NisScalerSdr.dxil (NIS_HDR_MODE=0), NisScalerPq.dxil (2).
#define NIS_HLSL 1
#define NIS_SCALER 1
#ifndef NIS_HDR_MODE
#define NIS_HDR_MODE 0
#endif
// NISOptimizer defaults for upscaling on AMD / Intel / NVIDIA (NIS_Config.h):
// 32 x 24 pixel blocks; 256 threads works on every vendor.
#define NIS_BLOCK_WIDTH 32
#define NIS_BLOCK_HEIGHT 24
#define NIS_THREAD_GROUP_SIZE 256

cbuffer cb : register(b0)
{
    float kDetectRatio;
    float kDetectThres;
    float kMinContrastRatio;
    float kRatioNorm;

    float kContrastBoost;
    float kEps;
    float kSharpStartY;
    float kSharpScaleY;

    float kSharpStrengthMin;
    float kSharpStrengthScale;
    float kSharpLimitMin;
    float kSharpLimitScale;

    float kScaleX;
    float kScaleY;

    float kDstNormX;
    float kDstNormY;
    float kSrcNormX;
    float kSrcNormY;

    uint kInputViewportOriginX;
    uint kInputViewportOriginY;
    uint kInputViewportWidth;
    uint kInputViewportHeight;

    uint kOutputViewportOriginX;
    uint kOutputViewportOriginY;
    uint kOutputViewportWidth;
    uint kOutputViewportHeight;

    float reserved0;
    float reserved1;
};

SamplerState samplerLinearClamp : register(s1);
Texture2D in_texture : register(t0);
Texture2D coef_scaler : register(t1);
Texture2D coef_usm : register(t2);
RWTexture2D<float4> out_texture : register(u0);

#include "../third_party/nis/NIS_Scaler.h"

[numthreads(NIS_THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 blockIdx : SV_GroupID, uint3 threadIdx : SV_GroupThreadID)
{
    NVScaler(blockIdx.xy, threadIdx.x);
}
