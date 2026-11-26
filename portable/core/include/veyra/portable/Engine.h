// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once
// The Linux / macOS frame pipeline without Qt: capture frames in, "what to show at this moment" out.
//
//   push(frame, arrival)   capture thread: duplicate removal, Frame Pacing Repair, luma pyramid;
//                          a worker thread then updates Live HUD Lock and estimates motion both ways
//                          between the last two real frames.
//   pick(displayTime)      render thread, once per refresh: the pair of real frames and the phase t
//                          for that moment, with its motion fields. Never blocks on the worker.
//
// Timing: generated frames sit between two real frames, so the picture runs one real-frame interval
// (plus the motion estimation time, plus up to one capture refresh with Frame Pacing Repair) behind
// the capture, like every frame generation. With frame generation off, the newest frame is shown at
// once. Times are nanoseconds on one steady clock.
//
// What the picture waits beyond that (4.1; people who tried the macOS build found the delay higher
// than on Windows with the same capture card). Three things were found by reading and are changed:
//   - Motion for a pair started only after the new frame's colour conversion (6 ms for 1080p, 10 ms
//     for 1440p on the build machine). It needs the brightness picture only, so it starts before the
//     conversion now and the two run side by side.
//   - Before the first pair was measured the allowance for motion was its 6 ms floor, so the first
//     seconds of every session ran late, and each late refresh added a millisecond of margin (up to
//     40) that was given back at two thousandths of a millisecond a refresh: over five minutes at
//     60 Hz. Every session began with tens of milliseconds it did not need. The allowance now starts
//     at kStartBudgetMs until a pair has been measured.
//   - The margin is given back when it is shown to be spare: for every pair the engine notes how long
//     before it was needed its motion was ready (the slack), and while the least slack of the last
//     pairs stays above a guard, the margin comes down by that excess, half a millisecond a pair at
//     most. It never probes by running late; the old slow giving back is still there under it.
#include "veyra/portable/CrosshairHold.h"
#include "veyra/portable/HudLock.h"
#include "veyra/portable/Image.h"
#include "veyra/portable/Motion.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace veyra::portable {

struct EngineSettings {
    bool frameGeneration = true;
    double targetFps = 0;        // 0: one new picture every display refresh ("match display")
    bool removeDuplicates = true;
    bool pacingRepair = false;
    bool hudLock = false;
    bool crosshairHold = false;  // Crosshair Hold (CrosshairHold.h); off unless switched on
    bool subtitleHold = false;   // Subtitle Hold: the same rule in the subtitle band; off unless switched on
    int quality = 1;             // 0 fast (vectors from the quarter-size picture), 1 balanced (half size)
    double estimateBudgetMs = 0; // headroom for motion estimation in the display delay; 0 = measured
};

struct ContentFrame {
    uint64_t id = 0;
    int64_t time = 0;     // content time after Frame Pacing Repair
    int64_t arrival = 0;  // when the capture delivered it
    std::shared_ptr<const Bgra> image;
    std::shared_ptr<const Pyramid> pyramid;
    // Crosshair Hold: this frame's part, and what a generated frame between the frame before
    // (holdPairWith) and this one needs. Both null with the switch off or no crosshair on screen.
    std::shared_ptr<const HoldFrame> hold;
    std::shared_ptr<const HoldPair> holdPair;
    // Subtitle Hold's, the same for the subtitle band (holdPairWith serves both).
    std::shared_ptr<const HoldFrame> subHold;
    std::shared_ptr<const HoldPair> subHoldPair;
    uint64_t holdPairWith = 0;
};

struct PairFields {
    uint64_t aId = 0, bId = 0;
    int64_t readyNs = 0;  // when the motion was ready, on the engine's clock
    MotionField forward, backward;
    std::shared_ptr<const Gray> hudMask;  // null when Live HUD Lock is off
    double estimateMs = 0;
};

struct Pick {
    std::shared_ptr<const Bgra> a, b;          // b == a when only one real frame is shown
    std::shared_ptr<const PairFields> fields;  // null: show the nearer real frame (no generation)
    std::shared_ptr<const HoldPair> hold;      // Crosshair Hold's part for this pair; null: nothing is held
    std::shared_ptr<const HoldPair> subHold;   // Subtitle Hold's part; null: nothing is held
    uint64_t aId = 0, bId = 0;
    float t = 0;                               // 0 = a, 1 = b
    bool generated() const { return fields && t > 0.f && t < 1.f && a != b; }
    bool valid() const { return a != nullptr; }
};

struct EngineStats {
    // stats(now) gives the three rates as a readout shows them: falling off when what they count stops.
    double transportFps = 0;   // frames per second arriving from the capture
    double contentFps = 0;     // real (non-duplicate) pictures per second
    double outputFps = 0;      // different pictures shown per second
    double latencyMs = 0;      // display time minus arrival of the newest real frame it needs
    double estimateMs = 0;     // motion estimation time per pair (both directions)
    double delayMs = 0;        // current display delay behind the content
    uint64_t framesIn = 0, duplicates = 0, kept = 0, sceneCuts = 0, pacingRepaired = 0;
    uint64_t lateFields = 0;   // refreshes that showed a real frame because the motion was not ready yet
    double marginMs = 0;       // extra delay this machine turned out to need (0..40)
    double slackMs = 0;        // least time before it was needed that a recent pair's motion was ready
    uint64_t picks = 0;        // refreshes asked for
    int width = 0, height = 0;
    // Crosshair Hold: 0 not asked for, 1 running; and how many pixels the newest real frame holds.
    int holdState = 0;
    int holdPixels = 0;
    int subHoldState = 0;      // Subtitle Hold, the same
    int subHoldPixels = 0;
};

class Engine {
public:
    explicit Engine(EngineSettings settings = {});
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void setSettings(const EngineSettings& settings);
    EngineSettings settings() const;
    // Capture thread (one producer). luma: full-size luma of the frame; image: called only for frames
    // that are not duplicates, returns the BGRA picture (same size as luma). A size change starts over.
    void push(Gray&& luma, const std::function<Bgra()>& image, int64_t arrivalNs);
    // Convenience: luma computed from the image.
    void push(Bgra&& image, int64_t arrivalNs);
    // Render thread, once per refresh. refreshHz: the display's refresh rate (for the output count).
    Pick pick(int64_t displayNs);
    EngineStats stats() const;
    // The same, for a readout at `nowNs` (the clock the frames are stamped with): the three rates fall off
    // when what they count stops, and read 0 a second after the last one, instead of keeping the last rate.
    EngineStats stats(int64_t nowNs) const;
    // The newest real frame kept (null before the first): what a dialog shows of the picture. Read only.
    std::shared_ptr<const Bgra> newestImage() const;
    // Drops all frames (source change, resolution change).
    void reset();
    // Tests: waits until the worker has finished every queued pair.
    void waitIdle();
    // Tests: the clock the worker reads when a pair's motion is ready (the same clock arrival and
    // display times are on). The default is the steady clock in nanoseconds, which is what the app
    // stamps its frames and refreshes with.
    void setClock(std::function<int64_t()> clock);
    // Tests: while held, queued pairs are not estimated (a slow machine, on a simulated clock).
    void holdWorker(bool hold);

    // The allowance for motion before any pair has been measured, and what the margin is steered by.
    static constexpr double kStartBudgetMs = 24.0;
    static constexpr double kSlackGuardMs = 4.0;   // least slack the margin is never taken below
    static constexpr double kSlackStepMs = 0.5;    // most the margin comes down for one pair
    static constexpr size_t kSlackPairs = 240;     // pairs looked back on (8 s at 30 FPS)
    static constexpr size_t kSlackMinPairs = 45;   // pairs before anything is given back

private:
    // A pair to estimate: the two brightness pyramids and the frames' numbers. Not the frames: the
    // newer one is still being converted to colour when its pair starts.
    struct Job { uint64_t aId = 0, bId = 0; std::shared_ptr<const Pyramid> a, b; };
    void workerLoop();
    void resetLocked();

    mutable std::mutex m_;
    std::condition_variable cv_, idle_;
    EngineSettings s_;
    std::deque<std::shared_ptr<const ContentFrame>> frames_;
    std::deque<std::shared_ptr<const PairFields>> pairs_;
    std::deque<Job> jobs_;
    bool busy_ = false, stop_ = false, hudReset_ = false, held_ = false;
    std::function<int64_t()> clock_;
    std::deque<double> slack_;       // ms, one per pair, newest last
    uint64_t slackSeenB_ = 0;        // the newest pair already noted
    uint64_t nextId_ = 1, generation_ = 0;
    std::shared_ptr<const Gray> lastQuarter_;
    int64_t lastArrival_ = 0, lastKeptArrival_ = 0, lastContent_ = 0;
    int lastWidth_ = 0, lastHeight_ = 0;
    std::deque<int64_t> arrivals_, keptTimes_;
    // The capture's arrivals over the last kReadoutWindowNs, for the readout's signal rate only: frames
    // that come over USB in bunches made a rate over the 32 of arrivals_ (half a second) read 64 for a
    // 60 Hz card; over two seconds a bunch moves it by about 1 %. Pacing keeps reading arrivals_.
    std::deque<int64_t> readoutArrivals_;
    static constexpr int64_t kReadoutWindowNs = 2'000'000'000;
    double delayNs_ = 0;
    double lateMarginMs_ = 0;  // extra delay this machine needed (motion not ready in time)
    double estimateMsAvg_ = 0;
    // Output bookkeeping (render thread under the same lock).
    uint64_t lastShownA_ = 0, lastShownB_ = 0, lastNeededId_ = 0;
    float lastShownT_ = -1;
    std::deque<int64_t> shownTimes_, pickTimes_;
    int64_t lastPick_ = 0;
    double displayHz_ = 0;
    double latencyAvgMs_ = 0;
    EngineStats st_;
    struct Pacing;
    std::unique_ptr<Pacing> pacing_;
    HudLock hud_;  // worker thread only
    CrosshairHold hold_;      // the thread that pushes frames only; others ask for a reset through holdReset_
    bool holdReset_ = false;
    CrosshairHold subHold_{HoldKind::Subtitles};   // the same for the subtitle band
    bool subHoldReset_ = false;
    MotionField lastForward_, lastBackward_;
    std::thread worker_;
};

}  // namespace veyra::portable
