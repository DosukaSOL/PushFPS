// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PushFPS sound profiles: filter responses match the preset descriptions, Off is
// bit-identical, nothing clips, a change crossfades without a click, the process
// wide setting round-trips, and bad input is sanitised.
#include "veyra/sink/SoundProfile.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace veyra::sink;

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    failures += !ok;
}
SoundProfileSettings preset(SoundPreset p) { SoundProfileSettings s; s.preset = int(p); return s; }
std::vector<float> sine(double hz, double amplitude, size_t frames, unsigned channels) {
    std::vector<float> out(frames * channels);
    for (size_t i = 0; i < frames; ++i)
        for (unsigned c = 0; c < channels; ++c)
            out[i * channels + c] = float(amplitude * std::sin(2.0 * 3.14159265358979323846 * hz * double(i) / 48000.0));
    return out;
}
double rms(const std::vector<float>& v, size_t skipFrames, unsigned channels) {
    double sum = 0; size_t n = 0;
    for (size_t i = skipFrames * channels; i < v.size(); ++i) { sum += double(v[i]) * v[i]; ++n; }
    return n ? std::sqrt(sum / double(n)) : 0;
}
double peak(const std::vector<float>& v) { double p = 0; for (float x : v) p = (std::max)(p, double(std::abs(x))); return p; }
// Steady-state gain in dB of a profile for a sine at `hz` (quiet, so the limiter stays out).
double measuredDb(const SoundProfileSettings& s, double hz) {
    SoundProcessor p; p.configure(s, 2);
    auto in = sine(hz, 0.05, 48000, 2), out = in;
    p.process(out.data(), 48000);
    return 20.0 * std::log10(rms(out, 24000, 2) / rms(in, 24000, 2));
}
}

int main() {
    // Off: not a single sample changes.
    {
        SoundProcessor p; p.configure(preset(SoundPreset::Off), 2);
        auto in = sine(440, 0.8, 4800, 2), out = in;
        p.process(out.data(), 4800);
        check(!p.active() && out == in, "Off leaves the samples bit-identical");
    }
    // A Custom profile with every band at 0 dB is Off as well.
    {
        SoundProcessor p; SoundProfileSettings s = preset(SoundPreset::Custom); p.configure(s, 2);
        check(!p.active(), "Custom with flat bands is inactive");
    }
    // Responses: the presets do what their names say.
    check(measuredDb(preset(SoundPreset::BassBoost), 60) > 4.5, "Bass boost raises 60 Hz by about 6 dB");
    check(std::abs(measuredDb(preset(SoundPreset::BassBoost), 5000)) < 1.0, "Bass boost leaves 5 kHz alone");
    check(measuredDb(preset(SoundPreset::Voice), 2500) > 4.0, "Voice clarity lifts 2.5 kHz");
    check(measuredDb(preset(SoundPreset::Voice), 40) < -6.0, "Voice clarity cuts rumble below 90 Hz");
    check(measuredDb(preset(SoundPreset::Footsteps), 3000) > 5.0, "Footsteps lifts 3 kHz");
    check(measuredDb(preset(SoundPreset::Footsteps), 60) < -4.0, "Footsteps lowers the bass");
    {
        SoundProfileSettings s = preset(SoundPreset::Custom);
        s.customDb = {0, 0, 9, 0, 0};
        check(std::abs(measuredDb(s, 910) - 9.0) < 1.0, "Custom EQ: +9 dB at 910 Hz");
        check(std::abs(measuredDb(s, 14000)) < 1.0, "Custom EQ: 14 kHz untouched when its band is 0");
        check(std::abs(SoundProcessor::presetResponseDb(s, 910) - 9.0) < 0.2, "Preview curve matches the filters");
    }
    // Night mode narrows the gap between quiet and loud.
    {
        auto run = [](double amplitude) {
            SoundProcessor p; p.configure(preset(SoundPreset::Night), 2);
            auto v = sine(1000, amplitude, 48000, 2); p.process(v.data(), 48000); return rms(v, 24000, 2);
        };
        const double quiet = run(0.01), loud = run(0.5);
        const double inRange = 20 * std::log10(0.5 / 0.01), outRange = 20 * std::log10(loud / quiet);
        check(outRange < inRange - 10.0, "Night mode compresses the loud/quiet range by more than 10 dB");
    }
    // Boosts never clip: a full-scale bass tone stays under the -0.5 dBFS ceiling.
    {
        for (int pr = 1; pr < kSoundPresetCount; ++pr) {
            SoundProfileSettings s = preset(SoundPreset(pr));
            if (pr == int(SoundPreset::Custom)) s.customDb = {12, 12, 12, 12, 12};
            SoundProcessor p; p.configure(s, 2);
            auto v = sine(pr == 3 ? 3000 : 60, 0.99, 48000, 2); p.process(v.data(), 48000);
            char what[96]; std::snprintf(what, sizeof what, "preset %d stays under -0.5 dBFS at full scale", pr);
            check(peak(v) <= 0.945, what);
        }
    }
    // Switching profiles mid-stream does not click: no sample-to-sample jump
    // larger than the signal's own largest step.
    {
        SoundProcessor p; p.configure(preset(SoundPreset::Off), 2);
        auto v = sine(200, 0.3, 9600, 2);
        p.process(v.data(), 4800);
        p.configure(preset(SoundPreset::BassBoost), 2);
        p.process(v.data() + 4800 * 2, 4800);
        double maxStep = 0;
        for (size_t i = 1; i < 9600; ++i) maxStep = (std::max)(maxStep, double(std::abs(v[i * 2] - v[(i - 1) * 2])));
        const double sineStep = 0.3 * 2 * 3.14159265358979323846 * 200 / 48000 * 2.2;   // boosted slope, with margin
        check(maxStep < sineStep, "a profile change crossfades without a click");
    }
    // Multichannel: each channel filtered on its own, 8 channels supported.
    {
        SoundProcessor p; p.configure(preset(SoundPreset::BassBoost), 8);
        auto v = sine(60, 0.05, 48000, 8); p.process(v.data(), 48000);
        bool same = true; for (size_t i = 24000; i < 48000; ++i) same &= std::abs(v[i * 8] - v[i * 8 + 7]) < 1e-6f;
        check(same && std::isfinite(v.back()), "8-channel audio: every channel processed the same way");
    }
    // Garbage in: NaN samples become silence, an unknown preset is Off, gains clamp.
    {
        SoundProfileSettings bad; bad.preset = 99; bad.customDb = {NAN, 40, -40, 0, 0};
        const auto clean = bad.sanitized();
        check(clean.preset == 0 && clean.customDb[0] == 0 && clean.customDb[1] == 12 && clean.customDb[2] == -12,
              "unknown preset becomes Off; EQ gains clamp to +/-12 dB; NaN gain becomes 0");
        SoundProcessor p; p.configure(preset(SoundPreset::Voice), 2);
        std::vector<float> v(200, NAN); p.process(v.data(), 100);
        bool finite = true; for (float x : v) finite &= std::isfinite(x);
        check(finite, "NaN input samples come out as finite silence");
    }
    // Process-wide setting.
    {
        const auto before = soundProfileVersion();
        SoundProfileSettings s = preset(SoundPreset::Cinema);
        setSoundProfile(s);
        check(soundProfile() == s && soundProfileVersion() == before + 1, "setSoundProfile stores the choice and bumps the version");
        setSoundProfile(s);
        check(soundProfileVersion() == before + 1, "setting the same profile again changes nothing");
    }
    std::printf("%s: %d failure(s)\n", failures ? "FAILED" : "OK", failures);
    return failures ? 1 : 0;
}
