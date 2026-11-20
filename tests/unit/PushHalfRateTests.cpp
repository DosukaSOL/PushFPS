// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// 60 -> 30 selection (engine/CaptureHalfRate.h), as a simulation of a 60 Hz capture: every slot carries the
// number of the game frame shown in it and the picture number the capture callback gave it.
//  - The class decides every sample exactly as 3.2.10's did (a copy of that class is kept here).
//  - Why 4.0's version was taken back in 4.0.1 (a copy of it is kept here too): it passed a due sample over
//    when its picture number repeated the last kept one. A picture number tolerates a capture card's noise,
//    so small motion stays a "repeat" for a while, and those samples are real game frames. On a steady
//    30 FPS game it handed on 22 to 27 frames a second with pairs two game frames apart; the plain grid
//    hands on 30, every game frame.
// Pure: builds anywhere (g++ too).
#include "veyra/engine/CaptureHalfRate.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

// 3.2.10's class, as released.
namespace before {
// The class exactly as it was in 3.2.10 (the reference the unchanged cases are compared with).
class CaptureHalfRate {
public:
    static constexpr unsigned kSecondCopies=3;
    void reset(){anchored_=false;havePicture_=false;previousNew_=false;shift_=false;secondCopies_=0;lastSequence_=nextSequence_=0;}
    static bool supported(double fps){return std::isfinite(fps)&&fps>=59&&fps<=61;}
    // picture: the sample's picture number, nullptr when unknown.
    bool accept(int64_t pts,int64_t transportInterval,bool boundary,const uint64_t* picture=nullptr,uint64_t sequence=0){
        if(transportInterval<=0){reset();return true;}
        const bool repeat=picture&&havePicture_&&*picture==lastPicture_;
        const bool adjacent=sequence&&lastSequence_?sequence==lastSequence_+1:
            anchored_&&pts>last_&&pts-last_>=transportInterval*3/4&&pts-last_<=transportInterval*5/4;
        const bool secondCopy=repeat&&previousNew_&&adjacent;
        previousNew_=picture&&!repeat;
        havePicture_=picture!=nullptr;if(picture)lastPicture_=*picture;
        const bool sequenceRestart=sequence&&lastSequence_&&sequence<=lastSequence_;
        lastSequence_=sequence;
        if(boundary||!anchored_||sequenceRestart||pts<last_||pts-last_>transportInterval*6){
            anchored_=true;next_=pts+transportInterval*2;nextSequence_=sequence?sequence+2:0;last_=pts;shift_=false;secondCopies_=0;return true;
        }
        last_=pts;
        if(sequence&&!nextSequence_)nextSequence_=sequence;
        const bool beforeDue=sequence?sequence<nextSequence_:pts+transportInterval/4<next_;
        if(beforeDue){
            if(!shift_||!picture||repeat||!adjacent)return false;
            // The first copy of the next picture: move the grid onto it.
            shift_=false;secondCopies_=0;next_=pts+transportInterval*2;nextSequence_=sequence?sequence+2:0;++shifts_;return true;
        }
        const auto period=transportInterval*2;
        next_+=((pts+transportInterval/4-next_)/period+1)*period;
        // Driver PTS can run at the wrong speed. When available the transport
        // sequence selects one callback per pair, preserving the original PTS.
        // A late reader consumes the newest sample once, then starts a new pair
        // there; it must not immediately take an adjacent catch-up sample.
        if(sequence)nextSequence_=sequence+2;
        secondCopies_=secondCopy?secondCopies_+1:0;
        shift_=secondCopies_>=kSecondCopies;
        ++grid_;secondKept_+=secondCopy;
        return true;
    }
    uint64_t shifts()const{return shifts_;}
    // Samples the grid kept, and how many of them were second copies (diagnostics).
    uint64_t gridKept()const{return grid_;}
    uint64_t secondCopiesKept()const{return secondKept_;}
private:
    bool anchored_=false,havePicture_=false,previousNew_=false,shift_=false;int64_t next_=0,last_=0;
    uint64_t lastSequence_=0,nextSequence_=0;
    uint64_t lastPicture_=0,shifts_=0,grid_=0,secondKept_=0;unsigned secondCopies_=0;
};
}

// 4.0's class, as released: what was tried.
namespace tried40 {
class CaptureHalfRate {
public:
    static constexpr unsigned kSecondCopies=3;
    // How long a repeated picture counts as a hitch: 5 samples, 83 ms at 60 Hz
    // (2.5 frames of a 30 fps game, the same window ContentDedup uses there).
    static constexpr unsigned kHitchSlots=5;
    void reset(){anchored_=false;havePicture_=false;previousNew_=false;shift_=false;secondCopies_=0;lastSequence_=nextSequence_=0;
        keptValid_=still_=waiting_=false;}
    static bool supported(double fps){return std::isfinite(fps)&&fps>=59&&fps<=61;}
    // picture: the sample's picture number, nullptr when unknown.
    bool accept(int64_t pts,int64_t transportInterval,bool boundary,const uint64_t* picture=nullptr,uint64_t sequence=0){
        if(transportInterval<=0){reset();return true;}
        const bool repeat=picture&&havePicture_&&*picture==lastPicture_;
        const bool adjacent=sequence&&lastSequence_?sequence==lastSequence_+1:
            anchored_&&pts>last_&&pts-last_>=transportInterval*3/4&&pts-last_<=transportInterval*5/4;
        const bool secondCopy=repeat&&previousNew_&&adjacent;
        previousNew_=picture&&!repeat;
        havePicture_=picture!=nullptr;if(picture)lastPicture_=*picture;
        const bool sequenceRestart=sequence&&lastSequence_&&sequence<=lastSequence_;
        lastSequence_=sequence;
        if(boundary||!anchored_||sequenceRestart||pts<last_||pts-last_>transportInterval*6){
            anchored_=true;next_=pts+transportInterval*2;nextSequence_=sequence?sequence+2:0;last_=pts;shift_=false;secondCopies_=0;
            still_=waiting_=false;keep(pts,picture,sequence);return true;
        }
        last_=pts;
        if(sequence&&!nextSequence_)nextSequence_=sequence;
        const bool beforeDue=sequence?sequence<nextSequence_:pts+transportInterval/4<next_;
        const bool keptAgain=picture&&keptValid_&&*picture==keptPicture_;
        if(beforeDue){
            // The first new picture after a still: taken at once (see the note above).
            if(still_&&picture&&keptValid_&&!keptAgain){
                still_=waiting_=shift_=false;secondCopies_=0;next_=pts+transportInterval*2;nextSequence_=sequence?sequence+2:0;
                ++stillEnds_;keep(pts,picture,sequence);return true;
            }
            if(!shift_||!picture||repeat||!adjacent)return false;
            // The first copy of the next picture: move the grid onto it.
            shift_=false;secondCopies_=0;next_=pts+transportInterval*2;nextSequence_=sequence?sequence+2:0;++shifts_;
            still_=false;keep(pts,picture,sequence);return true;
        }
        if(keptAgain&&!still_){
            const bool hitch=sequence&&keptSequence_?sequence-keptSequence_<kHitchSlots:
                pts-keptPts_+transportInterval/4<transportInterval*int64_t(kHitchSlots);
            // Due, but the game has not drawn its next picture yet: wait for it.
            if(hitch){waiting_=true;++repeatsSkipped_;return false;}
            still_=true;   // it stands still: from here it is kept on the grid
        }
        const auto period=transportInterval*2;
        // After a passed-over repeat the grid starts again from this sample; otherwise it keeps its phase.
        if(waiting_)next_=pts+period;
        else next_+=((pts+transportInterval/4-next_)/period+1)*period;
        // Driver PTS can run at the wrong speed. When available the transport
        // sequence selects one callback per pair, preserving the original PTS.
        // A late reader consumes the newest sample once, then starts a new pair
        // there; it must not immediately take an adjacent catch-up sample.
        if(sequence)nextSequence_=sequence+2;
        // A sample taken after a passed-over repeat is the first copy of a new picture (or the
        // still's own refresh), so the phase is where it belongs and nothing is to be moved.
        if(waiting_){shift_=false;secondCopies_=0;}
        else{secondCopies_=secondCopy?secondCopies_+1:0;shift_=secondCopies_>=kSecondCopies;secondKept_+=secondCopy;}
        if(!keptAgain)still_=false;
        repeatsHeld_+=keptAgain;
        waiting_=false;
        ++grid_;keep(pts,picture,sequence);
        return true;
    }
    uint64_t shifts()const{return shifts_;}
    // Samples the grid kept, and how many of them were second copies (diagnostics).
    uint64_t gridKept()const{return grid_;}
    uint64_t secondCopiesKept()const{return secondKept_;}
    // Due samples passed over because they showed the last kept picture again; such samples kept
    // all the same because the picture stood still past kHitchSlots; and new pictures taken at once
    // after a still.
    uint64_t repeatsSkipped()const{return repeatsSkipped_;}
    uint64_t repeatsHeld()const{return repeatsHeld_;}
    uint64_t stillEnds()const{return stillEnds_;}
private:
    void keep(int64_t pts,const uint64_t* picture,uint64_t sequence){
        keptValid_=picture!=nullptr;if(picture)keptPicture_=*picture;keptPts_=pts;keptSequence_=sequence;
    }
    bool keptValid_=false,still_=false,waiting_=false;
    uint64_t keptPicture_=0,keptSequence_=0,repeatsSkipped_=0,repeatsHeld_=0,stillEnds_=0;int64_t keptPts_=0;
    bool anchored_=false,havePicture_=false,previousNew_=false,shift_=false;int64_t next_=0,last_=0;
    uint64_t lastSequence_=0,nextSequence_=0;
    uint64_t lastPicture_=0,shifts_=0,grid_=0,secondKept_=0;unsigned secondCopies_=0;
};
}

using veyra::engine::CaptureHalfRate;
static int failures = 0;
static void check(bool ok, const std::string& label) { std::printf("%s %s\n", ok ? "PASS" : "FAIL", label.c_str()); if (!ok) ++failures; }

struct Stream {
    std::vector<int> durations;          // slots each picture lasts
    bool sequence = true;                // transport sequence numbers known
    bool pictures = true;                // picture numbers known
    int jitter = 0;                      // +- 100 ns units on every time stamp
    std::int64_t interval = 166667;      // 60 Hz
    unsigned seed = 1;
    int lostEvery = 0;                   // every n-th transport sample never arrives
};
template <class Sampler> static std::vector<bool> decisions(const Stream& s) {
    Sampler h; std::vector<bool> out; std::mt19937 rng(s.seed); std::uniform_int_distribution<int> j(-s.jitter, s.jitter);
    std::vector<std::uint64_t> at;
    for (std::size_t p = 0; p < s.durations.size(); ++p) for (int k = 0; k < s.durations[p]; ++k) at.push_back(p + 1);
    for (int n = 0; n < int(at.size()); ++n) {
        if (s.lostEvery && n % s.lostEvery == s.lostEvery - 1) continue;
        const std::int64_t pts = std::int64_t(n) * s.interval + (s.jitter ? j(rng) : 0);
        const std::uint64_t picture = at[n];
        out.push_back(h.accept(pts, s.interval, n == 0, s.pictures ? &picture : nullptr, s.sequence ? std::uint64_t(n + 1) : 0));
    }
    return out;
}
// A game at a steady rate in a 60 Hz capture; `phase` moves where its pictures start.
static std::vector<int> steady(double fps, int seconds, double phase) {
    std::vector<int> d; const int total = seconds * 60; const double per = 60.0 / fps; int last = -1, count = 0;
    for (int n = 0; n < total; ++n) { const int p = int((n + phase) / per); if (p != last) { if (count) d.push_back(count); count = 0; last = p; } ++count; }
    if (count) d.push_back(count);
    return d;
}

// A steady 30 FPS game: every game frame fills two slots. The comparison reports a new picture for only
// `seen` of the game's frames (small motion inside its noise tolerance).
struct Handed { int kept = 0, gameFrames = 0, longestStep = 0, longPairs = 0; };
template <class Sampler> static Handed steadyThirty(double seen, int seconds) {
    const int slots = seconds * 60;
    std::mt19937 rng(11); std::uniform_real_distribution<double> u(0, 1);
    std::vector<std::uint64_t> ids(slots); std::vector<int> frameOf(slots);
    std::uint64_t id = 1;
    for (int f = 0; f * 2 < slots; ++f) {
        if (f == 0 || u(rng) < seen) ++id;
        for (int c = 0; c < 2 && f * 2 + c < slots; ++c) { ids[f * 2 + c] = id; frameOf[f * 2 + c] = f; }
    }
    Sampler s; Handed o; int last = -1;
    for (int n = 0; n < slots; ++n) {
        const std::uint64_t picture = ids[n];
        if (!s.accept(std::int64_t(n) * 166667, 166667, false, &picture, std::uint64_t(n + 1))) continue;
        ++o.kept;
        if (last < 0) ++o.gameFrames;
        else { const int step = frameOf[n] - last; if (step > 0) ++o.gameFrames; if (step > o.longestStep) o.longestStep = step; if (step > 1) ++o.longPairs; }
        last = frameOf[n];
    }
    return o;
}

int main() {
    const int seconds = 20;
    // --- every sample is decided exactly as in 3.2.10 ------------------------------------------------
    {
        struct Case { const char* what; Stream s; };
        std::vector<Case> cases;
        for (double fps : {60.0, 30.0, 40.0, 50.0, 29.0, 26.0, 24.0, 20.0, 10.0}) for (double phase : {0.0, 1.0}) {
            Stream s; s.durations = steady(fps, seconds, phase); cases.push_back({"picture numbers and sequence", s});
            s.sequence = false; s.jitter = 15000; cases.push_back({"picture numbers, jittered time stamps", s});
            s.interval = 166833; cases.push_back({"59.94 Hz", s});
            s.sequence = true; s.jitter = 0; s.interval = 166667; s.lostEvery = 37; cases.push_back({"lost transport samples", s});
        }
        for (double fps : {60.0, 30.0, 29.0, 26.0, 24.0, 20.0, 10.0}) for (bool sequence : {true, false}) {
            Stream s; s.durations = steady(fps, seconds, 1); s.pictures = false; s.sequence = sequence; s.jitter = sequence ? 0 : 12000;
            cases.push_back({"no picture numbers", s});
        }
        {   // A 30 FPS game with hitches, stills and single-slot pictures.
            Stream s; std::mt19937 rng(5); std::uniform_int_distribution<int> pick(0, 99);
            for (int n = 0; n < 900; ++n) { const int r = pick(rng); s.durations.push_back(r < 70 ? 2 : r < 82 ? 3 : r < 90 ? 1 : r < 97 ? 4 : 30); }
            cases.push_back({"a game with hitches and stills", s});
        }
        int same = 0;
        for (const auto& c : cases) {
            const bool equal = decisions<before::CaptureHalfRate>(c.s) == decisions<CaptureHalfRate>(c.s);
            if (!equal) check(false, std::string("decided exactly as in 3.2.10: ") + c.what);
            same += equal;
        }
        std::printf("INFO %d of %zu streams decided exactly as in 3.2.10\n", same, cases.size());
        check(same == int(cases.size()), "every stream is decided exactly as in 3.2.10");
    }
    // --- a real game frame is never thrown away because its picture number repeats ---------------------
    for (double seen : {1.0, 0.9, 0.7, 0.5, 0.34}) {
        const Handed now = steadyThirty<CaptureHalfRate>(seen, seconds), tried = steadyThirty<tried40::CaptureHalfRate>(seen, seconds);
        char label[160];
        std::snprintf(label, sizeof label, "a steady 30 FPS game is handed on whole when the comparison sees %.0f %% of its pictures (30 a second, every game frame)", seen * 100);
        check(now.kept == 30 * seconds && now.gameFrames == 30 * seconds && now.longestStep == 1 && now.longPairs == 0, label);
        std::printf("INFO seen %.0f %%: now %d kept, %d game frames, %d long pairs; 4.0 handed on %d game frames with %d pairs two frames apart\n",
            seen * 100, now.kept, now.gameFrames, now.longPairs, tried.gameFrames, tried.longPairs);
        if (seen < 1.0) check(tried.gameFrames < 28 * seconds && tried.longPairs > 2 * seconds, "what 4.0 did there: real frames dropped, pairs two game frames apart");
        else check(tried.gameFrames == 30 * seconds, "with every picture seen 4.0 handed on the same frames");
    }
    {   // A boundary (a new source, a format change) starts the grid again.
        CaptureHalfRate h; const std::uint64_t a = 7;
        check(h.accept(0, 166667, true, &a, 1), "the first sample is kept");
        check(!h.accept(166667, 166667, false, &a, 2), "its repeat one slot later is not due");
        check(h.accept(333334, 166667, true, &a, 3), "the first sample after a boundary is kept even with the same picture number");
        h.reset();
        check(h.accept(500000, 166667, false, &a, 4), "and after a reset");
        check(h.accept(1, 0, false, &a, 5), "an unknown transport interval keeps everything");
        // A due sample is kept whatever its picture number says.
        CaptureHalfRate g; const std::uint64_t same = 3;
        int kept = 0; for (int n = 0; n < 60; ++n) kept += g.accept(std::int64_t(n) * 166667, 166667, n == 0, &same, std::uint64_t(n + 1)) ? 1 : 0;
        check(kept == 30, "a picture number that never changes still gives every second sample");
    }
    std::printf(failures ? "%d FAILED\n" : "ALL PASSED (0 failures)\n", failures);
    return failures ? 1 : 0;
}
