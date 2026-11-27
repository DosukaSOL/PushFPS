// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#include "HudDetection.h"

#include <algorithm>
#include <thread>

namespace pushfps {
namespace hud = veyra::engine::hud;

HudDetection::HudDetection(EngineSource engine, QObject* parent) : QObject(parent), engine_(std::move(engine)) {
    timer_.setInterval(350);  // the Windows sampler's interval
    connect(&timer_, &QTimer::timeout, this, [this] { take(); });
}

HudDetection::~HudDetection() { timer_.stop(); }

bool HudDetection::start(int samples) {
    const auto engine = engine_ ? engine_() : nullptr;
    if (!engine || !engine->newestImage()) return false;
    ++revision_;
    running_ = true;
    failed_ = false;
    error_.clear();
    target_ = std::clamp(samples, int(hud::kMinFrames), 32);
    frames_.clear();
    run_.reset();
    take();  // the first frame now, the others 350 ms apart
    if (running_) timer_.start();
    return true;
}

void HudDetection::cancel() {
    timer_.stop();
    running_ = false;
    frames_.clear();
    run_.reset();
}

void HudDetection::take() {
    if (!running_) return;
    const auto engine = engine_ ? engine_() : nullptr;
    const auto image = engine ? engine->newestImage() : nullptr;
    hud::LumaFrame frame;
    if (!image || image->empty() ||
        !hud::reduce(image->px.data(), size_t(image->width) * 4, uint32_t(image->width), uint32_t(image->height),
                     hud::PixelLayout::Bgra8, frame)) {
        timer_.stop();
        running_ = false;
        failed_ = true;
        frames_.clear();
        return;
    }
    frames_.push_back(std::move(frame));
    if (int(frames_.size()) < target_) return;
    // The last frame: detect() over all of them on a thread of its own (it reads 12 grids of 1920 x 1080).
    timer_.stop();
    auto run = std::make_shared<Run>();
    run_ = run;
    std::thread([run, frames = std::move(frames_)] {
        auto found = hud::detect(frames);
        std::lock_guard lock(run->m);
        run->detection = std::move(found);
        run->done = true;
    }).detach();
    frames_.clear();
}

QVariantMap HudDetection::status() const {
    QVariantMap m;
    bool done = false;
    hud::Detection detection;
    if (run_) {
        std::lock_guard lock(run_->m);
        done = run_->done;
        if (done) detection = run_->detection;
    }
    m[QStringLiteral("running")] = running_ && !done;
    m[QStringLiteral("done")] = done;
    m[QStringLiteral("failed")] = failed_;
    m[QStringLiteral("taken")] = done ? target_ : run_ ? target_ : int(frames_.size());
    m[QStringLiteral("target")] = target_;
    m[QStringLiteral("error")] = error_;
    m[QStringLiteral("tooStill")] = detection.tooStill;
    m[QStringLiteral("motion")] = double(detection.motion);
    m[QStringLiteral("revision")] = revision_;
    QVariantList regions;
    for (const auto& r : detection.regions)
        regions << QVariantMap{{QStringLiteral("left"), double(r.left)}, {QStringLiteral("top"), double(r.top)},
                               {QStringLiteral("right"), double(r.right)}, {QStringLiteral("bottom"), double(r.bottom)},
                               {QStringLiteral("ellipse"), false}};
    m[QStringLiteral("regions")] = regions;
    return m;
}

}  // namespace pushfps
