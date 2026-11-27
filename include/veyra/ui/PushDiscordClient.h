#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// The Discord status's connection (PushFPS 4.5.8). ui/PushDiscordPresence.h decides what is shown and every
// byte that is written; this class only carries them: one thread looks for the Discord app's pipe, says
// which application it is, writes the newest activity when the Pacer says so and answers Discord's pings.
// Windows only. Made while the switch is on, destroyed when it goes off.
//
// What it protects against, since any program of the same user can make a pipe of that name:
//   - the pipe is opened with SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, so the program holding it can
//     learn which user PushFPS runs as, never act as that user (a named pipe server can otherwise
//     impersonate its client);
//   - what is written is the handshake, the activity, a pong (Discord's own bytes back) and nothing else;
//   - what is read goes a frame at a time, never longer than discord::kMaxFrame, and only its opcode, event
//     name and code are looked at (READY carries the user's name: never read);
//   - every read and write has a time limit, so a program that holds the pipe and never answers cannot hold
//     PushFPS: the connection is dropped and Discord looked for again later.
// Nothing about the user and nothing that is shown is logged: only that Discord was found or lost, and
// Discord's error codes.
#include "veyra/ui/PushDiscordPresence.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace veyra::ui {

class PushDiscordClient {
public:
    // Looking: Discord not found yet, or lost. Connected: Discord said READY. Refused: Discord does not know
    // the application id; nothing is tried again until the switch goes off and on.
    enum class State { Looking = 0, Connected = 1, Refused = 2 };

    // pipeBase is where the pipes are; the test gives one of its own, so it never talks to a real Discord.
    explicit PushDiscordClient(std::string applicationId, std::wstring pipeBase = L"\\\\.\\pipe\\discord-ipc-");
    // Clears the line when Discord shows one (Discord would also clear it when the pipe closes) and stops
    // the thread: about half a second at most.
    ~PushDiscordClient();
    PushDiscordClient(const PushDiscordClient&) = delete;
    PushDiscordClient& operator=(const PushDiscordClient&) = delete;

    // The newest line to show (show false: none). Cheap; the thread decides when Discord gets it.
    void set(const discord::Activity& wanted);
    State state() const { return State(state_.load()); }
    bool showing() const { return showing_.load(); }
    uint64_t updates() const { return updates_.load(); }   // activities written since the start

private:
    enum class Ended { NotFound, Failed, Lost, Stopped };
    void run();
    Ended session(uint32_t pid, uint64_t& nonce);   // one connection, from looking for the pipe to losing it
    bool pause(int ms);                            // waits ms or until stopped; true when stopped

    const std::string applicationId_;
    const std::wstring pipeBase_;
    std::mutex mutex_;
    std::condition_variable wake_;
    discord::Activity wanted_;
    bool changed_ = false;
    bool stop_ = false;
    std::atomic<int> state_{int(State::Looking)};
    std::atomic<bool> showing_{false};
    std::atomic<uint64_t> updates_{0};
    std::thread thread_;   // last: started once everything above exists
};

} // namespace veyra::ui
