// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PushFPS automatic Remote Play latency test (engine/PushInputLatency.h): presses the PS button on the
// PS5 through Remote Play, waits for the control centre to appear in the presented picture, closes
// it again, and repeats. PushFPS's share is its measured processing time (arrival to presentation).
#include "../QmlPlayerBridge_p.h"
#include "veyra/engine/PushInputLatencyProbe.h"
#include "veyra/Log.h"

#include <QDateTime>
#include <cmath>
#include <format>

namespace veyra::ui {
namespace {
constexpr qint64 kBaselineMs = 300, kHoldMs = 90, kAnswerTimeoutMs = 1200, kCloseWaitMs = 700, kSettleMs = 1300;
// The controller path exists only in builds with PS5 Remote Play; without it the test never starts
// (the snapshot never reports a Remote Play source), and this keeps such builds compiling.
template <class Engine> void sendController(Engine& engine, const remoteplay::ControllerState& state) {
#ifdef VEYRA_ENABLE_REMOTEPLAY
    engine.remotePlayController(state);
#else
    (void)engine; (void)state;
#endif
}
}

remoteplay::ControllerState QmlPlayerBridge::pushLatencyPress() const {
    remoteplay::ControllerState state;
    state.inputActive = true;   // a released pad, not focus loss
    if (impl_->latency.pressing) state.buttons = remoteplay::ControllerState::PS;
    return state;
}

bool QmlPlayerBridge::pushLatencyTestBegin(int trials) {
    auto& t = impl_->latency;
    const auto s = impl_->engine.snapshot();
    if (!s.remotePlay || !s.running) { emit notice(tr("延迟测试需要正在进行的 PS5 串流"), true); return false; }
    if (t.active) return true;
    t = {};
    t.timer = impl_->latency.timer;
    t.active = true; t.trials = std::clamp(trials, 3, 20); t.state = QStringLiteral("running");
    t.phaseStart = QDateTime::currentMSecsSinceEpoch();
    impl_->engine.inputLatencyProbe().beginWindow();
    if (!t.timer) {
        t.timer = new QTimer(this);
        t.timer->setTimerType(Qt::PreciseTimer);
        connect(t.timer, &QTimer::timeout, this, [this] { pushLatencyTick(); });
        impl_->latency.timer = t.timer;
    }
    t.timer->start(4);
    veyra::log::info("push-latency", std::format("remote play latency test started trials={}", t.trials));
    return true;
}

void QmlPlayerBridge::pushLatencyTestCancel() {
    auto& t = impl_->latency;
    if (!t.active) return;
    t.pressing = false;
    sendController(impl_->engine, pushLatencyPress());
    t.active = false; t.state = QStringLiteral("idle");
    if (t.timer) t.timer->stop();
    impl_->engine.inputLatencyProbe().endWindow();
    veyra::log::info("push-latency", "cancelled");
}

void QmlPlayerBridge::pushLatencyTick() {
    auto& t = impl_->latency;
    if (!t.active) { if (t.timer) t.timer->stop(); return; }
    auto& probe = impl_->engine.inputLatencyProbe();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const auto s = impl_->engine.snapshot();
    if (!s.remotePlay || !s.running) {
        t.error = tr("PS5 串流已停止，延迟测试已取消"); t.state = QStringLiteral("failed");
        t.active = false; t.pressing = false; if (t.timer) t.timer->stop(); probe.endWindow(); return;
    }
    const double answer = probe.poll();
    const auto press = [&](bool down) { t.pressing = down; sendController(impl_->engine, pushLatencyPress()); };
    switch (t.phase) {
    case 0:   // baseline frames, then the press
        if (now - t.phaseStart >= kBaselineMs) {
            press(true);
            probe.markSent(engine::PushInputLatencyProbe::now100ns());
            t.phase = 1; t.phaseStart = now;
        }
        break;
    case 1:   // hold the button briefly
        if (now - t.phaseStart >= kHoldMs) { press(false); t.phase = 2; }
        if (answer >= 0) { t.results.push_back(answer); t.phase = 3; t.phaseStart = now; }
        break;
    case 2:   // wait for the picture to answer
        if (answer >= 0) { t.results.push_back(answer); t.phase = 3; t.phaseStart = now; }
        else if (now - t.phaseStart >= kAnswerTimeoutMs) {
            veyra::log::info("push-latency", std::format("trial {} no answer seen", t.trial + 1));
            t.phase = 3; t.phaseStart = now;
        }
        break;
    case 3: {  // close the control centre (PS again), let it settle, next trial
        const qint64 since = now - t.phaseStart;
        if (since >= kCloseWaitMs && since < kCloseWaitMs + kHoldMs) { if (!t.pressing) press(true); }
        else if (since >= kCloseWaitMs + kHoldMs && t.pressing) press(false);
        if (since >= kCloseWaitMs + kHoldMs + kSettleMs) {
            // PushFPS's own processing time, sampled between trials.
            const auto& flow = impl_->snapshot.metrics.flow;
            if (flow.softwareLatencyMs && std::isfinite(*flow.softwareLatencyMs) && *flow.softwareLatencyMs > 0) {
                t.pushfpsMs += *flow.softwareLatencyMs; ++t.pushfpsSamples;
            }
            if (++t.trial >= t.trials) {
                t.active = false; t.state = QStringLiteral("done");
                if (t.timer) t.timer->stop();
                probe.endWindow();
                const auto sum = engine::latency::summarize(t.results, uint32_t(t.trials));
                veyra::log::info("push-latency", std::format("done answered={}/{} meanMs={:.1f} medianMs={:.1f} minMs={:.1f} maxMs={:.1f} pushfpsMs={:.1f}",
                    sum.answered, sum.trials, sum.meanMs, sum.medianMs, sum.minMs, sum.maxMs, t.pushfpsSamples ? t.pushfpsMs / t.pushfpsSamples : 0.0));
            } else {
                probe.beginWindow();
                t.phase = 0; t.phaseStart = now;
            }
        }
        break;
    }
    }
}

QVariantMap QmlPlayerBridge::pushLatencyTestStatus() const {
    const auto& t = impl_->latency;
    const auto sum = engine::latency::summarize(t.results, uint32_t(t.trials));
    const double pushfps = t.pushfpsSamples ? t.pushfpsMs / t.pushfpsSamples : 0.0;
    QVariantList list;
    for (double ms : t.results) list << ms;
    return QVariantMap{{"state", t.state}, {"error", t.error}, {"trial", t.trial}, {"trials", t.trials},
                       {"answered", int(sum.answered)}, {"results", list}, {"meanMs", sum.meanMs}, {"medianMs", sum.medianMs},
                       {"minMs", sum.minMs}, {"maxMs", sum.maxMs}, {"pushfpsMs", pushfps},
                       {"remoteMs", sum.answered ? std::max(0.0, sum.medianMs - pushfps) : 0.0}};
}

} // namespace veyra::ui
