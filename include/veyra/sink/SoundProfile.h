// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once
// PushFPS sound profiles: a small, allocation-free DSP stage on the player's
// float PCM (48 kHz, interleaved, 1-8 channels). It runs in AudioRenderer before
// the recording tap, so what the player hears and what a live recording keeps
// are the same. "Off" returns without touching a sample (bit-identical).
//
// Stages, in order: up to five biquad filters (RBJ Audio EQ Cookbook), an
// optional linked compressor, then a peak limiter that keeps boosted presets
// from clipping. A settings change crossfades from the old chain to the new one
// over 20 ms so switching never clicks. Free of platform headers so the maths is
// unit-tested without an audio device (veyra_sound_profile_tests).
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace veyra::sink {

enum class SoundPreset : int { Off = 0, BassBoost = 1, Voice = 2, Footsteps = 3, Night = 4, Cinema = 5, Custom = 6 };
inline constexpr int kSoundPresetCount = 7;
inline constexpr int kSoundEqBands = 5;
inline constexpr std::array<float, kSoundEqBands> kSoundEqBandHz{60.0f, 230.0f, 910.0f, 3600.0f, 14000.0f};
inline constexpr float kSoundEqMaxDb = 12.0f;

struct SoundProfileSettings {
    int preset = 0;                                   // SoundPreset
    std::array<float, kSoundEqBands> customDb{};      // Custom: per-band gain, -12..+12 dB
    bool operator==(const SoundProfileSettings&) const = default;
    // Out-of-range input becomes the nearest valid value (Off for an unknown preset).
    SoundProfileSettings sanitized() const {
        SoundProfileSettings s = *this;
        if (s.preset < 0 || s.preset >= kSoundPresetCount) s.preset = 0;
        for (float& g : s.customDb) g = std::isfinite(g) ? std::clamp(g, -kSoundEqMaxDb, kSoundEqMaxDb) : 0.0f;
        return s;
    }
};

struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;    // normalised (a0 = 1)
    enum class Kind { Peaking, LowShelf, HighShelf, HighPass };
    static Biquad make(Kind kind, double rate, double hz, double q, double gainDb) {
        const double w0 = 2.0 * 3.14159265358979323846 * std::clamp(hz, 10.0, rate * 0.45) / rate;
        const double cw = std::cos(w0), sw = std::sin(w0);
        const double A = std::pow(10.0, gainDb / 40.0);
        double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;
        switch (kind) {
        case Kind::Peaking: {
            const double alpha = sw / (2.0 * q);
            b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A;
            a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A;
            break;
        }
        case Kind::LowShelf:
        case Kind::HighShelf: {
            // Shelf slope S = q (1 = steepest without overshoot).
            const double alpha = sw / 2.0 * std::sqrt((A + 1.0 / A) * (1.0 / q - 1.0) + 2.0);
            const double k = 2.0 * std::sqrt(A) * alpha;
            if (kind == Kind::LowShelf) {
                b0 = A * ((A + 1) - (A - 1) * cw + k); b1 = 2 * A * ((A - 1) - (A + 1) * cw); b2 = A * ((A + 1) - (A - 1) * cw - k);
                a0 = (A + 1) + (A - 1) * cw + k; a1 = -2 * ((A - 1) + (A + 1) * cw); a2 = (A + 1) + (A - 1) * cw - k;
            } else {
                b0 = A * ((A + 1) + (A - 1) * cw + k); b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - k);
                a0 = (A + 1) - (A - 1) * cw + k; a1 = 2 * ((A - 1) - (A + 1) * cw); a2 = (A + 1) - (A - 1) * cw - k;
            }
            break;
        }
        case Kind::HighPass: {
            const double alpha = sw / (2.0 * q);
            b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2;
            a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha;
            break;
        }
        }
        return Biquad{b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
    }
    // Magnitude response in dB at `hz` (tests and the interface's EQ preview).
    double responseDb(double rate, double hz) const {
        const double w = 2.0 * 3.14159265358979323846 * hz / rate;
        const double cr = std::cos(w), ci = -std::sin(w), c2r = std::cos(2 * w), c2i = -std::sin(2 * w);
        const double nr = b0 + b1 * cr + b2 * c2r, ni = b1 * ci + b2 * c2i;
        const double dr = 1 + a1 * cr + a2 * c2r, di = a1 * ci + a2 * c2i;
        return 10.0 * std::log10((nr * nr + ni * ni) / (std::max)(1e-30, dr * dr + di * di));
    }
};

class SoundProcessor {
public:
    static constexpr unsigned kMaxChannels = 8;
    static constexpr double kRate = 48000.0;
    static constexpr int kFadeFrames = 960;           // 20 ms crossfade on every change

    // Audio thread. Cheap when nothing changed; a change starts the crossfade.
    void configure(const SoundProfileSettings& wanted, unsigned channels) {
        const SoundProfileSettings s = wanted.sanitized();
        channels = std::clamp(channels, 1u, kMaxChannels);
        if (configured_ && s == settings_ && channels == channels_) return;
        const bool channelsChanged = configured_ && channels != channels_;
        previous_ = current_;
        current_ = Chain::build(s);
        settings_ = s;
        channels_ = channels;
        // A new channel layout invalidates the old chain's state: no crossfade.
        fadeRemaining_ = (configured_ && !channelsChanged && (previous_.active || current_.active)) ? kFadeFrames : 0;
        configured_ = true;
    }
    bool active() const { return current_.active || fadeRemaining_ > 0; }
    const SoundProfileSettings& settings() const { return settings_; }

    void process(float* pcm, size_t frames) {
        if (!pcm || !frames || !active()) return;
        for (size_t i = 0; i < frames; ++i) {
            float* frame = pcm + i * channels_;
            if (fadeRemaining_ > 0) {
                float oldOut[kMaxChannels];
                for (unsigned c = 0; c < channels_; ++c) oldOut[c] = frame[c];
                previous_.run(oldOut, channels_);
                current_.run(frame, channels_);
                const float t = float(kFadeFrames - fadeRemaining_) / float(kFadeFrames);
                for (unsigned c = 0; c < channels_; ++c) frame[c] = oldOut[c] * (1.0f - t) + frame[c] * t;
                --fadeRemaining_;
            } else {
                current_.run(frame, channels_);
            }
        }
    }

    // The filters' combined response (no compressor or limiter), for the
    // interface's EQ curve and the tests.
    static double presetResponseDb(const SoundProfileSettings& s, double hz) {
        const Chain chain = Chain::build(s.sanitized());
        double db = 0;
        for (int f = 0; f < chain.filterCount; ++f) db += chain.filters[std::size_t(f)].responseDb(kRate, hz);
        return db;
    }

private:
    struct Chain {
        std::array<Biquad, kSoundEqBands> filters{};
        int filterCount = 0;
        bool active = false;
        // Compressor (linked across channels; 0 ratio = off).
        double threshDb = 0, ratio = 0, attack = 0, release = 0, makeupDb = 0;
        double envDb = -120;
        // Peak limiter at -0.5 dBFS, instant attack, 80 ms release.
        double limitGain = 1;
        std::array<std::array<double, 2 * kSoundEqBands>, kMaxChannels> state{};

        static Chain build(const SoundProfileSettings& s) {
            using K = Biquad::Kind;
            Chain c;
            auto add = [&](K kind, double hz, double q, double db) { c.filters[std::size_t(c.filterCount++)] = Biquad::make(kind, kRate, hz, q, db); };
            switch (SoundPreset(s.preset)) {
            case SoundPreset::Off: return c;
            case SoundPreset::BassBoost: add(K::LowShelf, 110, 0.9, 6.0); add(K::Peaking, 300, 1.0, -1.5); break;
            case SoundPreset::Voice: add(K::HighPass, 90, 0.707, 0); add(K::Peaking, 250, 1.0, -3.0); add(K::Peaking, 2500, 1.0, 5.0); add(K::Peaking, 5000, 1.5, 1.5); break;
            case SoundPreset::Footsteps: add(K::LowShelf, 150, 0.8, -6.0); add(K::Peaking, 3000, 1.2, 6.0); add(K::HighShelf, 8000, 0.8, 2.0); break;
            case SoundPreset::Night:
                add(K::LowShelf, 80, 0.8, -4.0); add(K::Peaking, 2500, 1.0, 2.0);
                c.threshDb = -28; c.ratio = 4; c.attack = 0.005; c.release = 0.150; c.makeupDb = 8;
                break;
            case SoundPreset::Cinema:
                add(K::LowShelf, 80, 0.9, 4.0); add(K::Peaking, 2500, 1.0, 2.0); add(K::HighShelf, 10000, 0.8, 3.0);
                c.threshDb = -18; c.ratio = 2; c.attack = 0.010; c.release = 0.200; c.makeupDb = 2;
                break;
            case SoundPreset::Custom: {
                bool any = false;
                for (int b = 0; b < kSoundEqBands; ++b) {
                    const double db = s.customDb[std::size_t(b)];
                    if (db == 0.0) continue;
                    any = true;
                    const K kind = b == 0 ? K::LowShelf : b == kSoundEqBands - 1 ? K::HighShelf : K::Peaking;
                    add(kind, kSoundEqBandHz[std::size_t(b)], b == 0 || b == kSoundEqBands - 1 ? 0.9 : 1.0, db);
                }
                if (!any) return c;
                break;
            }
            }
            c.active = true;
            return c;
        }

        void run(float* frame, unsigned channels) {
            if (!active) return;
            double peak = 0;
            double x[kMaxChannels];
            for (unsigned ch = 0; ch < channels; ++ch) {
                double v = std::isfinite(frame[ch]) ? frame[ch] : 0.0;
                auto& st = state[ch];
                for (int f = 0; f < filterCount; ++f) {
                    const Biquad& q = filters[std::size_t(f)];
                    double& s1 = st[std::size_t(2 * f)];
                    double& s2 = st[std::size_t(2 * f + 1)];
                    const double y = q.b0 * v + s1;             // transposed direct form II
                    s1 = q.b1 * v - q.a1 * y + s2;
                    s2 = q.b2 * v - q.a2 * y;
                    v = y;
                }
                x[ch] = v;
                peak = (std::max)(peak, std::abs(v));
            }
            double gain = 1.0;
            if (ratio > 0) {
                const double levelDb = 20.0 * std::log10((std::max)(peak, 1e-6));
                const double coeff = std::exp(-1.0 / (kRate * (levelDb > envDb ? attack : release)));
                envDb = levelDb + coeff * (envDb - levelDb);
                const double over = envDb - threshDb;
                const double reductionDb = over > 0 ? over / ratio - over : 0.0;
                gain = std::pow(10.0, (reductionDb + makeupDb) / 20.0);
                peak *= gain;
            }
            constexpr double ceiling = 0.944;                   // -0.5 dBFS
            const double needed = peak > ceiling ? ceiling / peak : 1.0;
            const double releaseCoeff = std::exp(-1.0 / (kRate * 0.080));
            limitGain = needed < limitGain ? needed : 1.0 + releaseCoeff * (limitGain - 1.0);
            limitGain = (std::min)(limitGain, needed);
            const double total = gain * limitGain;
            for (unsigned ch = 0; ch < channels; ++ch) frame[ch] = float(x[ch] * total);
        }
    };

    Chain current_{}, previous_{};
    SoundProfileSettings settings_{};
    unsigned channels_ = 2;
    int fadeRemaining_ = 0;
    bool configured_ = false;
};

// Process-wide choice, set by the interface and read by every renderer. The
// version lets the audio thread notice a change with one atomic load.
void setSoundProfile(const SoundProfileSettings& settings);
SoundProfileSettings soundProfile();
uint64_t soundProfileVersion();

} // namespace veyra::sink
