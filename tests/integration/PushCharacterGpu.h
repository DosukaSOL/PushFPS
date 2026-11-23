// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Character Hold on a real D3D12 device (the software renderer on build machines), with the release's own
// matting network: tests/integration/PushGpuTests.cpp runs this in the Windows build, and
// tools/character_probe runs it alone in a few minutes (the pushflow-models workflow).
//   - the network, through DirectML, finds a drawn figure and nothing else;
//   - the shaders give what pipeline/PushCharacter.h gives: the matte and the matted pictures from the
//     network's own answer, the blocks' motion (whole numbers decide, so it must be the same), the field and
//     the trust; and for a generated frame the frame at the fine size, every block's table, its place as found
//     and as settled, how far the frame is apart from the placed figure, where it differs, and every pixel the
//     figure is laid over, at a half and at a third;
//   - a generated frame in which the figure is right is left alone, a head the method showed as the world
//     comes back, and a frame that is neither real frame gets the figure laid over whole;
//   - nothing is written where nothing is laid over; a start-over forgets the frame before; a picture
//     without a person leaves generated frames untouched.
// `Gpu` is the test's device: device, queue, list, begin(), submitAndWait(), debugErrors().
#pragma once
#include "veyra/pipeline/PushCharacterPass.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace charactergpu {
using Microsoft::WRL::ComPtr;
namespace ch = veyra::pipeline::character;

inline ComPtr<ID3D12Resource> plainBuffer(ID3D12Device* device, uint64_t bytes, D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES hp{}; hp.Type = type;
    D3D12_RESOURCE_DESC bd{}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = bytes; bd.Height = 1;
    bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> r;
    const auto state = type == D3D12_HEAP_TYPE_UPLOAD ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_DEST;
    if (FAILED(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, state, nullptr, IID_PPV_ARGS(&r)))) return nullptr;
    return r;
}
inline void change(ID3D12GraphicsCommandList* list, ID3D12Resource* r, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
    if (from == to) return;
    D3D12_RESOURCE_BARRIER b{}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource = r;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; b.Transition.StateBefore = from; b.Transition.StateAfter = to;
    list->ResourceBarrier(1, &b);
}
// A texture's content, copied for reading once the list has run. The texture is in `state` and stays there.
struct Shot {
    ComPtr<ID3D12Resource> readback;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    uint32_t w = 0, h = 0, bpp = 0;
    std::vector<uint8_t> develop() const {
        std::vector<uint8_t> out(size_t(w) * h * bpp);
        uint8_t* mapped = nullptr;
        if (!readback || FAILED(readback->Map(0, nullptr, reinterpret_cast<void**>(&mapped))) || !mapped) return {};
        for (uint32_t y = 0; y < h; ++y) std::memcpy(out.data() + size_t(y) * w * bpp, mapped + footprint.Offset + size_t(y) * footprint.Footprint.RowPitch, size_t(w) * bpp);
        const D3D12_RANGE none{0, 0};
        readback->Unmap(0, &none);
        return out;
    }
};
inline Shot shoot(ID3D12Device* device, ID3D12GraphicsCommandList* list, ID3D12Resource* texture, D3D12_RESOURCE_STATES state, uint32_t bpp) {
    Shot s;
    const auto desc = texture->GetDesc();
    UINT64 total = 0;
    device->GetCopyableFootprints(&desc, 0, 1, 0, &s.footprint, nullptr, nullptr, &total);
    s.readback = plainBuffer(device, total, D3D12_HEAP_TYPE_READBACK);
    s.w = uint32_t(desc.Width); s.h = desc.Height; s.bpp = bpp;
    change(list, texture, state, D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION from{}, to{};
    from.pResource = texture; from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    to.pResource = s.readback.Get(); to.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; to.PlacedFootprint = s.footprint;
    list->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);
    change(list, texture, D3D12_RESOURCE_STATE_COPY_SOURCE, state);
    return s;
}
// Tight RGBA rows into a texture that is in the COMMON state and stays there. Keep the returned buffer until the list ran.
inline ComPtr<ID3D12Resource> fill(ID3D12Device* device, ID3D12GraphicsCommandList* list, ID3D12Resource* texture, const std::vector<uint8_t>& pixels) {
    const auto desc = texture->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT64 total = 0;
    device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, nullptr, nullptr, &total);
    auto upload = plainBuffer(device, total, D3D12_HEAP_TYPE_UPLOAD);
    uint8_t* mapped = nullptr;
    if (!upload || FAILED(upload->Map(0, nullptr, reinterpret_cast<void**>(&mapped))) || !mapped) return nullptr;
    const size_t row = size_t(desc.Width) * 4;
    for (uint32_t y = 0; y < desc.Height; ++y) std::memcpy(mapped + footprint.Offset + size_t(y) * footprint.Footprint.RowPitch, pixels.data() + size_t(y) * row, row);
    upload->Unmap(0, nullptr);
    change(list, texture, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_TEXTURE_COPY_LOCATION from{}, to{};
    from.pResource = upload.Get(); from.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; from.PlacedFootprint = footprint;
    to.pResource = texture; to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    list->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);
    change(list, texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
    return upload;
}
// A tensor's floats (its buffer is in the UNORDERED_ACCESS state and stays there).
inline ComPtr<ID3D12Resource> shootFloats(ID3D12Device* device, ID3D12GraphicsCommandList* list, ID3D12Resource* tensor, uint64_t floats) {
    auto readback = plainBuffer(device, floats * 4, D3D12_HEAP_TYPE_READBACK);
    change(list, tensor, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    list->CopyBufferRegion(readback.Get(), 0, tensor, 0, floats * 4);
    change(list, tensor, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    return readback;
}
inline std::vector<float> developFloats(ID3D12Resource* readback, uint64_t floats) {
    std::vector<float> out(size_t(floats), 0.0f);
    void* mapped = nullptr;
    if (!readback || FAILED(readback->Map(0, nullptr, &mapped)) || !mapped) return {};
    std::memcpy(out.data(), mapped, size_t(floats) * 4);
    const D3D12_RANGE none{0, 0};
    readback->Unmap(0, &none);
    return out;
}

// The drawn scene: a figure (a body and a head) before a world that changes quickly, as the model's own
// check draws it (scripts/pushflow/prepare_matte.py, picture()). `step` moves the figure 9 across and 3 down
// at the network's size; without a figure only the world is drawn.
struct Drawn { int w, h; std::vector<uint8_t> rgba; };
inline void figureCentre(float step, float& cx, float& cy) { cx = 900.0f + 9.0f * step; cy = 560.0f + 3.0f * step; }
inline Drawn draw(int w, int h, float step, bool figure) {
    Drawn d{w, h, std::vector<uint8_t>(size_t(w) * h * 4)};
    float cx = 0, cy = 0;
    figureCentre(step, cx, cy);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float u = (float(x) + 0.5f) * 1920.0f / float(w), v = (float(y) + 0.5f) * 1080.0f / float(h);   // the network's pixels
            float c[3] = {0.35f + 0.25f * std::sin(u / 97.0f + step), 0.40f + 0.20f * std::cos(v / 61.0f + 0.7f * step), 0.45f + 0.15f * std::sin((u + v) / 131.0f)};
            if (figure) {
                const float bx = (u - cx) / 150.0f, by = (v - cy - 140.0f) / 330.0f, hx = (u - cx) / 80.0f, hy = (v - cy + 250.0f) / 95.0f;
                if (bx * bx + by * by < 1.0f) { c[0] = 0.62f; c[1] = 0.12f; c[2] = 0.20f; }
                if (hx * hx + hy * hy < 1.0f) { c[0] = 0.55f; c[1] = 0.40f; c[2] = 0.30f; }
            }
            uint8_t* p = &d.rgba[(size_t(y) * w + x) * 4];
            for (int k = 0; k < 3; ++k) p[k] = ch::toByte(c[k]);
            p[3] = 255;
        }
    return d;
}
// Where a point of the network's picture lies in the drawn figure at `step`: under 1 inside it (0 at a middle),
// above 1 outside.
inline float figureInside(float step, float u, float v) {
    float cx = 0, cy = 0;
    figureCentre(step, cx, cy);
    const float bx = (u - cx) / 150.0f, by = (v - cy - 140.0f) / 330.0f, hx = (u - cx) / 80.0f, hy = (v - cy + 250.0f) / 95.0f;
    return (std::min)(bx * bx + by * by, hx * hx + hy * hy);
}
// A generated frame that is neither real frame: stripes, so every laid-over pixel shows.
inline Drawn stripes(int w, int h) {
    Drawn d{w, h, std::vector<uint8_t>(size_t(w) * h * 4)};
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            uint8_t* p = &d.rgba[(size_t(y) * w + x) * 4];
            p[0] = uint8_t(40 + ((x / 7) % 2) * 150); p[1] = uint8_t(60 + ((y / 5) % 2) * 120); p[2] = uint8_t((x + y) % 251); p[3] = uint8_t(200 + (x % 50));
        }
    return d;
}
inline float byteBetween(const std::vector<uint8_t>& rgba, int w, int h, int c, float x, float y) {
    x = x < 0.0f ? 0.0f : (x > float(w - 1) ? float(w - 1) : x);
    y = y < 0.0f ? 0.0f : (y > float(h - 1) ? float(h - 1) : y);
    const int x0 = int(x), y0 = int(y), x1 = (std::min)(x0 + 1, w - 1), y1 = (std::min)(y0 + 1, h - 1);
    const float fx = x - float(x0), fy = y - float(y0);
    const auto at = [&](int px, int py) { return float(rgba[(size_t(py) * w + px) * 4 + c]) * (1.0f / 255.0f); };
    const float top = at(x0, y0) + (at(x1, y0) - at(x0, y0)) * fx, bottom = at(x0, y1) + (at(x1, y1) - at(x0, y1)) * fx;
    return top + (bottom - top) * fy;
}

// Runs the whole test; `check` records one result. Returns false when the pass could not even start.
template <class Gpu>
bool run(Gpu& gpu, const std::wstring& runtimeDir, const std::function<void(bool, const std::string&)>& check) {
    constexpr DXGI_FORMAT F = DXGI_FORMAT_R8G8B8A8_UNORM;
    constexpr int W = 1280, H = 720;
    constexpr D3D12_RESOURCE_STATES kCommon = D3D12_RESOURCE_STATE_COMMON;
    auto* device = gpu.device.Get();
    const std::string name = "character hold: ";
    ComPtr<ID3D12Resource> real[2] = {veyra::pipeline::makeTexture(device, W, H, F, true), veyra::pipeline::makeTexture(device, W, H, F, true)};
    ComPtr<ID3D12Resource> generated = veyra::pipeline::makeTexture(device, W, H, F, true);
    check(real[0] && real[1] && generated, name + "textures created");
    veyra::pipeline::PushCharacterPass pass;
    const bool started = pass.initialize(device, gpu.queue.Get(), runtimeDir, W, H, F, 0);
    check(started && pass.ready() && pass.state() == "on", name + "the pass and its network start (" + pass.state() + ")");
    if (!started) return false;
    std::printf("  %s\n", pass.describe().c_str());
    pass.setSampleEvery(1);
    veyra::pipeline::DescriptorStager stager;
    check(stager.initialize(device, 256), name + "staging heap");
    ID3D12Resource* reals[2] = {real[0].Get(), real[1].Get()};
    ID3D12Resource* gens[1] = {generated.Get()};
    pass.createViews(device, stager, reals, gens, 1);
    veyra::pipeline::StateTracker tracker;
    const uint64_t plane = uint64_t(ch::kMatteW) * ch::kMatteH;

    struct Frame { std::vector<float> picture, answer; std::vector<uint8_t> matte8, fine, coarse, blocks, field; Drawn drawn; bool ok = false; };
    // One real frame through the pass, with everything it left read back.
    const auto step = [&](unsigned parity, const Drawn& drawn, bool startOver) {
        Frame f; f.drawn = drawn;
        if (!gpu.begin()) return f;
        auto upload = fill(device, gpu.list.Get(), real[parity].Get(), drawn.rgba);
        const bool packed = pass.pack(gpu.list.Get(), tracker, parity);
        if (!gpu.submitAndWait() || !packed) return f;
        if (!pass.run(startOver)) return f;
        if (!gpu.begin()) return f;
        const bool finished = pass.finish(gpu.list.Get(), tracker, parity);
        auto pictureRead = shootFloats(device, gpu.list.Get(), pass.pictureBuffer(), plane * 3);
        auto answerRead = shootFloats(device, gpu.list.Get(), pass.answerBuffer(), plane);
        const Shot matte = shoot(device, gpu.list.Get(), pass.matteTexture(parity), kCommon, 1);
        const Shot fine = shoot(device, gpu.list.Get(), pass.fineTexture(parity), kCommon, 4);
        const Shot coarse = shoot(device, gpu.list.Get(), pass.coarseTexture(parity), kCommon, 4);
        const Shot blocks = shoot(device, gpu.list.Get(), pass.blocksTexture(), kCommon, 16);
        const Shot field = shoot(device, gpu.list.Get(), pass.fieldTexture(), kCommon, 16);
        if (!gpu.submitAndWait() || !finished) return f;
        pass.harvest();
        f.picture = developFloats(pictureRead.Get(), plane * 3); f.answer = developFloats(answerRead.Get(), plane);
        f.matte8 = matte.develop(); f.fine = fine.develop(); f.coarse = coarse.develop(); f.blocks = blocks.develop(); f.field = field.develop();
        f.ok = !f.picture.empty() && !f.answer.empty() && !f.matte8.empty() && !f.fine.empty() && !f.coarse.empty() && !f.blocks.empty() && !f.field.empty();
        return f;
    };
    const auto floatAt = [](const std::vector<uint8_t>& bytes, size_t index) { float v = 0.0f; std::memcpy(&v, bytes.data() + index * 4, 4); return v; };

    // The first frame: nothing to compare it with yet.
    Frame a = step(0, draw(W, H, 0, true), true);
    check(a.ok, name + "a real frame goes through pack, the network and finish");
    if (!a.ok) return true;
    check(!pass.composes(0, 0) && pass.pairParity() == -1, name + "one frame alone makes no pair");
    {
        // What the network said about the drawn figure.
        float cx = 0, cy = 0; figureCentre(0, cx, cy);
        size_t covered = 0, core = 0, coreSolid = 0, away = 0, awayMarked = 0;
        for (int y = 0; y < ch::kMatteH; ++y)
            for (int x = 0; x < ch::kMatteW; ++x) {
                const uint8_t m = a.matte8[size_t(y) * ch::kMatteW + x];
                covered += m > 127 ? 1u : 0u;
                const float bx = (float(x) - cx) / 150.0f, by = (float(y) - cy - 140.0f) / 330.0f, hx = (float(x) - cx) / 80.0f, hy = (float(y) - cy + 250.0f) / 95.0f;
                const float inside = (std::min)(bx * bx + by * by, hx * hx + hy * hy);
                if (inside < 0.6f) { ++core; coreSolid += m > 200 ? 1u : 0u; }
                if (inside > 3.0f) { ++away; awayMarked += m > 50 ? 1u : 0u; }
            }
        const double share = 100.0 * double(covered) / double(plane);
        std::printf("  the matte covers %.2f %% of the picture; %.2f %% of the figure's core is solid; %.3f %% of what is away from it is marked\n", share,
                    100.0 * double(coreSolid) / double(core), 100.0 * double(awayMarked) / double(away));
        check(share > 4.0 && share < 15.0 && double(coreSolid) > 0.95 * double(core) && double(awayMarked) < 0.005 * double(away), name + "the network finds the drawn figure and nothing else");
    }
    // The shaders' matte and matted pictures against the header's, from the network's own answer.
    const auto compareMatted = [&](const Frame& f, const std::string& which) {
        std::vector<uint8_t> matte8, fine, coarse;
        ch::matted(f.picture.data(), f.answer.data(), matte8, fine, coarse);
        size_t matteOff = 0, fineOne = 0, fineMore = 0;
        for (size_t i = 0; i < matte8.size(); ++i) matteOff += matte8[i] != f.matte8[i] ? 1u : 0u;
        for (size_t i = 0; i < fine.size(); ++i) { const int d = std::abs(int(fine[i]) - int(f.fine[i])); fineOne += d == 1 ? 1u : 0u; fineMore += d > 1 ? 1u : 0u; }
        // The coarse picture is whole numbers of the fine one the shader itself made.
        size_t coarseOff = 0;
        for (int y = 0; y < ch::kCoarseH; ++y)
            for (int x = 0; x < ch::kCoarseW; ++x)
                for (int c = 0; c < 4; ++c) {
                    const auto at = [&](int dx, int dy) { return int(f.fine[(size_t(2 * y + dy) * ch::kFineW + size_t(2 * x + dx)) * 4 + c]); };
                    coarseOff += f.coarse[(size_t(y) * ch::kCoarseW + x) * 4 + c] != uint8_t((at(0, 0) + at(1, 0) + at(0, 1) + at(1, 1) + 2) >> 2) ? 1u : 0u;
                }
        std::printf("  %s: matte bytes that differ %zu; matted picture values one apart %zu, more %zu; coarse values that differ %zu\n", which.c_str(), matteOff, fineOne, fineMore, coarseOff);
        check(matteOff <= matte8.size() / 10000 && fineMore == 0 && fineOne <= fine.size() / 1000 && coarseOff == 0, name + which + ": the matte and the matted pictures are the header's");
    };
    compareMatted(a, "first frame");

    // The second frame: the figure has moved 9 across and 3 down at the network's size (half of it at the fine size: 2.25, 0.75).
    Frame b = step(1, draw(W, H, 1, true), false);
    check(b.ok && pass.pairParity() == 1, name + "the second frame makes a pair");
    if (!b.ok) return true;
    compareMatted(b, "second frame");
    const ch::Picture a0{ch::kCoarseW, ch::kCoarseH, a.coarse.data()}, b0{ch::kCoarseW, ch::kCoarseH, b.coarse.data()};
    const ch::Picture a1{ch::kFineW, ch::kFineH, a.fine.data()}, b1{ch::kFineW, ch::kFineH, b.fine.data()};
    const ch::Field want = ch::search(a0, b0, a1, b1);
    ch::Field got;      // the shader's field, in the header's form (for the compose reference below)
    got.x.resize(ch::kBlockCount); got.y.resize(ch::kBlockCount); got.trust.resize(ch::kBlockCount);
    {
        int active = 0, activeOff = 0, figureBlocks = 0;
        double blockOff = 0, fieldOff = 0, trustOff = 0, mx = 0, my = 0, trust = 0;
        for (int i = 0; i < ch::kBlockCount; ++i) {
            const bool judged = floatAt(b.blocks, size_t(i) * 4 + 3) >= 0.5f;
            activeOff += judged != want.blocks[i].active ? 1 : 0;
            active += judged ? 1 : 0;
            blockOff = (std::max)({blockOff, double(std::fabs(floatAt(b.blocks, size_t(i) * 4) - want.blocks[i].fx)), double(std::fabs(floatAt(b.blocks, size_t(i) * 4 + 1) - want.blocks[i].fy)),
                                   double(std::fabs(floatAt(b.blocks, size_t(i) * 4 + 2) - want.blocks[i].sure))});
            got.x[i] = floatAt(b.field, size_t(i) * 4); got.y[i] = floatAt(b.field, size_t(i) * 4 + 1); got.trust[i] = floatAt(b.field, size_t(i) * 4 + 2);
            fieldOff = (std::max)({fieldOff, double(std::fabs(got.x[i] - want.x[i])), double(std::fabs(got.y[i] - want.y[i]))});
            trustOff = (std::max)(trustOff, double(std::fabs(got.trust[i] - want.trust[i])));
            // Blocks wholly inside the figure of the newer frame.
            const ch::Span s = ch::fineBlock(i % ch::kBlocksX, i / ch::kBlocksX);
            bool all = true;
            for (int y = s.y0; y < s.y1 && all; ++y)
                for (int x = s.x0; x < s.x1; ++x) if (b.fine[(size_t(y) * ch::kFineW + x) * 4 + 3] < 250) { all = false; break; }
            if (all) { ++figureBlocks; mx += got.x[i]; my += got.y[i]; trust += got.trust[i]; }
        }
        if (figureBlocks) { mx /= figureBlocks; my /= figureBlocks; trust /= figureBlocks; }
        std::printf("  blocks judged %d (differ from the header: %d); largest difference: blocks %.2g, field %.2g, trust %.2g\n", active, activeOff, blockOff, fieldOff, trustOff);
        std::printf("  inside the figure (%d blocks): half-motion %.2f, %.2f (true 2.25, 0.75), trust %.2f; the pass counts %u blocks with a person\n", figureBlocks, mx, my, trust, pass.activeBlocks());
        check(active > 20 && activeOff == 0 && blockOff < 1e-3, name + "each block's motion is the header's");
        check(fieldOff < 5e-3 && trustOff < 5e-3, name + "the smoothed field and the trust are the header's");
        check(figureBlocks > 10 && std::fabs(mx - 2.25) < 0.6 && std::fabs(my - 0.75) < 0.6 && trust > 0.9, name + "the figure's own motion is found and trusted");
        check(pass.activeBlocks() == unsigned(active) && pass.samplesWithPeople() >= 1, name + "the pass counts the blocks that have a person");
    }
    // A generated frame through every step of its own, each step against the header from what the step before
    // left on the card. `truth`: the frame in which the figure is right (the same drawing at that time).
    struct Result { bool ok = false; size_t laid = 0; double changedInFigure = 0, patchError = 0, patchMade = 0; size_t figurePixels = 0, patchPixels = 0; };
    const auto composeAt = [&](const Drawn& made, const Drawn& truth, float t, float step, const std::string& which, int px0 = 0, int py0 = 0, int px1 = 0, int py1 = 0) {
        Result res;
        if (!gpu.begin()) { check(false, name + which + ": list"); return res; }
        auto upload = fill(device, gpu.list.Get(), generated.Get(), made.rgba);
        const bool can = pass.composes(0, 1);
        const bool done = can && pass.compose(gpu.list.Get(), tracker, 0, 1, t);
        const Shot madeShot = shoot(device, gpu.list.Get(), pass.madeTexture(), kCommon, 4), tablesShot = shoot(device, gpu.list.Get(), pass.tablesTexture(), kCommon, 4);
        const Shot placedShot = shoot(device, gpu.list.Get(), pass.placedTexture(), kCommon, 16), placeShot = shoot(device, gpu.list.Get(), pass.placeTexture(), kCommon, 16);
        const Shot apartShot = shoot(device, gpu.list.Get(), pass.apartTexture(), kCommon, 2), differShot = shoot(device, gpu.list.Get(), pass.differTexture(), kCommon, 1);
        const Shot out = shoot(device, gpu.list.Get(), generated.Get(), kCommon, 4);
        if (!gpu.submitAndWait() || !done) { check(false, name + which + ": the generated frame is composed"); return res; }
        const std::vector<uint8_t> made1 = madeShot.develop(), tablesRaw = tablesShot.develop(), placedRaw = placedShot.develop(), placeRaw = placeShot.develop(), apartRaw = apartShot.develop(),
                                   differ = differShot.develop(), result = out.develop();
        if (made1.empty() || tablesRaw.empty() || placedRaw.empty() || placeRaw.empty() || apartRaw.empty() || differ.empty() || result.empty()) { check(false, name + which + ": read back"); return res; }
        const bool newer = ch::sourceIsNewer(t);
        const float share = ch::shareOf(t);
        const Frame& source = newer ? b : a;
        const ch::Picture source1{ch::kFineW, ch::kFineH, source.fine.data()}, madePicture{ch::kFineW, ch::kFineH, made1.data()};
        // The frame at the fine size: the header's, from the frame as the network would be given it.
        {
            std::vector<float> planar(size_t(plane) * 3);
            for (int y = 0; y < ch::kMatteH; ++y)
                for (int x = 0; x < ch::kMatteW; ++x)
                    for (int c = 0; c < 3; ++c)
                        planar[size_t(c) * plane + size_t(y) * ch::kMatteW + x] = byteBetween(made.rgba, W, H, c, (float(x) + 0.5f) * float(W) / float(ch::kMatteW) - 0.5f, (float(y) + 0.5f) * float(H) / float(ch::kMatteH) - 0.5f);
            std::vector<uint8_t> want1;
            ch::madeFine(planar.data(), want1);
            size_t one = 0, more = 0;
            for (size_t i = 0; i < want1.size(); ++i) { const int d = std::abs(int(want1[i]) - int(made1[i])); one += d == 1 ? 1u : 0u; more += d > 1 ? 1u : 0u; }
            // A mean of four taps often lies exactly between two bytes, and the last bit of the arithmetic then decides
            // which it becomes (4 % of the values on the software renderer): one apart is the same picture, more is not.
            check(more == 0 && one <= want1.size() / 6, name + which + ": the generated frame at the fine size is the header's (" + std::to_string(one) + " values one apart, none more)");
        }
        // Every judged block's table, from the pictures the shaders themselves made: whole numbers, so the same.
        std::vector<ch::Table> tables(ch::kBlockCount);
        size_t tableOff = 0, judged = 0;
        for (int by = 0; by < ch::kBlocksY; ++by)
            for (int bx = 0; bx < ch::kBlocksX; ++bx) {
                const int i = by * ch::kBlocksX + bx;
                if (floatAt(b.blocks, size_t(i) * 4 + 3) < 0.5f) continue;
                ++judged;
                tables[i] = ch::tableOf(source1, madePicture, bx, by, {got.x[i], got.y[i]}, share);
                for (int e = 0; e < ch::kPlaceCandidates; ++e) {
                    uint32_t v = 0;
                    std::memcpy(&v, tablesRaw.data() + (size_t(by * ch::kPlaceAcross + e / ch::kPlaceAcross) * size_t(ch::kBlocksX * ch::kPlaceAcross) + size_t(bx * ch::kPlaceAcross + e % ch::kPlaceAcross)) * 4, 4);
                    if (v != tables[i].apart[e] * 2048u + tables[i].solid[e]) ++tableOff;
                }
            }
        check(judged > 20 && tableOff == 0, name + which + ": every block's table is the header's (" + std::to_string(tableOff) + " of " + std::to_string(judged * ch::kPlaceCandidates) + " differ)");
        // Each block's place as found, and as settled with its neighbours.
        ch::Placing placing;
        placing.x.resize(ch::kBlockCount); placing.y.resize(ch::kBlockCount); placing.trust.resize(ch::kBlockCount);
        {
            std::vector<float> px(ch::kBlockCount), py(ch::kBlockCount), fit(ch::kBlockCount), sx(ch::kBlockCount), sy(ch::kBlockCount);
            double placedOff = 0, settledOff = 0; size_t foundOff = 0, found = 0;
            for (int by = 0; by < ch::kBlocksY; ++by)
                for (int bx = 0; bx < ch::kBlocksX; ++bx) {
                    const int i = by * ch::kBlocksX + bx;
                    const ch::Placed want = ch::placeBlock(tables.data(), bx, by, {got.x[i], got.y[i]}, share);
                    px[i] = floatAt(placedRaw, size_t(i) * 4); py[i] = floatAt(placedRaw, size_t(i) * 4 + 1);
                    const float left = floatAt(placedRaw, size_t(i) * 4 + 2); const bool was = floatAt(placedRaw, size_t(i) * 4 + 3) >= 0.5f;
                    foundOff += was != want.found ? 1u : 0u; found += was ? 1u : 0u;
                    placedOff = (std::max)({placedOff, double(std::fabs(px[i] - want.x)), double(std::fabs(py[i] - want.y)), double(std::fabs(left - want.left))});
                    fit[i] = ch::fitOf(left, was); sx[i] = share * got.x[i]; sy[i] = share * got.y[i];
                }
            for (int by = 0; by < ch::kBlocksY; ++by)
                for (int bx = 0; bx < ch::kBlocksX; ++bx) {
                    const int i = by * ch::kBlocksX + bx;
                    const ch::Motion want = ch::settleAt(px.data(), py.data(), fit.data(), sx.data(), sy.data(), bx, by);
                    placing.x[i] = floatAt(placeRaw, size_t(i) * 4); placing.y[i] = floatAt(placeRaw, size_t(i) * 4 + 1); placing.trust[i] = floatAt(placeRaw, size_t(i) * 4 + 2);
                    settledOff = (std::max)({settledOff, double(std::fabs(placing.x[i] - want.x)), double(std::fabs(placing.y[i] - want.y)), double(std::fabs(placing.trust[i] - got.trust[i])),
                                             double(std::fabs(floatAt(placeRaw, size_t(i) * 4 + 3) - fit[i]))});
                }
            std::printf("  %s: %zu blocks placed (found differs in %zu); largest difference: as found %.2g, settled %.2g\n", which.c_str(), found, foundOff, placedOff, settledOff);
            check(found > 20 && foundOff == 0 && placedOff < 1e-3 && settledOff < 5e-3, name + which + ": each block's place is the header's, as found and as settled");
        }
        // How far apart, and where it differs: whole numbers from the places the shader wrote.
        {
            std::vector<uint16_t> apart(size_t(ch::kFineW) * ch::kFineH);
            std::memcpy(apart.data(), apartRaw.data(), apart.size() * 2);
            size_t apartOff = 0, differOff = 0, solid = 0;
            for (int y = 0; y < ch::kFineH; ++y)
                for (int x = 0; x < ch::kFineW; ++x) {
                    const size_t i = size_t(y) * ch::kFineW + x;
                    apartOff += ch::apartAt(placing, source1, madePicture, x, y) != apart[i] ? 1u : 0u;
                    solid += apart[i] ? 1u : 0u;
                }
            for (int y = 0; y < ch::kFineH; ++y)
                for (int x = 0; x < ch::kFineW; ++x) differOff += ch::differAt(placing, apart.data(), x, y) != differ[size_t(y) * ch::kFineW + x] ? 1u : 0u;
            std::printf("  %s: solid pixels %zu; apart values that differ %zu, differ values that differ %zu\n", which.c_str(), solid, apartOff, differOff);
            // A place that falls right between two pixels may round either way: a few values may differ.
            check(solid > 2000 && apartOff <= solid / 500 && differOff <= solid / 100, name + which + ": how far apart and where it differs are the header's");
        }
        // Every pixel of the frame that comes out.
        size_t one = 0, more = 0, touched = 0, alphaOff = 0, changed = 0;
        const uint8_t* sourceMatte = source.matte8.data();
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                const ch::Laid laid = ch::laidAt(placing, differ.data(), sourceMatte, x, y, W, H);
                const uint8_t* g = &made.rgba[(size_t(y) * W + x) * 4];
                const uint8_t* r = &result[(size_t(y) * W + x) * 4];
                const uint8_t* want = &truth.rgba[(size_t(y) * W + x) * 4];
                const float u = (float(x) + 0.5f) * 1920.0f / float(W), v = (float(y) + 0.5f) * 1080.0f / float(H);
                const bool inFigure = figureInside(step, u, v) < 0.85f;
                if (inFigure) {
                    ++res.figurePixels;
                    int moved = 0; double error = 0, before = 0;
                    for (int c = 0; c < 3; ++c) { moved = (std::max)(moved, std::abs(int(r[c]) - int(g[c]))); error += std::abs(int(r[c]) - int(want[c])); before += std::abs(int(g[c]) - int(want[c])); }
                    if (moved > 3) ++changed;
                    if (x >= px0 && x < px1 && y >= py0 && y < py1) { ++res.patchPixels; res.patchError += error / 3; res.patchMade += before / 3; }
                }
                // Right at the least weight the two may decide differently by a rounding: those pixels may be either.
                const bool edge = std::fabs(laid.weight - ch::kLeast) < 2e-4f;
                if (laid.weight < ch::kLeast || edge) {
                    if (std::memcmp(g, r, 4) != 0 && !edge) ++touched;
                    if (!edge) continue;
                    if (std::memcmp(g, r, 4) == 0) continue;
                }
                ++res.laid;
                for (int c = 0; c < 3; ++c) {
                    const float want8 = float(g[c]) * (1.0f / 255.0f) * (1.0f - laid.weight) + byteBetween(source.drawn.rgba, W, H, c, laid.x, laid.y) * laid.weight;
                    const int d = std::abs(int(ch::toByte(want8)) - int(r[c]));
                    one += d == 1 ? 1u : 0u; more += d > 1 ? 1u : 0u;
                }
                alphaOff += r[3] != g[3] ? 1u : 0u;
            }
        if (res.figurePixels) res.changedInFigure = double(changed) / double(res.figurePixels);
        if (res.patchPixels) { res.patchError /= double(res.patchPixels); res.patchMade /= double(res.patchPixels); }
        std::printf("  %s: laid over %zu pixels; values one apart %zu, more %zu; pixels changed where nothing is laid over %zu; %.2f %% of the figure changed\n", which.c_str(), res.laid, one, more, touched,
                    100.0 * res.changedInFigure);
        check(more <= res.laid / 2000 && one <= res.laid / 4 + 16 && alphaOff == 0, name + which + ": every laid-over pixel is the header's");
        check(touched == 0, name + which + ": nothing else in the generated frame is touched");
        res.ok = true;
        return res;
    };
    // The method's frame as it should be: the same drawing half-way. It is left alone.
    const Drawn right = draw(W, H, 0.5f, true), world = draw(W, H, 0.5f, false);
    {
        const Result r = composeAt(right, right, 0.5f, 0.5f, "a frame that is right");
        check(r.ok && r.figurePixels > 20000 && r.changedInFigure < 0.01, name + "a generated frame in which the figure is right is left alone");
    }
    // The method showed the world where the head is: the head comes back, from the newer real frame.
    {
        float cx = 0, cy = 0; figureCentre(0.5f, cx, cy);
        const int x0 = int((cx - 100.0f) * float(W) / 1920.0f), x1 = int((cx + 100.0f) * float(W) / 1920.0f), y0 = int((cy - 360.0f) * float(H) / 1080.0f), y1 = int((cy - 150.0f) * float(H) / 1080.0f);
        Drawn broken = right;
        for (int y = y0; y < y1; ++y) std::memcpy(&broken.rgba[(size_t(y) * W + x0) * 4], &world.rgba[(size_t(y) * W + x0) * 4], size_t(x1 - x0) * 4);
        const Result r = composeAt(broken, right, 0.5f, 0.5f, "a head shown as the world", x0, y0, x1, y1);
        std::printf("  the head: %.2f of 255 from the right frame where the made frame is %.2f (%zu pixels)\n", r.patchError, r.patchMade, r.patchPixels);
        check(r.ok && r.patchPixels > 3000 && r.patchMade > 15.0 && r.patchError < 6.0 && r.patchError * 4 < r.patchMade, name + "a head the method showed as the world comes back");
    }
    // A frame that is neither real frame (stripes): the figure is laid over whole, at a half and at a third.
    const Drawn striped = stripes(W, H);
    {
        const Result half = composeAt(striped, right, 0.5f, 0.5f, "at a half"), third = composeAt(striped, draw(W, H, 1.0f / 3.0f, true), 1.0f / 3.0f, 1.0f / 3.0f, "at a third");
        check(half.ok && third.ok && half.laid > 5000 && third.laid > 5000 && half.changedInFigure > 0.8 && third.changedInFigure > 0.8, name + "a frame that is nothing like the figure gets it laid over whole");
    }
    check(!pass.composes(0, 0), name + "a generated frame of the other parity is not composed from this pair");

    // Starting over forgets the frame before; the pair comes back with the frame after.
    Frame c = step(0, draw(W, H, 5, true), true);
    check(c.ok && pass.pairParity() == -1 && !pass.composes(0, 0), name + "a start-over forgets the frame before");
    Frame d = step(1, draw(W, H, 6, true), false);
    check(d.ok && pass.pairParity() == 1 && pass.composes(0, 1), name + "and the next frame makes a pair again");
    // No person: the count goes to nothing and a generated frame is left alone.
    Frame e = step(0, draw(W, H, 7, false), true);
    Frame f = step(1, draw(W, H, 8, false), false);
    check(e.ok && f.ok && pass.pairParity() == 1 && pass.activeBlocks() == 0 && !pass.composes(0, 1), name + "a picture without a person leaves generated frames alone");
    check(!gpu.debugErrors(), name + "no D3D12 debug-layer errors");
    std::printf("  real frames %llu, samples %llu, with a person %llu; the network call took %.1f ms of processor time on the last frame\n", static_cast<unsigned long long>(pass.realFrames()),
                static_cast<unsigned long long>(pass.samples()), static_cast<unsigned long long>(pass.samplesWithPeople()), pass.networkCpuMs());
    return true;
}
}
