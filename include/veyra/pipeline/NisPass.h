#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// NVIDIA Image Scaling (NIS, MIT, third_party/nis) as the super-resolution stage
// engine::kVideoSrNis: a spatial upscaler with adaptive sharpening that runs on
// every DirectX 12 GPU. Three compute passes on the graph's linear FP16 colour:
//   1. NisColor encode  input (linear) -> encoded_ (sRGB, or BT.2020 PQ for HDR)
//   2. NisScaler        encoded_ -> scaled_ at up to 2x per axis (NVScaler)
//   3. NisColor decode  scaled_ -> output (linear), bilinear to the exact output
//                       size when the factor is above 2x (or below 1x)
// The graph owns input and output; the pass owns its intermediates and the two
// coefficient tables. Sharpness is read every frame (NisSettings.h).
#include "veyra/pipeline/GpuPassUtils.h"
#include <array>
#include <string>

namespace veyra::pipeline {

// Size NIS itself renders for one axis: the output, kept within 1x..2x of the input.
constexpr uint32_t nisScaledExtent(uint32_t input, uint32_t output) {
    const uint32_t lo = input, hi = input * 2u;
    return output < lo ? lo : output > hi ? hi : output;
}

class NisPass {
public:
    static constexpr UINT kColorConstants = 8;
    static constexpr UINT kScalerConstants = 28;  // NISConfig (NIS_Config.h) without its 256-byte padding
    // hdr: the graph works in scRGB (HDR) rather than SDR linear BT.709.
    bool initialize(ID3D12Device* device, uint32_t inputWidth, uint32_t inputHeight,
                    uint32_t outputWidth, uint32_t outputHeight, bool hdr);
    void createViews(ID3D12Device* device, DescriptorStager& stager, ID3D12Resource* input, ID3D12Resource* output);
    // Leaves the output in NON_PIXEL_SHADER_RESOURCE, like the graph's ordinary resize.
    bool dispatch(ID3D12GraphicsCommandList* list, StateTracker& tracker, float sharpness);
    std::string describe() const;
    bool hdr() const { return hdr_; }

private:
    bool uploadCoefficients(ID3D12GraphicsCommandList* list, StateTracker& tracker);
    uint32_t inW_ = 0, inH_ = 0, outW_ = 0, outH_ = 0, scaledW_ = 0, scaledH_ = 0;
    bool hdr_ = false, coefficientsPending_ = false;
    float lastSharpness_ = -1.0f;
    std::array<uint32_t, kScalerConstants> config_{};
    ComputePass encode_, scaler_, decode_;
    ComPtr<ID3D12Resource> encoded_, scaled_, coefScale_, coefUsm_, coefScaleUpload_, coefUsmUpload_;
    UINT coefPitch_ = 0;
    ID3D12Resource* input_ = nullptr;
    ID3D12Resource* output_ = nullptr;
};
}
