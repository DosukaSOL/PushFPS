// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#include "veyra/sink/SoundProfile.h"

#include <atomic>
#include <mutex>

namespace veyra::sink {
namespace {
std::mutex& profileMutex() { static std::mutex m; return m; }
SoundProfileSettings& profileValue() { static SoundProfileSettings v; return v; }
std::atomic<uint64_t>& profileVersion() { static std::atomic<uint64_t> v{0}; return v; }
}

void setSoundProfile(const SoundProfileSettings& settings) {
    const SoundProfileSettings clean = settings.sanitized();
    {
        std::lock_guard lock(profileMutex());
        if (profileValue() == clean) return;
        profileValue() = clean;
    }
    profileVersion().fetch_add(1, std::memory_order_release);
}

SoundProfileSettings soundProfile() {
    std::lock_guard lock(profileMutex());
    return profileValue();
}

uint64_t soundProfileVersion() { return profileVersion().load(std::memory_order_acquire); }

} // namespace veyra::sink
