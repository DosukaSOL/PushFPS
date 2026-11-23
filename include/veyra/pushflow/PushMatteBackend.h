// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// The matting network of PushFPS Character Hold (pipeline/PushCharacter.h): for one picture it says, per
// pixel, how much of it is a person. Robust Video Matting (Lin, Yang, Saleemi, Sengupta; GPL-3.0), its
// MobileNetV3 model with the sizes fixed by scripts/pushflow/prepare_matte.py (1 x 3 x 1080 x 1920, the
// downsample ratio a constant, no foreground output), run through ONNX Runtime on DirectML on the graph's own
// command queue, like PushFlow and from PushFlow's runtime folder. Everything stays on the graphics card:
// the pass packs a frame into pictureBuffer(), run() queues the network, and the pass reads matteBuffer() in
// a later command list on the same queue; the queue's order is the only synchronisation.
//
// The network remembers: four recurrent states go from each run into the next (they are what keeps the matte
// steady from frame to frame). They live in two sets of buffers that change places; a run that starts over
// (a new scene) reads four buffers that stay zero.
#pragma once
#include <d3d12.h>
#include <cstdint>
#include <memory>
#include <string>

namespace veyra::pushflow {

class PushMatteBackend {
public:
    struct Status {
        bool ready = false;
        // "" not started; "on"; "not-built" (this build has no ONNX Runtime); "no-files"; "runtime" (the DLLs
        // or their entry points); "model" (the file is not the one prepare_matte.py makes); "session";
        // "failed" (it ran and then faulted or returned an error).
        std::string state;
        uint32_t width = 0, height = 0;   // the picture the network takes
        bool queued = false;              // runs do not wait for the graphics card
        bool poolHeld = false;
        double loadMs = 0.0;
    };
    PushMatteBackend();
    ~PushMatteBackend();
    PushMatteBackend(const PushMatteBackend&) = delete;
    PushMatteBackend& operator=(const PushMatteBackend&) = delete;

    // Are the runtime and the model in runtimeDir\pushflow? `missing`: the first file that is not.
    static bool installed(const std::wstring& runtimeDir, std::wstring* missing = nullptr);
    bool initialize(ID3D12Device* device, ID3D12CommandQueue* queue, const std::wstring& runtimeDir);
    void shutdown();
    // Planar floats, always in the UNORDERED_ACCESS state (as DirectML expects): [1, 3, height, width] and
    // [1, 1, height, width].
    ID3D12Resource* pictureBuffer() const;
    ID3D12Resource* matteBuffer() const;
    // One run for what pictureBuffer() holds once everything submitted to the queue so far has run.
    // startOver: this picture does not follow the one before (start, a seek, a cut).
    bool run(bool startOver);
    const Status& status() const;
    double lastRunCpuMs() const;
    uint64_t runs() const;

private:
    struct Impl;
    std::unique_ptr<Impl> p_;
};

}
