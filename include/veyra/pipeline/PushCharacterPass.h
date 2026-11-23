#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// PushFPS Character Hold on the graphics card (the rule itself is pipeline/PushCharacter.h).
//   Once a real frame, after it is final, in three steps round the matting network's run:
//     pack   (in a command list that is then submitted): the frame as the network takes it
//     run    (no list): the network is queued on the same queue
//     finish (in a later list): its answer becomes the matte and the matted pictures, and with the frame
//            before, the character's own motion and how far it is trusted (once a pair, whatever the multiplier)
//   Once a generated frame, compose, from a copy of the frame as the method made it:
//     the frame at the fine size; per block the table of how well the source frame's character fits it at each
//     offset round the search's answer; the place of each block (its neighbourhood's tables together), settled
//     with its neighbours where the method broke the frame; where the frame differs from the placed character;
//     and the character laid over there, written in place at the pixels that have something laid over.
// The real frames are never changed, and frame generation is given exactly what it always was.
#include "veyra/pipeline/GpuPassUtils.h"
#include "veyra/pipeline/PushCharacter.h"
#include "veyra/pushflow/PushMatteBackend.h"
#include <string>

namespace veyra::pipeline {

class PushCharacterPass {
public:
    static constexpr UINT kConstants = 4;
    static constexpr UINT kMaxGenerated = 32;
    // encoding: how the frames are encoded, for the network's sake, as PushFlow's pack takes it
    // (0 already encoded, 1 linear SDR, 2 scRGB). runtimeDir: where runtime\pushflow is (the model and its runtime).
    bool initialize(ID3D12Device* device, ID3D12CommandQueue* queue, const std::wstring& runtimeDir, uint32_t width, uint32_t height,
                    DXGI_FORMAT format, unsigned encoding);
    // real[0..1]: the graph's two real-frame textures by parity; generated: its generated-frame textures.
    void createViews(ID3D12Device* device, DescriptorStager& stager, ID3D12Resource* const real[2], ID3D12Resource* const* generated,
                     unsigned generatedCount);
    bool pack(ID3D12GraphicsCommandList* list, StateTracker& tracker, unsigned parity);
    // startOver: there is no valid frame before this one (start, seek, settings).
    bool run(bool startOver);
    bool finish(ID3D12GraphicsCommandList* list, StateTracker& tracker, unsigned parity);
    // generated[slot] lies `t` of the way from real frame 1 - parity to real frame parity.
    bool composes(unsigned slot, unsigned parity) const;
    bool compose(ID3D12GraphicsCommandList* list, StateTracker& tracker, unsigned slot, unsigned parity, float t);
    // "on", or why not (the backend's word: "no-files", "runtime", "model", "session", "failed", "not-built").
    const std::string& state() const { return matte_.status().state; }
    bool ready() const { return matte_.status().ready && !failed_; }
    std::string describe() const;
    // What the pass found. Every `sampleEvery` real frames (4 unless set) the blocks of that pair are copied to a
    // small buffer and counted one sample later (nothing ever waits for the card): how many of the 2040 blocks
    // had a person in them. While the count is 0 a generated frame is left alone without being copied or read.
    unsigned activeBlocks() const { return activeBlocks_; }
    uint64_t realFrames() const { return updates_; }
    uint64_t samples() const { return samples_; }
    uint64_t samplesWithPeople() const { return samplesWithPeople_; }
    double networkCpuMs() const { return matte_.lastRunCpuMs(); }
    void setSampleEvery(unsigned frames) { sampleEvery_ = frames ? frames : 1u; }
    void harvest();
    // For tests.
    ID3D12Resource* pictureBuffer() const { return matte_.pictureBuffer(); }
    ID3D12Resource* answerBuffer() const { return matte_.matteBuffer(); }
    ID3D12Resource* matteTexture(unsigned parity) const { return parity < 2 ? matte8_[parity].Get() : nullptr; }
    ID3D12Resource* fineTexture(unsigned parity) const { return parity < 2 ? fine_[parity].Get() : nullptr; }
    ID3D12Resource* coarseTexture(unsigned parity) const { return parity < 2 ? coarse_[parity].Get() : nullptr; }
    ID3D12Resource* blocksTexture() const { return blocks_.Get(); }
    ID3D12Resource* fieldTexture() const { return field_.Get(); }
    ID3D12Resource* madeTexture() const { return made1_.Get(); }
    ID3D12Resource* tablesTexture() const { return tables_.Get(); }
    ID3D12Resource* placedTexture() const { return placed_.Get(); }
    ID3D12Resource* placeTexture() const { return place_.Get(); }
    ID3D12Resource* apartTexture() const { return apart_.Get(); }
    ID3D12Resource* differTexture() const { return differ_.Get(); }
    int pairParity() const { return pair_; }

private:
    uint32_t width_ = 0, height_ = 0;
    unsigned encoding_ = 0, generatedCount_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN;
    pushflow::PushMatteBackend matte_;
    ComputePass packPass_, mattePass_, coarsePass_, searchPass_, smoothPass_, trustPass_;
    ComputePass madePass_, tablesPass_, placePass_, settlePass_, apartPass_, differPass_, composePass_;
    ComPtr<ID3D12Resource> matte8_[2], fine_[2], coarse_[2];   // per real frame, by parity
    ComPtr<ID3D12Resource> blocks_, smooth_, sums_, field_;    // per pair
    ComPtr<ID3D12Resource> copy_;                              // a generated frame as it was made
    ComPtr<ID3D12Resource> made1_, tables_, placed_, place_, apart_, differ_;   // per generated frame
    bool writable_[kMaxGenerated]{};
    bool have_[2]{};          // the matted pictures of that parity are this run's
    bool packed_ = false, ran_ = false, failed_ = false, startOver_ = true;
    int pair_ = -1;           // the parity of the newer frame the field is for (-1: none)
    ComPtr<ID3D12Resource> readback_;
    const uint8_t* mapped_ = nullptr;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint_{};
    unsigned sampleEvery_ = 4, activeBlocks_ = 0;
    bool samplePending_ = false, foundLogged_ = false;
    uint64_t updates_ = 0, samples_ = 0, samplesWithPeople_ = 0;
    ID3D12Resource* real_[2]{};
    ID3D12Resource* generated_[kMaxGenerated]{};
};
}
