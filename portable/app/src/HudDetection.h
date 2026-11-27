// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once
// Automatic HUD detection for the preview: the Windows build's detector (include/veyra/engine/PushHud.h,
// src/engine/PushHud.cpp, used as they are) fed the way the Windows sampler (PushHudSampler) feeds it: one
// real frame every 350 ms, reduced to the luma grid at once (so a frame is never kept), and detect() over
// all of them after the last. The frames are the engine's newest real frame (Engine::newestImage, read
// only), so nothing is drawn or copied while no detection runs. detect() runs on a thread of its own.
#include "veyra/engine/PushHud.h"
#include "veyra/portable/Engine.h"

#include <QObject>
#include <QTimer>
#include <QVariantMap>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace pushfps {

class HudDetection : public QObject {
public:
    using EngineSource = std::function<std::shared_ptr<veyra::portable::Engine>()>;
    explicit HudDetection(EngineSource engine, QObject* parent = nullptr);
    ~HudDetection() override;

    // Starts over `samples` frames (at least the detector's kMinFrames, at most 32). False with nothing playing.
    bool start(int samples);
    void cancel();
    // The Windows bridge's pushHudDetection() map: running, done, failed, taken, target, error, tooStill,
    // motion, regions ({left, top, right, bottom, ellipse}), revision.
    QVariantMap status() const;

private:
    struct Run {
        std::mutex m;
        bool done = false;
        veyra::engine::hud::Detection detection;
    };
    void take();

    EngineSource engine_;
    QTimer timer_;
    bool running_ = false, failed_ = false;
    int target_ = 0;
    quint64 revision_ = 0;
    QString error_;
    std::vector<veyra::engine::hud::LumaFrame> frames_;
    std::shared_ptr<Run> run_;
};

}  // namespace pushfps
