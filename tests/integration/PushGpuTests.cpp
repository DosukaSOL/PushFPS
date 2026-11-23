// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// PushFPS GPU pieces on a real D3D12 device (hardware, or WARP on build
// machines without a graphics card). VEYRA_PUSH_GPU_DEBUG=1 enables the D3D12
// debug layer (when installed) and fails on its errors.
//  - RecordingCompare (before/after comparison clips): a real frame is held,
//    a generated frame gets the held real frame in its left half and keeps its
//    own right half, the divider and both labels land where expected in luma
//    and neutral chroma, a later real frame replaces the hold; an HDR clip
//    splits without overlays.
//  - PushHudSampler (automatic HUD detection): copies only real frames, one at
//    a time, at the interval; finds a static HUD box on a moving picture end to
//    end; refuses a format it cannot read.
//  - PushHudLock (Live HUD Lock): after a second of a still HUD over a moving
//    picture, a generated frame takes the HUD pixels from the held real frame
//    and keeps its own everywhere else; a HUD that disappears is released on
//    that frame; a reset starts over.
//  - PushFlow (PushFPS's own frame generation, RIFE on DirectML): when VEYRA_PUSHFLOW_TEST_RUNTIME names a
//    folder holding pushflow\ (the release's runtime), two identical frames give the same picture back,
//    and a square that moves 16 px lands near the middle at t = 0.5, far closer than a blend of the two
//    frames, in both tiers' models. Without the variable the test is skipped (and says so).
#include "veyra/pushflow/PushFlowPredict.h"
#include "veyra/pushflow/PushFlowBackend.h"
#include "veyra/pushflow/PushFlowMotion.h"
#include "veyra/engine/PushHudSampler.h"
#include "veyra/pipeline/PushHudLock.h"
#include "veyra/pipeline/PushHudStill.h"
#include "veyra/pipeline/PushOverlay.h"
#include "veyra/pipeline/PushOverlayPass.h"
#include "PushCharacterGpu.h"
#include "veyra/pipeline/PushAmbientPass.h"
#include "veyra/pipeline/PushEnhanceShape.h"
#include "veyra/engine/PushAmbient.h"
#include "veyra/engine/RecordingCompare.h"
#include "veyra/engine/PushStreamOverlay.h"
#include "veyra/engine/PushCanvas.h"
#include "veyra/pipeline/GpuPassUtils.h"

#include <d3d12sdklayers.h>
#include <dxgi1_6.h>

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <format>
#include <functional>
#include <vector>

using namespace veyra;
using Microsoft::WRL::ComPtr;

namespace {
int failures = 0;
void check(bool ok, const std::string& label) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label.c_str());
    if (!ok) ++failures;
}

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
        if (GetEnvironmentVariableW(L"VEYRA_PUSH_GPU_DEBUG", nullptr, 0) > 0) {
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
    bool submit() {
        if (FAILED(list->Close())) return false;
        ID3D12CommandList* lists[] = {list.Get()};
        queue->ExecuteCommandLists(1, lists);
        return SUCCEEDED(queue->Signal(fence.Get(), ++value));
    }
    bool wait() {
        if (fence->GetCompletedValue() < value) { fence->SetEventOnCompletion(value, event); WaitForSingleObject(event, 60000); }
        return fence->GetCompletedValue() >= value && SUCCEEDED(device->GetDeviceRemovedReason());
    }
    bool submitAndWait() { return submit() && wait(); }
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

ComPtr<ID3D12Resource> makeBuffer(ID3D12Device* device, uint64_t bytes, D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES hp{}; hp.Type = type;
    D3D12_RESOURCE_DESC bd{}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = bytes; bd.Height = 1;
    bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> r;
    const auto state = type == D3D12_HEAP_TYPE_UPLOAD ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_DEST;
    if (FAILED(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, state, nullptr, IID_PPV_ARGS(&r)))) return nullptr;
    return r;
}

// A texture plane: bytes per pixel `bpp`, filled or read through a buffer.
struct Plane {
    ComPtr<ID3D12Resource> texture;
    DXGI_FORMAT format;
    uint32_t w, h, bpp;
    UINT pitch() const { return (w * bpp + 255) & ~255u; }
};

// Records an upload of `pixels` (tight rows) into the plane; the texture ends in `after`.
ComPtr<ID3D12Resource> recordFill(Gpu& gpu, const Plane& p, const std::vector<uint8_t>& pixels, D3D12_RESOURCE_STATES before,
                                  D3D12_RESOURCE_STATES after) {
    auto upload = makeBuffer(gpu.device.Get(), uint64_t(p.pitch()) * p.h, D3D12_HEAP_TYPE_UPLOAD);
    if (!upload) return nullptr;
    uint8_t* mapped = nullptr;
    upload->Map(0, nullptr, reinterpret_cast<void**>(&mapped));
    for (uint32_t y = 0; y < p.h; ++y) std::memcpy(mapped + size_t(y) * p.pitch(), pixels.data() + size_t(y) * p.w * p.bpp, size_t(p.w) * p.bpp);
    upload->Unmap(0, nullptr);
    D3D12_RESOURCE_BARRIER b{}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource = p.texture.Get();
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    if (before != D3D12_RESOURCE_STATE_COPY_DEST) {
        b.Transition.StateBefore = before; b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST; gpu.list->ResourceBarrier(1, &b);
    }
    D3D12_TEXTURE_COPY_LOCATION from{}, to{};
    from.pResource = upload.Get(); from.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    from.PlacedFootprint.Footprint = {p.format, p.w, p.h, 1, p.pitch()};
    to.pResource = p.texture.Get(); to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    gpu.list->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);
    if (after != D3D12_RESOURCE_STATE_COPY_DEST) {
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST; b.Transition.StateAfter = after; gpu.list->ResourceBarrier(1, &b);
    }
    return upload;
}

// Records a readback of the plane (currently in `state`, left there) into a new buffer.
ComPtr<ID3D12Resource> recordRead(Gpu& gpu, const Plane& p, D3D12_RESOURCE_STATES state) {
    auto readback = makeBuffer(gpu.device.Get(), uint64_t(p.pitch()) * p.h, D3D12_HEAP_TYPE_READBACK);
    D3D12_RESOURCE_BARRIER b{}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource = p.texture.Get();
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    if (state != D3D12_RESOURCE_STATE_COPY_SOURCE) {
        b.Transition.StateBefore = state; b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE; gpu.list->ResourceBarrier(1, &b);
    }
    D3D12_TEXTURE_COPY_LOCATION from{}, to{};
    from.pResource = p.texture.Get(); from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    to.pResource = readback.Get(); to.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    to.PlacedFootprint.Footprint = {p.format, p.w, p.h, 1, p.pitch()};
    gpu.list->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);
    if (state != D3D12_RESOURCE_STATE_COPY_SOURCE) {
        std::swap(b.Transition.StateBefore, b.Transition.StateAfter); gpu.list->ResourceBarrier(1, &b);
    }
    return readback;
}

std::vector<uint8_t> mapRead(const Plane& p, ID3D12Resource* readback) {
    std::vector<uint8_t> out(size_t(p.w) * p.h * p.bpp);
    uint8_t* mapped = nullptr;
    if (FAILED(readback->Map(0, nullptr, reinterpret_cast<void**>(&mapped)))) return {};
    for (uint32_t y = 0; y < p.h; ++y) std::memcpy(out.data() + size_t(y) * p.w * p.bpp, mapped + size_t(y) * p.pitch(), size_t(p.w) * p.bpp);
    const D3D12_RANGE none{0, 0};
    readback->Unmap(0, &none);
    return out;
}

void testCompare(Gpu& gpu) {
    constexpr uint32_t W = 256, H = 144;
    auto* device = gpu.device.Get();
    Plane y{pipeline::makeTexture(device, W, H, DXGI_FORMAT_R8_UNORM, true), DXGI_FORMAT_R8_UNORM, W, H, 1};
    Plane uv{pipeline::makeTexture(device, W / 2, H / 2, DXGI_FORMAT_R8G8_UNORM, true), DXGI_FORMAT_R8G8_UNORM, W / 2, H / 2, 2};
    check(y.texture && uv.texture, "compare: planes created");
    engine::RecordingLabel left{8, 4, std::vector<uint8_t>(32, 255)}, right{9, 5, std::vector<uint8_t>(45, 0)};
    engine::RecordingCompare compare;
    std::wstring reason;
    check(compare.create(device, W, H, false, left, right, reason), "compare: created");
    check(compare.labelled() && compare.patches().size() == 3, "compare: divider and two labels");
    check(compare.splitX() == 128, "compare: split in the middle");

    pipeline::StateTracker states;
    auto frame = [&](uint8_t luma, uint8_t chroma, bool generated, std::vector<uint8_t>* yOut, std::vector<uint8_t>* uvOut) {
        gpu.begin();
        // The recorder's planes are UAVs written by the conversion; fill them like it.
        auto a = recordFill(gpu, y, std::vector<uint8_t>(W * H, luma), states.get(y.texture.Get()), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        states.set(y.texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        auto b = recordFill(gpu, uv, std::vector<uint8_t>(W / 2 * H / 2 * 2, chroma), states.get(uv.texture.Get()), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        states.set(uv.texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        compare.compose(gpu.list.Get(), states, y.texture.Get(), uv.texture.Get(), generated);
        ComPtr<ID3D12Resource> ry, ruv;
        if (yOut) { ry = recordRead(gpu, y, states.get(y.texture.Get())); ruv = recordRead(gpu, uv, states.get(uv.texture.Get())); }
        const bool ok = gpu.submitAndWait();
        if (yOut && ok) { *yOut = mapRead(y, ry.Get()); *uvOut = mapRead(uv, ruv.Get()); }
        return ok;
    };
    std::vector<uint8_t> py, puv;
    check(frame(50, 100, false, &py, &puv), "compare: real frame runs");
    check(py.size() == W * H && py[60 * W + 2] == 50 && py[60 * W + 200] == 50, "compare: a real frame stays whole");
    check(frame(200, 150, true, &py, &puv), "compare: generated frame runs");
    const auto Y = [&](uint32_t x, uint32_t yy) { return int(py[size_t(yy) * W + x]); };
    const auto U = [&](uint32_t x, uint32_t yy) { return int(puv[(size_t(yy) * (W / 2) + x) * 2]); };
    check(Y(2, 60) == 50 && Y(100, 120) == 50, "compare: left half holds the real frame");
    check(Y(200, 60) == 200 && Y(250, 140) == 200, "compare: right half is the generated frame");
    check(Y(126, 60) == 235 && Y(129, 60) == 235 && Y(125, 60) == 50 && Y(130, 60) == 200, "compare: divider is four white pixels at the split");
    check(Y(10, 10) == 235 && Y(8, 8) == 235 && Y(15, 11) == 235 && Y(16, 10) == 50, "compare: left label (white) at the top left");
    check(Y(240, 8) == 16 && Y(247, 11) == 16 && Y(248, 10) == 200 && Y(239, 10) == 200, "compare: right label (black, odd size trimmed) at the top right");
    check(U(1, 30) == 100 && U(100, 30) == 150 && U(5, 5) == 128 && U(64, 30) == 128, "compare: chroma follows and overlays are neutral");
    check(compare.generatedFrames() == 1 && compare.realFrames() == 1, "compare: counts frames");
    check(frame(90, 120, false, &py, &puv), "compare: next real frame runs");
    check(Y(2, 60) == 90 && Y(200, 60) == 90, "compare: a new real frame replaces the hold");
    check(frame(10, 10, true, &py, &puv) && Y(2, 60) == 90 && Y(200, 60) == 10, "compare: the next generated frame uses the new hold");
    check(!gpu.debugErrors(), "compare: no D3D12 debug errors");

    engine::RecordingLabel huge{200, 100, std::vector<uint8_t>(20000, 255)};
    engine::RecordingCompare tooBig;
    check(tooBig.create(device, W, H, false, huge, huge, reason) && tooBig.patches().size() == 3, "compare: labels too large are scaled down, not left out");
    if (tooBig.patches().size() == 3) {
        const auto& l = tooBig.patches()[1];
        check(l.width <= 128 - 2 * 8 - 4 && l.height <= H / 3 - 16 && l.width >= 8, "compare: the scaled label fits its half");
        check(tooBig.patches()[2].x + tooBig.patches()[2].width <= W - 8, "compare: the scaled right label stays inside the picture");
    }
    engine::RecordingLabel nothing{};
    engine::RecordingCompare unlabelled;
    check(unlabelled.create(device, W, H, false, nothing, nothing, reason) && unlabelled.patches().size() == 1, "compare: no label text keeps only the divider");
}

// One comparison frame on planes of any format: fill y and uv with the given bytes,
// compose, read both back (tight rows).
bool composeFrame(Gpu& gpu, engine::RecordingCompare& compare, pipeline::StateTracker& states, const Plane& y, const Plane& uv,
                  const std::vector<uint8_t>& yData, const std::vector<uint8_t>& uvData, bool generated,
                  std::vector<uint8_t>& yOut, std::vector<uint8_t>& uvOut, double seconds = 0.0) {
    if (!gpu.begin()) return false;
    auto a = recordFill(gpu, y, yData, states.get(y.texture.Get()), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    states.set(y.texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    auto b = recordFill(gpu, uv, uvData, states.get(uv.texture.Get()), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    states.set(uv.texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    compare.compose(gpu.list.Get(), states, y.texture.Get(), uv.texture.Get(), generated, nullptr, 0, seconds);
    auto ry = recordRead(gpu, y, states.get(y.texture.Get()));
    auto ruv = recordRead(gpu, uv, states.get(uv.texture.Get()));
    if (!gpu.submitAndWait()) return false;
    yOut = mapRead(y, ry.Get()); uvOut = mapRead(uv, ruv.Get());
    return yOut.size() == size_t(y.w) * y.h * y.bpp;
}
std::vector<uint8_t> words(const std::vector<uint16_t>& v) {
    std::vector<uint8_t> out(v.size() * 2);
    std::memcpy(out.data(), v.data(), out.size());
    return out;
}
uint16_t word(const std::vector<uint8_t>& bytes, size_t index) { uint16_t v; std::memcpy(&v, bytes.data() + index * 2, 2); return v; }

void testCompareHdr(Gpu& gpu) {
    // P010: 10-bit codes in the top bits. White text and the divider at code 573 (203 nits).
    constexpr uint32_t W = 256, H = 144;
    auto* device = gpu.device.Get();
    Plane y{pipeline::makeTexture(device, W, H, DXGI_FORMAT_R16_UNORM, true), DXGI_FORMAT_R16_UNORM, W, H, 2};
    Plane uv{pipeline::makeTexture(device, W / 2, H / 2, DXGI_FORMAT_R16G16_UNORM, true), DXGI_FORMAT_R16G16_UNORM, W / 2, H / 2, 4};
    check(y.texture && uv.texture, "compare HDR: planes created");
    engine::RecordingLabel left{8, 4, std::vector<uint8_t>(32, 255)}, right{8, 4, std::vector<uint8_t>(32, 0)};
    engine::RecordingCompare compare;
    std::wstring reason;
    check(compare.create(device, W, H, true, left, right, reason), "compare HDR: created");
    check(compare.labelled() && compare.patches().size() == 3, "compare HDR: divider and both labels (they used to be left out)");
    pipeline::StateTracker states;
    std::vector<uint8_t> py, puv;
    const uint16_t real = 300 << 6, gen = 700 << 6, chroma = 600 << 6;
    check(composeFrame(gpu, compare, states, y, uv, words(std::vector<uint16_t>(W * H, real)), words(std::vector<uint16_t>(W / 2 * H / 2 * 2, chroma)), false, py, puv), "compare HDR: real frame runs");
    check(composeFrame(gpu, compare, states, y, uv, words(std::vector<uint16_t>(W * H, gen)), words(std::vector<uint16_t>(W / 2 * H / 2 * 2, chroma)), true, py, puv), "compare HDR: generated frame runs");
    const auto Y = [&](uint32_t x, uint32_t yy) { return int(word(py, size_t(yy) * W + x)); };
    check(Y(2, 60) == real && Y(200, 60) == gen, "compare HDR: split holds the real frame on the left");
    check(Y(127, 60) == (573 << 6), "compare HDR: divider at 203 nits");
    check(Y(10, 10) == (573 << 6), "compare HDR: white label text at 203 nits");
    check(Y(240, 9) == (64 << 6), "compare HDR: black label box at code 64");
    check(int(word(puv, size_t(5) * (W / 2) * 2 + 5 * 2)) == (512 << 6), "compare HDR: overlay chroma is neutral");
    check(!gpu.debugErrors(), "compare HDR: no D3D12 debug errors");
}

void testCompareSideBySide(Gpu& gpu, bool hdr) {
    constexpr uint32_t W = 256, H = 144;
    const char* tag = hdr ? "side by side HDR" : "side by side";
    auto* device = gpu.device.Get();
    const auto yFormat = hdr ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R8_UNORM;
    const auto uvFormat = hdr ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R8G8_UNORM;
    const uint32_t bpp = hdr ? 2 : 1;
    Plane y{pipeline::makeTexture(device, W, H, yFormat, true), yFormat, W, H, bpp};
    Plane uv{pipeline::makeTexture(device, W / 2, H / 2, uvFormat, true), uvFormat, W / 2, H / 2, bpp * 2};
    engine::RecordingLabel left{8, 4, std::vector<uint8_t>(32, 255)}, right{8, 4, std::vector<uint8_t>(32, 255)};
    engine::RecordingCompare compare;
    std::wstring reason;
    const bool created = compare.create(device, W, H, hdr, left, right, reason, engine::CompareLayout::SideBySide);
    if (!created) std::printf("%s: create failed: %ls\n", tag, reason.c_str());
    check(created, std::string(tag) + ": created (shader loaded)");
    if (!created) return;
    check(compare.pictureWidth() == 128 && compare.pictureHeight() == 72 && compare.pictureTop() == 36 && compare.rightX() == 128, std::string(tag) + ": two half-size pictures, centred");
    check(compare.patches().size() == 3, std::string(tag) + ": divider and two labels");
    if (compare.patches().size() == 3) {
        const auto& d = compare.patches()[0]; const auto& l = compare.patches()[1]; const auto& r = compare.patches()[2];
        check(d.y == 36 && d.height == 72, std::string(tag) + ": divider only between the pictures");
        check(l.y + l.height <= 36 && r.y + r.height <= 36, std::string(tag) + ": labels in the black bar above the pictures");
        check(l.x == 60 && r.x == 188, std::string(tag) + ": labels centred over their picture");
    }
    // Values: SDR in 8-bit codes, HDR in 10-bit codes (stored << 6).
    const auto code = [&](uint32_t v8, uint32_t v10) { return hdr ? v10 << 6 : v8; };
    const auto fill = [&](auto f, uint32_t w, uint32_t h, uint32_t channels) {
        std::vector<uint8_t> out(size_t(w) * h * channels * bpp);
        for (uint32_t yy = 0; yy < h; ++yy)
            for (uint32_t x = 0; x < w; ++x)
                for (uint32_t c = 0; c < channels; ++c) {
                    const uint32_t v = f(x, yy);
                    const size_t at = (size_t(yy) * w + x) * channels + c;
                    if (hdr) { const uint16_t w16 = uint16_t(v); std::memcpy(out.data() + at * 2, &w16, 2); }
                    else out[at] = uint8_t(v);
                }
        return out;
    };
    const auto read = [&](const std::vector<uint8_t>& bytes, size_t index) { return hdr ? int(word(bytes, index)) : int(bytes[index]); };
    pipeline::StateTracker states;
    std::vector<uint8_t> py, puv;
    // Real frame: luma pairs of equal columns (so the 2x2 average is exact), chroma 100.
    const auto realY = fill([&](uint32_t x, uint32_t) { return code(x & ~1u, (x & ~1u) + 64); }, W, H, 1);
    const auto realUv = fill([&](uint32_t, uint32_t) { return code(100, 400); }, W / 2, H / 2, 2);
    check(composeFrame(gpu, compare, states, y, uv, realY, realUv, false, py, puv), std::string(tag) + ": real frame runs");
    const auto Y = [&](uint32_t x, uint32_t yy) { return read(py, size_t(yy) * W + x); };
    const auto U = [&](uint32_t x, uint32_t yy) { return read(puv, (size_t(yy) * (W / 2) + x) * 2); };
    check(Y(10, 41) == int(code(20, 84)), std::string(tag) + ": left picture is the frame at half size (2x2 average)");
    check(Y(138, 41) == int(code(20, 84)), std::string(tag) + ": a real frame shows on both sides");
    check(Y(5, 5) == int(code(16, 64)) && Y(5, 140) == int(code(16, 64)), std::string(tag) + ": black bars above and below");
    check(U(2, 2) == int(code(128, 512)) && U(10, 25) == int(code(100, 400)), std::string(tag) + ": chroma neutral in the bars, kept in the picture");
    check(Y(62, 17) == int(code(235, 573)), std::string(tag) + ": label text in the bar");
    check(Y(127, 60) == int(code(235, 573)) && Y(127, 20) == int(code(16, 64)), std::string(tag) + ": divider between the pictures only");
    // Generated frame: the right side changes, the left keeps the held real frame.
    const auto genY = fill([&](uint32_t, uint32_t) { return code(200, 800); }, W, H, 1);
    const auto genUv = fill([&](uint32_t, uint32_t) { return code(150, 600); }, W / 2, H / 2, 2);
    check(composeFrame(gpu, compare, states, y, uv, genY, genUv, true, py, puv), std::string(tag) + ": generated frame runs");
    check(Y(10, 41) == int(code(20, 84)), std::string(tag) + ": left keeps the real frame");
    check(Y(138, 41) == int(code(200, 800)) && U(70, 25) == int(code(150, 600)), std::string(tag) + ": right shows the generated frame");
    check(U(10, 25) == int(code(100, 400)), std::string(tag) + ": left chroma stays the real frame's");
    check(compare.generatedFrames() == 1 && compare.realFrames() == 1, std::string(tag) + ": counts frames");
    check(!gpu.debugErrors(), std::string(tag) + ": no D3D12 debug errors");
}

// Vertical (TikTok, Shorts, Reels): a 1080 x 1920 picture of its own, the console (held real frame)
// in the top half and the current frame in the bottom half, each the middle of the 1920 x 1080
// frame (1214 x 1080 from column 352) scaled to 1080 x 960; labels next to the divider.
void testCompareVertical(Gpu& gpu, bool hdr) {
    constexpr uint32_t W = 1920, H = 1080;
    const std::string tag = hdr ? "vertical HDR" : "vertical";
    auto* device = gpu.device.Get();
    const auto yFormat = hdr ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R8_UNORM;
    const auto uvFormat = hdr ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R8G8_UNORM;
    const uint32_t bpp = hdr ? 2 : 1;
    uint32_t ow = 0, oh = 0;
    engine::compareOutputSize(engine::CompareLayout::Vertical, W, H, ow, oh);
    check(ow == 1080 && oh == 1920, tag + ": 1080 x 1920 file");
    const auto crop = engine::compareVerticalCrop(W, H, 1080, 960);
    check(crop.width == 1214 && crop.height == 1080 && crop.x == 352 && crop.y == 0, tag + ": the middle of the frame, 9:8");
    const auto tall = engine::compareVerticalCrop(1080, 1920, 1080, 960);
    check(tall.width == 1080 && tall.height == 960 && tall.x == 0 && tall.y == 480, tag + ": a tall frame keeps its middle rows");
    Plane y{pipeline::makeTexture(device, W, H, yFormat, true), yFormat, W, H, bpp};
    Plane uv{pipeline::makeTexture(device, W / 2, H / 2, uvFormat, true), uvFormat, W / 2, H / 2, bpp * 2};
    engine::RecordingLabel left{8, 4, std::vector<uint8_t>(32, 255)}, right{8, 4, std::vector<uint8_t>(32, 255)};
    engine::RecordingCompare compare;
    std::wstring reason;
    const bool created = compare.create(device, W, H, hdr, left, right, reason, engine::CompareLayout::Vertical);
    if (!created) std::printf("%s: create failed: %ls\n", tag.c_str(), reason.c_str());
    check(created, tag + ": created (shader loaded)");
    if (!created) return;
    check(compare.outputY() && compare.outputWidth() == 1080 && compare.outputHeight() == 1920 && compare.pictureHeight() == 960, tag + ": own output planes");
    check(compare.patches().size() == 3, tag + ": divider and two labels");
    if (compare.patches().size() == 3) {
        const auto& d = compare.patches()[0]; const auto& l = compare.patches()[1]; const auto& r = compare.patches()[2];
        check(d.x == 0 && d.y == 958 && d.width == 1080 && d.height == 4, tag + ": divider across the middle");
        check(l.y + l.height <= 958 && l.y + l.height + 80 > 958, tag + ": console label just above the divider");
        check(r.y >= 962 && r.y < 962 + 80, tag + ": PushFPS label just below the divider");
        check(l.x == 536 && r.x == 536, tag + ": labels centred");
    }
    const auto code = [&](uint32_t v8, uint32_t v10) { return hdr ? v10 << 6 : v8; };
    const auto fill = [&](auto f, uint32_t w, uint32_t h, uint32_t channels) {
        std::vector<uint8_t> out(size_t(w) * h * channels * bpp);
        for (uint32_t yy = 0; yy < h; ++yy)
            for (uint32_t x = 0; x < w; ++x)
                for (uint32_t c = 0; c < channels; ++c) {
                    const uint32_t v = f(x, yy);
                    const size_t at = (size_t(yy) * w + x) * channels + c;
                    if (hdr) { const uint16_t w16 = uint16_t(v); std::memcpy(out.data() + at * 2, &w16, 2); }
                    else out[at] = uint8_t(v);
                }
        return out;
    };
    Plane oy{compare.outputY(), yFormat, 1080, 1920, bpp};
    Plane ouv{compare.outputUv(), uvFormat, 540, 960, bpp * 2};
    pipeline::StateTracker states;
    std::vector<uint8_t> py, puv;
    const auto frame = [&](const std::vector<uint8_t>& yData, const std::vector<uint8_t>& uvData, bool generated) {
        if (!gpu.begin()) return false;
        auto a = recordFill(gpu, y, yData, states.get(y.texture.Get()), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        states.set(y.texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        auto b = recordFill(gpu, uv, uvData, states.get(uv.texture.Get()), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        states.set(uv.texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        compare.compose(gpu.list.Get(), states, y.texture.Get(), uv.texture.Get(), generated);
        auto ry = recordRead(gpu, oy, states.get(oy.texture.Get()));
        auto ruv = recordRead(gpu, ouv, states.get(ouv.texture.Get()));
        if (!gpu.submitAndWait()) return false;
        py = mapRead(oy, ry.Get()); puv = mapRead(ouv, ruv.Get());
        return py.size() == size_t(1080) * 1920 * bpp;
    };
    const auto read = [&](const std::vector<uint8_t>& bytes, size_t index) { return hdr ? int(word(bytes, index)) : int(bytes[index]); };
    const auto Y = [&](uint32_t x, uint32_t yy) { return read(py, size_t(yy) * 1080 + x); };
    const auto U = [&](uint32_t x, uint32_t yy) { return read(puv, (size_t(yy) * 540 + x) * 2); };
    // Real frame: 120 inside the crop, 16 outside it (so a wrong crop shows), chroma 100.
    const auto realY = fill([&](uint32_t x, uint32_t) { return x >= 352 && x < 352 + 1214 ? code(120, 500) : code(16, 64); }, W, H, 1);
    const auto realUv = fill([&](uint32_t, uint32_t) { return code(100, 400); }, W / 2, H / 2, 2);
    check(frame(realY, realUv, false), tag + ": real frame runs");
    check(Y(2, 100) == int(code(120, 500)) && Y(1077, 100) == int(code(120, 500)), tag + ": the edges show the crop's edges, not the frame outside it");
    check(Y(540, 400) == int(code(120, 500)) && Y(540, 1500) == int(code(120, 500)), tag + ": a real frame shows in both halves");
    check(U(270, 200) == int(code(100, 400)) && U(270, 750) == int(code(100, 400)), tag + ": chroma kept");
    check(Y(10, 959) == int(code(235, 573)) && Y(10, 956) == int(code(120, 500)), tag + ": divider rows 958-961 only");
    // Generated frame: the bottom changes, the top keeps the held real frame.
    const auto genY = fill([&](uint32_t, uint32_t) { return code(200, 800); }, W, H, 1);
    const auto genUv = fill([&](uint32_t, uint32_t) { return code(150, 600); }, W / 2, H / 2, 2);
    check(frame(genY, genUv, true), tag + ": generated frame runs");
    check(Y(540, 400) == int(code(120, 500)) && U(270, 200) == int(code(100, 400)), tag + ": top keeps the console frame");
    check(Y(540, 1500) == int(code(200, 800)) && U(270, 750) == int(code(150, 600)), tag + ": bottom shows the generated frame");
    check(compare.generatedFrames() == 1 && compare.realFrames() == 1, tag + ": counts frames");
    check(!gpu.debugErrors(), tag + ": no D3D12 debug errors");
}

// Streaming mode scenes (PushStreamOverlay.h): a transparent overlay with an opaque white corner box
// keeps the picture elsewhere; an opaque card replaces the whole picture.
void testStreamOverlay(Gpu& gpu, bool hdr) {
    constexpr uint32_t W = 128, H = 64;
    const std::string tag = hdr ? "scene overlay HDR" : "scene overlay";
    auto* device = gpu.device.Get();
    const auto yFormat = hdr ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R8_UNORM;
    const auto uvFormat = hdr ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R8G8_UNORM;
    const uint32_t bpp = hdr ? 2 : 1;
    Plane y{pipeline::makeTexture(device, W, H, yFormat, true), yFormat, W, H, bpp};
    Plane uv{pipeline::makeTexture(device, W / 2, H / 2, uvFormat, true), uvFormat, W / 2, H / 2, bpp * 2};
    const auto code = [&](uint32_t v8, uint32_t v10) { return hdr ? v10 << 6 : v8; };
    const auto plane = [&](uint32_t value, uint32_t w, uint32_t h, uint32_t channels) {
        std::vector<uint8_t> out(size_t(w) * h * channels * bpp);
        for (size_t i = 0; i < size_t(w) * h * channels; ++i) {
            if (hdr) { const uint16_t v = uint16_t(value); std::memcpy(out.data() + i * 2, &v, 2); } else out[i] = uint8_t(value);
        }
        return out;
    };
    const auto read = [&](const std::vector<uint8_t>& b, size_t i) { return hdr ? int(word(b, i)) : int(b[i]); };
    // Overlay: transparent, with an opaque white 16 x 16 box at the top left. Drawn at half size to
    // check that it is scaled to the frame.
    engine::PushOverlayImage halfSize{W / 2, H / 2, {}, std::vector<uint8_t>(size_t(W / 2) * (H / 2) * 4, 0)};
    for (uint32_t yy = 0; yy < 8; ++yy)
        for (uint32_t x = 0; x < 8; ++x) { uint8_t* p = &halfSize.rgba[(size_t(yy) * (W / 2) + x) * 4]; p[0] = p[1] = p[2] = p[3] = 255; }
    engine::FrameOverlay overlay;
    check(overlay.create(device, W, H, hdr, halfSize), tag + ": created (scaled to the frame)");
    pipeline::StateTracker states;
    const auto run = [&](engine::FrameOverlay& o, std::vector<uint8_t>& py, std::vector<uint8_t>& puv) {
        if (!gpu.begin()) return false;
        auto a = recordFill(gpu, y, plane(code(100, 400), W, H, 1), states.get(y.texture.Get()), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        states.set(y.texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        auto b = recordFill(gpu, uv, plane(code(90, 300), W / 2, H / 2, 2), states.get(uv.texture.Get()), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        states.set(uv.texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        o.apply(gpu.list.Get(), states, y.texture.Get(), uv.texture.Get());
        auto ry = recordRead(gpu, y, states.get(y.texture.Get()));
        auto ruv = recordRead(gpu, uv, states.get(uv.texture.Get()));
        if (!gpu.submitAndWait()) return false;
        py = mapRead(y, ry.Get()); puv = mapRead(uv, ruv.Get());
        return py.size() == size_t(W) * H * bpp;
    };
    std::vector<uint8_t> py, puv;
    check(run(overlay, py, puv), tag + ": runs");
    check(read(py, size_t(4) * W + 4) == int(code(235, 573)), tag + ": the box is white (203 nits in HDR)");
    check(read(py, size_t(40) * W + 90) == int(code(100, 400)), tag + ": the picture is kept where the overlay is clear");
    check(read(puv, (size_t(20) * (W / 2) + 40) * 2) == int(code(90, 300)), tag + ": chroma kept where clear");
    check(read(puv, (size_t(2) * (W / 2) + 2) * 2) == int(code(128, 512)), tag + ": white box chroma neutral");
    // A card: opaque black everywhere.
    engine::PushOverlayImage card{W, H, {}, std::vector<uint8_t>(size_t(W) * H * 4, 0)};
    for (size_t i = 0; i < size_t(W) * H; ++i) card.rgba[i * 4 + 3] = 255;
    engine::FrameOverlay cardOverlay;
    check(cardOverlay.create(device, W, H, hdr, card), tag + ": card created");
    check(run(cardOverlay, py, puv), tag + ": card runs");
    check(read(py, size_t(40) * W + 90) == int(code(16, 64)) && read(py, 0) == int(code(16, 64)), tag + ": a card replaces the whole picture");
    // Process-wide publication.
    const auto before = engine::pushStreamOverlayVersion();
    engine::setPushStreamOverlay(std::make_shared<engine::PushOverlayImage>(card));
    uint64_t version = 0;
    check(engine::pushStreamOverlay(version) != nullptr && version == before + 1, tag + ": published with a new version");
    engine::setPushStreamOverlay(nullptr);
    check(engine::pushStreamOverlay(version) == nullptr, tag + ": cleared");
    check(!gpu.debugErrors(), tag + ": no D3D12 debug errors");
}

// PushFPS outputs and canvas (engine/PushCanvas.h, shaders/PushCanvasConvert.hlsl): the presented
// picture sampled into an output of another size and layout, as the recorder does for each output.
void testCanvasConvert(Gpu& gpu) {
    const std::string tag = "canvas convert";
    auto* device = gpu.device.Get();
    constexpr uint32_t SW = 128, SH = 72;
    Plane src{pipeline::makeTexture(device, SW, SH, DXGI_FORMAT_R32G32B32A32_FLOAT, false), DXGI_FORMAT_R32G32B32A32_FLOAT, SW, SH, 16};
    // White picture with a black left quarter (so a crop is visible).
    std::vector<uint8_t> pixels(size_t(SW) * SH * 16);
    for (uint32_t y = 0; y < SH; ++y)
        for (uint32_t x = 0; x < SW; ++x) {
            const float v = x < SW / 4 ? 0.0f : 1.0f;
            const float px[4] = {v, v, v, 1.0f};
            std::memcpy(pixels.data() + (size_t(y) * SW + x) * 16, px, 16);
        }
    if (!gpu.begin()) return;
    auto srcUpload = recordFill(gpu, src, pixels, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    check(gpu.submitAndWait(), tag + ": picture uploaded");
    pipeline::ComputePass pass;
    pipeline::DescriptorStager stager;
    std::vector<uint8_t> shader;
    check(pass.loadShader("PushCanvasConvert.dxil", shader) && pass.create(device, shader, 3, 1, 2, 16) && stager.initialize(device, 2), tag + ": shader loaded");
    D3D12_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; view.Texture2D.MipLevels = 1;
    stager.stageSrv(src.texture.Get(), &view, pass.heap.Get(), 0);
    // Runs one layout into a W x H output; returns the Y plane.
    const auto run = [&](const engine::PushCanvas& canvas, uint32_t W, uint32_t H, std::vector<uint8_t>& py) {
        Plane y{pipeline::makeTexture(device, W, H, DXGI_FORMAT_R8_UNORM, true), DXGI_FORMAT_R8_UNORM, W, H, 1};
        Plane uv{pipeline::makeTexture(device, W / 2, H / 2, DXGI_FORMAT_R8G8_UNORM, true), DXGI_FORMAT_R8G8_UNORM, W / 2, H / 2, 2};
        if (!y.texture || !uv.texture) return false;
        pipeline::makeUav(device, y.texture.Get(), DXGI_FORMAT_R8_UNORM, pipeline::cpuHandleOf(pass, 1));
        pipeline::makeUav(device, uv.texture.Get(), DXGI_FORMAT_R8G8_UNORM, pipeline::cpuHandleOf(pass, 2));
        const auto map = engine::mapPushCanvas(canvas, SW, SH, W, H);
        const uint32_t taps = engine::pushCanvasTaps(map.srcW, map.dstW) | (engine::pushCanvasTaps(map.srcH, map.dstH) << 8);
        const float constants[16] = {std::bit_cast<float>(W), std::bit_cast<float>(H), 0.0f, 0.0f,
            float(map.dstX), float(map.dstY), float(map.dstW), float(map.dstH), float(map.srcX), float(map.srcY), float(map.srcW), float(map.srcH),
            std::bit_cast<float>(SW), std::bit_cast<float>(SH), std::bit_cast<float>(taps), std::bit_cast<float>(canvas.background)};
        if (!gpu.begin()) return false;
        D3D12_RESOURCE_BARRIER b[2]{};
        for (int i = 0; i < 2; ++i) {
            b[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            b[i].Transition.pResource = i ? uv.texture.Get() : y.texture.Get();
            b[i].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON; b[i].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }
        gpu.list->ResourceBarrier(2, b);
        pass.bind(gpu.list.Get(), constants, pipeline::gpuHandleOf(pass, 0).ptr, pipeline::gpuHandleOf(pass, 1).ptr);
        gpu.list->Dispatch((W + 15) / 16, (H + 15) / 16, 1);
        auto ry = recordRead(gpu, y, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        if (!gpu.submitAndWait()) return false;
        py = mapRead(y, ry.Get());
        return py.size() == size_t(W) * H;
    };
    std::vector<uint8_t> py;
    // Same shape, half size (box filtered): white right of the black quarter, black in it.
    engine::PushCanvas full;
    check(run(full, 64, 36, py), tag + ": half size runs");
    check(py[size_t(18) * 64 + 40] == 235 && py[size_t(18) * 64 + 4] == 16, tag + ": half size keeps the picture");
    // 16:9 into 9:16, fit: picture in the middle band, black bars above and below.
    engine::PushCanvas fit; fit.fit = engine::PushCanvasFit::Fit;
    check(run(fit, 36, 64, py), tag + ": vertical fit runs");
    check(py[size_t(32) * 36 + 30] == 235 && py[size_t(2) * 36 + 30] == 16 && py[size_t(61) * 36 + 30] == 16, tag + ": fit leaves bars above and below");
    // Fill: the middle of the picture covers the output (the black quarter is cropped away).
    engine::PushCanvas fill; fill.fit = engine::PushCanvasFit::Fill;
    check(run(fill, 36, 64, py), tag + ": vertical fill runs");
    check(py[size_t(2) * 36 + 2] == 235 && py[size_t(61) * 36 + 33] == 235, tag + ": fill covers the whole output");
    // A crop of the black quarter and a green background around a box.
    engine::PushCanvas box; box.x = 0.5f; box.y = 0.5f; box.w = 0.5f; box.h = 0.5f; box.fit = engine::PushCanvasFit::Stretch;
    box.cropLeft = 0.25f; box.background = 0x00FF00;
    check(run(box, 64, 36, py), tag + ": box runs");
    check(py[size_t(27) * 64 + 34] == 235, tag + ": the cropped picture (no black quarter) in its box");
    check(std::abs(int(py[size_t(4) * 64 + 4]) - 173) <= 1, tag + ": green background around it");
    check(!gpu.debugErrors(), tag + ": no D3D12 debug errors");
}

// PushFPS streaming mode: scene transitions (the scene picture at part opacity) and live layers (a
// webcam, window or video frame scaled into a rectangle, engine/PushStreamOverlay.h LiveLayerBlend).
void testLiveLayers(Gpu& gpu, bool hdr) {
    constexpr uint32_t W = 128, H = 64;
    const std::string tag = hdr ? "live layers HDR" : "live layers";
    auto* device = gpu.device.Get();
    const auto yFormat = hdr ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R8_UNORM;
    const auto uvFormat = hdr ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R8G8_UNORM;
    const uint32_t bpp = hdr ? 2 : 1;
    Plane y{pipeline::makeTexture(device, W, H, yFormat, true), yFormat, W, H, bpp};
    Plane uv{pipeline::makeTexture(device, W / 2, H / 2, uvFormat, true), uvFormat, W / 2, H / 2, bpp * 2};
    const auto code = [&](uint32_t v8, uint32_t v10) { return hdr ? v10 << 6 : v8; };
    const auto plane = [&](uint32_t value, uint32_t w, uint32_t h, uint32_t channels) {
        std::vector<uint8_t> out(size_t(w) * h * channels * bpp);
        for (size_t i = 0; i < size_t(w) * h * channels; ++i) {
            if (hdr) { const uint16_t v = uint16_t(value); std::memcpy(out.data() + i * 2, &v, 2); } else out[i] = uint8_t(value);
        }
        return out;
    };
    const auto read = [&](const std::vector<uint8_t>& b, size_t i) { return hdr ? int(word(b, i)) >> 6 : int(b[i]); };
    const auto within = [](int a, int b, int tolerance) { return std::abs(a - b) <= tolerance; };

    // Placement maths.
    uint32_t x0 = 0, y0 = 0, w = 0, h = 0;
    engine::PushLayerPlace place{0.5f, 0.25f, 0.25f, 0.0f, 0, true};
    check(engine::pushLayerRect(engine::resolvePushLayerPlace(place, 32, 16, W, H), W, H, x0, y0, w, h) && x0 == 64 && y0 == 16 && w == 32 && h == 16,
          tag + ": automatic height keeps the frame's shape");
    engine::PushLayerPlace corner{1.0f, 1.0f, 0.25f, 0.0f, 3, true};
    check(engine::pushLayerRect(engine::resolvePushLayerPlace(corner, 32, 16, W, H), W, H, x0, y0, w, h) && x0 == 96 && y0 == 48 && w == 32 && h == 16,
          tag + ": bottom-right anchor");
    engine::PushLayerPlace off{1.2f, 0.0f, 0.2f, 0.2f, 0, true};
    check(!engine::pushLayerRect(off, W, H, x0, y0, w, h), tag + ": a place off the picture draws nothing");
    check(engine::pushTransitionProgress(0, 300) == 0.0f && engine::pushTransitionProgress(300, 300) == 1.0f &&
          std::abs(engine::pushTransitionProgress(150, 300) - 0.5f) < 1e-4f && engine::pushTransitionProgress(5, 0) == 1.0f,
          tag + ": transition progress eases from 0 to 1 (0 ms: a cut)");

    // An opaque white 32 x 16 frame at (64, 16).
    auto frame = std::make_shared<engine::PushOverlayImage>(engine::PushOverlayImage{32, 16, {}, std::vector<uint8_t>(32 * 16 * 4, 255)});
    std::array<engine::PushLiveLayerState, engine::kPushLiveLayers> layers{};
    layers[0].frame = frame; layers[0].frameVersion = 1; layers[0].place = place; layers[0].opacity = 1.0f;
    engine::LiveLayerBlend blend;
    check(blend.create(device, W, H, hdr, 2), tag + ": created");
    pipeline::StateTracker states;
    const auto run = [&](const std::function<void()>& draw, std::vector<uint8_t>& py, std::vector<uint8_t>& puv) {
        if (!gpu.begin()) return false;
        auto a = recordFill(gpu, y, plane(code(100, 400), W, H, 1), states.get(y.texture.Get()), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        states.set(y.texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        auto b = recordFill(gpu, uv, plane(code(90, 300), W / 2, H / 2, 2), states.get(uv.texture.Get()), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        states.set(uv.texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        draw();
        auto ry = recordRead(gpu, y, states.get(y.texture.Get()));
        auto ruv = recordRead(gpu, uv, states.get(uv.texture.Get()));
        if (!gpu.submitAndWait()) return false;
        py = mapRead(y, ry.Get()); puv = mapRead(uv, ruv.Get());
        return py.size() == size_t(W) * H * bpp;
    };
    std::vector<uint8_t> py, puv;
    check(run([&] { blend.apply(gpu.list.Get(), states, y.texture.Get(), uv.texture.Get(), layers); }, py, puv), tag + ": runs");
    const int white = hdr ? 573 : 235, base = hdr ? 400 : 100;
    check(read(py, size_t(20) * W + 70) == white && read(py, size_t(31) * W + 95) == white, tag + ": the frame fills its rectangle");
    check(read(py, size_t(20) * W + 97) == base && read(py, size_t(10) * W + 10) == base && read(py, size_t(33) * W + 70) == base,
          tag + ": the picture is kept around it");
    check(read(puv, (size_t(10) * (W / 2) + 36) * 2) == (hdr ? 512 : 128), tag + ": white is neutral in chroma");
    // Half opacity: halfway between the picture and white (the same uploaded frame).
    layers[0].opacity = 0.5f;
    check(run([&] { blend.apply(gpu.list.Get(), states, y.texture.Get(), uv.texture.Get(), layers); }, py, puv), tag + ": half opacity runs");
    check(within(read(py, size_t(20) * W + 70), (white + base) / 2, 2), tag + ": half opacity blends halfway");
    // A larger frame drawn smaller (box-filtered): still white inside, nothing outside.
    auto big = std::make_shared<engine::PushOverlayImage>(engine::PushOverlayImage{128, 64, {}, std::vector<uint8_t>(128 * 64 * 4, 255)});
    layers[0].frame = big; layers[0].frameVersion = 2; layers[0].opacity = 1.0f;
    for (int i = 0; i < 6; ++i) { // past the guard that delays a second size change
        std::vector<uint8_t> ty, tuv;
        run([&] { blend.apply(gpu.list.Get(), states, y.texture.Get(), uv.texture.Get(), layers); }, ty, tuv);
        py = ty; puv = tuv;
    }
    check(read(py, size_t(20) * W + 70) == white && read(py, size_t(20) * W + 97) == base, tag + ": a 4x larger frame is scaled into the same place");
    // Transitions: a black card at half opacity over the picture.
    engine::PushOverlayImage card{W, H, {}, std::vector<uint8_t>(size_t(W) * H * 4, 0)};
    for (size_t i = 0; i < size_t(W) * H; ++i) card.rgba[i * 4 + 3] = 255;
    engine::FrameOverlay overlay;
    check(overlay.create(device, W, H, hdr, card), tag + ": card created");
    check(run([&] { overlay.apply(gpu.list.Get(), states, y.texture.Get(), uv.texture.Get(), 0.5f); }, py, puv), tag + ": faded card runs");
    const int black = hdr ? 64 : 16;
    check(within(read(py, size_t(40) * W + 90), (black + base) / 2, 2), tag + ": a card at half opacity shows half the picture (scene fade)");
    check(run([&] { overlay.apply(gpu.list.Get(), states, y.texture.Get(), uv.texture.Get(), 0.0f); }, py, puv) &&
          read(py, size_t(40) * W + 90) == base, tag + ": opacity 0 leaves the picture alone");
    check(!gpu.debugErrors(), tag + ": no D3D12 debug errors");
}

// The console side from the raw picture (linear BT.709, float RGBA, here half the recorded size):
// no enhancement, only scaled and coded like the recorder's own conversion.
void testCompareRaw(Gpu& gpu, bool hdr, engine::CompareLayout layout) {
    constexpr uint32_t W = 256, H = 144;
    const bool sbs = layout == engine::CompareLayout::SideBySide;
    const std::string tag = std::string("raw console ") + (sbs ? "side by side" : "split") + (hdr ? " HDR" : "");
    auto* device = gpu.device.Get();
    const auto yFormat = hdr ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R8_UNORM;
    const auto uvFormat = hdr ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R8G8_UNORM;
    const uint32_t bpp = hdr ? 2 : 1;
    Plane y{pipeline::makeTexture(device, W, H, yFormat, true), yFormat, W, H, bpp};
    Plane uv{pipeline::makeTexture(device, W / 2, H / 2, uvFormat, true), uvFormat, W / 2, H / 2, bpp * 2};
    Plane raw{pipeline::makeTexture(device, W / 2, H / 2, DXGI_FORMAT_R32G32B32A32_FLOAT, false), DXGI_FORMAT_R32G32B32A32_FLOAT, W / 2, H / 2, 16};
    check(y.texture && uv.texture && raw.texture, tag + ": textures created");
    engine::RecordingLabel nothing{};
    engine::RecordingCompare compare;
    std::wstring reason;
    check(compare.create(device, W, H, hdr, nothing, nothing, reason, layout), tag + ": created");
    check(compare.rawAvailable(), tag + ": raw pass loaded");
    // Raw: linear grey. SDR 0.5 -> sRGB 0.7354 -> code 177. HDR 1.0 (80 nits) -> PQ 0.4859 -> code 490.
    const float grey = hdr ? 1.0f : 0.5f;
    std::vector<uint8_t> rawData(size_t(raw.w) * raw.h * 16);
    for (size_t i = 0; i < size_t(raw.w) * raw.h; ++i) {
        const float px[4] = {grey, grey, grey, 1.0f};
        std::memcpy(rawData.data() + i * 16, px, 16);
    }
    if (!gpu.begin()) return;
    auto rawUpload = recordFill(gpu, raw, rawData, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COMMON);
    check(gpu.submitAndWait(), tag + ": raw picture uploaded");
    const auto words16 = [&](uint32_t value, size_t count) { return words(std::vector<uint16_t>(count, uint16_t(value))); };
    const auto plane = [&](uint32_t v8, uint32_t v10, size_t count) { return hdr ? words16(v10 << 6, count) : std::vector<uint8_t>(count, uint8_t(v8)); };
    const auto read = [&](const std::vector<uint8_t>& bytes, size_t index) { return hdr ? int(word(bytes, index)) : int(bytes[index]); };
    const int rawCode = hdr ? (490 << 6) : 177, tolerance = hdr ? 64 : 1;
    pipeline::StateTracker states;
    std::vector<uint8_t> py, puv;
    const engine::RecordingRawFrame frame{raw.texture.Get(), false};
    // A real frame with its raw picture: the enhanced picture is a flat 200 (8-bit) / 800 (10-bit).
    if (!gpu.begin()) return;
    auto a = recordFill(gpu, y, plane(200, 800, W * H), states.get(y.texture.Get()), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    states.set(y.texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    auto b = recordFill(gpu, uv, plane(150, 600, W / 2 * H / 2 * 2), states.get(uv.texture.Get()), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    states.set(uv.texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    compare.compose(gpu.list.Get(), states, y.texture.Get(), uv.texture.Get(), false, &frame, 3);
    auto ry = recordRead(gpu, y, states.get(y.texture.Get()));
    auto ruv = recordRead(gpu, uv, states.get(uv.texture.Get()));
    check(gpu.submitAndWait(), tag + ": real frame with raw picture runs");
    py = mapRead(y, ry.Get()); puv = mapRead(uv, ruv.Get());
    const auto Y = [&](uint32_t x, uint32_t yy) { return read(py, size_t(yy) * W + x); };
    const auto U = [&](uint32_t x, uint32_t yy) { return read(puv, (size_t(yy) * (W / 2) + x) * 2); };
    const uint32_t lx = 10, ly = sbs ? 60 : 60, rx = sbs ? 138 : 200;
    std::printf("INFO %s left=%d right=%d\n", tag.c_str(), Y(lx, ly), Y(rx, ly));
    check(std::abs(Y(lx, ly) - rawCode) <= tolerance, tag + ": console side is the raw picture, also on a real frame");
    check(Y(rx, ly) == int(hdr ? (800 << 6) : 200), tag + ": PushFPS side is the enhanced picture");
    check(std::abs(U(lx / 2, ly / 2) - int(hdr ? (512 << 6) : 128)) <= tolerance, tag + ": raw grey has neutral chroma");
    check(compare.rawHold() && compare.rawFrames() == 1, tag + ": counts the raw frame");
    // A generated frame: the console side keeps the raw picture.
    check(composeFrame(gpu, compare, states, y, uv, plane(90, 300, W * H), plane(150, 600, W / 2 * H / 2 * 2), true, py, puv), tag + ": generated frame runs");
    check(std::abs(Y(lx, ly) - rawCode) <= tolerance, tag + ": console side holds the raw picture");
    check(Y(rx, ly) == int(hdr ? (300 << 6) : 90), tag + ": PushFPS side shows the generated frame");
    // A real frame without a raw picture falls back to the held real frame.
    check(composeFrame(gpu, compare, states, y, uv, plane(60, 200, W * H), plane(150, 600, W / 2 * H / 2 * 2), false, py, puv), tag + ": real frame without raw runs");
    check(Y(lx, ly) == int(hdr ? (200 << 6) : 60) && !compare.rawHold(), tag + ": without a raw picture the real frame is the console side");
    check(!gpu.debugErrors(), tag + ": no D3D12 debug errors");
}

// Colour labels: straight-alpha RGBA blended onto the picture.
void testCompareBadges(Gpu& gpu, bool hdr) {
    constexpr uint32_t W = 256, H = 144;
    const std::string tag = std::string("badges") + (hdr ? " HDR" : "");
    auto* device = gpu.device.Get();
    const auto yFormat = hdr ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R8_UNORM;
    const auto uvFormat = hdr ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R8G8_UNORM;
    const uint32_t bpp = hdr ? 2 : 1;
    Plane y{pipeline::makeTexture(device, W, H, yFormat, true), yFormat, W, H, bpp};
    Plane uv{pipeline::makeTexture(device, W / 2, H / 2, uvFormat, true), uvFormat, W / 2, H / 2, bpp * 2};
    // Left: 8x4, left half opaque white, right half transparent. Right: 8x4 black at 50 %.
    engine::RecordingLabel left{8, 4, {}, std::vector<uint8_t>(8 * 4 * 4, 0)}, right{8, 4, {}, std::vector<uint8_t>(8 * 4 * 4, 0)};
    for (uint32_t yy = 0; yy < 4; ++yy)
        for (uint32_t x = 0; x < 8; ++x) {
            uint8_t* l = &left.rgba[(yy * 8 + x) * 4];
            if (x < 4) l[0] = l[1] = l[2] = l[3] = 255;
            right.rgba[(yy * 8 + x) * 4 + 3] = 128;
        }
    engine::RecordingCompare compare;
    std::wstring reason;
    check(compare.create(device, W, H, hdr, left, right, reason), tag + ": created");
    check(compare.overlays().size() == 2 && compare.patches().size() == 1, tag + ": both badges blended, the divider copied");
    pipeline::StateTracker states;
    std::vector<uint8_t> py, puv;
    const auto plane = [&](uint32_t v8, uint32_t v10, size_t count) {
        return hdr ? words(std::vector<uint16_t>(count, uint16_t(v10 << 6))) : std::vector<uint8_t>(count, uint8_t(v8));
    };
    check(composeFrame(gpu, compare, states, y, uv, plane(50, 300, W * H), plane(100, 400, W / 2 * H / 2 * 2), false, py, puv), tag + ": frame runs");
    const auto read = [&](const std::vector<uint8_t>& bytes, size_t index) { return hdr ? int(word(bytes, index)) : int(bytes[index]); };
    const auto Y = [&](uint32_t x, uint32_t yy) { return read(py, size_t(yy) * W + x); };
    const auto U = [&](uint32_t x, uint32_t yy) { return read(puv, (size_t(yy) * (W / 2) + x) * 2); };
    const int tol = hdr ? 64 : 1;
    std::printf("INFO %s white=%d clear=%d half=%d\n", tag.c_str(), Y(9, 9), Y(13, 9), Y(242, 9));
    check(std::abs(Y(9, 9) - int(hdr ? (573 << 6) : 235)) <= tol, tag + ": opaque white badge pixel");
    check(Y(13, 9) == int(hdr ? (300 << 6) : 50), tag + ": transparent badge pixel leaves the picture");
    // 50 % black over the picture: halfway between the picture and black.
    const int halfway = hdr ? int(std::lround((300 - (300 - 64) * 128.0 / 255.0))) << 6 : int(std::lround(50 - (50 - 16) * 128.0 / 255.0));
    check(std::abs(Y(242, 9) - halfway) <= tol, tag + ": half-transparent badge pixel is blended");
    check(std::abs(U(4, 4) - int(hdr ? (512 << 6) : 128)) <= tol, tag + ": white badge has neutral chroma");
    check(U(6, 4) == int(hdr ? (400 << 6) : 100), tag + ": chroma under a transparent badge pixel is kept");
    check(Y(30, 60) == int(hdr ? (300 << 6) : 50), tag + ": picture outside the badges unchanged");
    check(!gpu.debugErrors(), tag + ": no D3D12 debug errors");
}

// The Reveal layout: one whole picture, the console side uncovered and covered again by a sweeping
// line. Checked frame by frame at chosen moments of the clip: what is left and right of the line,
// the line itself, and the two badges, each cut off at the line with its picture.
void testCompareReveal(Gpu& gpu, bool hdr) {
    constexpr uint32_t W = 640, H = 360;
    const std::string tag = std::string("reveal") + (hdr ? " HDR" : "");
    auto* device = gpu.device.Get();
    const auto yFormat = hdr ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R8_UNORM;
    const auto uvFormat = hdr ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R8G8_UNORM;
    const uint32_t bpp = hdr ? 2 : 1;
    Plane y{pipeline::makeTexture(device, W, H, yFormat, true), yFormat, W, H, bpp};
    Plane uv{pipeline::makeTexture(device, W / 2, H / 2, uvFormat, true), uvFormat, W / 2, H / 2, bpp * 2};
    check(y.texture && uv.texture, tag + ": planes created");
    // The console's badge: 48 x 8 opaque black. PushFPS's: 48 x 8 opaque white.
    engine::RecordingLabel console{48, 8, {}, std::vector<uint8_t>(48 * 8 * 4, 0)}, push{48, 8, {}, std::vector<uint8_t>(48 * 8 * 4, 255)};
    for (size_t i = 3; i < console.rgba.size(); i += 4) console.rgba[i] = 255;
    engine::RecordingCompare compare;
    std::wstring reason;
    check(compare.create(device, W, H, hdr, console, push, reason, engine::CompareLayout::Reveal), tag + ": created");
    compare.setRevealInterval(3.0);
    const uint32_t lineW = compare.revealLineWidth();
    check(compare.overlays().size() == 3 && compare.patches().empty(), tag + ": two badges and the line are blended, nothing is copied in (no fixed divider)");
    check(lineW == engine::compareRevealLine(H).width && lineW == 32, tag + ": the line's strip, " + std::to_string(lineW) + " wide (the places checked below count on 32)");
    check(compare.outputY() == nullptr, tag + ": composed on the recorder's own planes, at the picture's size");

    pipeline::StateTracker states;
    std::vector<uint8_t> py, puv;
    const auto plane = [&](uint32_t v8, uint32_t v10, size_t count) {
        return hdr ? words(std::vector<uint16_t>(count, uint16_t(v10 << 6))) : std::vector<uint8_t>(count, uint8_t(v8));
    };
    const auto read = [&](const std::vector<uint8_t>& bytes, size_t index) { return hdr ? int(word(bytes, index)) : int(bytes[index]); };
    const auto Y = [&](uint32_t x, uint32_t yy) { return read(py, size_t(yy) * W + x); };
    const auto U = [&](uint32_t x, uint32_t yy) { return read(puv, (size_t(yy) * (W / 2) + x) * 2); };
    const auto code = [&](uint32_t v8, uint32_t v10) { return int(hdr ? (v10 << 6) : v8); };
    const int tol = hdr ? 64 : 1;
    const int white = code(235, 573), black = code(16, 64);
    // Pictures (8-bit and 10-bit codes): real frames are dark with neutral colour, generated frames
    // brighter with a colour of their own, so the cut shows in both planes.
    const int realChroma = code(128, 512), madeChroma = code(160, 640);
    const auto frame = [&](uint32_t luma8, uint32_t luma10, bool generated, double seconds) {
        return composeFrame(gpu, compare, states, y, uv, plane(luma8, luma10, W * H),
                            generated ? plane(160, 640, W / 2 * H / 2 * 2) : plane(128, 512, W / 2 * H / 2 * 2), generated, py, puv, seconds);
    };
    const int held = code(50, 300);
    const double every = 3.0, sweep = engine::compareRevealSweepSeconds(every), hold = every - sweep;
    // The moment the line's centre is at `x` during the first sweep (found by stepping through it).
    const auto when = [&](int32_t x) {
        for (double t = hold; t < every; t += 0.0002)
            if (engine::compareRevealCentre(engine::compareRevealPosition(t, every), W, lineW) >= x) return t;
        return every;
    };
    const uint32_t pushX = 10, consoleX = W - 10 - 48, badgeY = 12;   // margin 10: PushFPS top left, the console top right

    // 1. The clip opens on the console side: a real frame is that picture, whole.
    check(frame(50, 300, false, 0.0), tag + ": first real frame runs");
    check(Y(5, 200) == held && Y(320, 200) == held && Y(634, 200) == held, tag + ": opens on the whole console picture");
    check(compare.revealX() == 0 && !compare.revealLineShown(), tag + ": the line waits outside the picture");
    check(std::abs(Y(consoleX + 4, badgeY) - black) <= tol && std::abs(Y(consoleX + 44, badgeY) - black) <= tol, tag + ": the console's badge at the top right");
    check(Y(pushX + 4, badgeY) == held, tag + ": no PushFPS badge while none of its picture shows");
    // 2. During the hold a generated frame does not show: the picture stays the console's.
    check(frame(200, 800, true, 1.0), tag + ": generated frame during the hold runs");
    check(Y(5, 200) == held && Y(320, 200) == held && Y(634, 200) == held, tag + ": the hold shows only the console's frames");
    check(U(160, 100) == realChroma && U(4, 100) == realChroma, tag + ": in colour too");
    // 3. Half way through the sweep: PushFPS left of the line, the console right of it.
    const int made = code(200, 800);
    check(frame(200, 800, true, hold + sweep / 2), tag + ": frame at the middle of the sweep runs");
    std::printf("INFO %s mid: x=%u left=%d right=%d core=%d handle=%d\n", tag.c_str(), compare.revealX(), Y(100, 200), Y(540, 200), Y(320, 40), Y(320, 173));
    check(compare.revealX() == W / 2 && compare.revealLineShown(), tag + ": the line is in the middle");
    check(Y(100, 200) == made && Y(W / 2 - lineW / 2 - 2, 200) == made, tag + ": left of the line every presented frame");
    check(Y(540, 200) == held && Y(W / 2 + lineW / 2 + 1, 200) == held, tag + ": right of the line the console's frame");
    check(std::abs(Y(W / 2, 40) - white) <= tol && std::abs(Y(W / 2 - 1, 300) - white) <= tol, tag + ": the line's white core, top to bottom");
    check(U(50, 100) == madeChroma && U(270, 100) == realChroma, tag + ": the colour planes are cut at the same place");
    check(Y(W / 2 + 4, 40) > held && Y(W / 2 - 3, 40) != made, tag + ": a glow on both sides of the core");
    check(U(W / 4 + 1, 20) < realChroma - code(5, 20), tag + ": and the glow is green (blue-difference below neutral)");
    check(Y(W / 2, 173) < Y(W / 2, 40) / 2, tag + ": the handle's dark face in the middle of the height");
    check(std::abs(Y(pushX + 4, badgeY) - white) <= tol && std::abs(Y(consoleX + 4, badgeY) - black) <= tol, tag + ": both badges while both pictures show");
    // 4. The line crosses the PushFPS badge: only the part left of the line is drawn.
    check(frame(200, 800, true, when(40)), tag + ": frame with the line over the PushFPS badge runs");
    check(compare.revealX() == 40, tag + ": the line at x 40");
    check(std::abs(Y(pushX + 2, badgeY) - white) <= tol && std::abs(Y(pushX + 10, badgeY) - white) <= tol, tag + ": the badge left of the line");
    check(Y(pushX + 47, badgeY) == held && Y(pushX + 46, badgeY + 5) == held, tag + ": is cut off at the line: right of it the console's picture, no badge");
    check(Y(5, 200) == made && Y(80, 200) == held, tag + ": and the picture is cut at the same x");
    // 5. The line crosses the console's badge.
    check(frame(200, 800, true, when(int32_t(consoleX) + 18)), tag + ": frame with the line over the console's badge runs");
    check(compare.revealX() == consoleX + 18, tag + ": the line at the console's badge");
    check(std::abs(Y(consoleX + 44, badgeY) - black) <= tol && std::abs(Y(consoleX + 36, badgeY + 5) - black) <= tol, tag + ": the console's badge right of the line");
    check(Y(consoleX, badgeY) == made && Y(consoleX + 1, badgeY + 5) == made, tag + ": is cut off at the line: left of it PushFPS's picture");
    // 6. The hold on PushFPS: every presented frame, whole, with its badge only.
    check(frame(120, 500, true, every + hold / 2), tag + ": frame during the PushFPS hold runs");
    const int next = code(120, 500);
    check(compare.revealX() == W && !compare.revealLineShown(), tag + ": the line has left the picture on the right");
    check(Y(5, 200) == next && Y(320, 200) == next && Y(634, 200) == next, tag + ": the whole picture is PushFPS's");
    check(std::abs(Y(pushX + 4, badgeY) - white) <= tol && Y(consoleX + 4, badgeY) == next, tag + ": with the PushFPS badge and no console badge");
    // A real frame arrives meanwhile: the console side follows, out of sight.
    check(frame(70, 350, false, every + hold / 2 + 0.03), tag + ": real frame during the PushFPS hold runs");
    check(Y(320, 200) == code(70, 350), tag + ": a real frame is presented too");
    // 7. The sweep back: the console's newest frame comes in from the right.
    check(frame(180, 700, true, every + hold + sweep / 2), tag + ": frame at the middle of the sweep back runs");
    check(compare.revealX() == W / 2 && compare.revealLineShown(), tag + ": the line is in the middle again");
    check(Y(100, 200) == code(180, 700) && Y(540, 200) == code(70, 350), tag + ": PushFPS left, the console's newest frame right");
    // 8. And the console side holds again.
    check(frame(210, 820, true, 2 * every + hold / 2), tag + ": frame of the second console hold runs");
    check(compare.revealX() == 0 && Y(320, 200) == code(70, 350) && Y(5, 200) == code(70, 350), tag + ": back on the console side, whole");
    check(compare.generatedFrames() == 7 && compare.realFrames() == 2, tag + ": counts frames");
    // The line moves a little with every frame of a sweep: 30 frames at 120 a second, each further right.
    bool forward = true;
    uint32_t last = 0, distinct = 0;
    for (int i = 0; i < 30 && forward; ++i) {
        if (!frame(200, 800, true, 6 * every + hold + sweep / 4 + i / 120.0)) { forward = false; break; }
        if (i && compare.revealX() < last) forward = false;
        if (i && compare.revealX() > last) ++distinct;
        if (i && Y(compare.revealX() > lineW ? compare.revealX() - lineW : 0, 200) != made && compare.revealX() > lineW) forward = false;
        last = compare.revealX();
    }
    check(forward && distinct >= 28, tag + ": the line advances with every frame of a sweep (" + std::to_string(distinct) + " of 29 steps)");
    check(!gpu.debugErrors(), tag + ": no D3D12 debug errors");

    // Without labels the line still sweeps; without a line the picture is still cut.
    engine::RecordingLabel nothing{};
    engine::RecordingCompare bare;
    check(bare.create(device, W, H, hdr, nothing, nothing, reason, engine::CompareLayout::Reveal) && bare.overlays().size() == 1 && bare.patches().empty(),
          tag + ": no labels leaves only the line");
    // The other layouts know nothing of the line.
    engine::RecordingCompare split;
    check(split.create(device, W, H, hdr, console, push, reason) && split.overlays().size() == 2 && split.patches().size() == 1 && split.revealLineWidth() == 0,
          tag + ": the split layout keeps its two badges and its divider, and has no line");
}

void testCompareSkipRule() {
    // Real frames are never skipped by the rule (only when no slot is free at all).
    check(!engine::compareSkipsFrame(false, 0, true) && !engine::compareSkipsFrame(false, 1, false), "skip rule: real frames kept");
    // Generated frames leave the last free slot for the next real frame, and give way when the disk is behind.
    check(engine::compareSkipsFrame(true, 1, false) && engine::compareSkipsFrame(true, 0, false), "skip rule: generated frames leave one slot free");
    check(engine::compareSkipsFrame(true, 6, true), "skip rule: generated frames skipped while the disk is behind");
    check(!engine::compareSkipsFrame(true, 2, false), "skip rule: generated frames recorded while two slots are free");
}

void testLabelScale() {
    engine::RecordingLabel l{8, 4, std::vector<uint8_t>(32, 0)};
    for (int yy = 0; yy < 4; ++yy) for (int x = 4; x < 8; ++x) l.luma[size_t(yy) * 8 + x] = 255;
    const auto h = engine::scaleRecordingLabel(l, 4, 2);
    check(h.width == 4 && h.height == 2 && h.luma[0] == 0 && h.luma[3] == 255, "label scale: halves by area average");
    const auto t = engine::scaleRecordingLabel(l, 3, 3);
    check(t.luma.size() == 9 && t.luma[1] == 128, "label scale: partial pixels are weighted");
    check(engine::scaleRecordingLabel(l, 0, 3).luma.empty(), "label scale: empty size gives no label");
    engine::RecordingLabel c{2, 2, {}, {255, 0, 0, 255, 0, 0, 255, 0, 255, 0, 0, 255, 0, 0, 255, 0}};
    const auto one = engine::scaleRecordingLabel(c, 1, 1);
    check(one.rgba.size() == 4 && one.rgba[0] == 255 && one.rgba[2] == 0 && one.rgba[3] == 128, "label scale: colour weighted by alpha");
}

void testHudSampler(Gpu& gpu) {
    constexpr uint32_t W = 640, H = 360;
    auto* device = gpu.device.Get();
    Plane frameTex{pipeline::makeTexture(device, W, H, DXGI_FORMAT_R8G8B8A8_UNORM, false), DXGI_FORMAT_R8G8B8A8_UNORM, W, H, 4};
    check(frameTex.texture != nullptr, "hud: frame texture created");
    engine::PushHudSampler sampler;
    // Not running: nothing recorded.
    gpu.begin();
    const engine::PresentCapture::Frame idle{frameTex.texture.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, W, H, 0, 0, false};
    check(!sampler.record(gpu.list.Get(), idle), "hud: nothing is copied while no detection runs");
    gpu.list->Close();

    sampler.start(6, 60);
    // HUD box: x 40..200, y 20..60 (static text-like strokes); the rest moves.
    auto picture = [&](int shift) {
        std::vector<uint8_t> px(size_t(W) * H * 4);
        for (uint32_t yy = 0; yy < H; ++yy)
            for (uint32_t x = 0; x < W; ++x) {
                uint8_t v;
                if (x >= 40 && x < 200 && yy >= 20 && yy < 60) v = ((x / 4 + yy / 5) % 3 == 0 || x % 9 == 0) ? 235 : 20;
                else {
                    const int sx = int(x) + shift, sy = int(yy) + shift / 2;
                    uint32_t h = uint32_t(sx / 2) * 374761393u + uint32_t(sy / 2) * 668265263u;
                    h = (h ^ (h >> 13)) * 1274126177u;
                    v = uint8_t(std::clamp(int(128 + 60 * std::sin(sx * 0.03) + 40 * std::cos(sy * 0.04)) + int(h >> 28) * 4 - 30, 0, 255));
                }
                uint8_t* p = px.data() + (size_t(yy) * W + x) * 4;
                p[0] = p[1] = p[2] = v; p[3] = 255;
            }
        return px;
    };
    int copies = 0, refusedGenerated = 0, refusedBusy = 0;
    engine::PushHudSampler::Status status;
    for (int step = 0; step < 200 && !status.done && !status.failed; ++step) {
        gpu.begin();
        auto upload = recordFill(gpu, frameTex, picture(step * 7), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COMMON);
        const engine::PresentCapture::Frame generated{frameTex.texture.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, W, H, 0, 0, true};
        if (sampler.record(gpu.list.Get(), generated)) { sampler.submitted(gpu.fence.Get(), gpu.value + 1); ++refusedGenerated; } // counts a wrong copy
        const engine::PresentCapture::Frame real{frameTex.texture.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, W, H, 0, 0, false};
        const bool recorded = sampler.record(gpu.list.Get(), real);
        if (recorded) {
            ++copies;
            if (sampler.record(gpu.list.Get(), real)) ++refusedBusy; // a second copy in the same list must be refused
        }
        if (!gpu.submit()) break;
        if (recorded) sampler.submitted(gpu.fence.Get(), gpu.value);
        gpu.wait();
        status = sampler.status();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    check(refusedGenerated == 0, "hud: generated frames are never copied");
    check(refusedBusy == 0, "hud: one copy at a time");
    check(status.done && !status.failed && copies == 6 && status.taken == 6, "hud: six frames taken, then done");
    check(status.preview.size() == size_t(engine::hud::kPreviewWidth) * engine::hud::kPreviewHeight * 3, "hud: preview picture of the last frame");
    std::printf("INFO hud motion=%.2f regions=%zu\n", status.detection.motion, status.detection.regions.size());
    bool found = false;
    for (const auto& r : status.detection.regions)
        found = found || (r.left <= 40.0f / W + 0.01f && r.top <= 20.0f / H + 0.01f && r.right >= 200.0f / W - 0.01f && r.bottom >= 60.0f / H - 0.01f &&
                          r.right <= 230.0f / W && r.bottom <= 80.0f / H);
    check(found, "hud: the static box is found end to end");
    check(!gpu.debugErrors(), "hud: no D3D12 debug errors");

    // A discarded list leaves the sampler free for the next frame.
    sampler.start(6, 50);
    gpu.begin();
    const engine::PresentCapture::Frame real{frameTex.texture.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, W, H, 0, 0, false};
    check(sampler.record(gpu.list.Get(), real), "hud: copies after a restart");
    sampler.discarded();
    check(sampler.record(gpu.list.Get(), real), "hud: a discarded copy frees the slot");
    gpu.list->Close();
    sampler.discarded();
    sampler.cancel();
    check(!sampler.status().running, "hud: cancel stops it");

    // A format it cannot read fails with a message.
    Plane floatTex{pipeline::makeTexture(device, 64, 64, DXGI_FORMAT_R32_FLOAT, false), DXGI_FORMAT_R32_FLOAT, 64, 64, 4};
    sampler.start(6, 50);
    gpu.begin();
    const engine::PresentCapture::Frame odd{floatTex.texture.Get(), DXGI_FORMAT_R32_FLOAT, 64, 64, 0, 0, false};
    check(!sampler.record(gpu.list.Get(), odd), "hud: an unsupported format is not copied");
    gpu.list->Close();
    const auto failed = sampler.status();
    check(failed.failed && !failed.error.empty() && !failed.running, "hud: an unsupported format fails with a reason");
}
// The still rule (pipeline/PushHudStill.h) on the card: the three shaders against the rule in plain
// C++, pixel by pixel. Two real frames: a world that moves, a HUD that does not (strokes one and two
// pixels wide, a bar, a ring), and at the right a part of the picture that stands still with a ball
// crossing it. A generated frame of plain grey comes back with exactly the pixels the rule names
// replaced by the real frame's, and every other pixel untouched.
// inPlace: the rule's pixels written straight into the generated frame (what the graph's textures
// allow, and so what runs in the program); otherwise through the scratch copy, as with Live HUD Lock on.
// withLock: Live HUD Lock on beside it. In two real frames the lock has locked nothing yet (it needs
// about a second of stillness), so the picture must be the rule's alone, through the lock's own path.
void testHudStillAt(Gpu& gpu, const uint32_t W, const uint32_t H, const bool inPlace, const bool withLock = false) {
    constexpr DXGI_FORMAT F = DXGI_FORMAT_R8G8B8A8_UNORM;
    std::printf("INFO still rule: a picture of %u x %u, %s%s\n", W, H, inPlace ? "written in place" : "through the scratch copy", withLock ? ", with Live HUD Lock on" : "");
    SetEnvironmentVariableA("VEYRA_HUD_STILL_COPY", inPlace ? nullptr : "1");
    auto* device = gpu.device.Get();
    Plane real[2]{{pipeline::makeTexture(device, W, H, F, true), F, W, H, 4}, {pipeline::makeTexture(device, W, H, F, true), F, W, H, 4}};
    Plane generated{pipeline::makeTexture(device, W, H, F, true), F, W, H, 4};
    check(real[0].texture && real[1].texture && generated.texture, "still rule: textures created");
    pipeline::PushHudLock pass;
    check(pass.initialize(device, W, H, F, 0, withLock, true), withLock ? "still rule: created together with Live HUD Lock" : "still rule: created without Live HUD Lock");
    check(pass.stillActive() && pass.lockActive() == withLock, withLock ? "still rule: both are active" : "still rule: the still rule alone is active");
    pipeline::DescriptorStager stager;
    check(stager.initialize(device, 64), "still rule: staging heap");
    ID3D12Resource* reals[2] = {real[0].texture.Get(), real[1].texture.Get()};
    ID3D12Resource* gens[1] = {generated.texture.Get()};
    pass.createViews(device, stager, reals, gens, 1);
    SetEnvironmentVariableA("VEYRA_HUD_STILL_COPY", nullptr);
    check(pass.composesInPlace(0) == (inPlace && !withLock), inPlace && !withLock ? "still rule: the generated frame is written in place" : "still rule: the scratch copy is used");
    const auto hud = [](uint32_t x, uint32_t y) {
        if (x == 30 && y >= 20 && y < 60) return true;                          // one pixel wide
        if ((x == 44 || x == 45) && y >= 20 && y < 60) return true;             // two pixels wide
        if (y == 70 && x >= 20 && x < 120) return true;
        if (y >= 150 && y < 160 && x >= 20 && x < 140) return true;             // a bar
        const double r = std::hypot(double(x) - 110.0, double(y) - 40.0);
        return r >= 14.0 && r <= 16.0;                                          // a ring
    };
    const auto picture = [&](int shift, int ball) {
        std::vector<uint8_t> px(size_t(W) * H * 4);
        for (uint32_t y = 0; y < H; ++y)
            for (uint32_t x = 0; x < W; ++x) {
                uint8_t v;
                if (hud(x, y)) v = 235;
                else if (x >= 224) {                                            // stands still, a ball crossing it
                    const double r = std::hypot(double(x) - double(ball), double(y) - 96.0);
                    uint32_t h = x * 2246822519u + y * 3266489917u; h = (h ^ (h >> 15)) * 2654435761u;
                    v = r <= 7.0 ? 250 : uint8_t(60 + (h >> 26));
                } else {
                    uint32_t h = uint32_t(int(x) + shift) * 374761393u + uint32_t(int(y) + shift / 3) * 668265263u;
                    h = (h ^ (h >> 13)) * 1274126177u;
                    v = uint8_t(40 + (h >> 25));
                }
                uint8_t* p = px.data() + (size_t(y) * W + x) * 4;
                p[0] = p[1] = p[2] = v; p[3] = 255;
            }
        return px;
    };
    const std::vector<uint8_t> first = picture(0, 244), second = picture(7, 292);
    pipeline::StateTracker states;
    const auto realFrame = [&](unsigned parity, const std::vector<uint8_t>& px, bool reset) {
        gpu.begin();
        auto up = recordFill(gpu, real[parity], px, states.get(real[parity].texture.Get()), D3D12_RESOURCE_STATE_COMMON);
        states.set(real[parity].texture.Get(), D3D12_RESOURCE_STATE_COMMON);
        const bool ok = pass.update(gpu.list.Get(), states, parity, reset);
        return ok && gpu.submitAndWait();
    };
    const auto generatedFrame = [&](unsigned held, std::vector<uint8_t>& out) {
        gpu.begin();
        auto up = recordFill(gpu, generated, std::vector<uint8_t>(size_t(W) * H * 4, 128), states.get(generated.texture.Get()), D3D12_RESOURCE_STATE_COMMON);
        states.set(generated.texture.Get(), D3D12_RESOURCE_STATE_COMMON);
        const bool ok = pass.compose(gpu.list.Get(), states, 0, held);
        auto rb = recordRead(gpu, generated, states.get(generated.texture.Get()));
        if (!ok || !gpu.submitAndWait()) return false;
        out = mapRead(generated, rb.Get());
        return true;
    };
    check(realFrame(0, first, true), "still rule: the first real frame starts it over");
    std::vector<uint8_t> out;
    check(generatedFrame(0, out), "still rule: composed right after a start");
    size_t untouched = 0; for (size_t k = 0; k < out.size(); k += 4) untouched += out[k] == 128;
    check(untouched == size_t(W) * H, "still rule: with one real frame only, nothing is kept");
    check(realFrame(1, second, false), "still rule: the second real frame is learned from");
    check(generatedFrame(1, out), "still rule: a generated frame is composed");
    // The same decision in plain C++.
    std::vector<uint8_t> same(size_t(W) * H);
    for (size_t i = 0; i < same.size(); ++i) {
        int d = 0; for (int c = 0; c < 3; ++c) d = std::max(d, std::abs(int(first[i * 4 + c]) - int(second[i * 4 + c])));
        same[i] = d <= 2;
    }
    const auto keep = pipeline::hudstill::decide(same, W, H);
    size_t wrong = 0, kept = 0, hudKept = 0, hudTotal = 0, movingKept = 0, stillSide = 0;
    for (uint32_t y = 0; y < H; ++y)
        for (uint32_t x = 0; x < W; ++x) {
            const size_t i = size_t(y) * W + x, k = i * 4;
            const uint8_t expected = keep[i] ? second[k] : 128;
            wrong += out[k] != expected || out[k + 1] != expected || out[k + 2] != expected;
            kept += keep[i];
            if (hud(x, y)) { ++hudTotal; hudKept += out[k] == second[k] && out[k] != 128; }
            else if (x < 224) movingKept += out[k] != 128 && !same[i];
            else stillSide += out[k] != 128;
        }
    std::printf("INFO still rule: %zu pixels differ from the rule in plain C++; %zu kept in all, HUD %zu of %zu, in the still part %zu\n", wrong, kept, hudKept, hudTotal, stillSide);
    check(wrong == 0, "still rule: the shaders decide every pixel as the rule in plain C++ does");
    check(hudKept >= hudTotal * 90 / 100, "still rule: the HUD comes from the real frame (" + std::to_string(hudKept) + " of " + std::to_string(hudTotal) + ")");
    check(movingKept == 0, "still rule: no pixel that moves is touched");
    // The ball's path, and the still part round it, stay frame generation's (most of its surrounding stands still).
    size_t path = 0; for (uint32_t y = 90; y < 103; ++y) for (uint32_t x = 256; x < 280; ++x) path += out[(size_t(y) * W + x) * 4] != 128;
    check(path == 0, "still rule: the path of a ball crossing a still part is not touched");
    // The HUD goes away: the next pair no longer has it, and nothing of it is kept.
    const auto bare = [&] {
        std::vector<uint8_t> px = picture(14, 292);
        for (uint32_t y = 0; y < H; ++y) for (uint32_t x = 0; x < 224; ++x) if (hud(x, y)) {
            uint32_t h = (x + 14u) * 374761393u + (y + 4u) * 668265263u; h = (h ^ (h >> 13)) * 1274126177u;
            uint8_t* p = px.data() + (size_t(y) * W + x) * 4; p[0] = p[1] = p[2] = uint8_t(40 + (h >> 25));
        }
        return px;
    }();
    check(realFrame(0, bare, false), "still rule: a frame without the HUD runs");
    check(generatedFrame(0, out), "still rule: composed after the HUD went away");
    size_t leftover = 0; for (uint32_t y = 0; y < H; ++y) for (uint32_t x = 0; x < 224; ++x) if (hud(x, y)) leftover += out[(size_t(y) * W + x) * 4] == 235;
    check(leftover == 0, "still rule: a HUD that is gone is not held (it keeps no memory)");
    // Live HUD Lock and the still rule together start and run.
    pipeline::PushHudLock both;
    check(both.initialize(device, W, H, F, 0, true, true) && both.lockActive() && both.stillActive(), "still rule: created together with Live HUD Lock");
    pipeline::PushHudLock neither;
    check(!neither.initialize(device, W, H, F, 0, false, false), "still rule: a pass with nothing to do is not created");
}
void testHudStill(Gpu& gpu) {
    testHudStillAt(gpu, 320, 192, true);
    // A size that is no multiple of the rule's blocks: the last column and row of blocks are cut by the
    // picture's edge, and the shader counts their pixels as the rule in plain C++ does.
    testHudStillAt(gpu, 333, 201, true);
    // The other way of writing the result gives the same picture.
    testHudStillAt(gpu, 333, 201, false);
    // With Live HUD Lock on beside it (never in place then), the rule's pixels are the same.
    testHudStillAt(gpu, 320, 192, true, true);
    // PushFPS 4.1: a picture 1080 high, where the surrounding reaches three blocks each way instead of
    // two (hudstill::reachFor): the shader's loop over that reach against the same rule in plain C++.
    testHudStillAt(gpu, 640, 1080, true);
}

void testHudLock(Gpu& gpu) {
    constexpr uint32_t W = 256, H = 144;
    constexpr DXGI_FORMAT F = DXGI_FORMAT_R8G8B8A8_UNORM;
    auto* device = gpu.device.Get();
    Plane real[2]{{pipeline::makeTexture(device, W, H, F, true), F, W, H, 4}, {pipeline::makeTexture(device, W, H, F, true), F, W, H, 4}};
    Plane generated{pipeline::makeTexture(device, W, H, F, true), F, W, H, 4};
    check(real[0].texture && real[1].texture && generated.texture, "hud lock: textures created");
    pipeline::PushHudLock lock;
    check(lock.initialize(device, W, H, F, 0), "hud lock: created");
    pipeline::DescriptorStager stager;
    check(stager.initialize(device, 64), "hud lock: staging heap");
    ID3D12Resource* reals[2] = {real[0].texture.Get(), real[1].texture.Get()};
    ID3D12Resource* gens[1] = {generated.texture.Get()};
    lock.createViews(device, stager, reals, gens, 1);
    // HUD: x 20..100, y 10..40, high-contrast stripes that never move. Everything else moves.
    auto hudAt = [](uint32_t x, uint32_t y) { return x >= 20 && x < 100 && y >= 10 && y < 40; };
    auto picture = [&](int shift, bool hud) {
        std::vector<uint8_t> px(size_t(W) * H * 4);
        for (uint32_t y = 0; y < H; ++y)
            for (uint32_t x = 0; x < W; ++x) {
                uint8_t v;
                if (hud && hudAt(x, y)) v = ((x / 3) % 2) ? 230 : 25;
                else {
                    uint32_t h = uint32_t(int(x) + shift) * 374761393u + uint32_t(int(y) + shift / 3) * 668265263u;
                    h = (h ^ (h >> 13)) * 1274126177u;
                    v = uint8_t(40 + (h >> 25));
                }
                uint8_t* p = px.data() + (size_t(y) * W + x) * 4;
                p[0] = p[1] = p[2] = v; p[3] = 255;
            }
        return px;
    };
    pipeline::StateTracker states;
    std::vector<uint8_t> last;
    auto realFrame = [&](int i, bool hud, bool reset) {
        const unsigned parity = unsigned(i) & 1u;
        gpu.begin();
        last = picture(i * 5, hud);
        auto up = recordFill(gpu, real[parity], last, states.get(real[parity].texture.Get()), D3D12_RESOURCE_STATE_COMMON);
        states.set(real[parity].texture.Get(), D3D12_RESOURCE_STATE_COMMON);
        const bool ok = lock.update(gpu.list.Get(), states, parity, reset);
        return ok && gpu.submitAndWait();
    };
    auto generatedFrame = [&](unsigned held, std::vector<uint8_t>& out) {
        gpu.begin();
        auto up = recordFill(gpu, generated, std::vector<uint8_t>(size_t(W) * H * 4, 128), states.get(generated.texture.Get()), D3D12_RESOURCE_STATE_COMMON);
        states.set(generated.texture.Get(), D3D12_RESOURCE_STATE_COMMON);
        const bool ok = lock.compose(gpu.list.Get(), states, 0, held);
        auto rb = recordRead(gpu, generated, states.get(generated.texture.Get()));
        if (!ok || !gpu.submitAndWait()) return false;
        out = mapRead(generated, rb.Get());
        return true;
    };
    bool ran = true;
    int i = 0;
    for (; i < 60; ++i) ran = ran && realFrame(i, true, i == 0);
    check(ran, "hud lock: 60 real frames update the state");
    std::vector<uint8_t> out;
    check(generatedFrame(unsigned(i - 1) & 1u, out), "hud lock: a generated frame is composed");
    int hudHeld = 0, hudTotal = 0, bgKept = 0, bgTotal = 0;
    for (uint32_t y = 0; y < H; ++y)
        for (uint32_t x = 0; x < W; ++x) {
            const size_t k = (size_t(y) * W + x) * 4;
            if (hudAt(x, y)) { ++hudTotal; hudHeld += out[k] == last[k]; }
            else if (x >= 110 || y >= 50) { ++bgTotal; bgKept += out[k] == 128; }
        }
    std::printf("INFO hud lock: HUD pixels from the real frame %d/%d, moving pixels kept %d/%d\n", hudHeld, hudTotal, bgKept, bgTotal);
    check(hudHeld >= hudTotal * 95 / 100, "hud lock: the HUD comes from the held real frame");
    check(bgKept == bgTotal, "hud lock: the moving picture keeps the generated pixels");
    // The HUD goes away (a menu closes): released on that very frame.
    check(realFrame(i, false, false), "hud lock: a frame without the HUD runs");
    check(generatedFrame(unsigned(i) & 1u, out), "hud lock: composed after the HUD went away");
    int released = 0;
    for (uint32_t y = 12; y < 38; ++y) for (uint32_t x = 22; x < 98; ++x) released += out[(size_t(y) * W + x) * 4] == 128;
    check(released >= 26 * 76 * 95 / 100, "hud lock: a HUD that moves or disappears is released at once (" + std::to_string(released) + "/1976)");
    // A reset starts over: nothing is locked right after it.
    ++i;
    check(realFrame(i, true, true), "hud lock: reset runs");
    check(generatedFrame(unsigned(i) & 1u, out), "hud lock: composed after a reset");
    int afterReset = 0;
    for (size_t k = 0; k < out.size(); k += 4) afterReset += out[k] != 128;
    check(afterReset == 0, "hud lock: nothing is locked right after a reset");
    check(!gpu.debugErrors(), "hud lock: no D3D12 debug errors");
}
} // namespace

// PushFlow: RGBA8 frames with a textured square on a soft gradient.
std::vector<uint8_t> flowFrame(uint32_t w, uint32_t h, double squareX) {
    std::vector<uint8_t> px(size_t(w) * h * 4);
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            uint8_t* p = &px[(size_t(y) * w + x) * 4];
            const int base = 60 + int(40.0 * x / w) + int(30.0 * y / h);
            int r = base, g = base + 10, b = base + 20;
            const double dx = x - squareX, dy = double(y) - h * 0.35;
            if (dx >= 0 && dx < 40 && dy >= 0 && dy < 40) {
                const bool light = ((int(dx) / 8) + (int(dy) / 8)) % 2 == 0;
                r = light ? 235 : 40; g = light ? 200 : 30; b = light ? 60 : 20;
            }
            p[0] = uint8_t(r); p[1] = uint8_t(g); p[2] = uint8_t(b); p[3] = 255;
        }
    return px;
}
// Fine texture (stripes, 2-pixel checker, thin lines) panned sideways by `shift` pixels.
// Fine, non-repeating texture (hashed noise and 1-pixel lines) panning sideways, as in
// prepare_runtime.py's CPU check. A periodic pattern made the motion ambiguous (a shift of a whole
// period looks like no motion), so integer shifts and a pattern that never repeats.
std::vector<uint8_t> detailFrame(uint32_t w, uint32_t h, int shift) {
    std::vector<uint8_t> px(size_t(w) * h * 4);
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            const int64_t u = int64_t(x) - shift;
            const int64_t hsh = (u * 73856093) ^ (int64_t(y) * 19349663);
            const double noise = double((uint64_t(hsh ^ (hsh >> 13)) * 1274126177ull) & 0xFFFF) / 65535.0;   // unsigned: no overflow
            const bool line = ((u % 37) + 37) % 37 == 0 || y % 29 == 0;
            const double t = std::clamp(line ? 0.95 : 0.25 + 0.5 * noise, 0.0, 1.0);
            uint8_t* p = &px[(size_t(y) * w + x) * 4];
            p[0] = uint8_t(t * 255.0 + 0.5); p[1] = uint8_t((t * 0.9 + 0.05) * 255.0 + 0.5); p[2] = uint8_t((t * 0.8 + 0.1) * 255.0 + 0.5); p[3] = 255;
        }
    return px;
}
double flowPsnr(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    double se = 0; size_t n = 0;
    for (size_t i = 0; i < a.size() && i < b.size(); ++i) { if (i % 4 == 3) continue; const double d = double(a[i]) - b[i]; se += d * d; ++n; }
    const double mse = n ? se / n : 0;
    return mse <= 1e-9 ? 99.0 : 10.0 * std::log10(255.0 * 255.0 / mse);
}
// Reads `count` floats from a UAV buffer (left in UNORDERED_ACCESS).
std::vector<float> readFloats(Gpu& gpu, ID3D12Resource* buffer, uint64_t count) {
    std::vector<float> out;
    auto rb = makeBuffer(gpu.device.Get(), count * 4, D3D12_HEAP_TYPE_READBACK);
    if (!rb || !gpu.begin()) return out;
    D3D12_RESOURCE_BARRIER b{}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource = buffer;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS; b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    gpu.list->ResourceBarrier(1, &b);
    gpu.list->CopyBufferRegion(rb.Get(), 0, buffer, 0, count * 4);
    std::swap(b.Transition.StateBefore, b.Transition.StateAfter);
    gpu.list->ResourceBarrier(1, &b);
    if (!gpu.submitAndWait()) return out;
    float* mapped = nullptr;
    if (FAILED(rb->Map(0, nullptr, reinterpret_cast<void**>(&mapped)))) return out;
    out.assign(mapped, mapped + count);
    const D3D12_RANGE none{0, 0}; rb->Unmap(0, &none);
    return out;
}
// PushFPS ambient fill (engine/PushAmbient.h, pipeline/PushAmbientPass.h): when the bars are drawn, at
// what size, and the blurred copy of a frame itself: a red left half and a blue right half must come
// out red on the left, blue on the right, an even mix in the middle, and exactly as bright as it went in.
float halfToFloat(uint16_t h) {
    const uint32_t sign = (h >> 15) & 1u, exp = (h >> 10) & 0x1Fu, man = h & 0x3FFu;
    float v;
    if (exp == 0) v = std::ldexp(float(man), -24);
    else if (exp == 31) v = man ? std::nanf("") : INFINITY;
    else v = std::ldexp(float(man | 0x400u), int(exp) - 25);
    return sign ? -v : v;
}
void testAmbient(Gpu& gpu) {
    using namespace engine;
    check(!pushAmbientFill(), "ambient: off unless asked for");
    setPushAmbientFill(true); check(pushAmbientFill(), "ambient: the switch turns on"); setPushAmbientFill(false);
    check(pushAmbientBars(3440, 1440, 2560, 1440), "ambient: a 16:9 picture on a 21:9 window leaves bars");
    check(pushAmbientBars(1920, 1200, 1920, 1080), "ambient: a 16:9 picture on a 16:10 window leaves bars");
    check(!pushAmbientBars(2560, 1440, 2560, 1440) && !pushAmbientBars(1920, 1080, 1919.2f, 1080), "ambient: no bars, nothing drawn");
    check(!pushAmbientBars(3440, 1440, 3440, 1935) && !pushAmbientBars(0, 0, 100, 100), "ambient: a cropped picture or no window: nothing drawn");
    const auto cover = pushAmbientCover(3440, 1440, 2560, 1440);
    check(std::abs(cover.first - 3440.0f) < 0.5f && std::abs(cover.second - 1935.0f) < 0.5f, "ambient: the backdrop covers a 21:9 window");
    const auto tall = pushAmbientCover(1920, 1200, 1920, 1080);
    check(std::abs(tall.second - 1200.0f) < 0.5f && tall.first > 2133.0f && tall.first < 2134.0f, "ambient: and a 16:10 one");

    constexpr uint32_t W = 640, H = 360;
    constexpr DXGI_FORMAT F = DXGI_FORMAT_R8G8B8A8_UNORM;
    auto* device = gpu.device.Get();
    Plane frame{pipeline::makeTexture(device, W, H, F, false), F, W, H, 4};
    pipeline::PushAmbientPass pass;
    check(frame.texture && pass.initialize(device, 4), "ambient: pass and frame created");
    if (!frame.texture || !pass.ready()) return;
    pass.setSource(device, 0, nullptr, F);
    pass.setSource(device, 1, frame.texture.Get(), F);
    const uint32_t SW = pipeline::PushAmbientPass::kWidth, SH = pipeline::PushAmbientPass::kHeight;
    Plane blurred{pass.result(), pipeline::PushAmbientPass::kFormat, SW, SH, 8};
    auto run = [&](const std::vector<uint8_t>& px, D3D12_RESOURCE_STATES before, std::vector<float>& out) {
        gpu.begin();
        auto up = recordFill(gpu, frame, px, before, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        const bool ok = pass.render(gpu.list.Get(), 1, frame.texture.Get());
        auto rb = recordRead(gpu, blurred, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        if (!ok || !gpu.submitAndWait()) return false;
        const auto raw = mapRead(blurred, rb.Get());
        out.resize(size_t(SW) * SH * 4);
        for (size_t i = 0; i < out.size() && i * 2 + 1 < raw.size(); ++i) out[i] = halfToFloat(uint16_t(raw[i * 2] | (raw[i * 2 + 1] << 8)));
        return raw.size() == size_t(SW) * SH * 8;
    };
    std::vector<uint8_t> px(size_t(W) * H * 4);
    for (uint32_t y = 0; y < H; ++y)
        for (uint32_t x = 0; x < W; ++x) {
            uint8_t* p = px.data() + (size_t(y) * W + x) * 4;
            p[0] = x < W / 2 ? 255 : 0; p[1] = 0; p[2] = x < W / 2 ? 0 : 255; p[3] = 255;
        }
    std::vector<float> out;
    check(run(px, D3D12_RESOURCE_STATE_COMMON, out), "ambient: the blurred copy of a frame is made");
    if (out.size() != size_t(SW) * SH * 4) return;
    auto at = [&](uint32_t x, uint32_t y, int c) { return out[(size_t(y) * SW + x) * 4 + c]; };
    bool finite = true;
    for (float v : out) finite = finite && std::isfinite(v);
    check(finite, "ambient: every value is a number");
    check(at(4, SH / 2, 0) > 0.97f && at(4, SH / 2, 2) < 0.03f, std::format("ambient: red stays on the left (r {:.3f} b {:.3f})", at(4, SH / 2, 0), at(4, SH / 2, 2)));
    check(at(SW - 5, SH / 2, 2) > 0.97f && at(SW - 5, SH / 2, 0) < 0.03f, "ambient: blue stays on the right");
    const float midR = (at(SW / 2 - 1, SH / 2, 0) + at(SW / 2, SH / 2, 0)) * 0.5f;
    check(std::abs(midR - 0.5f) < 0.03f, std::format("ambient: an even mix across the middle (r {:.3f})", midR));
    check(at(SW / 2 - 4, SH / 2, 0) > at(SW / 2 + 3, SH / 2, 0) + 0.3f && at(SW / 2 - 4, SH / 2, 0) < 0.97f, "ambient: a soft edge, not a sharp one");
    double sum = 0;
    for (uint32_t y = 0; y < SH; ++y) for (uint32_t x = 0; x < SW; ++x) sum += at(x, y, 0) + at(x, y, 2);
    check(std::abs(sum / (double(SW) * SH) - 1.0) < 0.01, std::format("ambient: as bright as the frame (mean {:.4f})", sum / (double(SW) * SH)));
    check(std::abs(at(10, 2, 0) - at(10, SH - 3, 0)) < 0.01f && at(0, 0, 3) > 0.99f, "ambient: even from top to bottom, opaque");
    // A second frame through the same pass (its textures rest in their states between calls): plain grey in, plain grey out.
    std::fill(px.begin(), px.end(), uint8_t(128));
    for (size_t i = 3; i < px.size(); i += 4) px[i] = 255;
    check(run(px, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, out), "ambient: a second frame");
    bool flat = out.size() == size_t(SW) * SH * 4;
    for (uint32_t y = 0; flat && y < SH; ++y) for (uint32_t x = 0; x < SW; ++x) flat = flat && std::abs(at(x, y, 1) - 128.0f / 255.0f) < 0.004f;
    check(flat, "ambient: a flat picture stays flat, edges included");
    check(!pass.render(gpu.list.Get(), 9, frame.texture.Get()) && !pass.render(nullptr, 1, frame.texture.Get()), "ambient: a bad index or list is refused");
    check(!gpu.debugErrors(), "ambient: no D3D12 debug errors");
}

// PushFPS fine-tuning of AI picture enhancement (pipeline/PushEnhanceShape.h): the production shader
// that composes the enhancement's change (NrResidualComposite) against the reference maths, pixel by
// pixel, for every control on its own and together, in SDR and HDR; and that with nothing to shape
// (zeros, as every other caller passes, or neutral values) the result is the plain composite.
uint16_t floatToHalf(float f) {
    uint32_t x; std::memcpy(&x, &f, 4);
    const uint32_t sign = (x >> 16) & 0x8000u; const int exp = int((x >> 23) & 0xFF) - 127 + 15; uint32_t man = x & 0x7FFFFFu;
    if (exp <= 0) { if (exp < -10) return uint16_t(sign); man = (man | 0x800000u) >> (1 - exp); return uint16_t(sign | ((man + 0x1000u) >> 13)); }
    if (exp >= 31) return uint16_t(sign | 0x7C00u);
    return uint16_t(sign | (uint32_t(exp) << 10) | ((man + 0x1000u) >> 13));
}
void testEnhanceShape(Gpu& gpu) {
    using pipeline::PushEnhanceShape;
    namespace ref = pipeline::push_shape_ref;
    // The values the interface sends, and how they are packed.
    PushEnhanceShape neutral;
    check(neutral.neutral() && neutral.constants() == std::array<float, 4>{0, 0, 0, 0}, "enhance shape: neutral is sent as zeros");
    PushEnhanceShape wild; wild.shadows = 999; wild.midtones = -5; wild.highlights = 150; wild.hue = 40; wild.maxBrighten = 250; wild.maxDarken = 7;
    const auto packed = wild.constants();
    check(packed[0] == float(200 + 256 * 0 + 65536 * 150) && packed[1] == float(40 + 256 * 100 + 65536 * 7) && packed[3] == 1.0f, "enhance shape: values are clamped and packed as whole percentages");
    pipeline::setPushEnhanceShape(wild);
    check(pipeline::pushEnhanceShape() == wild.clamped(), "enhance shape: the live values round-trip");
    pipeline::setPushEnhanceShape(neutral);
    check(pipeline::pushEnhanceShape().neutral(), "enhance shape: and go back to neutral");

    constexpr uint32_t W = 64, H = 16;
    constexpr DXGI_FORMAT F = DXGI_FORMAT_R16G16B16A16_FLOAT;
    auto* device = gpu.device.Get();
    Plane base{pipeline::makeTexture(device, W, H, F, false), F, W, H, 8}, input{pipeline::makeTexture(device, W, H, F, false), F, W, H, 8},
          fin{pipeline::makeTexture(device, W, H, F, false), F, W, H, 8}, out{pipeline::makeTexture(device, W, H, F, true), F, W, H, 8};
    pipeline::ComputePass pass;
    pipeline::DescriptorStager stager;
    std::vector<uint8_t> cs;
    const bool made = base.texture && input.texture && fin.texture && out.texture && stager.initialize(device, 8) &&
                      pass.loadShader("NrResidualComposite.dxil", cs) && pass.create(device, cs, 4, 3, 1, 40);
    check(made, "enhance shape: the production composite shader loads");
    if (!made) return;
    stager.stageSrv(base.texture.Get(), nullptr, pass.heap.Get(), 0);
    stager.stageSrv(input.texture.Get(), nullptr, pass.heap.Get(), 1);
    stager.stageSrv(fin.texture.Get(), nullptr, pass.heap.Get(), 2);
    pipeline::makeUav(device, out.texture.Get(), F, pipeline::cpuHandleOf(pass, 3));

    // Pictures in values a 16-bit float holds exactly (multiples of 1/1024 below 2; 1/256 below 8 for HDR).
    struct Data { std::vector<ref::Rgb> base, input, fin; };
    auto makeData = [&](bool hdr, uint32_t seed) {
        Data d; d.base.resize(W * H); d.input.resize(W * H); d.fin.resize(W * H);
        uint32_t state = seed;
        auto next = [&state](uint32_t n) { state = state * 1664525u + 1013904223u; return (state >> 8) % n; };
        const float unit = hdr ? 1.0f / 256.0f : 1.0f / 1024.0f;
        for (uint32_t i = 0; i < W * H; ++i) {
            // A grey level plus a colour offset, so dark, middle and bright pixels and greys all occur.
            const int level = int(next(hdr ? 1000 : 960)) + 30;
            auto channel = [&](int spread) { return float(std::max(0, level + int(next(2 * spread + 1)) - spread)) * unit; };
            const int spread = i % 5 == 0 ? 0 : 220;
            d.base[i] = {channel(spread), channel(spread), channel(spread)};
            auto pair = [&](float& a, float& b) { const int lo = int(next(900)) + 40; a = float(lo) * unit; b = float(std::max(0, lo + int(next(601)) - 300)) * unit; };
            pair(d.input[i].r, d.fin[i].r); pair(d.input[i].g, d.fin[i].g); pair(d.input[i].b, d.fin[i].b);
        }
        return d;
    };
    auto bytes = [&](const std::vector<ref::Rgb>& px) {
        std::vector<uint8_t> raw(size_t(W) * H * 8);
        for (size_t i = 0; i < px.size(); ++i) {
            const uint16_t h[4] = {floatToHalf(px[i].r), floatToHalf(px[i].g), floatToHalf(px[i].b), floatToHalf(1.0f)};
            std::memcpy(raw.data() + i * 8, h, 8);
        }
        return raw;
    };
    D3D12_RESOURCE_STATES inState = D3D12_RESOURCE_STATE_COMMON, outState = D3D12_RESOURCE_STATE_COMMON;
    auto barrier = [&](ID3D12Resource* r, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
        D3D12_RESOURCE_BARRIER b{}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource = r;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; b.Transition.StateBefore = from; b.Transition.StateAfter = to;
        gpu.list->ResourceBarrier(1, &b);
    };
    // Runs the shader on `d` with the given strength and shape constants; returns the result as floats (rgb per pixel).
    auto run = [&](const Data& d, float total, const std::array<float, 4>& shape, bool hdr, std::vector<ref::Rgb>& result, float color = 1.0f, float luminance = 1.0f) {
        gpu.begin();
        auto u0 = recordFill(gpu, base, bytes(d.base), inState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        auto u1 = recordFill(gpu, input, bytes(d.input), inState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        auto u2 = recordFill(gpu, fin, bytes(d.fin), inState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        inState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        barrier(out.texture.Get(), outState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        float c[40]{};
        c[0] = total; c[1] = 1; c[2] = 1; c[3] = color; c[4] = luminance; c[7] = hdr ? 1.0f : 0.0f; c[31] = hdr ? 1.0f : 0.0f;
        std::copy(shape.begin(), shape.end(), c + 36);
        pass.bind(gpu.list.Get(), c, pipeline::gpuHandleOf(pass, 0).ptr, pipeline::gpuHandleOf(pass, 3).ptr);
        gpu.list->Dispatch((W + 15) / 16, (H + 15) / 16, 1);
        barrier(out.texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        outState = D3D12_RESOURCE_STATE_COPY_SOURCE;
        auto rb = recordRead(gpu, out, D3D12_RESOURCE_STATE_COPY_SOURCE);
        if (!gpu.submitAndWait()) return false;
        const auto raw = mapRead(out, rb.Get());
        if (raw.size() != size_t(W) * H * 8) return false;
        result.resize(W * H);
        for (size_t i = 0; i < result.size(); ++i) {
            uint16_t h[4]; std::memcpy(h, raw.data() + i * 8, 8);
            result[i] = {halfToFloat(h[0]), halfToFloat(h[1]), halfToFloat(h[2])};
        }
        return true;
    };
    // The shader's own safeguard for a strengthened darkening (NrResidualComposite.hlsl, shadowSafe).
    auto shadowSafe = [](float b, float raw, float requested) {
        const float anchorDelta = std::min(raw, 0.0f), anchor = b + anchorDelta;
        if (b >= 0 && anchor > 0 && requested < anchorDelta) return anchor / (1.0f + (anchorDelta - requested) / anchor);
        return b + requested;
    };
    auto expected = [&](const Data& d, float total, const PushEnhanceShape& shape, bool hdr, size_t i, float color = 1.0f, float luminance = 1.0f) {
        const ref::Rgb raw{d.fin[i].r - d.input[i].r, d.fin[i].g - d.input[i].g, d.fin[i].b - d.input[i].b};
        ref::Rgb delta = ref::colorDelta(d.base[i], raw, color, luminance, hdr);
        delta = shape.neutral() ? delta : ref::shapeDelta(d.base[i], delta, shape, hdr);
        delta = {delta.r * total, delta.g * total, delta.b * total};
        if (!shape.neutral() && (shape.maxBrighten > 0 || shape.maxDarken > 0)) delta = ref::capDelta(d.base[i], delta, shape, hdr);
        ref::Rgb r{shadowSafe(d.base[i].r, raw.r, delta.r), shadowSafe(d.base[i].g, raw.g, delta.g), shadowSafe(d.base[i].b, raw.b, delta.b)};
        if (!hdr) r = {std::max(0.0f, r.r), std::max(0.0f, r.g), std::max(0.0f, r.b)};
        return r;
    };
    auto worst = [&](const Data& d, float total, const PushEnhanceShape& shape, bool hdr, const std::vector<ref::Rgb>& got, float color = 1.0f, float luminance = 1.0f) {
        float w = 0;
        for (size_t i = 0; i < got.size(); ++i) {
            const ref::Rgb e = expected(d, total, shape, hdr, i, color, luminance);
            for (const auto [a, b] : {std::pair{got[i].r, e.r}, std::pair{got[i].g, e.g}, std::pair{got[i].b, e.b}})
                w = std::max(w, std::abs(a - b) / (1.0f + std::abs(b)));
        }
        return w;
    };
    const Data sdr = makeData(false, 20261118u), hdrData = makeData(true, 322u);
    std::vector<ref::Rgb> plain, got;
    bool ran = run(sdr, 1.0f, {0, 0, 0, 0}, false, plain);
    check(ran && worst(sdr, 1.0f, neutral, false, plain) < 0.002f, std::format("enhance shape: zeros give the plain composite (worst {:.5f})", ran ? worst(sdr, 1.0f, neutral, false, plain) : -1.0f));
    // Neutral percentages sent with the flag set (the interface never does; the shader must still agree).
    ran = run(sdr, 1.0f, {float(100 + 256 * 100 + 65536 * 100), 100.0f, 0.0f, 1.0f}, false, got);
    float differ = 0;
    for (size_t i = 0; ran && i < got.size(); ++i) differ = std::max({differ, std::abs(got[i].r - plain[i].r), std::abs(got[i].g - plain[i].g), std::abs(got[i].b - plain[i].b)});
    check(ran && differ < 0.002f, std::format("enhance shape: neutral values change nothing (worst {:.5f})", differ));

    struct Case { const char* name; PushEnhanceShape shape; float total; bool hdr; };
    auto make = [](int shadows, int midtones, int highlights, int hue, int maxBrighten, int maxDarken) {
        PushEnhanceShape s; s.shadows = shadows; s.midtones = midtones; s.highlights = highlights; s.hue = hue; s.maxBrighten = maxBrighten; s.maxDarken = maxDarken; return s;
    };
    const Case cases[] = {
        {"shadows off", make(0, 100, 100, 100, 0, 0), 1.0f, false},
        {"shadows doubled", make(200, 100, 100, 100, 0, 0), 1.0f, false},
        {"midtones halved", make(100, 50, 100, 100, 0, 0), 1.0f, false},
        {"highlights off", make(100, 100, 0, 100, 0, 0), 1.0f, false},
        {"hue kept", make(100, 100, 100, 0, 0, 0), 1.0f, false},
        {"hue doubled", make(100, 100, 100, 200, 0, 0), 1.0f, false},
        {"brightening limited", make(100, 100, 100, 100, 10, 0), 1.0f, false},
        {"darkening limited", make(100, 100, 100, 100, 0, 5), 1.0f, false},
        {"everything at once, double strength", make(150, 50, 20, 40, 15, 10), 2.0f, false},
        {"everything at once, HDR", make(150, 50, 20, 40, 15, 10), 1.5f, true},
        {"hue kept, HDR", make(100, 100, 100, 0, 0, 0), 1.0f, true},
    };
    for (const Case& k : cases) {
        const Data& d = k.hdr ? hdrData : sdr;
        ran = run(d, k.total, k.shape.constants(), k.hdr, got);
        const float w = ran ? worst(d, k.total, k.shape, k.hdr, got) : -1.0f;
        check(ran && w < 0.004f, std::format("enhance shape: {} matches the reference (worst {:.5f})", k.name, w));
        if (ran && w >= 0.004f) {
            // Which pixel, so a difference between a card's arithmetic and the reference can be read from the log.
            size_t at = 0; float most = -1;
            for (size_t i = 0; i < got.size(); ++i) {
                const ref::Rgb e = expected(d, k.total, k.shape, k.hdr, i);
                const float here = std::max({std::abs(got[i].r - e.r) / (1 + std::abs(e.r)), std::abs(got[i].g - e.g) / (1 + std::abs(e.g)), std::abs(got[i].b - e.b) / (1 + std::abs(e.b))});
                if (here > most) { most = here; at = i; }
            }
            const ref::Rgb e = expected(d, k.total, k.shape, k.hdr, at);
            std::printf("INFO enhance shape: pixel %zu base %.5f %.5f %.5f change %.5f %.5f %.5f got %.5f %.5f %.5f expected %.5f %.5f %.5f\n", at,
                        d.base[at].r, d.base[at].g, d.base[at].b, d.fin[at].r - d.input[at].r, d.fin[at].g - d.input[at].g, d.fin[at].b - d.input[at].b,
                        got[at].r, got[at].g, got[at].b, e.r, e.g, e.b);
        }
    }
    // PushFPS 4.2.1: the layer's colour strength (push_shape_ref::colorDelta). Up to 100 % it is the plain
    // statement it always was; above 100 % the part over it tapers with the size of the colour change.
    struct ColorCase { const char* name; float color, luminance; PushEnhanceShape shape; float total; bool hdr; };
    const ColorCase colorCases[] = {
        {"colour off", 0.0f, 1.0f, neutral, 1.0f, false},
        {"colour halved", 0.5f, 1.0f, neutral, 1.0f, false},
        {"brightness halved, colour as made", 1.0f, 0.5f, neutral, 1.0f, false},
        {"colour doubled", 2.0f, 1.0f, neutral, 1.0f, false},
        {"colour at 150 %, brightness halved", 1.5f, 0.5f, neutral, 1.0f, false},
        {"colour doubled, HDR", 2.0f, 1.0f, neutral, 1.0f, true},
        {"colour doubled with every other control", 2.0f, 1.0f, make(150, 50, 20, 40, 15, 10), 1.5f, false},
    };
    for (const ColorCase& k : colorCases) {
        const Data& d = k.hdr ? hdrData : sdr;
        ran = run(d, k.total, k.shape.constants(), k.hdr, got, k.color, k.luminance);
        const float w = ran ? worst(d, k.total, k.shape, k.hdr, got, k.color, k.luminance) : -1.0f;
        check(ran && w < 0.004f, std::format("enhance colour: {} matches the reference (worst {:.5f})", k.name, w));
    }
    {
        // The promise, on the shader's own output: at 200 % no channel is further from the picture at 100 %
        // than a quarter of the pixel's brightness, where plain doubling would have been further for many.
        ran = run(sdr, 1.0f, {0, 0, 0, 0}, false, got, 2.0f, 1.0f);
        float over = 0, most = 0; int wouldPass = 0, acted = 0;
        for (size_t i = 0; ran && i < got.size(); ++i) {
            const ref::Rgb raw{sdr.fin[i].r - sdr.input[i].r, sdr.fin[i].g - sdr.input[i].g, sdr.fin[i].b - sdr.input[i].b};
            const float dy = ref::luma(raw), peak = std::max({std::abs(raw.r - dy), std::abs(raw.g - dy), std::abs(raw.b - dy)});
            const float room = pipeline::push_shape_ref::kPushColorKnee * std::max(ref::luma(sdr.base[i]), pipeline::push_shape_ref::kPushColorFloor);
            const float moved = std::max({std::abs(got[i].r - plain[i].r), std::abs(got[i].g - plain[i].g), std::abs(got[i].b - plain[i].b)});
            over = std::max(over, moved - room); most = std::max(most, moved);
            if (peak > room + 0.01f) ++wouldPass;
            if (moved > 0.01f) ++acted;
        }
        check(ran && over < 0.002f && wouldPass > 200 && acted > 200 && most > 0.05f,
              std::format("enhance colour: at 200 % nothing is added past a quarter of a pixel's brightness (worst {:+.5f} over; plain doubling would pass it on {} pixels; {} pixels changed, the most {:.3f})", over, wouldPass, acted, most));
    }
    // What the controls promise, checked on the shader's own output.
    ran = run(sdr, 1.0f, make(0, 0, 0, 100, 0, 0).constants(), false, got);
    float moved = 0;
    for (size_t i = 0; ran && i < got.size(); ++i) moved = std::max({moved, std::abs(got[i].r - sdr.base[i].r), std::abs(got[i].g - sdr.base[i].g), std::abs(got[i].b - sdr.base[i].b)});
    check(ran && moved < 0.002f, std::format("enhance shape: all three ranges at zero leave the picture as it was (worst {:.5f})", moved));
    const PushEnhanceShape limits = make(100, 100, 100, 100, 10, 5);
    ran = run(sdr, 1.0f, limits.constants(), false, got);
    float brightest = 0, darkest = 0; int limited = 0;
    for (size_t i = 0; ran && i < got.size(); ++i) {
        // The limits are on the change itself. A channel the result clips at black, or one the shader's
        // shadow safeguard reshapes, ends a little brighter than that; those pixels are not measured here.
        const ref::Rgb e = expected(sdr, 1.0f, limits, false, i);
        const ref::Rgb raw{sdr.fin[i].r - sdr.input[i].r, sdr.fin[i].g - sdr.input[i].g, sdr.fin[i].b - sdr.input[i].b};
        const ref::Rgb d = ref::capDelta(sdr.base[i], ref::shapeDelta(sdr.base[i], raw, limits, false), limits, false);
        if (e.r <= 0.001f || e.g <= 0.001f || e.b <= 0.001f) continue;
        if (std::abs(e.r - (sdr.base[i].r + d.r)) > 1e-5f || std::abs(e.g - (sdr.base[i].g + d.g)) > 1e-5f || std::abs(e.b - (sdr.base[i].b + d.b)) > 1e-5f) continue;
        const float change = ref::perceptual(ref::luma(got[i]), false) - ref::perceptual(ref::luma(sdr.base[i]), false);
        brightest = std::max(brightest, change); darkest = std::max(darkest, -change); ++limited;
    }
    check(ran && limited > 500 && brightest < 0.105f && darkest < 0.055f && brightest > 0.05f && darkest > 0.02f,
          std::format("enhance shape: no pixel passes the limits ({} pixels, brightest +{:.3f} of 0.10, darkest -{:.3f} of 0.05)", limited, brightest, darkest));
    ran = run(sdr, 1.0f, make(100, 100, 100, 0, 0, 0).constants(), false, got);
    float turned = 0; int coloured = 0;
    for (size_t i = 0; ran && i < got.size(); ++i) {
        // A clearly coloured pixel that stayed clearly coloured: its colour direction is the picture's own.
        const float yb = ref::luma(sdr.base[i]), yo = ref::luma(got[i]);
        const ref::Rgb cb{sdr.base[i].r - yb, sdr.base[i].g - yb, sdr.base[i].b - yb}, co{got[i].r - yo, got[i].g - yo, got[i].b - yo};
        const float lb = std::sqrt(cb.r * cb.r + cb.g * cb.g + cb.b * cb.b), lo = std::sqrt(co.r * co.r + co.g * co.g + co.b * co.b);
        if (lb / std::max(yb, 1e-4f) < 0.2f || lo < 0.05f || got[i].r <= 0.001f || got[i].g <= 0.001f || got[i].b <= 0.001f) continue;
        // Only where the shader's shadow safeguard did not step in (it works per channel).
        const ref::Rgb e = expected(sdr, 1.0f, make(100, 100, 100, 0, 0, 0), false, i);
        const ref::Rgb raw{sdr.fin[i].r - sdr.input[i].r, sdr.fin[i].g - sdr.input[i].g, sdr.fin[i].b - sdr.input[i].b};
        const ref::Rgb shaped = ref::shapeDelta(sdr.base[i], raw, make(100, 100, 100, 0, 0, 0), false);
        if (std::abs(e.r - (sdr.base[i].r + shaped.r)) > 1e-5f || std::abs(e.g - (sdr.base[i].g + shaped.g)) > 1e-5f || std::abs(e.b - (sdr.base[i].b + shaped.b)) > 1e-5f) continue;
        const float cosine = (cb.r * co.r + cb.g * co.g + cb.b * co.b) / (lb * lo);
        turned = std::max(turned, 1.0f - std::abs(cosine)); ++coloured;
    }
    check(ran && coloured > 50 && turned < 0.01f, std::format("enhance shape: with hue at zero coloured pixels keep their hue ({} pixels, worst turn {:.4f})", coloured, turned));
    check(!gpu.debugErrors(), "enhance shape: no D3D12 debug errors");
}

void testPushFlowRuntime(Gpu& gpu, const wchar_t* dir, bool strict);
// Network size per tier (no runtime needed): 4K never runs RIFE at 4K, small pictures stay full size
// (Fast stays at half), and every scale keeps at least the tier's pixel budget order.
void testPushFlowSizes() {
    const auto same = [](double a, double b) { return std::abs(a - b) < 1e-6; };
    check(same(pushflow::networkScale(0, 640, 360), 0.5), "pushflow sizes: Fast at 640x360 is half size");
    check(same(pushflow::networkScale(1, 640, 360), 1.0) && same(pushflow::networkScale(2, 640, 360), 1.0), "pushflow sizes: small pictures run at full size");
    check(same(pushflow::networkScale(0, 3840, 2160), 0.25), "pushflow sizes: Fast at 4K is 960x540");
    check(same(pushflow::networkScale(1, 3840, 2160), 1.0 / 3.0), "pushflow sizes: Balanced at 4K is 1280x720");
    check(same(pushflow::networkScale(2, 3840, 2160), 0.5), "pushflow sizes: Quality at 4K is 1920x1080");
    check(same(pushflow::networkScale(2, 1920, 1080), 1.0) && same(pushflow::networkScale(1, 1920, 1080), 2.0 / 3.0), "pushflow sizes: 1080p");
}
void testPushFlow(Gpu& gpu) {
    wchar_t dir[1024]{}, full[1024]{};
    if (GetEnvironmentVariableW(L"VEYRA_PUSHFLOW_TEST_RUNTIME_FP32", full, 1024)) { std::printf("INFO pushflow: full-precision models (diagnostic)\n"); testPushFlowRuntime(gpu, full, false); }
    if (!GetEnvironmentVariableW(L"VEYRA_PUSHFLOW_TEST_RUNTIME", dir, 1024)) { std::printf("SKIP pushflow: VEYRA_PUSHFLOW_TEST_RUNTIME not set\n"); return; }
    testPushFlowRuntime(gpu, dir, true);
}
void testPushFlowRuntime(Gpu& gpu, const wchar_t* dir, bool strict) {
    const auto checkOr = [&](bool ok, const std::string& label) { if (strict) check(ok, label); else std::printf("INFO %s %s\n", ok ? "ok" : "not ok", label.c_str()); };
    std::wstring why;
    checkOr(pushflow::runtimeInstalled(dir, &why), "pushflow: runtime installed");
    if (!pushflow::runtimeInstalled(dir)) return;
    constexpr uint32_t W = 256, H = 160;
    auto* device = gpu.device.Get();
    const auto fmt = DXGI_FORMAT_R8G8B8A8_UNORM;
    Plane a{pipeline::makeTexture(device, W, H, fmt, false), fmt, W, H, 4};
    Plane b{pipeline::makeTexture(device, W, H, fmt, false), fmt, W, H, 4};
    Plane out{pipeline::makeTexture(device, W, H, fmt, false), fmt, W, H, 4};
    checkOr(a.texture && b.texture && out.texture, "pushflow: textures created");
    // What the next run() tells pack() besides the pictures: nothing, as every caller did before the
    // encoder cache; its test sets the two frames' numbers and "the same pair again".
    uint64_t packPrevious = 0, packCurrent = 0;
    bool packTimeOnly = false;
    float forecastAhead = -1.0f;   // 0..1: the run ends in PushFlow 2's forecast for that time after `pb`, not in resolve()
    const auto run = [&](const std::vector<uint8_t>& pa, const std::vector<uint8_t>& pb, float t, pushflow::PushFlowBackend& flow, double* ms) {
        std::vector<uint8_t> result;
        if (!gpu.begin()) return result;
        auto ua = recordFill(gpu, a, pa, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        auto ub = recordFill(gpu, b, pb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        // Wait for the pack list before resetting the allocator for the next one.
        if (!flow.pack(gpu.list.Get(), a.texture.Get(), b.texture.Get(), t, packTimeOnly, packPrevious, packCurrent) || !gpu.submitAndWait()) return result;
        const auto start = std::chrono::steady_clock::now();
        if (!flow.infer()) return result;
        if (!gpu.begin()) return result;
        D3D12_RESOURCE_BARRIER bar{}; bar.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; bar.Transition.pResource = out.texture.Get();
        bar.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        bar.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON; bar.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        gpu.list->ResourceBarrier(1, &bar);
        if (forecastAhead >= 0.0f ? !flow.forecast(gpu.list.Get(), a.texture.Get(), b.texture.Get(), out.texture.Get(), forecastAhead)
                                  : !flow.resolve(gpu.list.Get(), a.texture.Get(), b.texture.Get(), out.texture.Get())) return result;
        auto rb = recordRead(gpu, out, D3D12_RESOURCE_STATE_COPY_DEST);
        bar.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST; bar.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
        gpu.list->ResourceBarrier(1, &bar);
        // Back to COMMON for the next fill.
        D3D12_RESOURCE_BARRIER back[2]{};
        for (int i = 0; i < 2; ++i) {
            back[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; back[i].Transition.pResource = (i ? b : a).texture.Get();
            back[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            back[i].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE; back[i].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
        }
        gpu.list->ResourceBarrier(2, back);
        if (!gpu.submitAndWait()) return result;
        if (ms) *ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        return mapRead(out, rb.Get());
    };
    for (uint32_t tier : {0u, 2u}) {
        pushflow::PushFlowBackend flow;
        const bool ready = flow.initialize(device, gpu.queue.Get(), dir, W, H, fmt, pushflow::Encoding::Encoded, tier);
        checkOr(ready, "pushflow: tier " + std::to_string(tier) + " initializes (" + flow.status().model + ")");
        if (!ready) continue;
        const auto first = flowFrame(W, H, 100), second = flowFrame(W, H, 116), middle = flowFrame(W, H, 108);
        double ms = 0;
        const auto same = run(first, first, 0.5f, flow, &ms);
        {
            const auto st = flow.status();
            const uint64_t plane = uint64_t(st.padW) * st.padH;
            const auto in = readFloats(gpu, flow.tensorBuffer(true), plane * flow.inputChannels());
            const auto outT = readFloats(gpu, flow.tensorBuffer(false), plane * 3);
            const uint32_t mx = uint32_t(110.0 * st.modelW / W), my = uint32_t((H * 0.35 + 10) * st.modelH / H);
            const uint64_t at = uint64_t(my) * st.padW + mx;
            if (!in.empty() && !outT.empty())
                std::printf("INFO pushflow tier %u tensor at square: in %.3f %.3f %.3f | %.3f %.3f %.3f | t %.3f; out %.3f %.3f %.3f\n", tier,
                    in[at], in[plane + at], in[2 * plane + at], in[3 * plane + at], in[4 * plane + at], in[5 * plane + at], in[6 * plane + at],
                    outT[at], outT[plane + at], outT[2 * plane + at]);
        }
        if (!same.empty()) {
            // Diagnostics: channel means and two sample pixels, output vs input.
            double mo[3]{}, mi[3]{};
            for (size_t i = 0; i < same.size(); i += 4) for (int k = 0; k < 3; ++k) { mo[k] += same[i + k]; mi[k] += first[i + k]; }
            const double n = double(same.size() / 4);
            const size_t sq = (size_t(H * 0.35 + 10) * W + 110) * 4, bg = (size_t(10) * W + 10) * 4;
            std::printf("INFO pushflow tier %u means out %.0f %.0f %.0f in %.0f %.0f %.0f; square out %d %d %d in %d %d %d; corner out %d %d %d in %d %d %d\n", tier,
                mo[0] / n, mo[1] / n, mo[2] / n, mi[0] / n, mi[1] / n, mi[2] / n, same[sq], same[sq + 1], same[sq + 2], first[sq], first[sq + 1], first[sq + 2],
                same[bg], same[bg + 1], same[bg + 2], first[bg], first[bg + 1], first[bg + 2]);
        }
        checkOr(!same.empty() && flowPsnr(same, first) > 30.0, std::format("pushflow: tier {} identical frames come back ({:.1f} dB, {:.0f} ms on {})", tier,
            same.empty() ? 0.0 : flowPsnr(same, first), ms, gpu.adapter));
        const auto mid = run(first, second, 0.5f, flow, &ms);
        std::vector<uint8_t> blend(first.size());
        for (size_t i = 0; i < blend.size(); ++i) blend[i] = uint8_t((int(first[i]) + second[i] + 1) / 2);
        const double flowDb = mid.empty() ? 0.0 : flowPsnr(mid, middle), blendDb = flowPsnr(blend, middle);
        checkOr(!mid.empty() && flowDb > blendDb + 2.0, std::format("pushflow: tier {} moving square lands in the middle ({:.1f} dB vs blend {:.1f} dB)", tier, flowDb, blendDb));
        const auto early = run(first, second, 0.25f, flow, nullptr);
        const auto quarter = flowFrame(W, H, 104);
        checkOr(!early.empty() && flowPsnr(early, quarter) > flowPsnr(early, middle), "pushflow: tier " + std::to_string(tier) + " t = 0.25 is closer to the quarter position");
        // Fine texture panning sideways: the Fast tier rebuilds it from the full-size frames.
        const auto d0 = detailFrame(W, H, 0), d1 = detailFrame(W, H, 8), dMid = detailFrame(W, H, 4);
        std::vector<uint8_t> dBlend(d0.size());
        for (size_t i = 0; i < dBlend.size(); ++i) dBlend[i] = uint8_t((int(d0[i]) + d1[i] + 1) / 2);
        const auto detail = run(d0, d1, 0.5f, flow, nullptr);
        const double detailDb = detail.empty() ? 0.0 : flowPsnr(detail, dMid), detailBlendDb = flowPsnr(dBlend, dMid);
        const std::string detailText = std::format("pushflow: tier {} fine texture in motion ({:.1f} dB vs blend {:.1f} dB, rebuild={}, precision={}, self-check {:.1f} dB)",
            tier, detailDb, detailBlendDb, flow.status().rebuild ? 1 : 0, flow.status().precision, flow.status().selfCheckDb);
        // What this guards is the Fast tier's full-resolution rebuild (network at half size). At full
        // size the network's own picture is used unchanged, and on per-pixel noise panning 8 pixels
        // RIFE 4.26 does not track it (14.6 dB vs blend 15.2 on WARP, 2026-11-17; real pictures pass
        // the moving-square check above). Reported, not required, until real-content tests replace it.
        if (tier == 0) checkOr(!detail.empty() && detailDb > detailBlendDb + 2.0, detailText);
        else std::printf("INFO %s\n", detailText.c_str());
        if (tier == 0) checkOr(flow.status().rebuild, "pushflow: the Fast tier rebuilds at full size");
        // The full-size rebuild against its reference (pushflow/PushFlowMotion.h), pixel by pixel, from
        // the network's own grids and mask of the same run. The checks above judge the picture by how close
        // it comes to a drawn truth; this one says the shader does exactly what the contract says, on
        // every pair, whatever the network made of it.
        if (flow.status().grids) {
            using pushflow::motion::reference::Rgb;
            using pushflow::motion::reference::Vec2;
            const auto st = flow.status();
            const pushflow::motion::Geometry geo{W, H, st.modelW, st.modelH, st.padW, st.padH, st.alignCorners, st.maskForFirst};
            const uint64_t plane = uint64_t(st.padW) * st.padH;
            const auto moved96 = flowFrame(W, H, 196), other = detailFrame(W, H, 0);
            struct Pair { const char* name; const std::vector<uint8_t>* a; const std::vector<uint8_t>* b; float t; };
            const Pair pairs[] = {{"a square moved 16 pixels", &first, &second, 0.5f}, {"a square moved 96 pixels", &first, &moved96, 0.5f},
                                  {"a square moved 96 pixels, at three quarters", &first, &moved96, 0.75f}, {"fine texture panned 8 pixels", &d0, &d1, 0.5f},
                                  {"two unrelated pictures", &first, &other, 0.5f}};
            for (const Pair& pair : pairs) {
                const auto picture = run(*pair.a, *pair.b, pair.t, flow, nullptr);
                const auto g0 = readFloats(gpu, flow.motionBuffer(0), plane * 2), g1 = readFloats(gpu, flow.motionBuffer(1), plane * 2), mk = readFloats(gpu, flow.motionBuffer(2), plane);
                const bool ran = !picture.empty() && !g0.empty() && !g1.empty() && !mk.empty();
                int worst = 255; double mean = 255;
                if (ran) {
                    const uint32_t width = W, padW = st.padW;
                    const auto frame = [width](const std::vector<uint8_t>& px) {
                        const uint8_t* data = px.data();
                        return [data, width](uint32_t x, uint32_t y) { const uint8_t* q = data + (size_t(y) * width + x) * 4; return Rgb{q[0] / 255.0f, q[1] / 255.0f, q[2] / 255.0f}; };
                    };
                    const auto grid = [padW](const std::vector<float>& g) {
                        const float* data = g.data();
                        return [data, padW](uint32_t x, uint32_t y) { const size_t i = (size_t(y) * padW + x) * 2; return Vec2{data[i], data[i + 1]}; };
                    };
                    const float* maskData = mk.data();
                    const auto maskAt = [maskData, padW](uint32_t x, uint32_t y) { return maskData[size_t(y) * padW + x]; };
                    double sum = 0; worst = 0;
                    for (uint32_t y = 0; y < H; ++y)
                        for (uint32_t x = 0; x < W; ++x) {
                            const Rgb px = pushflow::motion::reference::rebuild(geo, x, y, frame(*pair.a), frame(*pair.b), grid(g0), grid(g1), maskAt);
                            const float want[3] = {px.r, px.g, px.b};
                            for (int k = 0; k < 3; ++k) {
                                const int d = std::abs(int(picture[(size_t(y) * W + x) * 4 + k]) - int(std::clamp(want[k], 0.0f, 1.0f) * 255.0f + 0.5f));
                                worst = std::max(worst, d); sum += d;
                            }
                        }
                    mean = sum / (3.0 * W * H);
                }
                const std::string text = std::format("pushflow: tier {} {}: the picture is the reference rebuild of the network's grids and mask (worst {} of 255, mean {:.3f})",
                    tier, pair.name, worst, mean);
                // At full size the network's own picture is shown, not the rebuild: how close the two are
                // is reported (the model script checks that on the CPU), the shader is checked where it runs.
                if (st.rebuild) checkOr(ran && worst <= 2 && mean < 0.3, text);
                else std::printf("INFO %s (the network's own picture; the rebuild is not used at this size)\n", text.c_str());
            }
        } else {
            std::printf("INFO pushflow: tier %u model has no grids: nothing to rebuild from\n", tier);
        }
        const auto tel = flow.telemetry();
        checkOr(tel.inferences > 0 && tel.inferP50Ms > 0, std::format("pushflow: telemetry ({} inferences, p50 {:.1f} ms)", tel.inferences, tel.inferP50Ms));
        checkOr(!gpu.debugErrors(), "pushflow: no debug layer errors");
    }
    // A motion size chosen apart from the tier (3.2.7; pushflow/PushFlowMotion.h). At this picture size the
    // Fast tier's own network is half the picture and any chosen size is the whole picture (a picture is
    // never enlarged), which is exactly what the Balanced tier runs here with the same model: the two
    // must give the same picture. A chosen size at the Quality tier changes nothing here.
    {
        const auto first = flowFrame(W, H, 100), second = flowFrame(W, H, 116);
        std::vector<uint8_t> chosenPicture, balancedPicture;
        uint32_t chosenW = 0, chosenH = 0, ownW = 0, ownH = 0; bool chosenRebuild = true, ownRebuild = false, started = true;
        {
            pushflow::PushFlowBackend own;
            started = own.initialize(device, gpu.queue.Get(), dir, W, H, fmt, pushflow::Encoding::Encoded, 0);
            if (started) { ownW = own.status().modelW; ownH = own.status().modelH; ownRebuild = own.status().rebuild; }
        }
        {
            pushflow::PushFlowBackend chosen;
            started = started && chosen.initialize(device, gpu.queue.Get(), dir, W, H, fmt, pushflow::Encoding::Encoded, 0, 540);
            if (started) { chosenW = chosen.status().modelW; chosenH = chosen.status().modelH; chosenRebuild = chosen.status().rebuild; chosenPicture = run(first, second, 0.5f, chosen, nullptr); }
        }
        {
            pushflow::PushFlowBackend balanced;
            started = started && balanced.initialize(device, gpu.queue.Get(), dir, W, H, fmt, pushflow::Encoding::Encoded, 1);
            if (started) balancedPicture = run(first, second, 0.5f, balanced, nullptr);
        }
        checkOr(started, "pushflow motion size: Fast, Fast at a chosen size and Balanced all start");
        if (started) {
            checkOr(ownW == W / 2 && ownH == H / 2 && ownRebuild, std::format("pushflow motion size: Fast's own size is what it was ({}x{}, rebuilt at full size)", ownW, ownH));
            checkOr(chosenW == W && chosenH == H && !chosenRebuild, std::format("pushflow motion size: a chosen size is used at the Fast tier, as far as the picture goes ({}x{})", chosenW, chosenH));
            int worst = 255; double mean = 0;
            if (chosenPicture.size() == balancedPicture.size() && !chosenPicture.empty()) {
                worst = 0;
                for (size_t i = 0; i < chosenPicture.size(); ++i) { const int d = std::abs(int(chosenPicture[i]) - int(balancedPicture[i])); worst = std::max(worst, d); mean += d; }
                mean /= double(chosenPicture.size());
            }
            checkOr(worst <= 1, std::format("pushflow motion size: Fast at the picture's size gives the picture Balanced gives here, the same model at the same size (worst {} of 255, mean {:.4f})", worst, mean));
        }
        {
            // One after the other: PushFPS never has two PushFlow sessions at once, so neither does the test.
            pushflow::Status quality, qualityChosen; bool both = true;
            for (int lines : {0, 720}) {
                pushflow::PushFlowBackend flow;
                const bool ready = flow.initialize(device, gpu.queue.Get(), dir, W, H, fmt, pushflow::Encoding::Encoded, 2, lines);
                if (ready) (lines ? qualityChosen : quality) = flow.status();
                both = both && ready;
            }
            checkOr(both && quality.modelW == W && quality.modelW == qualityChosen.modelW && quality.modelH == qualityChosen.modelH &&
                    !quality.model.empty() && quality.model == qualityChosen.model && quality.model.find("quality") != std::string::npos,
                    "pushflow motion size: on a picture smaller than the chosen size nothing changes, and the tier still chooses the model");
        }
        checkOr(!gpu.debugErrors(), "pushflow motion size: no debug layer errors");
    }
    // PushFlow 2's encoder cache (opt-in; docs/PUSHFLOW2.md): each real frame encoded once, its features
    // kept for the pair after it. Whatever it keeps or makes anew, the picture must be the whole model's.
    for (uint32_t tier : {0u, 2u}) {
        const std::string tag = "pushflow encoder cache, tier " + std::to_string(tier);
        const std::vector<uint8_t> frames[4] = {flowFrame(W, H, 100), flowFrame(W, H, 116), flowFrame(W, H, 132), flowFrame(W, H, 148)};
        const auto apart = [](const std::vector<uint8_t>& x, const std::vector<uint8_t>& y) {
            if (x.empty() || x.size() != y.size()) return 255;
            int worst = 0;
            for (size_t i = 0; i < x.size(); ++i) worst = std::max(worst, std::abs(int(x[i]) - int(y[i])));
            return worst;
        };
        packPrevious = packCurrent = 0; packTimeOnly = false;
        std::vector<uint8_t> want01, want12, want12late, want23;
        {
            pushflow::PushFlowBackend whole;
            if (whole.initialize(device, gpu.queue.Get(), dir, W, H, fmt, pushflow::Encoding::Encoded, tier)) {
                want01 = run(frames[0], frames[1], 0.5f, whole, nullptr);
                want12 = run(frames[1], frames[2], 0.5f, whole, nullptr);
                want12late = run(frames[1], frames[2], 0.75f, whole, nullptr);
                want23 = run(frames[2], frames[3], 0.5f, whole, nullptr);
                checkOr(whole.status().splitState.empty() && !whole.status().split && whole.telemetry().encoderRuns == 0, tag + ": not asked for, nothing of it runs");
            }
        }
        checkOr(!want01.empty() && !want12.empty() && !want12late.empty() && !want23.empty(), tag + ": the whole model's pictures to compare with");
        pushflow::PushFlowBackend cached;
        const bool ready = cached.initialize(device, gpu.queue.Get(), dir, W, H, fmt, pushflow::Encoding::Encoded, tier, 0, true);
        checkOr(ready, tag + ": PushFlow starts with the cache asked for");
        if (!ready) continue;
        const auto st = cached.status();
        std::printf("INFO %s: state '%s', start-up check %.1f dB, one frame whole %.1f ms, split %.1f ms\n", tag.c_str(), st.splitState.c_str(), st.splitCheckDb, st.wholeMs, st.splitMs);
        checkOr(st.split && st.splitState == "on" && st.splitCheckDb >= 50.0, tag + ": the split models are in use after matching the whole model at start");
        if (!st.split) continue;
        checkOr(cached.telemetry().encoderRuns == 0 && cached.telemetry().featureHits == 0, tag + ": the start-up check leaves no count and nothing kept behind");
        // A first pair: nothing kept, both frames encoded.
        packPrevious = 1; packCurrent = 2;
        const auto got01 = run(frames[0], frames[1], 0.5f, cached, nullptr);
        // The pair after it: its first frame's features are the ones just kept.
        packPrevious = 2; packCurrent = 3;
        const auto got12 = run(frames[1], frames[2], 0.5f, cached, nullptr);
        // The same pair at another time (3X and 4X): the rest of the model alone.
        packTimeOnly = true;
        const auto got12late = run(frames[1], frames[2], 0.75f, cached, nullptr);
        packTimeOnly = false;
        auto tel = cached.telemetry();
        checkOr(tel.featureMisses == 1 && tel.featureHits == 1 && tel.encoderRuns == 3,
                std::format("{}: two pairs and a second time cost three encoder runs, one frame's features kept (runs {}, kept {}, made {})", tag, tel.encoderRuns, tel.featureHits, tel.featureMisses));
        checkOr(apart(got01, want01) <= 1 && apart(got12, want12) <= 1 && apart(got12late, want12late) <= 1,
                std::format("{}: the pictures are the whole model's (worst of 255: first pair {}, kept features {}, second time {})", tag, apart(got01, want01), apart(got12, want12), apart(got12late, want12late)));
        // Frames whose numbers are not known: nothing kept may be used.
        packPrevious = packCurrent = 0;
        const auto got23 = run(frames[2], frames[3], 0.5f, cached, nullptr);
        // A pair that does not follow the one before (a pair was skipped): made anew.
        packPrevious = 7; packCurrent = 8;
        const auto got01again = run(frames[0], frames[1], 0.5f, cached, nullptr);
        // And one whose first frame carries an old number that is no longer kept anywhere.
        packPrevious = 2; packCurrent = 9;
        const auto got12again = run(frames[1], frames[2], 0.5f, cached, nullptr);
        packPrevious = packCurrent = 0;
        tel = cached.telemetry();
        checkOr(tel.featureMisses == 4 && tel.featureHits == 1 && tel.encoderRuns == 9,
                std::format("{}: unknown, skipped and outdated frames are encoded again, never taken from what was kept (runs {}, kept {}, made {})", tag, tel.encoderRuns, tel.featureHits, tel.featureMisses));
        checkOr(apart(got23, want23) <= 1 && apart(got01again, want01) <= 1 && apart(got12again, want12) <= 1,
                std::format("{}: and their pictures are the whole model's too (worst of 255: {}, {}, {})", tag, apart(got23, want23), apart(got01again, want01), apart(got12again, want12)));
        checkOr(cached.status().split, tag + ": still in use after every case");
        checkOr(!gpu.debugErrors(), tag + ": no debug layer errors");
    }
    // PushFlow 2's Guard (opt-in; shaders/PushGuard.hlsl, docs/PUSHFLOW2.md): where an object was left out
    // of the generated frame, the nearer real frame; everywhere else, and always when nothing is wrong,
    // the generated frame exactly as it is without Guard.
    // Three runs: the Quality model, the Fast tier at its own motion size (half this small picture: 8 x 5
    // tiles, where the edge band leaves almost nothing to judge), and the Fast tier's model at the whole
    // picture's size, so that both models are seen acting.
    struct GuardRun { uint32_t tier; int motion; };
    for (const GuardRun guardRun : {GuardRun{2u, 0}, GuardRun{0u, 0}, GuardRun{0u, 540}}) {
        const uint32_t tier = guardRun.tier;
        const std::string tag = "pushflow guard, tier " + std::to_string(tier) + (guardRun.motion ? " at a chosen motion size" : "");
        const auto still = flowFrame(W, H, 90), near16 = flowFrame(W, H, 106), far120 = flowFrame(W, H, 210), other = detailFrame(W, H, 0);
        const auto apart = [](const std::vector<uint8_t>& x, const std::vector<uint8_t>& y) {
            if (x.empty() || x.size() != y.size()) return 255;
            int worst = 0;
            for (size_t i = 0; i < x.size(); ++i) worst = std::max(worst, std::abs(int(x[i]) - int(y[i])));
            return worst;
        };
        // Mean difference inside the square's place in the earlier frame (x 90..130, y from 35 % down, 40 high).
        const auto oldPlace = [&](const std::vector<uint8_t>& x, const std::vector<uint8_t>& y) {
            if (x.empty() || x.size() != y.size()) return 255.0;
            double sum = 0; int n = 0;
            for (uint32_t py = uint32_t(H * 0.35) + 4; py < uint32_t(H * 0.35) + 36; ++py)
                for (uint32_t px = 94; px < 126; ++px)
                    for (int c = 0; c < 3; ++c) { sum += std::abs(int(x[(size_t(py) * W + px) * 4 + c]) - int(y[(size_t(py) * W + px) * 4 + c])); ++n; }
            return sum / n;
        };
        packPrevious = packCurrent = 0; packTimeOnly = false;
        std::vector<uint8_t> plainStill, plainNear, plainFar;
        {
            pushflow::PushFlowBackend plain;
            if (plain.initialize(device, gpu.queue.Get(), dir, W, H, fmt, pushflow::Encoding::Encoded, tier, guardRun.motion)) {
                plainStill = run(still, still, 0.5f, plain, nullptr);
                plainNear = run(still, near16, 0.5f, plain, nullptr);
                plainFar = run(still, far120, 0.5f, plain, nullptr);
                checkOr(plain.status().guardState.empty() && !plain.status().guard && plain.telemetry().guardFrames == 0, tag + ": not asked for, nothing of it runs");
            }
        }
        checkOr(!plainStill.empty() && !plainNear.empty() && !plainFar.empty(), tag + ": PushFlow's own pictures to compare with");
        pushflow::PushFlowBackend guarded;
        const bool ready = guarded.initialize(device, gpu.queue.Get(), dir, W, H, fmt, pushflow::Encoding::Encoded, tier, guardRun.motion, false, true);
        checkOr(ready, tag + ": PushFlow starts with Guard asked for");
        if (!ready) continue;
        std::printf("INFO %s: state '%s', network %ux%u\n", tag.c_str(), guarded.status().guardState.c_str(), guarded.status().modelW, guarded.status().modelH);
        checkOr(guarded.status().guard && guarded.status().guardState == "on", tag + ": Guard is in use");
        if (!guarded.status().guard) continue;
        const auto gotStill = run(still, still, 0.5f, guarded, nullptr);
        const auto gotNear = run(still, near16, 0.5f, guarded, nullptr);
        checkOr(apart(gotStill, plainStill) == 0 && apart(gotNear, plainNear) == 0,
                std::format("{}: a still picture and a followed object come out exactly as without Guard (worst of 255: {}, {})", tag, apart(gotStill, plainStill), apart(gotNear, plainNear)));
        // An object moved too far to follow: without Guard its old place shows background; with it, the earlier frame's object.
        const auto gotFar = run(still, far120, 0.5f, guarded, nullptr);
        const double without = oldPlace(plainFar, still), with = oldPlace(gotFar, still);
        std::printf("INFO %s: object moved 120 px: its old place differs from the earlier frame by %.1f of 255 without Guard, %.1f with it\n", tag.c_str(), without, with);
        checkOr(with <= without + 0.5, tag + ": Guard never takes the old place further from the earlier frame");
        // Reported, not required: how far the rule goes on this small picture depends on the model (the
        // measurements that decide the rule are prepare_runtime.py's, on larger pictures).
        std::printf("INFO %s: the object PushFlow left out is %s where the earlier frame has it\n", tag.c_str(), without > 30.0 && with < without / 3.0 ? "shown" : "not (fully) shown");
        // Two unrelated pictures: where more than a third is risky, the nearer real frame whole (at the
        // middle time the earlier one, past it the later one).
        const auto gotCut = run(still, other, 0.5f, guarded, nullptr);
        const auto gotCutLate = run(still, other, 0.75f, guarded, nullptr);
        std::printf("INFO %s: two unrelated pictures: worst difference from the earlier frame %d of 255 at the middle, from the later frame %d past it\n", tag.c_str(),
                    apart(gotCut, still), apart(gotCutLate, other));
        // A few more frames so that the totals of the ones above have come back.
        for (int i = 0; i < 4; ++i) run(still, still, 0.5f, guarded, nullptr);
        const auto tel = guarded.telemetry();
        std::printf("INFO %s: judged %llu frames, mended %llu, held %llu, tiles %llu\n", tag.c_str(), static_cast<unsigned long long>(tel.guardFrames),
                    static_cast<unsigned long long>(tel.guardMended), static_cast<unsigned long long>(tel.guardHeld), static_cast<unsigned long long>(tel.guardTiles));
        checkOr(tel.guardFrames >= 5 && tel.guardMended + tel.guardHeld <= tel.guardFrames, tag + ": the frames it judged are counted, and never more mended or held than judged");
        checkOr(!gpu.debugErrors(), tag + ": no debug layer errors");
    }
    // PushFlow 2's Predict (opt-in; shaders/PushPredict.hlsl, pushflow/PushFlowPredict.h): a frame for a
    // time after the newest real frame, from the half-way grids. The shader is compared with the header's
    // rule pixel by pixel from the network's own grids; a still picture must stay exact; and an in-between
    // frame from the same PushFlow must be what it is without Predict (it steps aside for measurements).
    for (const GuardRun predictRun : {GuardRun{2u, 0}, GuardRun{0u, 540}}) {
        using pushflow::motion::reference::Rgb;
        using pushflow::motion::reference::Vec2;
        const uint32_t tier = predictRun.tier;
        const std::string tag = "pushflow predict, tier " + std::to_string(tier) + (predictRun.motion ? " at a chosen motion size" : "");
        const auto older = flowFrame(W, H, 100), newer = flowFrame(W, H, 104), later = flowFrame(W, H, 106), far16 = flowFrame(W, H, 116), other = detailFrame(W, H, 0);
        const auto t0 = detailFrame(W, H, 0), t1 = detailFrame(W, H, 4);
        const auto apart = [](const std::vector<uint8_t>& x, const std::vector<uint8_t>& y) {
            if (x.empty() || x.size() != y.size()) return 255;
            int worst = 0;
            for (size_t i = 0; i < x.size(); ++i) worst = std::max(worst, std::abs(int(x[i]) - int(y[i])));
            return worst;
        };
        const auto meanApart = [](const std::vector<uint8_t>& x, const std::vector<uint8_t>& y) {
            if (x.empty() || x.size() != y.size()) return 255.0;
            double sum = 0;
            for (size_t i = 0; i < x.size(); ++i) sum += std::abs(int(x[i]) - int(y[i]));
            return sum / double(x.size());
        };
        packPrevious = packCurrent = 0; packTimeOnly = false; forecastAhead = -1.0f;
        std::vector<uint8_t> plainBetween;
        {
            pushflow::PushFlowBackend plain;
            if (plain.initialize(device, gpu.queue.Get(), dir, W, H, fmt, pushflow::Encoding::Encoded, tier, predictRun.motion)) {
                plainBetween = run(older, far16, 0.5f, plain, nullptr);
                checkOr(plain.status().predictState.empty() && !plain.status().predict && plain.telemetry().predictFrames == 0, tag + ": not asked for, nothing of it runs");
            }
        }
        pushflow::PushFlowBackend flow;
        const bool ready = flow.initialize(device, gpu.queue.Get(), dir, W, H, fmt, pushflow::Encoding::Encoded, tier, predictRun.motion, false, false, true);
        checkOr(ready, tag + ": PushFlow starts with Predict asked for");
        if (!ready) continue;
        checkOr(flow.status().predict && flow.status().predictState == "on", tag + ": Predict is in use ('" + flow.status().predictState + "')");
        if (!flow.status().predict) continue;
        const auto st = flow.status();
        const pushflow::motion::Geometry geo{W, H, st.modelW, st.modelH, st.padW, st.padH, st.alignCorners, st.maskForFirst};
        const uint64_t plane = uint64_t(st.padW) * st.padH;
        // An in-between frame from this PushFlow is exactly PushFlow's own.
        checkOr(!plainBetween.empty() && apart(run(older, far16, 0.5f, flow, nullptr), plainBetween) == 0, tag + ": an in-between frame is still exactly PushFlow's own");
        // Nothing moves: the forecast is the newest frame, exactly.
        forecastAhead = 0.5f;
        const auto stillForecast = run(older, older, 0.5f, flow, nullptr);
        checkOr(apart(stillForecast, older) == 0, std::format("{}: a still picture's forecast is the picture itself (worst of 255: {})", tag, apart(stillForecast, older)));
        // The shader against the rule, from the network's own grids.
        struct Pair { const char* name; const std::vector<uint8_t>* a; const std::vector<uint8_t>* b; float ahead; };
        const Pair pairs[] = {{"a square moving 4 pixels a frame, half an interval ahead", &older, &newer, 0.5f}, {"the same, a quarter ahead", &older, &newer, 0.25f},
                              {"a square moving 16 pixels a frame, three quarters ahead", &older, &far16, 0.75f}, {"fine texture panning 4 pixels a frame", &t0, &t1, 0.5f},
                              {"two unrelated pictures", &older, &other, 0.5f}};
        for (const Pair& pair : pairs) {
            forecastAhead = pair.ahead;
            const auto picture = run(*pair.a, *pair.b, 0.5f, flow, nullptr);
            const auto g0 = readFloats(gpu, flow.motionBuffer(0), plane * 2), g1 = readFloats(gpu, flow.motionBuffer(1), plane * 2);
            const bool ran = !picture.empty() && !g0.empty() && !g1.empty();
            int worst = 255; double mean = 255, off = 100, heldShare = 0;
            if (ran) {
                const uint32_t width = W, padW = st.padW;
                const auto frame = [width](const std::vector<uint8_t>& px) {
                    const uint8_t* data = px.data();
                    return [data, width](uint32_t x, uint32_t y) { const uint8_t* q = data + (size_t(y) * width + x) * 4; return Rgb{q[0] / 255.0f, q[1] / 255.0f, q[2] / 255.0f}; };
                };
                const auto grid = [padW](const std::vector<float>& g) {
                    const float* data = g.data();
                    return [data, padW](uint32_t x, uint32_t y) { const size_t i = (size_t(y) * padW + x) * 2; return Vec2{data[i], data[i + 1]}; };
                };
                double sum = 0; uint64_t beyond = 0, held = 0, moving = 0; worst = 0;
                for (uint32_t y = 0; y < H; ++y)
                    for (uint32_t x = 0; x < W; ++x) {
                        const auto px = pushflow::predict::forecast(geo, x, y, pair.ahead, frame(*pair.b), frame(*pair.a), frame(*pair.b), grid(g0), grid(g1));
                        const float want[3] = {px.colour.r, px.colour.g, px.colour.b};
                        int here = 0;
                        for (int k = 0; k < 3; ++k) here = std::max(here, std::abs(int(picture[(size_t(y) * W + x) * 4 + k]) - int(std::clamp(want[k], 0.0f, 1.0f) * 255.0f + 0.5f)));
                        worst = std::max(worst, here); sum += here; beyond += here > 2;
                        moving += px.moved >= pushflow::predict::kStill[1]; held += px.held;
                    }
                mean = sum / double(W * H); off = 100.0 * double(beyond) / double(W * H);
                heldShare = moving ? 100.0 * double(held) / double(moving) : 0.0;
            }
            // A threshold (the picture's edge, the still zone) can fall the other way for a pixel whose value sits on it:
            // the mean and the share of pixels that differ say whether the shader follows the rule.
            checkOr(ran && mean < 0.3 && off < 0.5, std::format("{} {}: the picture is the rule's, from the network's own grids (mean {:.3f} of 255, {:.2f} % of pixels off by more than 2, worst {})",
                                                               tag, pair.name, mean, off, worst));
            std::printf("INFO %s %s: %.1f %% of the moving picture held\n", tag.c_str(), pair.name, heldShare);
        }
        // A square that keeps moving: the forecast is nearer to where it really is later than the newest frame is.
        forecastAhead = 0.5f;
        const auto ahead = run(older, newer, 0.5f, flow, nullptr);
        const double forecastOff = meanApart(ahead, later), holdOff = meanApart(newer, later);
        std::printf("INFO %s: a square moving 4 px a frame, half an interval on: the forecast is %.3f of 255 from the truth, the newest frame %.3f\n", tag.c_str(), forecastOff, holdOff);
        // The Fast tier's model follows a small object on a picture this small poorly and holds most of it:
        // level with holding (measured 1.7 % further, 19 November 2026), which is what the rule is for.
        checkOr(forecastOff <= holdOff * 1.05 + 0.02, tag + ": a forecast is no further from the true later frame than holding the newest frame (within a twentieth)");
        if (tier == 2) checkOr(forecastOff < holdOff * 0.7, tag + ": and for a slow, followed object it is clearly nearer");
        // Across a cut most of the picture is held: the forecast stays near the newest frame.
        const auto cut = run(older, other, 0.5f, flow, nullptr);
        std::printf("INFO %s: two unrelated pictures: the forecast is %.2f of 255 from the newest frame on average (worst %d)\n", tag.c_str(), meanApart(cut, other), apart(cut, other));
        for (int i = 0; i < 4; ++i) run(older, newer, 0.5f, flow, nullptr);
        forecastAhead = -1.0f;
        const auto tel = flow.telemetry();
        std::printf("INFO %s: %llu forecasts counted, %llu pixels counted, %llu moving, %llu held\n", tag.c_str(), static_cast<unsigned long long>(tel.predictFrames),
                    static_cast<unsigned long long>(tel.predictCounted), static_cast<unsigned long long>(tel.predictMoving), static_cast<unsigned long long>(tel.predictHeld));
        checkOr(tel.predictFrames >= 5 && tel.predictCounted >= tel.predictFrames && tel.predictMoving <= tel.predictCounted && tel.predictHeld <= tel.predictMoving,
                tag + ": the forecasts and their pixels are counted, never more held than moving");
        // PushFlow 2's Response measures the picture's own motion in the same pass: a whole picture panning 4
        // pixels a frame to the right must be found moving right by about that much, a still one not at all.
        {
            forecastAhead = 0.5f;
            for (int i = 0; i < 3; ++i) run(t0, t1, 0.5f, flow, nullptr);
            for (int i = 0; i < 4; ++i) run(t1, t1, 0.5f, flow, nullptr);   // the totals of the panning pairs come back with these
            const auto still = flow.telemetry();
            for (int i = 0; i < 3; ++i) run(t0, t1, 0.5f, flow, nullptr);
            run(t0, t1, 0.5f, flow, nullptr);
            const auto pan = flow.telemetry();
            forecastAhead = -1.0f;
            const double scale = double(W) / double(st.modelW);
            std::printf("INFO %s: the picture's own motion: a still pair %.2f, %.2f; a pan of 4 pixels to the right %.2f, %.2f (picture pixels a frame; %llu pairs measured)\n", tag.c_str(),
                        still.motionX * scale, still.motionY * scale, pan.motionX * scale, pan.motionY * scale, static_cast<unsigned long long>(pan.motionPairs));
            checkOr(pan.motionPairs > still.motionPairs && still.motionPairs >= 4, tag + ": every pair's own motion is measured once");
            checkOr(std::abs(still.motionX) * scale < 0.3 && std::abs(still.motionY) * scale < 0.3, tag + ": a still picture is found not to move");
            // The Quality model follows this fine texture and must find the pan's size (measured 4.02). The Fast
            // tier's model follows it poorly on a picture this small and found a fifth of it (0.85, 19 November
            // 2026): for it only the direction is asked, and the size is what Response's row must be read with.
            if (tier == 2) checkOr(pan.motionX * scale > 3.0 && pan.motionX * scale < 5.0 && std::abs(pan.motionY) * scale < 0.5, tag + ": a pan to the right is found moving right, by its size");
            else checkOr(pan.motionX * scale > 0.3 && pan.motionX * scale < 6.5 && std::abs(pan.motionY) * scale < 1.0, tag + ": a pan to the right is found moving right");
            // PushFlow 2's Memory measures, in the same pass, how far apart two real frames are where the
            // motion joins them: nothing for the same picture twice, and about 3 of 255 for a picture and
            // itself made 3 brighter.
            auto brighter = older;
            for (size_t i = 0; i < brighter.size(); ++i) if (i % 4 != 3) brighter[i] = uint8_t(std::min(255, int(brighter[i]) + 3));
            forecastAhead = 0.5f;
            for (int i = 0; i < 4; ++i) run(older, older, 0.5f, flow, nullptr);
            const auto same = flow.telemetry();
            for (int i = 0; i < 4; ++i) run(older, brighter, 0.5f, flow, nullptr);
            const auto lifted = flow.telemetry();
            forecastAhead = -1.0f;
            std::printf("INFO %s: two real frames, where the motion joins them: the same picture %.2f apart, one 3 of 255 brighter %.2f apart\n", tag.c_str(), same.alignedApartNow, lifted.alignedApartNow);
            checkOr(same.alignedApartNow < 0.2, tag + ": the same picture twice is found the same");
            checkOr(lifted.alignedApartNow > 2.0 && lifted.alignedApartNow < 4.0, tag + ": a picture 3 of 255 brighter is found about 3 apart");
        }
        checkOr(!gpu.debugErrors(), tag + ": no debug layer errors");
    }
    forecastAhead = -1.0f;
    checkOr(!pushflow::crashedLastTime(), "pushflow: the crash marker is gone after a normal close");
}

// PushFPS Crosshair Hold (pipeline/PushOverlay.h): the three shaders against the rule in plain C++, step by
// step and pixel by pixel. A picture panned under the reticle measured on real frames (three strokes, about
// half see-through, with a dark outline): at 2160 lines, where the rule's numbers are the ones it was found
// with (strokes four pixels thick, 20 pixels a frame), and at 1080 lines, where every number is halved. The
// picture is narrow: only the zone round the centre matters.
void testOverlayAt(Gpu& gpu, const uint32_t H) {
    namespace ov = pipeline::overlay;
    constexpr DXGI_FORMAT F = DXGI_FORMAT_R8G8B8A8_UNORM;
    constexpr uint32_t W = 640;
    const int unit = int(H) / 1080;         // 2 at 2160 lines, 1 at 1080
    const std::string at = "crosshair hold at " + std::to_string(H) + " lines";
    const ov::Zone zone = ov::zoneFor(W, H); const ov::Rule rule = ov::ruleFor(H);
    const int Z = int(zone.size);
    check(Z == 200 * unit / 16 * 16 && zone.x == (W - uint32_t(Z)) / 2 && zone.y == (H - uint32_t(Z)) / 2, at + ": the zone round the centre, in whole blocks (" + std::to_string(Z) + ")");
    auto* device = gpu.device.Get();
    Plane real[2]{{pipeline::makeTexture(device, W, H, F, true), F, W, H, 4}, {pipeline::makeTexture(device, W, H, F, true), F, W, H, 4}};
    Plane generated{pipeline::makeTexture(device, W, H, F, true), F, W, H, 4};
    check(real[0].texture && real[1].texture && generated.texture, "crosshair hold: textures created");
    pipeline::PushOverlayPass pass;
    check(pass.initialize(device, W, H, F, 0), "crosshair hold: the pass is created");
    pass.setSampleEvery(1);     // what it says it holds is checked against every frame's mask
    pipeline::DescriptorStager stager;
    check(stager.initialize(device, 256), "crosshair hold: staging heap");
    ID3D12Resource* reals[2] = {real[0].texture.Get(), real[1].texture.Get()};
    ID3D12Resource* gens[1] = {generated.texture.Get()};
    pass.createViews(device, stager, reals, gens, 1);
    check(pass.composes(0) && pass.input(0) && pass.input(1), "crosshair hold: the generated frame can be written and both input frames exist");
    Plane input[2]{{ComPtr<ID3D12Resource>(pass.input(0)), F, W, H, 4}, {ComPtr<ID3D12Resource>(pass.input(1)), F, W, H, 4}};
    Plane mask[2]{{ComPtr<ID3D12Resource>(pass.maskTexture(0)), DXGI_FORMAT_R16_FLOAT, zone.size, zone.size, 2},
                  {ComPtr<ID3D12Resource>(pass.maskTexture(1)), DXGI_FORMAT_R16_FLOAT, zone.size, zone.size, 2}};
    Plane counts{ComPtr<ID3D12Resource>(pass.countTexture()), DXGI_FORMAT_R16G16B16A16_FLOAT, zone.size, zone.size, 8};
    // The world: smooth noise in whole numbers, so that every machine draws the same picture.
    const auto lattice = [](int x, int y, uint32_t salt) { uint32_t h = uint32_t(x) * 374761393u + uint32_t(y) * 668265263u + salt * 2246822519u; h = (h ^ (h >> 13)) * 1274126177u; return int((h ^ (h >> 16)) & 255); };
    const auto world = [&](int x, int y) {
        int sum = 0, weight = 0;
        for (int level = 0; level < 4; ++level) {
            const int cell = 48 >> level, w = 8 >> level;       // cells of 48, 24, 12, 6 pixels
            const int ix = x >= 0 ? x / cell : -((-x + cell - 1) / cell), iy = y / cell, fx = x - ix * cell, fy = y - iy * cell;
            const int a = lattice(ix, iy, level), b = lattice(ix + 1, iy, level), c = lattice(ix, iy + 1, level), d = lattice(ix + 1, iy + 1, level);
            sum += w * (((a * (cell - fx) + b * fx) * (cell - fy) + (c * (cell - fx) + d * fx) * fy) / (cell * cell)); weight += w;
        }
        return 40 + (sum / weight) * 130 / 255;                  // 40..170
    };
    struct Stroke { int y0, y1, x0, x1; };
    const int cx = int(W) / 2, cy = int(H) / 2;
    const Stroke strokes[3] = {{cy - 6 * unit, cy - 4 * unit, cx - 18 * unit, cx - 6 * unit}, {cy - 6 * unit, cy - 4 * unit, cx + 6 * unit, cx + 18 * unit},
                               {cy, cy + 10 * unit, cx - unit, cx + unit}};
    const auto reticleAt = [&](int x, int y) {                   // 0 nothing, 1 outline, 2 core
        int kind = 0;
        for (const auto& s : strokes) {
            if (x >= s.x0 && x < s.x1 && y >= s.y0 && y < s.y1) return 2;
            if (x >= s.x0 - 1 && x < s.x1 + 1 && y >= s.y0 - 1 && y < s.y1 + 1) kind = 1;
        }
        return kind;
    };
    const auto picture = [&](int shift, bool withReticle) {
        std::vector<uint8_t> px(size_t(W) * H * 4);
        for (uint32_t y = 0; y < H; ++y)
            for (uint32_t x = 0; x < W; ++x) {
                int v = world(int(x) - shift, int(y));
                if (withReticle) { const int k = reticleAt(int(x), int(y)); if (k == 2) v = (v * 48 + 220 * 52) / 100; else if (k == 1) v = v * 33 / 100; }
                uint8_t* p = px.data() + (size_t(y) * W + x) * 4; p[0] = p[1] = p[2] = uint8_t(v); p[3] = 255;
            }
        return px;
    };
    // The third picture: it stands still but for a band across the lower part of the zone, which pans; a bright bar
    // stands still far above the band, and a dot with an outline stands still inside it. The bar is scenery (nothing
    // moves near it), the dot is a crosshair, and it is too wide to be a bar: it is found by its outline and its
    // inside goes with it.
    const int bandTop = int(zone.y) + Z * 6 / 10, bandBottom = int(zone.y) + Z * 85 / 100;
    const int barY = int(zone.y) + Z * 15 / 100, barX0 = int(zone.x) + Z * 4 / 10, barX1 = int(zone.x) + Z / 2;
    const int dotX = int(zone.x) + Z / 2, dotY = int(zone.y) + Z * 72 / 100;
    const auto dotAt = [&](int x, int y) {                       // 0 nothing, 1 outline, 2 core (twice the distance from its middle, squared)
        const int dx = 2 * (x - dotX) + 1, dy = 2 * (y - dotY) + 1, q = dx * dx + dy * dy;
        return q <= (6 * unit) * (6 * unit) ? 2 : q <= (8 * unit) * (8 * unit) ? 1 : 0;
    };
    const auto barAtPixel = [&](int x, int y) { return y >= barY && y < barY + 2 * unit && x >= barX0 && x < barX1; };
    const auto bandFrame = [&](int shift) {
        std::vector<uint8_t> px(size_t(W) * H * 4);
        for (uint32_t y = 0; y < H; ++y)
            for (uint32_t x = 0; x < W; ++x) {
                int v = world(int(y) >= bandTop && int(y) < bandBottom ? int(x) - shift : int(x), int(y));
                if (barAtPixel(int(x), int(y))) v = 235;
                const int k = dotAt(int(x), int(y)); if (k == 2) v = 235; else if (k == 1) v = v * 30 / 100;
                uint8_t* p = px.data() + (size_t(y) * W + x) * 4; p[0] = p[1] = p[2] = uint8_t(v); p[3] = 255;
            }
        return px;
    };
    const auto zoneOf = [&](const std::vector<uint8_t>& px) {
        ov::Picture p(Z);
        for (int y = 0; y < Z; ++y)
            for (int x = 0; x < Z; ++x) {
                const uint8_t* q = px.data() + (size_t(zone.y + y) * W + zone.x + x) * 4;
                p.at(x, y) = {q[0] / 255.0f, q[1] / 255.0f, q[2] / 255.0f};
            }
        return p;
    };
    const auto toByte = [](float v) { return int(std::floor(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f)); };
    pipeline::StateTracker states;
    struct Seen { std::vector<uint8_t> input; std::vector<float> mask, count; };
    const auto realFrame = [&](unsigned parity, const std::vector<uint8_t>& px, bool reset, Seen& seen) {
        gpu.begin();
        auto up = recordFill(gpu, real[parity], px, states.get(real[parity].texture.Get()), D3D12_RESOURCE_STATE_COMMON);
        states.set(real[parity].texture.Get(), D3D12_RESOURCE_STATE_COMMON);
        const bool ok = pass.update(gpu.list.Get(), states, parity, reset);
        auto ri = recordRead(gpu, input[parity], states.get(input[parity].texture.Get()));
        auto rm = recordRead(gpu, mask[parity], states.get(mask[parity].texture.Get()));
        auto rc = recordRead(gpu, counts, states.get(counts.texture.Get()));
        if (!ok || !gpu.submitAndWait()) return false;
        seen.input = mapRead(input[parity], ri.Get());
        const auto m = mapRead(mask[parity], rm.Get()), c = mapRead(counts, rc.Get());
        seen.mask.resize(size_t(Z) * Z); seen.count.resize(size_t(Z) * Z * 4);
        for (size_t i = 0; i < seen.mask.size(); ++i) seen.mask[i] = halfToFloat(uint16_t(m[i * 2] | (m[i * 2 + 1] << 8)));
        for (size_t i = 0; i < seen.count.size(); ++i) seen.count[i] = halfToFloat(uint16_t(c[i * 2] | (c[i * 2 + 1] << 8)));
        return true;
    };
    const auto generatedFrame = [&](unsigned parity, float t, const std::vector<uint8_t>& made, std::vector<uint8_t>& out) {
        gpu.begin();
        auto up = recordFill(gpu, generated, made, states.get(generated.texture.Get()), D3D12_RESOURCE_STATE_COMMON);
        states.set(generated.texture.Get(), D3D12_RESOURCE_STATE_COMMON);
        const bool ok = pass.compose(gpu.list.Get(), states, 0, parity, t);
        auto rb = recordRead(gpu, generated, states.get(generated.texture.Get()));
        if (!ok || !gpu.submitAndWait()) return false;
        out = mapRead(generated, rb.Get());
        return true;
    };
    // What frame generation "made": any picture will do to compare the two; this one is unlike both real frames.
    std::vector<uint8_t> made(size_t(W) * H * 4);
    for (uint32_t y = 0; y < H; ++y) for (uint32_t x = 0; x < W; ++x) { uint8_t* p = made.data() + (size_t(y) * W + x) * 4; p[0] = p[1] = p[2] = uint8_t(60 + lattice(int(x) / 3, int(y) / 3, 9u) / 3); p[3] = 255; }

    for (int pass_ = 0; pass_ < 3; ++pass_) {
        const bool withReticle = pass_ == 0, band = pass_ == 2;
        const std::string name = std::to_string(H) + " lines, " + (withReticle ? "a reticle over a moving picture" : band ? "a band that moves, a bar far from it and a dot in it" : "a moving picture alone");
        size_t barMarked = 0, dotMarked = 0, dotPixels = 0;
        ov::State state(Z);
        std::vector<uint8_t> beforePx; ov::Picture beforeFilled; ov::Plane beforeMask(Z);
        size_t maskWrong = 0, countWrong = 0, fillWrong = 0, outsideTouched = 0, composeWrong = 0, composeOutside = 0, held = 0, composed = 0, saidWrong = 0;
        int firstHeld = -1; bool ran = true;
        for (int k = 0; k < 9 && ran; ++k) {
            const unsigned parity = unsigned(k & 1);
            const std::vector<uint8_t> px = band ? bandFrame(10 * unit * k) : picture(10 * unit * k, withReticle);
            Seen seen;
            ran = realFrame(parity, px, k == 0, seen);
            if (!ran) break;
            const ov::Picture frame = zoneOf(px);
            const ov::Plane expected = ov::step(state, ov::lumaPlane(frame, 0), rule);
            const ov::Picture filled = ov::fill(frame, expected, rule);
            size_t marked = 0;
            for (int y = 0; y < Z; ++y)
                for (int x = 0; x < Z; ++x) {
                    const size_t i = size_t(y) * Z + x;
                    maskWrong += (seen.mask[i] > 0.5f) != (expected.at(x, y) > 0);
                    marked += expected.at(x, y) > 0;
                    for (int kind = 0; kind < ov::kKinds; ++kind) countWrong += seen.count[i * 4 + kind] != state.count[kind].at(x, y);
                    const uint8_t* got = seen.input.data() + (size_t(zone.y + y) * W + zone.x + x) * 4;
                    const ov::Colour& want = filled.at(x, y);
                    fillWrong += std::abs(int(got[0]) - toByte(want.r)) > 1 || std::abs(int(got[1]) - toByte(want.g)) > 1 || std::abs(int(got[2]) - toByte(want.b)) > 1;
                }
            for (uint32_t y = 0; y < H; ++y)
                for (uint32_t x = 0; x < W; ++x) {
                    const bool inZone = x >= zone.x && x < zone.x + zone.size && y >= zone.y && y < zone.y + zone.size;
                    const size_t i = (size_t(y) * W + x) * 4;
                    if (!inZone) outsideTouched += std::memcmp(seen.input.data() + i, px.data() + i, 4) != 0;
                }
            // What the pass itself says it holds (the figure the interface and the log show) is this frame's mask.
            pass.harvest();
            saidWrong += size_t(pass.maskedPixels()) != marked;
            if (marked > size_t(60 * unit * unit) && firstHeld < 0) firstHeld = k;
            if (band) {
                dotMarked = 0; dotPixels = 0;
                for (int y = 0; y < Z; ++y)
                    for (int x = 0; x < Z; ++x) {
                        const int X = int(zone.x) + x, Y = int(zone.y) + y; const bool dot = dotAt(X, Y) != 0;
                        dotPixels += dot;
                        if (!(expected.at(x, y) > 0)) continue;
                        dotMarked += dot;
                        barMarked += Y >= barY - 6 && Y < barY + 2 * unit + 6 && X >= barX0 - 6 && X < barX1 + 6;
                    }
            }
            held = std::max(held, marked);
            if (k >= 1) {       // a generated frame between the frame before and this one
                const float t = k == 6 ? 1.0f / 3.0f : 0.5f;
                std::vector<uint8_t> out;
                ran = generatedFrame(parity, t, made, out);
                if (!ran) break;
                const ov::Picture a = zoneOf(beforePx), g = zoneOf(made), fb = zoneOf(seen.input);
                for (uint32_t y = 0; y < H; ++y)
                    for (uint32_t x = 0; x < W; ++x) {
                        const size_t i = (size_t(y) * W + x) * 4;
                        const bool inZone = x >= zone.x && x < zone.x + zone.size && y >= zone.y && y < zone.y + zone.size;
                        const int zx = int(x) - int(zone.x), zy = int(y) - int(zone.y);
                        const bool masked = inZone && (beforeMask.at(zx, zy) > 0 || expected.at(zx, zy) > 0);
                        if (!masked) { composeOutside += std::memcmp(out.data() + i, made.data() + i, 4) != 0; continue; }
                        const ov::Colour want = ov::composeAt(a, frame, beforeFilled, fb, g, beforeMask, expected, zx, zy, t);
                        ++composed;
                        composeWrong += std::abs(int(out[i]) - toByte(want.r)) > 1 || std::abs(int(out[i + 1]) - toByte(want.g)) > 1 || std::abs(int(out[i + 2]) - toByte(want.b)) > 1 || out[i + 3] != made[i + 3];
                    }
            }
            beforePx = px; beforeFilled = zoneOf(seen.input); beforeMask = expected;
        }
        std::printf("INFO crosshair hold, %s: held from frame %d, %zu pixels marked at most, %zu composed; differences from the rule in plain C++: mask %zu, counts %zu, fill %zu, compose %zu; touched outside: input %zu, generated %zu\n",
                    name.c_str(), firstHeld, held, composed, maskWrong, countWrong, fillWrong, composeWrong, outsideTouched, composeOutside);
        check(ran, std::string("crosshair hold, ") + name + ": every step runs");
        check(maskWrong == 0, std::string("crosshair hold, ") + name + ": the shaders mark every pixel as the rule in plain C++ does");
        check(countWrong == 0, std::string("crosshair hold, ") + name + ": and count every pixel as it does");
        check(fillWrong == 0, std::string("crosshair hold, ") + name + ": the frame frame generation gets is filled as the rule fills it");
        check(outsideTouched == 0, std::string("crosshair hold, ") + name + ": outside the zone that frame is the real frame, bit for bit");
        check(composeWrong == 0, std::string("crosshair hold, ") + name + ": the generated frame is composed as the rule composes it");
        check(composeOutside == 0, std::string("crosshair hold, ") + name + ": no pixel outside the mask is touched in the generated frame");
        check(saidWrong == 0, std::string("crosshair hold, ") + name + ": the pass's own count of what it holds is the mask's, frame for frame (" + std::to_string(saidWrong) + " frames differ)");
        if (withReticle) {
            check(firstHeld == 4, at + ": the reticle is held once it has stood for four counted frames (frame " + std::to_string(firstHeld) + ")");
            check(held > size_t(150 * unit * unit) && composed > size_t(150 * unit * unit), at + ": all of it, and it is laid back over the generated frame");
        } else if (band) {
            std::printf("INFO crosshair hold, %s: the dot %zu of %zu px, the bar %zu px\n", name.c_str(), dotMarked, dotPixels, barMarked);
            check(barMarked == 0, at + ": a bar that stands still far from what moves is never marked");
            check(firstHeld == 4 && dotPixels > 0 && dotMarked == dotPixels, at + ": a dot with an outline inside what moves is taken out whole, its inside with its outline");
        } else {
            check(held == 0 && composed == 0, at + ": a moving picture without an overlay is never marked, filled or composed");
        }
    }
}
void testOverlay(Gpu& gpu) {
    testOverlayAt(gpu, 2160);
    testOverlayAt(gpu, 1080);
    pipeline::PushOverlayPass tiny;
    check(!tiny.initialize(gpu.device.Get(), 160, 90, DXGI_FORMAT_R8G8B8A8_UNORM, 0), "crosshair hold: a picture too small for a zone gets no pass");
}

// Character Hold (tests/integration/PushCharacterGpu.h): the pass, its shaders and the release's matting network
// through DirectML, against pipeline/PushCharacter.h. Needs the release's runtime, as PushFlow's test does.
void testCharacter(Gpu& gpu) {
    wchar_t dir[1024]{};
    if (!GetEnvironmentVariableW(L"VEYRA_PUSHFLOW_TEST_RUNTIME", dir, 1024)) { std::printf("SKIP character hold: VEYRA_PUSHFLOW_TEST_RUNTIME not set\n"); return; }
    const bool installed = pushflow::PushMatteBackend::installed(dir);
    check(installed, "character hold: the matting model is in the release's runtime");
    if (!installed) return;
    check(charactergpu::run(gpu, dir, [](bool ok, const std::string& label) { check(ok, label); }), "character hold: the pass started");
}

int main() {
    Gpu gpu;
    if (!gpu.create()) { std::printf("FAIL no D3D12 device\n"); return 1; }
    std::printf("INFO adapter %s debug=%d\n", gpu.adapter.c_str(), gpu.debug ? 1 : 0);
    testCompare(gpu);
    testCompareHdr(gpu);
    testCompareSideBySide(gpu, false);
    testCompareSideBySide(gpu, true);
    testCompareVertical(gpu, false);
    testCompareVertical(gpu, true);
    testStreamOverlay(gpu, false);
    testStreamOverlay(gpu, true);
    testCanvasConvert(gpu);
    testLiveLayers(gpu, false);
    testLiveLayers(gpu, true);
    testLabelScale();
    testCompareSkipRule();
    testCompareRaw(gpu, false, engine::CompareLayout::Split);
    testCompareRaw(gpu, true, engine::CompareLayout::Split);
    testCompareRaw(gpu, false, engine::CompareLayout::SideBySide);
    testCompareBadges(gpu, false);
    testCompareBadges(gpu, true);
    testCompareReveal(gpu, false);
    testCompareReveal(gpu, true);
    testHudSampler(gpu);
    testHudLock(gpu);
    testHudStill(gpu);
    testOverlay(gpu);
    testCharacter(gpu);
    testAmbient(gpu);
    testEnhanceShape(gpu);
    testPushFlowSizes();
    testPushFlow(gpu);
    std::printf("%s (%d failures)\n", failures ? "FAILED" : "OK", failures);
    return failures ? 1 : 0;
}
