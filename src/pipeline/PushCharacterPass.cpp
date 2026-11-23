// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#include "veyra/pipeline/PushCharacterPass.h"
#include <bit>
#include <cstring>
#include <format>

namespace veyra::pipeline {
namespace {
constexpr DXGI_FORMAT kMatteFormat = DXGI_FORMAT_R8_UNORM;
constexpr DXGI_FORMAT kPictureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;       // a matted picture
constexpr DXGI_FORMAT kBlockFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;     // per block: found, smoothed, the field
constexpr DXGI_FORMAT kSumFormat = DXGI_FORMAT_R32G32B32A32_UINT;        // per block: the four sums
constexpr D3D12_RESOURCE_STATES kRead = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
constexpr D3D12_RESOURCE_STATES kWrite = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
// Descriptor slots of each pass (the layouts createViews fills and the steps bind).
//   pack:    0 the picture's tensor (UAV); 1 + parity the real frame.
//   matte:   0 nothing a shader reads; 1 + 4 parity: the picture's tensor, the answer's, the matte, the fine picture (UAVs).
//   coarse:  2 parity the fine picture; 2 parity + 1 the coarse one (UAV).
//   search:  5 parity: older coarse, newer coarse, older fine, newer fine; then the found blocks (UAV).
//   smooth:  5 parity: the found blocks, older fine, newer fine; then the smoothed motion and the sums (UAVs).
//   trust:   0 the smoothed motion, 1 the sums, 2 the field (UAV).
//   compose: 7 (2 slot + parity): the copy, older frame, newer frame, older matte, newer matte, the field; then the generated frame (UAV).
constexpr UINT kComposeStride = 7;
constexpr UINT kGroups16 = 16, kGroups8 = 8, kGroups4 = 4;
UINT groups(uint32_t size, UINT per) { return (size + per - 1) / per; }
void bufferUav(ID3D12Device* device, ID3D12Resource* buffer, uint64_t floats, D3D12_CPU_DESCRIPTOR_HANDLE at) {
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
    uav.Format = DXGI_FORMAT_UNKNOWN; uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uav.Buffer.NumElements = UINT(floats); uav.Buffer.StructureByteStride = 4;
    device->CreateUnorderedAccessView(buffer, nullptr, &uav, at);
}
ComPtr<ID3D12Resource> makeBlockReadback(ID3D12Device* device, uint64_t size) {
    D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC bd{}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = size; bd.Height = 1; bd.DepthOrArraySize = 1;
    bd.MipLevels = 1; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> r;
    if (FAILED(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&r)))) return nullptr;
    return r;
}
}

std::string PushCharacterPass::describe() const {
    unsigned writable = 0;
    for (unsigned i = 0; i < generatedCount_; ++i) writable += writable_[i] ? 1u : 0u;
    return std::format("character hold {}x{} encoding={} network={}x{} state={} generated={} writable={}", width_, height_, encoding_, matte_.status().width,
                       matte_.status().height, matte_.status().state, generatedCount_, writable);
}

bool PushCharacterPass::initialize(ID3D12Device* device, ID3D12CommandQueue* queue, const std::wstring& runtimeDir, uint32_t width, uint32_t height,
                                   DXGI_FORMAT format, unsigned encoding) {
    if (!device || !queue || width < 64 || height < 64 || format == DXGI_FORMAT_UNKNOWN) return false;
    width_ = width; height_ = height; format_ = format; encoding_ = encoding;
    if (!matte_.initialize(device, queue, runtimeDir)) {
        veyra::log::warn("character-hold", std::format("the matting network did not start: {}", matte_.status().state));
        return false;
    }
    if (matte_.status().width != uint32_t(character::kMatteW) || matte_.status().height != uint32_t(character::kMatteH)) {
        veyra::log::error("character-hold", std::format("the model takes {}x{}, this build {}x{}", matte_.status().width, matte_.status().height, character::kMatteW, character::kMatteH));
        return false;
    }
    for (auto& texture : matte8_) texture = makeTexture(device, character::kMatteW, character::kMatteH, kMatteFormat, true);
    for (auto& texture : fine_) texture = makeTexture(device, character::kFineW, character::kFineH, kPictureFormat, true);
    for (auto& texture : coarse_) texture = makeTexture(device, character::kCoarseW, character::kCoarseH, kPictureFormat, true);
    blocks_ = makeTexture(device, character::kBlocksX, character::kBlocksY, kBlockFormat, true);
    smooth_ = makeTexture(device, character::kBlocksX, character::kBlocksY, kBlockFormat, true);
    sums_ = makeTexture(device, character::kBlocksX, character::kBlocksY, kSumFormat, true);
    field_ = makeTexture(device, character::kBlocksX, character::kBlocksY, kBlockFormat, true);
    copy_ = makeTexture(device, width_, height_, format_, false);
    if (!matte8_[0] || !matte8_[1] || !fine_[0] || !fine_[1] || !coarse_[0] || !coarse_[1] || !blocks_ || !smooth_ || !sums_ || !field_ || !copy_) return false;
    std::vector<uint8_t> cs;
    if (!packPass_.loadShader("PushCharacterPack.dxil", cs) || !packPass_.create(device, cs, 3, 1, 1, kConstants)) return false;
    if (!mattePass_.loadShader("PushCharacterMatte.dxil", cs) || !mattePass_.create(device, cs, 1 + 2 * 4, 1, 4, kConstants)) return false;
    if (!coarsePass_.loadShader("PushCharacterCoarse.dxil", cs) || !coarsePass_.create(device, cs, 4, 1, 1, kConstants)) return false;
    if (!searchPass_.loadShader("PushCharacterSearch.dxil", cs) || !searchPass_.create(device, cs, 10, 4, 1, kConstants)) return false;
    if (!smoothPass_.loadShader("PushCharacterSmooth.dxil", cs) || !smoothPass_.create(device, cs, 10, 3, 2, kConstants)) return false;
    if (!trustPass_.loadShader("PushCharacterTrust.dxil", cs) || !trustPass_.create(device, cs, 3, 2, 1, kConstants)) return false;
    if (!composePass_.loadShader("PushCharacterCompose.dxil", cs) || !composePass_.create(device, cs, kMaxGenerated * 2 * kComposeStride, 6, 1, kConstants)) return false;
    // What the pass found comes back through a small buffer that stays mapped; without it the pass works, uncounted.
    {
        const auto desc = blocks_->GetDesc();
        UINT64 total = 0;
        device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint_, nullptr, nullptr, &total);
        void* memory = nullptr;
        if (total) readback_ = makeBlockReadback(device, total);
        if (readback_ && SUCCEEDED(readback_->Map(0, nullptr, &memory)) && memory) {
            std::memset(memory, 0, size_t(total));
            mapped_ = static_cast<const uint8_t*>(memory);
        } else { readback_.Reset(); mapped_ = nullptr; }
    }
    veyra::log::info("character-hold", std::format("created {}", describe()));
    return true;
}

// Counts the sample taken last. The card may not have written it yet: then this counts the sample before it once
// more, which is as good; nothing here waits.
void PushCharacterPass::harvest() {
    if (!mapped_ || !samplePending_) return;
    samplePending_ = false;
    unsigned active = 0;
    const uint8_t* base = mapped_ + footprint_.Offset;
    for (int y = 0; y < character::kBlocksY; ++y) {
        const uint8_t* row = base + size_t(y) * footprint_.Footprint.RowPitch;
        for (int x = 0; x < character::kBlocksX; ++x) {
            float judged = 0.0f;
            std::memcpy(&judged, row + size_t(x) * 16u + 12u, 4u);   // (x, y, how distinct, judged)
            active += judged >= 0.5f ? 1u : 0u;
        }
    }
    activeBlocks_ = active; ++samples_;
    if (active) ++samplesWithPeople_;
    if (active && !foundLogged_) {
        foundLogged_ = true;
        veyra::log::info("character-hold", std::format("found: a person in {} of {} blocks, {} real frames after the start", active, character::kBlockCount, updates_));
    }
    if (samples_ % 256 == 0)
        veyra::log::info("character-hold", std::format("real frames={} samples={} with a person={} blocks now={} network call ms={:.2f}", updates_, samples_, samplesWithPeople_,
                                                        activeBlocks_, matte_.lastRunCpuMs()));
}

void PushCharacterPass::createViews(ID3D12Device* device, DescriptorStager& stager, ID3D12Resource* const real[2], ID3D12Resource* const* generated,
                                    unsigned generatedCount) {
    real_[0] = real[0]; real_[1] = real[1];
    generatedCount_ = std::min<unsigned>(generatedCount, kMaxGenerated);
    for (unsigned i = 0; i < generatedCount_; ++i) generated_[i] = generated[i];
    for (unsigned slot = 0; slot < kMaxGenerated; ++slot) writable_[slot] = false;
    if (!ready() || !real_[0] || !real_[1]) return;
    const uint64_t plane = uint64_t(character::kMatteW) * character::kMatteH;
    bufferUav(device, matte_.pictureBuffer(), plane * 3, cpuHandleOf(packPass_, 0));
    stager.stageSrv(real_[0], nullptr, mattePass_.heap.Get(), 0);
    stager.stageSrv(smooth_.Get(), nullptr, trustPass_.heap.Get(), 0);
    stager.stageSrv(sums_.Get(), nullptr, trustPass_.heap.Get(), 1);
    makeUav(device, field_.Get(), kBlockFormat, cpuHandleOf(trustPass_, 2));
    for (unsigned parity = 0; parity < 2; ++parity) {
        stager.stageSrv(real_[parity], nullptr, packPass_.heap.Get(), 1 + parity);
        const UINT matteBase = 1 + 4 * parity;
        bufferUav(device, matte_.pictureBuffer(), plane * 3, cpuHandleOf(mattePass_, matteBase));
        bufferUav(device, matte_.matteBuffer(), plane, cpuHandleOf(mattePass_, matteBase + 1));
        makeUav(device, matte8_[parity].Get(), kMatteFormat, cpuHandleOf(mattePass_, matteBase + 2));
        makeUav(device, fine_[parity].Get(), kPictureFormat, cpuHandleOf(mattePass_, matteBase + 3));
        stager.stageSrv(fine_[parity].Get(), nullptr, coarsePass_.heap.Get(), 2 * parity);
        makeUav(device, coarse_[parity].Get(), kPictureFormat, cpuHandleOf(coarsePass_, 2 * parity + 1));
        const UINT pairBase = 5 * parity;
        stager.stageSrv(coarse_[1 - parity].Get(), nullptr, searchPass_.heap.Get(), pairBase);
        stager.stageSrv(coarse_[parity].Get(), nullptr, searchPass_.heap.Get(), pairBase + 1);
        stager.stageSrv(fine_[1 - parity].Get(), nullptr, searchPass_.heap.Get(), pairBase + 2);
        stager.stageSrv(fine_[parity].Get(), nullptr, searchPass_.heap.Get(), pairBase + 3);
        makeUav(device, blocks_.Get(), kBlockFormat, cpuHandleOf(searchPass_, pairBase + 4));
        stager.stageSrv(blocks_.Get(), nullptr, smoothPass_.heap.Get(), pairBase);
        stager.stageSrv(fine_[1 - parity].Get(), nullptr, smoothPass_.heap.Get(), pairBase + 1);
        stager.stageSrv(fine_[parity].Get(), nullptr, smoothPass_.heap.Get(), pairBase + 2);
        makeUav(device, smooth_.Get(), kBlockFormat, cpuHandleOf(smoothPass_, pairBase + 3));
        makeUav(device, sums_.Get(), kSumFormat, cpuHandleOf(smoothPass_, pairBase + 4));
    }
    for (unsigned slot = 0; slot < generatedCount_; ++slot) {
        if (!generated_[slot]) continue;
        const auto desc = generated_[slot]->GetDesc();
        if (!(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) || desc.Format != format_ || desc.Width != width_ || desc.Height != height_) continue;
        for (unsigned parity = 0; parity < 2; ++parity) {
            const UINT base = (slot * 2 + parity) * kComposeStride;
            stager.stageSrv(copy_.Get(), nullptr, composePass_.heap.Get(), base);
            stager.stageSrv(real_[1 - parity], nullptr, composePass_.heap.Get(), base + 1);
            stager.stageSrv(real_[parity], nullptr, composePass_.heap.Get(), base + 2);
            stager.stageSrv(matte8_[1 - parity].Get(), nullptr, composePass_.heap.Get(), base + 3);
            stager.stageSrv(matte8_[parity].Get(), nullptr, composePass_.heap.Get(), base + 4);
            stager.stageSrv(field_.Get(), nullptr, composePass_.heap.Get(), base + 5);
            makeUav(device, generated_[slot], format_, cpuHandleOf(composePass_, base + 6));
        }
        writable_[slot] = true;
    }
}

bool PushCharacterPass::pack(ID3D12GraphicsCommandList* list, StateTracker& tracker, unsigned parity) {
    packed_ = false;
    if (!list || parity > 1 || !ready() || !real_[0] || !real_[1]) return false;
    tracker.transition(list, real_[parity], kRead);
    float c[kConstants]{};
    c[0] = std::bit_cast<float>(width_); c[1] = std::bit_cast<float>(height_); c[2] = std::bit_cast<float>(uint32_t(encoding_));
    packPass_.bind(list, c, gpuHandleOf(packPass_, 1 + parity).ptr, gpuHandleOf(packPass_, 0).ptr);
    list->Dispatch(groups(character::kMatteW, kGroups16), groups(character::kMatteH, kGroups16), 1);
    tracker.uavBarrier(list, matte_.pictureBuffer());
    tracker.transition(list, real_[parity], D3D12_RESOURCE_STATE_COMMON);
    packed_ = true;
    return true;
}

bool PushCharacterPass::run(bool startOver) {
    ran_ = false;
    if (!packed_ || !ready()) return false;
    packed_ = false;
    startOver_ = startOver;
    if (!matte_.run(startOver)) { failed_ = true; pair_ = -1; have_[0] = have_[1] = false; return false; }
    ran_ = true;
    return true;
}

bool PushCharacterPass::finish(ID3D12GraphicsCommandList* list, StateTracker& tracker, unsigned parity) {
    if (!list || parity > 1 || !ran_ || !ready()) { pair_ = -1; return false; }
    ran_ = false;
    float c[kConstants]{};
    // The network's answer: the matte, and the matted picture at the fine size.
    tracker.transition(list, matte8_[parity].Get(), kWrite);
    tracker.transition(list, fine_[parity].Get(), kWrite);
    mattePass_.bind(list, c, gpuHandleOf(mattePass_, 0).ptr, gpuHandleOf(mattePass_, 1 + 4 * parity).ptr);
    list->Dispatch(groups(character::kFineW, kGroups8), groups(character::kFineH, kGroups8), 1);
    tracker.uavBarrier(list, matte8_[parity].Get());
    tracker.uavBarrier(list, fine_[parity].Get());
    // The coarse one.
    tracker.transition(list, fine_[parity].Get(), kRead);
    tracker.transition(list, coarse_[parity].Get(), kWrite);
    coarsePass_.bind(list, c, gpuHandleOf(coarsePass_, 2 * parity).ptr, gpuHandleOf(coarsePass_, 2 * parity + 1).ptr);
    list->Dispatch(groups(character::kCoarseW, kGroups8), groups(character::kCoarseH, kGroups8), 1);
    tracker.uavBarrier(list, coarse_[parity].Get());
    if (startOver_) have_[1 - parity] = false;
    have_[parity] = true;
    pair_ = -1;
    if (have_[1 - parity]) {
        const UINT blockGroupsX = groups(character::kBlocksX, kGroups4), blockGroupsY = groups(character::kBlocksY, kGroups4);
        const UINT pairBase = 5 * parity;
        // The character's own motion, per block.
        for (ID3D12Resource* r : {coarse_[0].Get(), coarse_[1].Get(), fine_[0].Get(), fine_[1].Get()}) tracker.transition(list, r, kRead);
        tracker.transition(list, blocks_.Get(), kWrite);
        searchPass_.bind(list, c, gpuHandleOf(searchPass_, pairBase).ptr, gpuHandleOf(searchPass_, pairBase + 4).ptr);
        list->Dispatch(blockGroupsX, blockGroupsY, 1);
        tracker.uavBarrier(list, blocks_.Get());
        // Smoothed, and what each block's own pixels say.
        tracker.transition(list, blocks_.Get(), kRead);
        tracker.transition(list, smooth_.Get(), kWrite);
        tracker.transition(list, sums_.Get(), kWrite);
        smoothPass_.bind(list, c, gpuHandleOf(smoothPass_, pairBase).ptr, gpuHandleOf(smoothPass_, pairBase + 3).ptr);
        list->Dispatch(blockGroupsX, blockGroupsY, 1);
        tracker.uavBarrier(list, smooth_.Get());
        tracker.uavBarrier(list, sums_.Get());
        // How far each region is trusted.
        tracker.transition(list, smooth_.Get(), kRead);
        tracker.transition(list, sums_.Get(), kRead);
        tracker.transition(list, field_.Get(), kWrite);
        trustPass_.bind(list, c, gpuHandleOf(trustPass_, 0).ptr, gpuHandleOf(trustPass_, 2).ptr);
        list->Dispatch(blockGroupsX, blockGroupsY, 1);
        tracker.uavBarrier(list, field_.Get());
        pair_ = int(parity);
        // Every few real frames this pair's blocks go to the count buffer, and the sample before it is counted.
        if (mapped_ && updates_ % sampleEvery_ == 0) {
            harvest();
            tracker.transition(list, blocks_.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
            D3D12_TEXTURE_COPY_LOCATION to{}, from{};
            to.pResource = readback_.Get(); to.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; to.PlacedFootprint = footprint_;
            from.pResource = blocks_.Get(); from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; from.SubresourceIndex = 0;
            list->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);
            samplePending_ = true;
        }
    }
    ++updates_;
    for (ID3D12Resource* r : {matte8_[parity].Get(), fine_[0].Get(), fine_[1].Get(), coarse_[0].Get(), coarse_[1].Get(), blocks_.Get(), smooth_.Get(), sums_.Get(), field_.Get()})
        tracker.transition(list, r, D3D12_RESOURCE_STATE_COMMON);
    return true;
}

bool PushCharacterPass::composes(unsigned slot, unsigned parity) const {
    if (slot >= generatedCount_ || parity > 1 || !writable_[slot] || !ready() || pair_ != int(parity)) return false;
    // Nobody in the picture, as far as the last count says: the frame is left as it was made.
    return !(samples_ > 0 && activeBlocks_ == 0);
}

bool PushCharacterPass::compose(ID3D12GraphicsCommandList* list, StateTracker& tracker, unsigned slot, unsigned parity, float t) {
    if (!list || !composes(slot, parity) || !real_[0] || !real_[1]) return false;
    auto* generated = generated_[slot];
    // The generated frame as it was made: the frame itself is written in place.
    tracker.transition(list, generated, D3D12_RESOURCE_STATE_COPY_SOURCE);
    tracker.transition(list, copy_.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
    list->CopyResource(copy_.Get(), generated);
    ID3D12Resource* const reads[] = {copy_.Get(), real_[0], real_[1], matte8_[0].Get(), matte8_[1].Get(), field_.Get()};
    for (ID3D12Resource* r : reads) tracker.transition(list, r, kRead);
    tracker.transition(list, generated, kWrite);
    float c[kConstants]{};
    c[0] = std::bit_cast<float>(width_); c[1] = std::bit_cast<float>(height_); c[2] = std::clamp(t, 0.0f, 1.0f);
    const UINT base = (slot * 2 + parity) * kComposeStride;
    composePass_.bind(list, c, gpuHandleOf(composePass_, base).ptr, gpuHandleOf(composePass_, base + 6).ptr);
    list->Dispatch(groups(width_, kGroups8), groups(height_, kGroups8), 1);
    tracker.uavBarrier(list, generated);
    tracker.transition(list, generated, D3D12_RESOURCE_STATE_COMMON);
    for (ID3D12Resource* r : reads) tracker.transition(list, r, D3D12_RESOURCE_STATE_COMMON);
    return true;
}
}
