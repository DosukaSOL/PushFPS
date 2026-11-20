# PushFPS roadmap

Where PushFPS is going, in the order things depend on each other. No dates: a step is done when it has been
measured on real hardware, not when the code is written. [How things are measured](BENCHMARKING.md).

## The rules

1. **Real frames first.** The game's own frames always get through; generated frames come second, effects
   third, recording and overlays last. Nothing may build a queue that grows.
2. **No claim above its evidence.** A result from the software renderer proves correctness, never speed. A
   card benchmark is not a whole session. PushFPS's software timing is not button to screen.
3. **Generated frames are smoothness, not response.** They never make the console read the controller sooner,
   and no setting is described as if it did.
4. **Every card keeps working.** The universal path (DirectML) stays. Anything faster for one maker's cards is
   an option on top, with an automatic fallback.
5. **One change, one measurement.** Speed work ships switched on only after it was faster at equal picture
   quality on at least two classes of card. What does not prove itself is taken out again.

## Done in 3.2.5

- Frame generation that is on but shows no frames is said, with its cause, instead of failing silently.
- A performance report to copy (card, driver, settings, real and generated frames, software delay, nothing
  personal), a hardware report template and the benchmarking guide.
- A passed PushFlow half-precision test holds for one card, driver and PushFPS version only.
- PS5 Remote Play: buttons no longer stay pressed after fast combinations. Xbox: damaged frames are rejected
  and recovered from, with optional software decoding. Elgato 4K X: HDR is detected automatically.
- Reveal comparison clips, and split and reveal clips at up to 4K.
- The first [tested graphics cards](../README.md#tested-graphics-cards).

## Done in 4.0

Written and tested on the software renderer; **none of it has been measured on a real card yet**, which is the
next step for each (rule 2).

- What stands still in front of a moving picture keeps its real pixels in generated frames (the HUD), with
  every method PushFPS runs itself. Its own switch, **off by default since 4.0.1**: on a real card it added
  artifacts around the edges of moving things. It stays an experiment until it has been seen on real games.
- 60 → 30 duplicate removal keeps its rhythm when a game dips under 30.
- Lip sync by hand, on top of automatic sync.
- A line between the two sides of the live comparison. The latency readout can be dragged.
- The record button asks first: horizontal or vertical, fill or fit, the size and the quality, with the
  bitrate shown before it starts.
- A new capture card starts with its own sound input chosen where that is beyond doubt.
- PS5 Remote Play: a frame arriving while the queue is read is no longer taken for too old; quick presses
  are not lost; recoveries are logged with their cause.
- AV1 files on the card's own decoder, Dolby Vision 8.4 base layers shown as HLG, a display that reports the
  format's limit as its peak no longer believed.
- The performance report says how the picture reaches the screen and how long the screen took after the
  hand-over.

Still open after 4.0: ghosting in the moving world beside a character (nothing measured so far improves it).

## Next: can be built now, ships after it is measured

These need card test results and performance reports from real cards before they are switched on by default.

| Area | Step | What it should bring |
|---|---|---|
| PushFlow | Encode every real frame once: split the network into an encoder and a motion part, and reuse a frame's features for the pair before it, the pair after it and every in-between time | Less work per generated frame, most at 3× and 4× |
| PushFlow | Take redundant warps and full-size temporary pictures out of the network | Less time and memory per frame |
| PushFlow | Half precision checked per card with worst-case pictures, not only an average | Faster where it is safe, never a broken frame |
| PushFlow | A fixed pool of buffers leased per frame, released only when the last reader is done | No allocation while playing, safer teardown |
| Correctness | The end-to-end test PushFlow has (real frames through the real graph) for every other method | A method can never again be selected and silently do nothing |
| Diagnostics | One frame number from capture to screen, with a per-frame log to export | Counters that add up; exact answers to "where did that frame go" |
| Capture cards | Show the mode asked for, the mode the card agreed to and the rate that arrives; exact 59.94, 60 and 50; no silent fallback | Fewer wrong-mode sessions |
| Cadence | Duplicate detection from the picture itself with confidence and hysteresis | Right more often on noisy cards and over Wi-Fi |
| Quality test | Works on hardware-decoded sources; adds structural similarity and the worst frame | A score for every source, and no hidden bad frame |
| Sound | A small clocked buffer with drift correction instead of skips | No clicks, steady lip sync |

The larger plan for PushFlow (finding wrong places in a generated frame, reusing a pair's motion, a separate
timeline for recording) and what was measured for it so far: [PushFlow 2](PUSHFLOW2.md).

## Needs hardware, or an agreement outside the code

| Step | What it waits for |
|---|---|
| An optional NVIDIA-native PushFlow path with a per-card engine cache | NVIDIA's licence terms for shipping the runtime; tests on RTX 30, 40 and 50 |
| Latency modes (lowest delay, balanced, smoothest) with their measured cost shown | Camera measurements on real screens |
| Smoothing real frame-rate dips, as an opt-in with its extra delay shown | Real games that dip; blind comparisons |
| Processed frames to OBS straight from the graphics card, and a virtual camera | Real OBS sessions at 60 and 120 FPS on several cards |
| Windows code signing | A certificate (application in progress) |

## Later

- Frame input API version 2: NV12 and P010, HDR metadata, per-frame "new / repeated / damaged" flags,
  optional compressed input, sound, and shared graphics card surfaces.
- Dolby Vision profile 5 and 7 input (shown as HDR10; this is not Dolby Vision output).
- Linux and macOS beyond the preview: tested capture cards, recording and going live.

## Not planned

- 4K neural inference by default. PushFlow works out motion at up to 1080p and rebuilds the picture at full
  size; that is what keeps 4K playable.
- Anything that predicts frames the game has not sent yet as the normal mode.

## How to help

Run **Test my graphics card**, play a session, copy the **performance report** (both in *Settings*) and open
a [hardware report](https://github.com/DosukaSOL/PushFPS/issues/new?template=hardware-report.yml). Every
step in the first table above is waiting for exactly that.
