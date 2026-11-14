// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#include "veyra/pipeline/NisPass.h"
#include "../../third_party/nis/NIS_Config.h"
#include <bit>
#include <cstddef>
#include <cstring>
#include <format>

namespace veyra::pipeline {
namespace {
constexpr DXGI_FORMAT kColorFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
// float4 coefficient rows: kFilterSize / 4 texels wide, one row per phase.
constexpr DXGI_FORMAT kCoefFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
constexpr UINT kCoefWidth = UINT(kFilterSize / 4), kCoefHeight = UINT(kPhaseCount);
constexpr UINT kCoefRowBytes = kCoefWidth * 16;
static_assert(offsetof(NISConfig, reserved1) == (NisPass::kScalerConstants - 1) * 4, "NISConfig layout changed");
static_assert(sizeof(coef_scale) == kCoefRowBytes * kCoefHeight && sizeof(coef_usm) == sizeof(coef_scale));
constexpr UINT alignedPitch(UINT bytes) {
    return (bytes + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) / D3D12_TEXTURE_DATA_PITCH_ALIGNMENT * D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
}
bool createPass(ID3D12Device* device, ComputePass& pass, const char* shader, UINT srvCount, UINT constants) {
    std::vector<uint8_t> cs;
    return pass.loadShader(shader, cs) && pass.create(device, cs, srvCount + 1, srvCount, 1, constants, 0, true);
}
}

std::string NisPass::describe() const {
    return std::format("NIS 1.0.3 {}x{} -> {}x{} (nis {}x{}){}", inW_, inH_, outW_, outH_, scaledW_, scaledH_,
        hdr_ ? " hdr=pq" : " sdr");
}

bool NisPass::initialize(ID3D12Device* device, uint32_t inputWidth, uint32_t inputHeight,
                         uint32_t outputWidth, uint32_t outputHeight, bool hdr) {
    if (!device || !inputWidth || !inputHeight || !outputWidth || !outputHeight) return false;
    inW_ = inputWidth; inH_ = inputHeight; outW_ = outputWidth; outH_ = outputHeight; hdr_ = hdr;
    scaledW_ = nisScaledExtent(inW_, outW_); scaledH_ = nisScaledExtent(inH_, outH_);
    lastSharpness_ = -1.0f;
    encoded_ = makeTexture(device, inW_, inH_, kColorFormat, true);
    scaled_ = makeTexture(device, scaledW_, scaledH_, kColorFormat, true);
    coefScale_ = makeTexture(device, kCoefWidth, kCoefHeight, kCoefFormat, false);
    coefUsm_ = makeTexture(device, kCoefWidth, kCoefHeight, kCoefFormat, false);
    coefPitch_ = alignedPitch(kCoefRowBytes);
    coefScaleUpload_ = makeUploadBuffer(device, uint64_t(coefPitch_) * kCoefHeight);
    coefUsmUpload_ = makeUploadBuffer(device, uint64_t(coefPitch_) * kCoefHeight);
    if (!encoded_ || !scaled_ || !coefScale_ || !coefUsm_ || !coefScaleUpload_ || !coefUsmUpload_) return false;
    const auto fill = [&](ID3D12Resource* upload, const float (*rows)[kFilterSize]) {
        void* p = nullptr;
        if (FAILED(upload->Map(0, nullptr, &p))) return false;
        for (UINT y = 0; y < kCoefHeight; ++y) std::memcpy(static_cast<uint8_t*>(p) + size_t(y) * coefPitch_, rows[y], kCoefRowBytes);
        upload->Unmap(0, nullptr);
        return true;
    };
    if (!fill(coefScaleUpload_.Get(), coef_scale) || !fill(coefUsmUpload_.Get(), coef_usm)) {
        veyra::log::error("nis", "coefficient upload map failed");
        return false;
    }
    coefficientsPending_ = true;
    if (!createPass(device, encode_, "NisColor.dxil", 1, kColorConstants) ||
        !createPass(device, scaler_, hdr_ ? "NisScalerPq.dxil" : "NisScalerSdr.dxil", 3, kScalerConstants) ||
        !createPass(device, decode_, "NisColor.dxil", 1, kColorConstants)) {
        veyra::log::error("nis", "NIS shaders could not be created");
        return false;
    }
    veyra::log::info("nis", std::format("created {}", describe()));
    return true;
}

void NisPass::createViews(ID3D12Device* device, DescriptorStager& stager, ID3D12Resource* input, ID3D12Resource* output) {
    input_ = input; output_ = output;
    stager.stageSrv(input, nullptr, encode_.heap.Get(), 0);
    makeUav(device, encoded_.Get(), kColorFormat, cpuHandleOf(encode_, 1));
    stager.stageSrv(encoded_.Get(), nullptr, scaler_.heap.Get(), 0);
    stager.stageSrv(coefScale_.Get(), nullptr, scaler_.heap.Get(), 1);
    stager.stageSrv(coefUsm_.Get(), nullptr, scaler_.heap.Get(), 2);
    makeUav(device, scaled_.Get(), kColorFormat, cpuHandleOf(scaler_, 3));
    stager.stageSrv(scaled_.Get(), nullptr, decode_.heap.Get(), 0);
    makeUav(device, output, kColorFormat, cpuHandleOf(decode_, 1));
}

// The upload buffers live as long as the pass; the graph drains before release.
bool NisPass::uploadCoefficients(ID3D12GraphicsCommandList* list, StateTracker& tracker) {
    const auto copy = [&](ID3D12Resource* texture, ID3D12Resource* upload) {
        tracker.transition(list, texture, D3D12_RESOURCE_STATE_COPY_DEST);
        D3D12_TEXTURE_COPY_LOCATION target{}, source{};
        target.pResource = texture; target.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        source.pResource = upload; source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        source.PlacedFootprint.Footprint = {kCoefFormat, kCoefWidth, kCoefHeight, 1, coefPitch_};
        list->CopyTextureRegion(&target, 0, 0, 0, &source, nullptr);
        tracker.transition(list, texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    };
    copy(coefScale_.Get(), coefScaleUpload_.Get());
    copy(coefUsm_.Get(), coefUsmUpload_.Get());
    coefficientsPending_ = false;
    return true;
}

bool NisPass::dispatch(ID3D12GraphicsCommandList* list, StateTracker& tracker, float sharpness) {
    if (!input_ || !output_ || input_ == output_) return false;
    if (coefficientsPending_ && !uploadCoefficients(list, tracker)) return false;
    if (sharpness != lastSharpness_) {
        NISConfig config{};
        if (!NVScalerUpdateConfig(config, sharpness, 0, 0, inW_, inH_, inW_, inH_, 0, 0, scaledW_, scaledH_, scaledW_, scaledH_,
                                  hdr_ ? NISHDRMode::PQ : NISHDRMode::None)) {
            veyra::log::error("nis", std::format("NVScalerUpdateConfig rejected {}", describe()));
            return false;
        }
        std::memcpy(config_.data(), &config, sizeof(uint32_t) * kScalerConstants);
        lastSharpness_ = sharpness;
    }
    const auto colorConstants = [&](uint32_t w, uint32_t h, bool decode) {
        std::array<float, kColorConstants> c{};
        c[0] = std::bit_cast<float>(w); c[1] = std::bit_cast<float>(h);
        c[2] = std::bit_cast<float>(uint32_t(hdr_ ? 1 : 0)); c[3] = std::bit_cast<float>(uint32_t(decode ? 1 : 0));
        c[4] = 1.0f / float(w); c[5] = 1.0f / float(h);
        return c;
    };
    // 1. encode
    tracker.transition(list, input_, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    tracker.transition(list, encoded_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    auto c = colorConstants(inW_, inH_, false);
    encode_.bind(list, c.data(), gpuHandleOf(encode_, 0).ptr, gpuHandleOf(encode_, 1).ptr);
    list->Dispatch((inW_ + 7) / 8, (inH_ + 7) / 8, 1);
    tracker.uavBarrier(list, encoded_.Get());
    // 2. NVScaler: one 256-thread group per 32 x 24 output block.
    tracker.transition(list, encoded_.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    tracker.transition(list, scaled_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    scaler_.bind(list, reinterpret_cast<const float*>(config_.data()), gpuHandleOf(scaler_, 0).ptr, gpuHandleOf(scaler_, 3).ptr);
    list->Dispatch((scaledW_ + 31) / 32, (scaledH_ + 23) / 24, 1);
    tracker.uavBarrier(list, scaled_.Get());
    // 3. decode to the working colour at the exact output size
    tracker.transition(list, scaled_.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    tracker.transition(list, output_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    c = colorConstants(outW_, outH_, true);
    decode_.bind(list, c.data(), gpuHandleOf(decode_, 0).ptr, gpuHandleOf(decode_, 1).ptr);
    list->Dispatch((outW_ + 7) / 8, (outH_ + 7) / 8, 1);
    tracker.uavBarrier(list, output_);
    tracker.transition(list, output_, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    return true;
}
}
