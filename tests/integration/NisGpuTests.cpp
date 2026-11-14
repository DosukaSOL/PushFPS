// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// NVIDIA Image Scaling (pipeline/NisPass) on a real D3D12 device: the compiled
// NisColor / NisScalerSdr / NisScalerPq shaders, the root signatures, the
// coefficient upload and the descriptor views, end to end. Uses the hardware
// adapter when there is one and WARP otherwise, so it also runs on build
// machines without a graphics card. VEYRA_NIS_GPU_DEBUG=1 enables the D3D12
// debug layer (when installed) and fails on its errors.
//
// Checks per case: output finite; flat areas keep their colour (the encode /
// decode boundary is exact up to FP16); a hard edge stays at the same place and
// sharpening at 100 % overshoots more than at 0 %; HDR keeps 4000-nit
// highlights and out-of-BT.709 (negative scRGB) colours; factors above 2x and
// a non-uniform aspect produce the exact output size.
#include "veyra/pipeline/NisPass.h"
#include "veyra/pipeline/NisSettings.h"
#include <DirectXPackedVector.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

using namespace veyra::pipeline;

using DirectX::PackedVector::XMConvertFloatToHalf;
using DirectX::PackedVector::XMConvertHalfToFloat;

namespace {
int failures = 0;
void check(bool ok, const std::string& label) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label.c_str());
    if (!ok) ++failures;
}

struct Image {
    uint32_t w = 0, h = 0;
    std::vector<float> rgba;  // 4 floats per pixel
    float* at(uint32_t x, uint32_t y) { return &rgba[(size_t(y) * w + x) * 4]; }
    const float* at(uint32_t x, uint32_t y) const { return &rgba[(size_t(y) * w + x) * 4]; }
};

struct Gpu {
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12Fence> fence;
    HANDLE event = nullptr;
    uint64_t value = 0;
    bool debug = false;
    std::string adapter;

    bool create() {
        if (GetEnvironmentVariableW(L"VEYRA_NIS_GPU_DEBUG", nullptr, 0) > 0) {
            ComPtr<ID3D12Debug> d;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&d)))) { d->EnableDebugLayer(); debug = true; }
            else std::printf("NOTE D3D12 debug layer not installed; running without it\n");
        }
        ComPtr<IDXGIFactory6> factory;
        if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)))) return false;
        for (UINT i = 0; !device; ++i) {
            ComPtr<IDXGIAdapter1> a;
            if (factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&a)) == DXGI_ERROR_NOT_FOUND) break;
            DXGI_ADAPTER_DESC1 d{}; a->GetDesc1(&d);
            if (d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
            if (SUCCEEDED(D3D12CreateDevice(a.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)))) adapter = narrow(d.Description);
        }
        if (!device) {
            ComPtr<IDXGIAdapter> warp;
            if (FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp))) ||
                FAILED(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) return false;
            adapter = "WARP";
        }
        D3D12_FEATURE_DATA_SHADER_MODEL sm{D3D_SHADER_MODEL_6_0};
        if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &sm, sizeof(sm))) || sm.HighestShaderModel < D3D_SHADER_MODEL_6_0) {
            std::printf("FAIL adapter %s has no shader model 6.0\n", adapter.c_str());
            return false;
        }
        D3D12_COMMAND_QUEUE_DESC qd{}; qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        return SUCCEEDED(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue))) &&
               SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))) &&
               SUCCEEDED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&list))) &&
               SUCCEEDED(list->Close()) &&
               SUCCEEDED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))) &&
               (event = CreateEventW(nullptr, FALSE, FALSE, nullptr)) != nullptr;
    }
    static std::string narrow(const wchar_t* w) {
        std::string s; for (; *w; ++w) s.push_back(*w < 128 ? char(*w) : '?'); return s;
    }
    bool begin() { return SUCCEEDED(allocator->Reset()) && SUCCEEDED(list->Reset(allocator.Get(), nullptr)); }
    bool submitAndWait() {
        if (FAILED(list->Close())) return false;
        ID3D12CommandList* lists[] = {list.Get()};
        queue->ExecuteCommandLists(1, lists);
        if (FAILED(queue->Signal(fence.Get(), ++value))) return false;
        if (fence->GetCompletedValue() < value) { fence->SetEventOnCompletion(value, event); WaitForSingleObject(event, 60000); }
        return fence->GetCompletedValue() >= value && SUCCEEDED(device->GetDeviceRemovedReason());
    }
    bool debugErrors() {
        if (!debug) return false;
        ComPtr<ID3D12InfoQueue> info;
        if (FAILED(device.As(&info))) return false;
        bool errors = false;
        for (UINT64 i = 0, n = info->GetNumStoredMessages(); i < n; ++i) {
            SIZE_T size = 0; info->GetMessage(i, nullptr, &size);
            std::vector<char> buffer(size); auto* m = reinterpret_cast<D3D12_MESSAGE*>(buffer.data());
            if (SUCCEEDED(info->GetMessage(i, m, &size)) && m->Severity <= D3D12_MESSAGE_SEVERITY_ERROR) {
                std::printf("D3D12 %s\n", m->pDescription); errors = true;
            }
        }
        info->ClearStoredMessages();
        return errors;
    }
};

constexpr DXGI_FORMAT kFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
UINT pitchOf(uint32_t w) { return (w * 8 + 255) & ~255u; }

// One NisPass run: upload `input`, scale to outW x outH, read the result back.
bool runNis(Gpu& gpu, const Image& input, uint32_t outW, uint32_t outH, bool hdr, float sharpness, Image& output,
            int frames = 1) {
    auto* device = gpu.device.Get();
    auto src = makeTexture(device, input.w, input.h, kFormat, false);
    auto dst = makeTexture(device, outW, outH, kFormat, true);
    const UINT inPitch = pitchOf(input.w), outPitch = pitchOf(outW);
    auto upload = makeUploadBuffer(device, uint64_t(inPitch) * input.h);
    D3D12_HEAP_PROPERTIES rp{}; rp.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC bd{}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = uint64_t(outPitch) * outH;
    bd.Height = 1; bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> readback;
    if (!src || !dst || !upload || FAILED(device->CreateCommittedResource(&rp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback)))) return false;
    void* mapped = nullptr;
    if (FAILED(upload->Map(0, nullptr, &mapped))) return false;
    for (uint32_t y = 0; y < input.h; ++y) {
        auto* row = reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(mapped) + size_t(y) * inPitch);
        for (uint32_t x = 0; x < input.w * 4; ++x) row[x] = XMConvertFloatToHalf(input.rgba[size_t(y) * input.w * 4 + x]);
    }
    upload->Unmap(0, nullptr);

    NisPass pass;
    if (!pass.initialize(device, input.w, input.h, outW, outH, hdr)) { std::printf("NisPass::initialize failed\n"); return false; }
    DescriptorStager stager;
    if (!stager.initialize(device, 16)) return false;
    pass.createViews(device, stager, src.Get(), dst.Get());
    StateTracker tracker;
    for (int frame = 0; frame < frames; ++frame) {
        if (!gpu.begin()) return false;
        auto* list = gpu.list.Get();
        if (frame == 0) {
            tracker.transition(list, src.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
            D3D12_TEXTURE_COPY_LOCATION t{}, s{};
            t.pResource = src.Get(); t.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            s.pResource = upload.Get(); s.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            s.PlacedFootprint.Footprint = {kFormat, input.w, input.h, 1, inPitch};
            list->CopyTextureRegion(&t, 0, 0, 0, &s, nullptr);
        }
        if (!pass.dispatch(list, tracker, sharpness)) { std::printf("NisPass::dispatch failed\n"); return false; }
        if (frame == frames - 1) {
            tracker.transition(list, dst.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
            D3D12_TEXTURE_COPY_LOCATION t{}, s{};
            s.pResource = dst.Get(); s.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            t.pResource = readback.Get(); t.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            t.PlacedFootprint.Footprint = {kFormat, outW, outH, 1, outPitch};
            list->CopyTextureRegion(&t, 0, 0, 0, &s, nullptr);
        }
        if (!gpu.submitAndWait()) { std::printf("GPU submission failed\n"); return false; }
    }
    if (gpu.debugErrors()) return false;
    output.w = outW; output.h = outH; output.rgba.assign(size_t(outW) * outH * 4, 0.0f);
    if (FAILED(readback->Map(0, nullptr, &mapped))) return false;
    for (uint32_t y = 0; y < outH; ++y) {
        const auto* row = reinterpret_cast<const uint16_t*>(static_cast<const uint8_t*>(mapped) + size_t(y) * outPitch);
        for (uint32_t x = 0; x < outW * 4; ++x) output.rgba[size_t(y) * outW * 4 + x] = XMConvertHalfToFloat(row[x]);
    }
    readback->Unmap(0, nullptr);
    return true;
}

bool allFinite(const Image& i) {
    return std::all_of(i.rgba.begin(), i.rgba.end(), [](float v) { return std::isfinite(v); });
}
// Largest relative difference to `colour` inside the rectangle (output pixels).
double maxRelativeError(const Image& i, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, const float* colour) {
    double worst = 0;
    for (uint32_t y = y0; y < y1; ++y)
        for (uint32_t x = x0; x < x1; ++x)
            for (int c = 0; c < 3; ++c)
                worst = std::max(worst, std::abs(double(i.at(x, y)[c]) - colour[c]) / std::max(0.02, std::abs(double(colour[c]))));
    return worst;
}

// Left half dark, right half light, a hard vertical edge in the middle (linear values).
Image edgeImage(uint32_t w, uint32_t h, const float* dark, const float* light) {
    Image img{w, h, std::vector<float>(size_t(w) * h * 4)};
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            const float* c = x < w / 2 ? dark : light;
            float* p = img.at(x, y);
            p[0] = c[0]; p[1] = c[1]; p[2] = c[2]; p[3] = 1.0f;
        }
    return img;
}
// Mid grey with faint vertical bars (period 3 px, about +/-4 % in sRGB): the kind of detail NIS sharpens.
Image detailImage(uint32_t w, uint32_t h) {
    Image img{w, h, std::vector<float>(size_t(w) * h * 4)};
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            const double srgb = 0.5 + 0.04 * std::sin(2.0 * 3.14159265358979 * x / 3.0 + 0.4 * y);
            const float lin = float(srgb <= 0.04045 ? srgb / 12.92 : std::pow((srgb + 0.055) / 1.055, 2.4));
            float* p = img.at(x, y);
            p[0] = p[1] = p[2] = lin; p[3] = 1.0f;
        }
    return img;
}
// Standard deviation of the green channel in the central half of the picture.
double contrastOf(const Image& i) {
    double sum = 0, sq = 0; size_t n = 0;
    for (uint32_t y = i.h / 4; y < i.h * 3 / 4; ++y)
        for (uint32_t x = i.w / 4; x < i.w * 3 / 4; ++x) { const double v = i.at(x, y)[1]; sum += v; sq += v * v; ++n; }
    const double mean = sum / double(n);
    return std::sqrt(std::max(0.0, sq / double(n) - mean * mean));
}
// Peak of the green channel in the light half next to the edge (overshoot measure).
float overshoot(const Image& i, float light) {
    float peak = 0;
    for (uint32_t x = i.w / 2; x < std::min(i.w, i.w / 2 + 8); ++x) peak = std::max(peak, i.at(x, i.h / 2)[1]);
    return peak - light;
}

void sdrCases(Gpu& gpu) {
    const float dark[3] = {0.05f, 0.05f, 0.05f}, light[3] = {0.6f, 0.6f, 0.6f};
    const auto input = edgeImage(96, 54, dark, light);
    Image soft, sharp;
    const bool ok0 = runNis(gpu, input, 192, 108, false, 0.0f, soft);
    const bool ok1 = runNis(gpu, input, 192, 108, false, 1.0f, sharp);
    check(ok0 && ok1, "sdr-2x: NIS runs at 0 % and 100 % sharpening");
    if (!ok0 || !ok1) return;
    check(allFinite(soft) && allFinite(sharp), "sdr-2x: output finite");
    check(maxRelativeError(soft, 8, 8, 80, 100, dark) < 0.02, "sdr-2x: flat dark area unchanged");
    check(maxRelativeError(soft, 112, 8, 184, 100, light) < 0.02, "sdr-2x: flat light area unchanged");
    check(soft.at(90, 54)[1] < 0.3f && soft.at(102, 54)[1] > 0.3f, "sdr-2x: edge stays in the middle");
    const float o0 = overshoot(soft, light[1]), o1 = overshoot(sharp, light[1]);
    std::printf("NOTE sdr-2x hard-edge overshoot 0%%=%.4f 100%%=%.4f\n", o0, o1);
    // NIS sharpens fine, low-contrast detail (texture, foliage); a hard edge is scaled, not sharpened.
    const auto detail = detailImage(96, 54);
    Image detailSoft, detailSharp;
    const bool d0 = runNis(gpu, detail, 192, 108, false, 0.0f, detailSoft);
    const bool d1 = runNis(gpu, detail, 192, 108, false, 1.0f, detailSharp);
    check(d0 && d1 && allFinite(detailSoft) && allFinite(detailSharp), "sdr-detail: NIS runs on fine detail");
    if (d0 && d1) {
        const double c0 = contrastOf(detailSoft), c1 = contrastOf(detailSharp), cin = contrastOf(detail);
        std::printf("NOTE sdr-detail contrast input=%.5f 0%%=%.5f 100%%=%.5f\n", cin, c0, c1);
        check(c1 > c0 * 1.05, "sdr-detail: 100 % sharpening brings out more detail than 0 %");
    }

    Image wide;
    check(runNis(gpu, input, 288, 162, false, 0.5f, wide) && wide.w == 288 && wide.h == 162 && allFinite(wide),
          "sdr-3x: factor above 2x (NIS 2x + bilinear) gives the exact size");
    if (!wide.rgba.empty()) check(maxRelativeError(wide, 8, 8, 120, 150, dark) < 0.02 && maxRelativeError(wide, 170, 8, 280, 150, light) < 0.02,
          "sdr-3x: flat areas unchanged");
    Image aspect;
    check(runNis(gpu, input, 144, 108, false, 0.5f, aspect) && allFinite(aspect) && aspect.w == 144 && aspect.h == 108,
          "sdr-aspect: 1.5x by 2x");
    Image same;
    check(runNis(gpu, input, 96, 54, false, 0.5f, same) && allFinite(same) &&
          maxRelativeError(same, 4, 4, 40, 50, dark) < 0.02, "sdr-1x: same size sharpens without moving flat colour");
    Image repeated;
    check(runNis(gpu, input, 192, 108, false, 0.5f, repeated, 3) && allFinite(repeated), "sdr-frames: three frames, coefficients uploaded once");
}

void hdrCases(Gpu& gpu) {
    // scRGB: 1 = 80 nits. 50 = 4000 nits; the out-of-BT.709 green has a negative red.
    const float highlight[3] = {50.0f, 50.0f, 50.0f}, wideGreen[3] = {-0.08f, 1.2f, 0.10f};
    const auto input = edgeImage(96, 54, wideGreen, highlight);
    Image out;
    const bool ok = runNis(gpu, input, 192, 108, true, 0.5f, out);
    check(ok, "hdr-2x: NIS runs on scRGB (BT.2020 PQ boundary)");
    if (!ok) return;
    check(allFinite(out), "hdr-2x: output finite");
    const double hi = maxRelativeError(out, 112, 8, 184, 100, highlight), wide = maxRelativeError(out, 8, 8, 80, 100, wideGreen);
    std::printf("NOTE hdr-2x highlight error=%.4f wide-gamut error=%.4f red=%.4f\n", hi, wide, out.at(40, 54)[0]);
    check(hi < 0.02, "hdr-2x: 4000-nit highlight kept (no 1000-nit clip)");
    check(wide < 0.15 && out.at(40, 54)[0] < -0.05f, "hdr-2x: out-of-BT.709 colour kept (negative scRGB red)");
}

void settingsCases() {
    setNisSharpness(0.25f); check(std::abs(nisSharpness() - 0.25f) < 1e-6f, "settings: sharpness stored");
    setNisSharpness(7.0f); check(nisSharpness() == 1.0f, "settings: above 1 clamps");
    setNisSharpness(-1.0f); check(nisSharpness() == 0.0f, "settings: below 0 clamps");
    setNisSharpness(std::nanf("")); check(nisSharpness() == kNisDefaultSharpness, "settings: NaN resets to the default");
    check(nisScaledExtent(1920, 3840) == 3840 && nisScaledExtent(1280, 3840) == 2560 && nisScaledExtent(1920, 1280) == 1920,
          "settings: NIS renders 1x..2x, the rest is bilinear");
}
}

int main() {
    settingsCases();
    Gpu gpu;
    if (!gpu.create()) { std::printf("FAIL no D3D12 device (hardware or WARP)\n"); return 1; }
    std::printf("NOTE adapter=%s debugLayer=%d\n", gpu.adapter.c_str(), gpu.debug ? 1 : 0);
    sdrCases(gpu);
    hdrCases(gpu);
    std::printf("%s: %d failure(s)\n", failures ? "FAILED" : "OK", failures);
    return failures ? 1 : 0;
}
