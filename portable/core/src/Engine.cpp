// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#include "veyra/portable/Engine.h"

#include "veyra/portable/Ambient.h"
#include "veyra/portable/Dedup.h"
#include "veyra/engine/PushAmbient.h"  // pure C++, shared with the Windows build: the gain
#include "veyra/engine/PushPacingRepair.h"  // pure C++, shared with the Windows build

#include <algorithm>
#include <chrono>
#include <cmath>
#include <future>
#include <vector>

namespace veyra::portable {

struct Engine::Pacing {
    veyra::engine::PushPacingRepair repair;
};

namespace {

constexpr size_t kMaxFrames = 10;  // ~300 ms of real frames at 30 FPS: enough for the delay plus slack
constexpr int64_t kMs = 1000000;

// Median interval of a list of increasing times, 0 if fewer than 3.
double medianInterval(const std::deque<int64_t>& times) {
    if (times.size() < 3) return 0;
    std::vector<int64_t> d;
    for (size_t i = 1; i < times.size(); ++i) d.push_back(times[i] - times[i - 1]);
    std::nth_element(d.begin(), d.begin() + d.size() / 2, d.end());
    return double(d[d.size() / 2]);
}

// Largest of the last n intervals, 0 if fewer than 2 times.
double maxInterval(const std::deque<int64_t>& times, size_t n) {
    double worst = 0;
    for (size_t i = times.size() > n + 1 ? times.size() - n - 1 : 0; i + 1 < times.size(); ++i) worst = std::max(worst, double(times[i + 1] - times[i]));
    return worst;
}

double rateOver(const std::deque<int64_t>& times) {
    if (times.size() < 2) return 0;
    const double span = double(times.back() - times.front());
    return span > 0 ? double(times.size() - 1) * 1e9 / span : 0;
}

// The rate a readout shows (stats(now)): rateOver, except that one whose events stop falls off instead of
// keeping the last rate it had. Once the newest event is older than a quarter of a second (or two of its
// own intervals, if longer), the span runs up to now; a second after the newest it is 0. The quarter
// second is there because the newest entries are written a little after the fact (a frame is kept once
// the next one is compared with it), so a picture that moves is never read as one that stopped.
// (Pacing reads rateOver: a picture that stands still must not change how the next one is timed.)
double liveRate(const std::deque<int64_t>& times, int64_t now) {
    if (times.size() < 2) return 0;
    const double span = double(times.back() - times.front());
    if (span <= 0) return 0;
    const double since = double(now - times.back());
    if (since > 1e9) return 0;
    const double interval = span / double(times.size() - 1);
    const bool stopped = since > std::max(2 * interval, 250e6);
    return double(times.size() - 1) * 1e9 / (stopped ? span + since : span);
}

}  // namespace

Engine::Engine(EngineSettings settings) : s_(settings), pacing_(std::make_unique<Pacing>()) {
    clock_ = [] { return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); };
    worker_ = std::thread([this] { workerLoop(); });
}

void Engine::setClock(std::function<int64_t()> clock) {
    std::lock_guard lk(m_);
    if (clock) clock_ = std::move(clock);
}

void Engine::holdWorker(bool hold) {
    {
        std::lock_guard lk(m_);
        held_ = hold;
    }
    cv_.notify_all();
}

Engine::~Engine() {
    {
        std::lock_guard lk(m_);
        stop_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void Engine::setSettings(const EngineSettings& settings) {
    std::lock_guard lk(m_);
    const bool rebuild = settings.quality != s_.quality || settings.hudLock != s_.hudLock;
    if (settings.pacingRepair != s_.pacingRepair) pacing_->repair.reset();
    if (settings.crosshairHold != s_.crosshairHold) holdReset_ = true;
    if (settings.subtitleHold != s_.subtitleHold) subHoldReset_ = true;
    s_ = settings;
    if (rebuild) {
        // Fields of the old quality / masks stay valid for display; the next pair uses the new ones.
        hudReset_ = true;
        lastForward_ = {};
        lastBackward_ = {};
        // Another quality costs another time: what was measured says nothing about it.
        slack_.clear();
    }
}

EngineSettings Engine::settings() const {
    std::lock_guard lk(m_);
    return s_;
}

void Engine::reset() {
    std::lock_guard lk(m_);
    resetLocked();
}

void Engine::resetLocked() {
    frames_.clear();
    pairs_.clear();
    jobs_.clear();
    lastQuarter_.reset();
    arrivals_.clear();
    readoutArrivals_.clear();
    keptTimes_.clear();
    lastArrival_ = lastKeptArrival_ = lastContent_ = 0;
    lastWidth_ = lastHeight_ = 0;
    delayNs_ = 0;
    lateMarginMs_ = 0;
    slack_.clear();
    slackSeenB_ = 0;
    pacing_->repair.reset();
    hudReset_ = true;
    holdReset_ = true;
    subHoldReset_ = true;
    lastForward_ = {};
    lastBackward_ = {};
    shownTimes_.clear();
    lastShownA_ = lastShownB_ = 0;
    lastShownT_ = -1;
    lastNeededId_ = 0;
    ++generation_;  // a pair being estimated right now is thrown away when it finishes
}

void Engine::push(Bgra&& image, int64_t arrivalNs) {
    if (image.empty()) return;
    Gray luma = lumaOf(image);
    auto holder = std::make_shared<Bgra>(std::move(image));
    push(std::move(luma), [&] { return std::move(*holder); }, arrivalNs);
}

void Engine::push(Gray&& luma, const std::function<Bgra()>& makeImage, int64_t arrivalNs) {
    if (luma.empty()) return;
    const int width = luma.width, height = luma.height;
    // Pyramid outside the lock: it is the expensive part of a duplicate.
    auto pyramid = std::make_shared<Pyramid>(Pyramid::of(std::move(luma), 4));
    std::shared_ptr<const Gray> quarter;
    if (!pyramid->levels.empty()) quarter = std::make_shared<const Gray>(pyramid->levels[std::min<size_t>(2, pyramid->levels.size() - 1)]);

    // Decided before the colour conversion, from arrival times alone: where the timing starts over,
    // and this frame's number. The pair's motion needs the brightness pictures only, so it is queued
    // here and estimated while the colour picture is made (the conversion was 6 to 10 ms in front of
    // every pair's motion).
    double transport = 0;
    bool boundary = false;
    uint64_t id = 0, generation = 0;
    bool queued = false;
    // Crosshair Hold: whether it is wanted for this frame, whether to start it over, and the frame before's part.
    bool holdWanted = false, holdStartOver = false;
    bool ambientWanted = false;
    std::shared_ptr<const HoldFrame> holdBefore;
    uint64_t holdBeforeId = 0;
    // Subtitle Hold: the same, in the subtitle band.
    bool subWanted = false, subStartOver = false;
    std::shared_ptr<const HoldFrame> subBefore;
    {
        std::lock_guard lk(m_);
        ++st_.framesIn;
        // A new size starts over: nothing can be compared or interpolated across it.
        if (width != lastWidth_ || height != lastHeight_) {
            if (lastWidth_ != 0) resetLocked();
            lastWidth_ = width;
            lastHeight_ = height;
        }
        st_.width = width;
        st_.height = height;
        // The clock must move forward; a capture timestamp that goes back is moved after the last one.
        if (arrivalNs <= lastArrival_) arrivalNs = lastArrival_ + 1;
        lastArrival_ = arrivalNs;
        arrivals_.push_back(arrivalNs);
        while (arrivals_.size() > 32) arrivals_.pop_front();
        readoutArrivals_.push_back(arrivalNs);
        while (readoutArrivals_.size() > 1 && readoutArrivals_.front() < arrivalNs - kReadoutWindowNs) readoutArrivals_.pop_front();
        if (s_.removeDuplicates && lastQuarter_ && quarter) {
            const auto r = compareForDuplicate(*lastQuarter_, *quarter);
            if (r.duplicate) { ++st_.duplicates; return; }
        }
        lastQuarter_ = quarter;
        transport = medianInterval(arrivals_);
        // A long pause (menu, loading, the source stopped) starts the timing over.
        const double expected = keptTimes_.size() >= 3 ? 1e9 / std::max(1e-9, rateOver(keptTimes_)) : 0;
        boundary = lastKeptArrival_ == 0 || (expected > 0 && double(arrivalNs - lastKeptArrival_) > 4.0 * expected) ||
                   double(arrivalNs - lastKeptArrival_) > 250.0 * kMs;
        id = nextId_++;
        generation = generation_;
        holdWanted = s_.crosshairHold && s_.frameGeneration;
        ambientWanted = s_.ambient;
        holdStartOver = holdReset_;
        holdReset_ = false;
        subWanted = s_.subtitleHold && s_.frameGeneration;
        subStartOver = subHoldReset_;
        subHoldReset_ = false;
        if ((holdWanted || subWanted) && !frames_.empty() && !boundary) {
            holdBefore = frames_.back()->hold;
            subBefore = frames_.back()->subHold;
            holdBeforeId = frames_.back()->id;
        }
        if (!frames_.empty() && !boundary && s_.frameGeneration) {
            // Only the newest pair matters: a worker that fell behind skips the older ones (they show the
            // nearer real frame instead of a generated one).
            jobs_.clear();
            jobs_.push_back({frames_.back()->id, id, frames_.back()->pyramid, pyramid});
            queued = true;
        }
    }
    if (queued) cv_.notify_one();
    // The colour picture only for real frames (the conversion costs more than everything above).
    auto frameImage = std::make_shared<const Bgra>(makeImage());
    if (frameImage->width != width || frameImage->height != height) return;

    // Crosshair Hold, on the colour picture (hold_ belongs to this thread). Every kept frame is looked at in
    // order; with the switch off nothing is kept from one frame to the next.
    std::shared_ptr<const HoldFrame> holdFrame;
    std::shared_ptr<const HoldPair> holdPair;
    if (holdStartOver || !holdWanted) hold_.reset();
    if (holdWanted) {
        holdFrame = hold_.frame(*frameImage);
        if (holdFrame && holdBefore) holdPair = CrosshairHold::pair(*holdBefore, *holdFrame);
    }
    std::shared_ptr<const HoldFrame> subFrame;
    std::shared_ptr<const HoldPair> subPair;
    if (subStartOver || !subWanted) subHold_.reset();
    if (subWanted) {
        subFrame = subHold_.frame(*frameImage);
        if (subFrame && subBefore) subPair = CrosshairHold::pair(*subBefore, *subFrame);
    }

    // Ambient fill's copy (about 2 ms at 4K here, on this thread and not the one that draws).
    std::shared_ptr<const Bgra> ambientImage;
    if (ambientWanted) ambientImage = std::make_shared<const Bgra>(ambient::make(*frameImage, veyra::engine::kPushAmbientEncodedGain));

    std::lock_guard lk(m_);
    // Started over meanwhile (another size, a reset): this frame belongs to what was thrown away.
    if (generation != generation_ || width != lastWidth_ || height != lastHeight_) return;
    lastKeptArrival_ = arrivalNs;
    int64_t content = arrivalNs;
    if (s_.pacingRepair) {
        // PushPacingRepair works in 100 ns units like the Windows engine.
        const auto r = pacing_->repair.apply(arrivalNs / 100, arrivalNs / 100, int64_t(transport / 100.0), boundary);
        content = r.pts * 100;
        if (r.repaired) ++st_.pacingRepaired;
    }
    if (content <= lastContent_) content = lastContent_ + 1;
    lastContent_ = content;
    keptTimes_.push_back(arrivalNs);
    while (keptTimes_.size() > 17) keptTimes_.pop_front();
    if (boundary && keptTimes_.size() > 1) { keptTimes_.clear(); keptTimes_.push_back(arrivalNs); }

    auto frame = std::make_shared<ContentFrame>();
    frame->id = id;
    frame->time = content;
    frame->arrival = arrivalNs;
    frame->image = std::move(frameImage);
    frame->pyramid = std::move(pyramid);
    frame->hold = holdFrame;
    frame->holdPair = holdPair;
    frame->subHold = subFrame;
    frame->subHoldPair = subPair;
    frame->holdPairWith = holdPair || subPair ? holdBeforeId : 0;
    frame->ambient = std::move(ambientImage);
    st_.holdState = holdWanted && holdFrame ? 1 : 0;
    st_.holdPixels = holdFrame ? holdFrame->masked : 0;
    st_.subHoldState = subWanted && subFrame ? 1 : 0;
    st_.subHoldPixels = subFrame ? subFrame->masked : 0;
    ++st_.kept;
    frames_.push_back(frame);
    while (frames_.size() > kMaxFrames) frames_.pop_front();
}

void Engine::workerLoop() {
    std::unique_lock lk(m_);
    for (;;) {
        cv_.wait(lk, [&] { return stop_ || (!held_ && !jobs_.empty()); });
        if (stop_) return;
        Job job = std::move(jobs_.front());
        jobs_.pop_front();
        busy_ = true;
        const uint64_t generation = generation_;
        const EngineSettings s = s_;
        const auto clock = clock_;
        MotionField prevF = lastForward_, prevB = lastBackward_;
        const bool resetHud = hudReset_;
        hudReset_ = false;
        lk.unlock();
        if (resetHud) hud_.reset();

        const auto started = std::chrono::steady_clock::now();
        MotionSettings ms;
        ms.finestLevel = s.quality <= 0 ? 2 : 1;
        const MotionEstimator estimator(ms);
        auto pair = std::make_shared<PairFields>();
        pair->aId = job.aId;
        pair->bId = job.bId;
        auto back = std::async(std::launch::async, [&] { return estimator.estimate(*job.b, *job.a, &prevB); });
        pair->forward = estimator.estimate(*job.a, *job.b, &prevF);
        pair->backward = back.get();
        std::shared_ptr<const Gray> mask;
        if (s.hudLock && job.b->levels.size() > 1) {
            // hud_ belongs to this thread; others only ask for a reset through hudReset_.
            mask = std::make_shared<const Gray>(hud_.update(job.b->levels[1]));
        }
        pair->hudMask = mask;
        pair->estimateMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        pair->readyNs = clock();

        lk.lock();
        busy_ = false;
        if (generation == generation_) {
            lastForward_ = pair->forward;
            lastBackward_ = pair->backward;
            if (pair->forward.sceneCut || pair->backward.sceneCut) ++st_.sceneCuts;
            estimateMsAvg_ = estimateMsAvg_ <= 0 ? pair->estimateMs : estimateMsAvg_ * 0.9 + pair->estimateMs * 0.1;
            pairs_.push_back(std::move(pair));
            while (pairs_.size() > kMaxFrames) pairs_.pop_front();
        }
        if (jobs_.empty()) idle_.notify_all();
    }
}

void Engine::waitIdle() {
    std::unique_lock lk(m_);
    idle_.wait(lk, [&] { return jobs_.empty() && !busy_; });
}

std::shared_ptr<const Bgra> Engine::newestImage() const {
    std::lock_guard lk(m_);
    return frames_.empty() ? nullptr : frames_.back()->image;
}

Pick Engine::pick(int64_t displayNs) {
    std::lock_guard lk(m_);
    Pick p;
    // Display refresh from the spacing of pick() calls (median of the last 16).
    if (lastPick_ > 0 && displayNs > lastPick_) {
        pickTimes_.push_back(displayNs - lastPick_);
        while (pickTimes_.size() > 16) pickTimes_.pop_front();
        std::vector<int64_t> d(pickTimes_.begin(), pickTimes_.end());
        std::nth_element(d.begin(), d.begin() + d.size() / 2, d.end());
        displayHz_ = d.size() >= 4 && d[d.size() / 2] > 0 ? 1e9 / double(d[d.size() / 2]) : 0;
    }
    lastPick_ = displayNs;
    ++st_.picks;
    if (frames_.empty()) return p;
    auto showOne = [&](const std::shared_ptr<const ContentFrame>& f) {
        p.a = p.b = f->image;
        p.ambientA = p.ambientB = f->ambient;
        p.aId = p.bId = f->id;
        p.t = 0;
    };
    // Mean real-frame interval (a 17/50 ms game has a median of either, but a mean of 33 ms).
    const double rate = keptTimes_.size() >= 3 ? rateOver(keptTimes_) : 0;
    const double interval = rate > 0 ? 1e9 / rate : 0;
    const double transport = medianInterval(arrivals_);
    if (!s_.frameGeneration || frames_.size() < 2 || interval <= 0) {
        showOne(frames_.back());
    } else {
        // Delay: one real-frame interval, the motion estimation time (with headroom), and up to one
        // capture refresh when Frame Pacing Repair may move a frame later. Moves slowly so a changing
        // estimate never makes the picture jump.
        // Without Frame Pacing Repair the next frame can come as late as the longest recent interval;
        // with it, frames sit on an even grid but may be shown up to one capture refresh after arrival.
        const double spacing = s_.pacingRepair ? interval + transport : std::max(interval, maxInterval(keptTimes_, 8));
        // Headroom: motion estimation (1.5x its average plus 4 ms of capture-arrival jitter), and one
        // display refresh, because pick() is asked for the moment the picture will be seen, which is
        // ahead of now.
        // Until a pair has been measured the allowance is kStartBudgetMs, not the 6 ms floor: with the
        // floor, the first seconds of every session ran late and filled the margin below.
        const double budget = s_.estimateBudgetMs > 0 ? s_.estimateBudgetMs
                              : estimateMsAvg_ > 0 ? std::clamp(estimateMsAvg_ * 1.5 + 4.0, 6.0, 40.0) : kStartBudgetMs;
        const double refresh = displayHz_ > 0 ? 1e9 / displayHz_ : 0.0;
        // Plus what this machine turned out to need: every refresh that found the motion not ready yet
        // adds 1 ms (up to 40), and it slowly gives that back while everything is on time.
        const double target = spacing + (budget + lateMarginMs_) * double(kMs) + refresh;
        if (delayNs_ <= 0) delayNs_ = target;
        delayNs_ += std::clamp(target - delayNs_, -0.25 * double(kMs), 0.25 * double(kMs));
        // The newest pair's slack, noted once: its motion was ready at readyNs, and the first refresh
        // that needs it is drawn at (A's time + the delay - one refresh). While the least slack of the
        // last pairs stays above the guard, the margin gives that excess back, a little for each pair;
        // what was noted so far shrinks with it, so one excess is never given back twice.
        if (!pairs_.empty() && pairs_.back()->bId > slackSeenB_ && pairs_.back()->readyNs > 0) {
            const auto& newest = pairs_.back();
            for (const auto& f : frames_) {
                if (f->id != newest->aId) continue;
                slackSeenB_ = newest->bId;
                const double slack = (double(f->time) + delayNs_ - refresh - double(newest->readyNs)) / double(kMs);
                // A time from another clock (or a pause) is no measurement.
                if (!std::isfinite(slack) || std::abs(slack) > 1000.0) break;
                slack_.push_back(slack);
                while (slack_.size() > kSlackPairs) slack_.pop_front();
                if (slack_.size() >= kSlackMinPairs && lateMarginMs_ > 0) {
                    const double least = *std::min_element(slack_.begin(), slack_.end());
                    const double give = std::min({least - kSlackGuardMs, kSlackStepMs, lateMarginMs_});
                    if (give > 0) {
                        lateMarginMs_ -= give;
                        for (auto& v : slack_) v -= give;
                    }
                }
                break;
            }
        }
        const int64_t tau = displayNs - int64_t(delayNs_);
        size_t k = 0;
        while (k + 1 < frames_.size() && frames_[k + 1]->time <= tau) ++k;
        const auto& A = frames_[k];
        if (k + 1 >= frames_.size() || tau < A->time) {
            showOne(tau < frames_.front()->time ? frames_.front() : A);
        } else {
            const auto& B = frames_[k + 1];
            std::shared_ptr<const PairFields> fields;
            for (const auto& f : pairs_)
                if (f->aId == A->id && f->bId == B->id) fields = f;
            const double span = double(B->time - A->time);
            float t = span > 0 ? float(double(tau - A->time) / span) : 0.f;
            t = std::clamp(t, 0.f, 1.f);
            if (span > 3.0 * interval) {
                // The game paused between these two frames: hold A, no stretched motion.
                showOne(A);
            } else if (!fields || fields->forward.sceneCut || fields->backward.sceneCut) {
                if (!fields) {
                    ++st_.lateFields;
                    // What was noted of earlier pairs was noted with less margin: it moves with it.
                    const double more = std::min(40.0, lateMarginMs_ + 1.0) - lateMarginMs_;
                    lateMarginMs_ += more;
                    for (auto& v : slack_) v += more;
                }
                showOne(t < 0.5f ? A : B);
            } else {
                // Pictures on a fixed grid per pair of real frames (4 steps for 33 ms at 120): every real
                // frame is shown as itself, and the cadence never wobbles. The grid comes from the
                // target, or from the measured refresh in "match display" mode, times this pair's span.
                const double outputRate = s_.targetFps > 0 ? s_.targetFps : displayHz_;
                if (outputRate > 0) {
                    const int steps = std::clamp(int(std::lround(outputRate * span / 1e9)), 1, 16);
                    t = std::floor(t * float(steps) + 1e-3f) / float(steps);
                }
                p.a = A->image;
                p.b = B->image;
                p.ambientA = A->ambient;
                p.ambientB = B->ambient;
                p.aId = A->id;
                p.bId = B->id;
                p.t = t;
                p.fields = fields;
                p.hold = B->holdPairWith == A->id ? B->holdPair : nullptr;
                p.subHold = B->holdPairWith == A->id ? B->subHoldPair : nullptr;
                if (t <= 0.f) { p.b = p.a; p.ambientB = p.ambientA; p.bId = p.aId; p.fields = nullptr; p.hold = nullptr; p.subHold = nullptr; }
                {
                    const double less = lateMarginMs_ - std::max(0.0, lateMarginMs_ - 0.002);
                    lateMarginMs_ -= less;
                    for (auto& v : slack_) v -= less;
                }
            }
        }
    }
    // Output bookkeeping.
    if (p.aId != lastShownA_ || p.bId != lastShownB_ || std::abs(p.t - lastShownT_) > 1e-4f) {
        shownTimes_.push_back(displayNs);
        while (shownTimes_.size() > 1 && shownTimes_.front() < displayNs - 1000 * kMs) shownTimes_.pop_front();
        lastShownA_ = p.aId;
        lastShownB_ = p.bId;
        lastShownT_ = p.t;
    }
    // Latency: from a real frame's arrival to the moment it is shown as itself, measured once per frame
    // (a held picture during a pause would otherwise count as ever-growing delay).
    const uint64_t neededId = p.generated() ? 0 : p.aId;
    int64_t needed = 0;
    if (neededId != 0 && neededId != lastNeededId_) {
        for (const auto& f : frames_)
            if (f->id == neededId) needed = f->arrival;
        lastNeededId_ = neededId;
    }
    if (needed > 0) {
        const double latency = double(displayNs - needed) / double(kMs);
        latencyAvgMs_ = latencyAvgMs_ <= 0 ? latency : latencyAvgMs_ * 0.95 + latency * 0.05;
    }
    return p;
}

EngineStats Engine::stats() const {
    std::lock_guard lk(m_);
    EngineStats s = st_;
    s.transportFps = rateOver(arrivals_);
    s.contentFps = rateOver(keptTimes_);
    s.outputFps = rateOver(shownTimes_);
    s.latencyMs = latencyAvgMs_;
    s.estimateMs = estimateMsAvg_;
    s.delayMs = delayNs_ / double(kMs);
    s.marginMs = lateMarginMs_;
    s.slackMs = slack_.empty() ? 0.0 : *std::min_element(slack_.begin(), slack_.end());
    return s;
}

EngineStats Engine::stats(int64_t nowNs) const {
    EngineStats s = stats();
    std::lock_guard lk(m_);
    s.transportFps = liveRate(readoutArrivals_, nowNs);
    s.contentFps = liveRate(keptTimes_, nowNs);
    s.outputFps = liveRate(shownTimes_, nowNs);
    return s;
}

}  // namespace veyra::portable
