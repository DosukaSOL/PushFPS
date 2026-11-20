#pragma once
#include <cmath>
#include <cstdint>
namespace veyra::engine {
// Explicit 60/59.94 transport -> 30/29.97 sampling before any GPU work.
// Keeps original PTS. It does not claim to detect unique pictures or repair
// irregular game cadence. Quarter-slot tolerance absorbs timestamp jitter.
//
// Phase (2026-10-27): a 30 fps game in 60 Hz shows every picture twice, and
// every second sample may sit on the second copies, so each picture reaches the
// GPU 16.7 ms after it arrived. With picture numbers (PictureCompare.h) a kept
// sample that shows the picture of the sample just before it is such a second
// copy; after kSecondCopies in a row the next sample, a first copy, is taken as
// well and the grid continues from it. 60 fps content never repeats and
// irregular content does not repeat on every kept sample, so both keep the
// plain grid; without picture numbers nothing changes.
//
// PushFPS 4.0 tried more and 4.0.1 took it back: this is 3.2.10's selection again, decision for decision.
// 4.0 passed a due sample over when its picture number repeated the last kept one and took the next new
// picture in its place, so that a game below 30 would not be handed on as pairs that do not move. But a
// picture number is not "the game drew a new picture": PictureCompare.h tolerates a capture card's noise
// (a band of 3 levels and 63 components at 4K), so small motion (a slight turn of the camera, a small
// thing that moves, an idle animation) stays a "repeat" until it has added up. Those samples are real
// game frames, and 4.0 threw them away. Simulated with both classes on a steady 30 FPS game
// (tests/unit/PushHalfRateTests.cpp): where the comparison sees 90 % of the new pictures, 4.0 handed on
// 27 a second instead of 30 with 3 pairs a second two game frames apart; at 50 to 70 %, 22 to 24 a second
// with 6 to 8 such pairs. Frame generation then interpolated twice the motion at exactly those places,
// which showed as artifacts round the character and ghosting of small moving things, on an uneven rhythm
// (the owner and a user, 20 November 2026; 3.2.10 was clean). The plain grid never drops a real frame:
// never pass a due sample over on the strength of a picture number again.
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
