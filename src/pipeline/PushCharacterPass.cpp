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
constexpr DXGI_FORMAT kTableFormat = DXGI_FORMAT_R32_UINT;               // per block and offset: the sum times 2048, plus the count
constexpr DXGI_FORMAT kApartFormat = DXGI_FORMAT_R16_UINT;               // per fine pixel: 0, or 1 + the difference
constexpr DXGI_FORMAT kDifferFormat = DXGI_FORMAT_R8_UINT;               // per fine pixel: 0..255
constexpr D3D12_RESOURCE_STATES kRead = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
constexpr D3D12_RESOURCE_STATES kWrite = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
// Descriptor slots of each pass (the layouts createViews fills and the steps bind).
//   pack:    0 the picture's tensor (UAV); 1 + parity the real frame.
//   matte:   0 nothing a shader reads; 1 + 4 parity: the picture's tensor, the answer's, the matte, the fine picture (UAVs).
//   coarse:  2 parity the fine picture; 2 parity + 1 the coarse one (UAV).
//   search:  5 parity: older coarse, newer coarse, older fine, newer fine; then the found blocks (UAV).
//   smooth:  5 parity: the found blocks, older fine, newer fine; then the smoothed motion and the sums (UAVs).
//   trust:   0 the smoothed motion, 1 the sums, 2 the field (UAV).
//   made:    0 the copy; 1 the generated frame at the fine size (UAV).
//   tables:  6 parity: the made frame, older fine, newer fine, the found blocks, the field; then the tables (UAV).
//   place:   0 the tables, 1 the found blocks, 2 the field; 3 the places as found (UAV).
//   settle:  0 the places as found, 1 the field; 2 the places (UAV).
//   apart:   5 parity: the made frame, older fine, newer fine, the places; then how far apart (UAV).
//   differ:  0 how far apart, 1 the places; 2 where it differs (UAV).
//   compose: 8 (2 slot + parity): the copy, older frame, newer frame, older matte, newer matte, the places, where it differs; then the generated frame (UAV).
constexpr UINT kComposeStride = 8;
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
    made1_ = makeTexture(device, character::kFineW, character::kFineH, kPictureFormat, true);
    tables_ = makeTexture(device, character::kBlocksX * character::kPlaceAcross, character::kBlocksY * character::kPlaceAcross, kTableFormat, true);
    placed_ = makeTexture(device, character::kBlocksX, character::kBlocksY, kBlockFormat, true);
    place_ = makeTexture(device, character::kBlocksX, character::kBlocksY, kBlockFormat, true);
    apart_ = makeTexture(device, character::kFineW, character::kFineH, kApartFormat, true);
    differ_ = makeTexture(device, character::kFineW, character::kFineH, kDifferFormat, true);
    if (!matte8_[0] || !matte8_[1] || !fine_[0] || !fine_[1] || !coarse_[0] || !coarse_[1] || !blocks_ || !smooth_ || !sums_ || !field_ || !copy_ || !made1_ || !tables_ || !placed_ ||
        !place_ || !apart_ || !differ_)
        return false;
    std::vector<uint8_t> cs;
    if (!packPass_.loadShader("PushCharacterPack.dxil", cs) || !packPass_.create(device, cs, 3, 1, 1, kConstants)) return false;
    if (!mattePass_.loadShader("PushCharacterMatte.dxil", cs) || !mattePass_.create(device, cs, 1 + 2 * 4, 1, 4, kConstants)) return false;
    if (!coarsePass_.loadShader("PushCharacterCoarse.dxil", cs) || !coarsePass_.create(device, cs, 4, 1, 1, kConstants)) return false;
    if (!searchPass_.loadShader("PushCharacterSearch.dxil", cs) || !searchPass_.create(device, cs, 10, 4, 1, kConstants)) return false;
    if (!smoothPass_.loadShader("PushCharacterSmooth.dxil", cs) || !smoothPass_.create(device, cs, 10, 3, 2, kConstants)) return false;
    if (!trustPass_.loadShader("PushCharacterTrust.dxil", cs) || !trustPass_.create(device, cs, 3, 2, 1, kConstants)) return false;
    if (!madePass_.loadShader("PushCharacterMade.dxil", cs) || !madePass_.create(device, cs, 2, 1, 1, kConstants)) return false;
    if (!tablesPass_.loadShader("PushCharacterTables.dxil", cs) || !tablesPass_.create(device, cs, 12, 5, 1, kConstants)) return false;
    if (!placePass_.loadShader("PushCharacterPlace.dxil", cs) || !placePass_.create(device, cs, 4, 3, 1, kConstants)) return false;
    if (!settlePass_.loadShader("PushCharacterSettle.dxil", cs) || !settlePass_.create(device, cs, 3, 2, 1, kConstants)) return false;
    if (!apartPass_.loadShader("PushCharacterApart.dxil", cs) || !apartPass_.create(device, cs, 10, 4, 1, kConstants)) return false;
    if (!differPass_.loadShader("PushCharacterDiffer.dxil", cs) || !differPass_.create(device, cs, 3, 2, 1, kConstants)) return false;
    if (!composePass_.loadShader("PushCharacterCompose.dxil", cs) || !composePass_.create(device, cs, kMaxGenerated * 2 * kComposeStride, 7, 1, kConstants)) return false;
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
    stager.stageSrv(copy_.Get(), nullptr, madePass_.heap.Get(), 0);
    makeUav(device, made1_.Get(), kPictureFormat, cpuHandleOf(madePass_, 1));
    stager.stageSrv(tables_.Get(), nullptr, placePass_.heap.Get(), 0);
    stager.stageSrv(blocks_.Get(), nullptr, placePass_.heap.Get(), 1);
    stager.stageSrv(field_.Get(), nullptr, placePass_.heap.Get(), 2);
    makeUav(device, placed_.Get(), kBlockFormat, cpuHandleOf(placePass_, 3));
    stager.stageSrv(placed_.Get(), nullptr, settlePass_.heap.Get(), 0);
    stager.stageSrv(field_.Get(), nullptr, settlePass_.heap.Get(), 1);
    makeUav(device, place_.Get(), kBlockFormat, cpuHandleOf(settlePass_, 2));
    stager.stageSrv(apart_.Get(), nullptr, differPass_.heap.Get(), 0);
    stager.stageSrv(place_.Get(), nullptr, differPass_.heap.Get(), 1);
    makeUav(device, differ_.Get(), kDifferFormat, cpuHandleOf(differPass_, 2));
    for (unsigned parity = 0; parity < 2; ++parity) {
        const UINT tableBase = 6 * parity, apartBase = 5 * parity;
        stager.stageSrv(made1_.Get(), nullptr, tablesPass_.heap.Get(), tableBase);
        stager.stageSrv(fine_[1 - parity].Get(), nullptr, tablesPass_.heap.Get(), tableBase + 1);
        stager.stageSrv(fine_[parity].Get(), nullptr, tablesPass_.heap.Get(), tableBase + 2);
        stager.stageSrv(blocks_.Get(), nullptr, tablesPass_.heap.Get(), tableBase + 3);
        stager.stageSrv(field_.Get(), nullptr, tablesPass_.heap.Get(), tableBase + 4);
        makeUav(device, tables_.Get(), kTableFormat, cpuHandleOf(tablesPass_, tableBase + 5));
        stager.stageSrv(made1_.Get(), nullptr, apartPass_.heap.Get(), apartBase);
        stager.stageSrv(fine_[1 - parity].Get(), nullptr, apartPass_.heap.Get(), apartBase + 1);
        stager.stageSrv(fine_[parity].Get(), nullptr, apartPass_.heap.Get(), apartBase + 2);
        stager.stageSrv(place_.Get(), nullptr, apartPass_.heap.Get(), apartBase + 3);
        makeUav(device, apart_.Get(), kApartFormat, cpuHandleOf(apartPass_, apartBase + 4));
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
            stager.stageSrv(place_.Get(), nullptr, composePass_.heap.Get(), base + 5);
            stager.stageSrv(differ_.Get(), nullptr, composePass_.heap.Get(), base + 6);
            makeUav(device, generated_[slot], format_, cpuHandleOf(composePass_, base + 7));
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
    t = std::clamp(t, 0.0f, 1.0f);
    const uint32_t newer = character::sourceIsNewer(t) ? 1u : 0u;
    const float share = character::shareOf(t);
    const UINT blockGroupsX = groups(character::kBlocksX, kGroups4), blockGroupsY = groups(character::kBlocksY, kGroups4);
    const UINT fineGroupsX = groups(character::kFineW, kGroups8), fineGroupsY = groups(character::kFineH, kGroups8);
    // The generated frame as it was made: the frame itself is written in place.
    tracker.transition(list, generated, D3D12_RESOURCE_STATE_COPY_SOURCE);
    tracker.transition(list, copy_.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
    list->CopyResource(copy_.Get(), generated);
    // It at the fine size, as the network would be given it.
    tracker.transition(list, copy_.Get(), kRead);
    tracker.transition(list, made1_.Get(), kWrite);
    {
        float c[kConstants]{};
        c[0] = std::bit_cast<float>(width_); c[1] = std::bit_cast<float>(height_); c[2] = std::bit_cast<float>(uint32_t(encoding_));
        madePass_.bind(list, c, gpuHandleOf(madePass_, 0).ptr, gpuHandleOf(madePass_, 1).ptr);
        list->Dispatch(fineGroupsX, fineGroupsY, 1);
        tracker.uavBarrier(list, made1_.Get());
    }
    // Per block and offset: how well the source frame's character fits the made frame there (one group a block).
    for (ID3D12Resource* r : {made1_.Get(), fine_[0].Get(), fine_[1].Get(), blocks_.Get(), field_.Get()}) tracker.transition(list, r, kRead);
    tracker.transition(list, tables_.Get(), kWrite);
    {
        float c[kConstants]{};
        c[0] = share; c[1] = std::bit_cast<float>(newer);
        tablesPass_.bind(list, c, gpuHandleOf(tablesPass_, 6 * parity).ptr, gpuHandleOf(tablesPass_, 6 * parity + 5).ptr);
        list->Dispatch(character::kBlocksX, character::kBlocksY, 1);
        tracker.uavBarrier(list, tables_.Get());
    }
    // Each block's place, from its neighbourhood's tables together.
    tracker.transition(list, tables_.Get(), kRead);
    tracker.transition(list, placed_.Get(), kWrite);
    {
        float c[kConstants]{};
        c[0] = share;
        placePass_.bind(list, c, gpuHandleOf(placePass_, 0).ptr, gpuHandleOf(placePass_, 3).ptr);
        list->Dispatch(blockGroupsX, blockGroupsY, 1);
        tracker.uavBarrier(list, placed_.Get());
    }
    // Settled with the neighbours where the method broke the frame.
    tracker.transition(list, placed_.Get(), kRead);
    tracker.transition(list, place_.Get(), kWrite);
    {
        float c[kConstants]{};
        c[0] = share;
        settlePass_.bind(list, c, gpuHandleOf(settlePass_, 0).ptr, gpuHandleOf(settlePass_, 2).ptr);
        list->Dispatch(blockGroupsX, blockGroupsY, 1);
        tracker.uavBarrier(list, place_.Get());
    }
    // Where the made frame differs from the placed character.
    tracker.transition(list, place_.Get(), kRead);
    tracker.transition(list, apart_.Get(), kWrite);
    {
        float c[kConstants]{};
        c[0] = std::bit_cast<float>(newer);
        apartPass_.bind(list, c, gpuHandleOf(apartPass_, 5 * parity).ptr, gpuHandleOf(apartPass_, 5 * parity + 4).ptr);
        list->Dispatch(fineGroupsX, fineGroupsY, 1);
        tracker.uavBarrier(list, apart_.Get());
    }
    tracker.transition(list, apart_.Get(), kRead);
    tracker.transition(list, differ_.Get(), kWrite);
    {
        float c[kConstants]{};
        differPass_.bind(list, c, gpuHandleOf(differPass_, 0).ptr, gpuHandleOf(differPass_, 2).ptr);
        list->Dispatch(fineGroupsX, fineGroupsY, 1);
        tracker.uavBarrier(list, differ_.Get());
    }
    // The character laid over there.
    ID3D12Resource* const reads[] = {copy_.Get(), real_[0], real_[1], matte8_[0].Get(), matte8_[1].Get(), place_.Get(), differ_.Get()};
    for (ID3D12Resource* r : reads) tracker.transition(list, r, kRead);
    tracker.transition(list, generated, kWrite);
    {
        float c[kConstants]{};
        c[0] = std::bit_cast<float>(width_); c[1] = std::bit_cast<float>(height_); c[2] = std::bit_cast<float>(newer);
        const UINT base = (slot * 2 + parity) * kComposeStride;
        composePass_.bind(list, c, gpuHandleOf(composePass_, base).ptr, gpuHandleOf(composePass_, base + 7).ptr);
        list->Dispatch(groups(width_, kGroups8), groups(height_, kGroups8), 1);
        tracker.uavBarrier(list, generated);
    }
    tracker.transition(list, generated, D3D12_RESOURCE_STATE_COMMON);
    for (ID3D12Resource* r : reads) tracker.transition(list, r, D3D12_RESOURCE_STATE_COMMON);
    for (ID3D12Resource* r : {made1_.Get(), fine_[0].Get(), fine_[1].Get(), blocks_.Get(), field_.Get(), tables_.Get(), placed_.Get(), apart_.Get()})
        tracker.transition(list, r, D3D12_RESOURCE_STATE_COMMON);
    return true;
}
}
