// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// The Discord status's connection: see ui/PushDiscordClient.h for what it guards against and
// ui/PushDiscordPresence.h for what is written.
#include "veyra/ui/PushDiscordClient.h"
#include "veyra/Brand.h"
#include "veyra/Log.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <format>

namespace veyra::ui {
namespace {
using discord::Op;

double steadySeconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// The client end of one of Discord's pipes, with every read and write under a time limit.
class Pipe {
public:
    Pipe() = default;
    Pipe(const Pipe&) = delete;
    Pipe& operator=(const Pipe&) = delete;
    ~Pipe() {
        close();
        if (event_) CloseHandle(event_);
    }
    bool open(const std::wstring& base) {
        if (!event_) event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!event_) return false;
        for (int i = 0; i < discord::kPipes; ++i) {
            const std::wstring name = base + std::to_wstring(i);
            // SECURITY_IDENTIFICATION: the program at the other end may learn who PushFPS runs as, never act
            // as that user.
            HANDLE h = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                   FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
            if (h != INVALID_HANDLE_VALUE) {
                handle_ = h;
                return true;
            }
        }
        return false;
    }
    void close() {
        if (handle_ == INVALID_HANDLE_VALUE) return;
        CancelIoEx(handle_, nullptr);
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
    // Bytes waiting to be read; -1 when the other end has gone.
    long long waiting() const {
        DWORD available = 0;
        if (!PeekNamedPipe(handle_, nullptr, 0, nullptr, &available, nullptr)) return -1;
        return (long long)available;
    }
    bool write(const std::string& bytes, DWORD limitMs) {
        return all(true, const_cast<char*>(bytes.data()), bytes.size(), limitMs);
    }
    bool read(char* into, size_t size, DWORD limitMs) { return all(false, into, size, limitMs); }

private:
    bool all(bool writing, char* data, size_t size, DWORD limitMs) {
        const ULONGLONG deadline = GetTickCount64() + limitMs;
        size_t done = 0;
        while (done < size) {
            DWORD moved = 0;
            if (!once(writing, data + done, DWORD(std::min<size_t>(size - done, 1u << 20)), moved, deadline) || moved == 0) return false;
            done += moved;
        }
        return true;
    }
    bool once(bool writing, char* data, DWORD size, DWORD& moved, ULONGLONG deadline) {
        OVERLAPPED overlapped{};
        overlapped.hEvent = event_;
        ResetEvent(event_);
        moved = 0;
        const BOOL started = writing ? WriteFile(handle_, data, size, nullptr, &overlapped) : ReadFile(handle_, data, size, nullptr, &overlapped);
        if (!started) {
            const DWORD error = GetLastError();
            // A pipe in message mode says "more data" for a part of a message: what was read counts.
            if (error != ERROR_IO_PENDING && !(error == ERROR_MORE_DATA && !writing)) return false;
        }
        const ULONGLONG now = GetTickCount64();
        const DWORD wait = now >= deadline ? 0 : DWORD(std::min<ULONGLONG>(deadline - now, 60000));
        if (WaitForSingleObject(event_, wait) != WAIT_OBJECT_0) {
            // Out of time: cancel, and wait for the cancel to finish, since `data` must outlive the I/O.
            CancelIoEx(handle_, &overlapped);
            DWORD ignored = 0;
            GetOverlappedResult(handle_, &overlapped, &ignored, TRUE);
            return false;
        }
        if (!GetOverlappedResult(handle_, &overlapped, &moved, FALSE))
            return !writing && GetLastError() == ERROR_MORE_DATA && moved > 0;
        return true;
    }

    HANDLE handle_ = INVALID_HANDLE_VALUE;
    HANDLE event_ = nullptr;
};

// One whole frame, or false (the other end gone, out of time, or a frame that is not Discord's).
bool readFrame(Pipe& pipe, Op& op, std::string& payload) {
    char head[8];
    if (!pipe.read(head, sizeof head, 1000)) return false;
    const auto h = discord::header(std::string_view(head, sizeof head));
    if (!h) return false;
    payload.assign(h->length, '\0');
    if (h->length > 0 && !pipe.read(payload.data(), h->length, 2000)) return false;
    op = h->op;
    return true;
}

std::string activityFrame(uint32_t pid, const discord::Activity& a, uint64_t nonce) {
    return discord::frame(Op::Frame, discord::setActivity(pid, a, nonce, brand::kProductName, brand::kWebsiteUrl));
}
} // namespace

PushDiscordClient::PushDiscordClient(std::string applicationId, std::wstring pipeBase)
    : applicationId_(std::move(applicationId)), pipeBase_(std::move(pipeBase)), thread_([this] { run(); }) {}

PushDiscordClient::~PushDiscordClient() {
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
    }
    wake_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void PushDiscordClient::set(const discord::Activity& wanted) {
    {
        std::lock_guard lock(mutex_);
        if (wanted == wanted_) return;
        wanted_ = wanted;
        changed_ = true;
    }
    wake_.notify_all();
}

bool PushDiscordClient::pause(int ms) {
    std::unique_lock lock(mutex_);
    wake_.wait_for(lock, std::chrono::milliseconds(ms), [this] { return stop_; });
    return stop_;
}

void PushDiscordClient::run() {
    const uint32_t pid = uint32_t(GetCurrentProcessId());
    uint64_t nonce = 0;
    bool saidNotFound = false;
    while (true) {
        if (state() == State::Refused) {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [this] { return stop_; });
            return;
        }
        const Ended ended = session(pid, nonce);
        if (ended == Ended::Stopped) return;
        if (ended == Ended::NotFound && !saidNotFound) {
            veyra::log::info("discord-status", "Discord not found; looking again every 15 s");
            saidNotFound = true;
        }
        const double until = steadySeconds() + (ended == Ended::Lost ? discord::kRetryLostSeconds : discord::kRetrySeconds);
        while (steadySeconds() < until)
            if (pause(250)) return;
    }
}

PushDiscordClient::Ended PushDiscordClient::session(uint32_t pid, uint64_t& nonce) {
    if (pause(0)) return Ended::Stopped;
    Pipe pipe;
    if (!pipe.open(pipeBase_)) return Ended::NotFound;
    if (!pipe.write(discord::frame(Op::Handshake, discord::handshake(applicationId_)), 2000)) {
        veyra::log::warn("discord-status", "the handshake could not be written");
        return Ended::Failed;
    }
    // Discord answers with READY (or closes, saying why) within a moment.
    bool ready = false;
    const double readyBy = steadySeconds() + 5.0;
    while (!ready && steadySeconds() < readyBy) {
        const long long waiting = pipe.waiting();
        if (waiting < 0) break;
        if (waiting == 0) {
            if (pause(20)) return Ended::Stopped;
            continue;
        }
        Op op = Op::Frame;
        std::string payload;
        if (!readFrame(pipe, op, payload)) break;
        const std::string event = op == Op::Frame ? discord::eventOf(payload) : std::string();
        if (event == "READY") ready = true;
        else if (op == Op::Close || event == "ERROR") {
            const int code = discord::codeOf(payload);
            veyra::log::warn("discord-status", std::format("Discord refused the connection: code {}", code));
            if (code == discord::kInvalidClientId) state_ = int(State::Refused);
            return Ended::Failed;
        } else if (op == Op::Ping && !pipe.write(discord::frame(Op::Pong, payload), 1000)) break;
    }
    if (!ready) {
        veyra::log::warn("discord-status", "Discord did not answer the handshake");
        return Ended::Failed;
    }
    state_ = int(State::Connected);
    veyra::log::info("discord-status", "connected to Discord");

    discord::Pacer pacer;
    int lastError = -1;
    bool stopping = false;
    while (true) {
        discord::Activity wanted;
        {
            std::lock_guard lock(mutex_);
            stopping = stop_;
            wanted = wanted_;
            changed_ = false;
        }
        if (stopping) {
            // Discord clears the line when the pipe closes; clearing it first makes that sure and immediate.
            if (pacer.showing()) pipe.write(activityFrame(pid, discord::Activity{}, ++nonce), 500);
            break;
        }
        // What Discord sent: pings are answered, a close ends the connection, errors are logged once each;
        // its answers to SET_ACTIVITY are read and left.
        bool lost = false;
        for (long long waiting = pipe.waiting(); waiting != 0 && !lost; waiting = pipe.waiting()) {
            Op op = Op::Frame;
            std::string payload;
            if (waiting < 0 || !readFrame(pipe, op, payload)) { lost = true; break; }
            if (op == Op::Ping) lost = !pipe.write(discord::frame(Op::Pong, payload), 1000);
            else if (op == Op::Close) {
                const int code = discord::codeOf(payload);
                veyra::log::warn("discord-status", std::format("Discord closed the connection: code {}", code));
                if (code == discord::kInvalidClientId) state_ = int(State::Refused);
                lost = true;
            } else if (op == Op::Frame && discord::eventOf(payload) == "ERROR") {
                const int code = discord::codeOf(payload);
                if (code != lastError) veyra::log::warn("discord-status", std::format("Discord did not take the status: code {}", code));
                lastError = code;
            }
        }
        if (lost) break;
        const double now = steadySeconds();
        if (pacer.due(wanted, now)) {
            if (!pipe.write(activityFrame(pid, wanted, ++nonce), 2000)) break;
            pacer.sent(wanted, now);
            showing_ = pacer.showing();
            ++updates_;
        }
        std::unique_lock lock(mutex_);
        wake_.wait_for(lock, std::chrono::milliseconds(250), [this] { return stop_ || changed_; });
    }
    showing_ = false;
    if (state() != State::Refused) state_ = int(State::Looking);
    if (stopping) return Ended::Stopped;
    veyra::log::info("discord-status", "the connection to Discord was lost");
    return Ended::Lost;
}

} // namespace veyra::ui
