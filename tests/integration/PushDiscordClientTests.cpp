// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// The Discord status's connection (ui/PushDiscordClient.h) against a pretend Discord: named pipes this test
// makes under a name of its own, so it never talks to a real Discord. It checks the bytes the client writes
// (handshake, the activity exactly as ui/PushDiscordPresence.h makes it, the pong, the clearing at the end),
// that it answers what Discord sends, gives up on an application id Discord refuses, drops a pipe that does
// not speak Discord's frames and finds it again, and never makes PushFPS wait for a program that holds the
// pipe and says nothing.
#include "veyra/ui/PushDiscordClient.h"
#include "veyra/Brand.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <thread>

using namespace veyra::ui;
using discord::Op;
static int failures = 0, checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #x); std::fflush(stdout); } } while (0)

namespace {
double seconds() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

std::wstring base(const wchar_t* what) {
    return L"\\\\.\\pipe\\pushfps-discord-test-" + std::to_wstring(GetCurrentProcessId()) + L"-" + what + L"-";
}

// The pretend Discord: one pipe instance, waited on and read with time limits so a failing client never
// hangs the test.
struct FakeDiscord {
    HANDLE pipe = INVALID_HANDLE_VALUE;
    explicit FakeDiscord(const std::wstring& name) {
        pipe = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1,
                                65536, 65536, 0, nullptr);
    }
    ~FakeDiscord() { if (pipe != INVALID_HANDLE_VALUE) CloseHandle(pipe); }
    FakeDiscord(const FakeDiscord&) = delete;
    FakeDiscord& operator=(const FakeDiscord&) = delete;
    bool accept(DWORD limitMs) {
        OVERLAPPED ov{};
        ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        bool ok = false;
        if (ConnectNamedPipe(pipe, &ov)) ok = true;
        else if (GetLastError() == ERROR_PIPE_CONNECTED) ok = true;
        else if (GetLastError() == ERROR_IO_PENDING) {
            if (WaitForSingleObject(ov.hEvent, limitMs) == WAIT_OBJECT_0) {
                DWORD ignored = 0;
                ok = GetOverlappedResult(pipe, &ov, &ignored, FALSE) != FALSE;
            } else {
                CancelIoEx(pipe, &ov);
                DWORD ignored = 0;
                GetOverlappedResult(pipe, &ov, &ignored, TRUE);
            }
        }
        CloseHandle(ov.hEvent);
        return ok;
    }
    // -1 when the client has gone.
    long long waiting() {
        DWORD n = 0;
        if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &n, nullptr)) return -1;
        return (long long)n;
    }
    bool io(bool writing, char* data, DWORD size) {
        OVERLAPPED ov{};
        ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        DWORD moved = 0;
        BOOL ok = writing ? WriteFile(pipe, data, size, nullptr, &ov) : ReadFile(pipe, data, size, nullptr, &ov);
        if (ok || GetLastError() == ERROR_IO_PENDING) {
            if (WaitForSingleObject(ov.hEvent, 3000) == WAIT_OBJECT_0) ok = GetOverlappedResult(pipe, &ov, &moved, FALSE);
            else { CancelIoEx(pipe, &ov); GetOverlappedResult(pipe, &ov, &moved, TRUE); ok = FALSE; }
        }
        CloseHandle(ov.hEvent);
        return ok && moved == size;
    }
    // Exactly `size` bytes within limitMs; false when they do not come or the client has gone.
    bool readExact(char* into, size_t size, DWORD limitMs) {
        const double until = seconds() + limitMs / 1000.0;
        size_t done = 0;
        while (done < size) {
            const long long w = waiting();
            if (w < 0) return false;
            if (w == 0) {
                if (seconds() > until) return false;
                Sleep(5);
                continue;
            }
            const DWORD take = DWORD(std::min<long long>(w, (long long)(size - done)));
            if (!io(false, into + done, take)) return false;
            done += take;
        }
        return true;
    }
    bool readFrame(Op& op, std::string& payload, DWORD limitMs) {
        char head[8];
        if (!readExact(head, 8, limitMs)) return false;
        const auto h = discord::header(std::string_view(head, 8));
        if (!h) return false;
        op = h->op;
        payload.assign(h->length, '\0');
        return h->length == 0 || readExact(payload.data(), h->length, 2000);
    }
    bool writeRaw(std::string bytes) { return io(true, bytes.data(), DWORD(bytes.size())); }
    bool writeFrame(Op op, const std::string& payload) { return writeRaw(discord::frame(op, payload)); }
    // The client has closed its end within limitMs.
    bool gone(DWORD limitMs) {
        const double until = seconds() + limitMs / 1000.0;
        while (seconds() < until) {
            const long long w = waiting();
            if (w < 0) return true;
            if (w > 0) {
                std::string skip(size_t(w), '\0');
                if (!io(false, skip.data(), DWORD(w))) return true;
            }
            Sleep(10);
        }
        return false;
    }
};

const char* kReady = "{\"cmd\":\"DISPATCH\",\"data\":{\"v\":1,\"config\":{},\"user\":{\"id\":\"1\",\"username\":\"evt\",\"global_name\":\"\\\"evt\\\":\\\"ERROR\"}},\"evt\":\"READY\",\"nonce\":null}";

discord::Activity sample(std::string_view game, int fps, int shown) {
    discord::Facts f;
    f.playing = true;
    f.kind = "capture";
    f.console = "ps5";
    f.game = game;
    f.gameFps = fps;
    f.shownFps = shown;
    f.startUnix = 1764250000;
    return discord::activityFor(f);
}

bool waitFor(const std::function<bool()>& condition, double limit) {
    const double until = seconds() + limit;
    while (seconds() < until) {
        if (condition()) return true;
        Sleep(10);
    }
    return condition();
}
} // namespace

int main() {
    const uint32_t pid = uint32_t(GetCurrentProcessId());
    const std::string website = veyra::brand::kWebsiteUrl, product = veyra::brand::kProductName;

    // No Discord: the client keeps looking and lets go at once.
    {
        auto* client = new PushDiscordClient(veyra::brand::kDiscordApplicationId, base(L"none"));
        Sleep(300);
        CHECK(client->state() == PushDiscordClient::State::Looking && !client->showing());
        const double t = seconds();
        delete client;
        CHECK(seconds() - t < 1.0);
    }

    // Discord on its third pipe: the handshake, READY, the line, a ping, a change, an error, and the line
    // cleared when the switch goes off.
    {
        const std::wstring b = base(L"main");
        FakeDiscord discordApp(b + L"2");
        CHECK(discordApp.pipe != INVALID_HANDLE_VALUE);
        const discord::Activity first = sample("GTA 6", 30, 120), second = sample("GTA 6", 30, 60);
        auto* client = new PushDiscordClient(veyra::brand::kDiscordApplicationId, b);
        client->set(first);
        CHECK(discordApp.accept(3000));
        Op op = Op::Frame;
        std::string payload;
        CHECK(discordApp.readFrame(op, payload, 3000) && op == Op::Handshake);
        CHECK(payload == discord::handshake(veyra::brand::kDiscordApplicationId));
        CHECK(client->state() == PushDiscordClient::State::Looking);   // not before READY
        CHECK(discordApp.writeFrame(Op::Frame, kReady));
        CHECK(waitFor([&] { return client->state() == PushDiscordClient::State::Connected; }, 3));
        // What was wanted before Discord was there goes at once.
        CHECK(discordApp.readFrame(op, payload, 3000) && op == Op::Frame);
        CHECK(payload == discord::setActivity(pid, first, 1, product, website));
        CHECK(waitFor([&] { return client->showing() && client->updates() == 1; }, 2));
        // Discord's answer to SET_ACTIVITY is read and left; a ping is answered with its own bytes.
        CHECK(discordApp.writeFrame(Op::Frame, "{\"cmd\":\"SET_ACTIVITY\",\"data\":{},\"evt\":null,\"nonce\":\"1\"}"));
        CHECK(discordApp.writeFrame(Op::Ping, "{\"x\":42}"));
        CHECK(discordApp.readFrame(op, payload, 3000) && op == Op::Pong && payload == "{\"x\":42}");
        // A change waits for the settle time and five seconds after the last send, then goes.
        const double changed = seconds();
        client->set(second);
        CHECK(discordApp.readFrame(op, payload, 9000) && op == Op::Frame);
        CHECK(payload == discord::setActivity(pid, second, 2, product, website));
        CHECK(seconds() - changed >= discord::kSettleSeconds - 0.3);
        // An error does not end the connection.
        CHECK(discordApp.writeFrame(Op::Frame, "{\"cmd\":\"SET_ACTIVITY\",\"data\":{\"code\":4002,\"message\":\"x\"},\"evt\":\"ERROR\",\"nonce\":\"2\"}"));
        Sleep(400);
        CHECK(client->state() == PushDiscordClient::State::Connected);
        // Switched off: the line is cleared, then the pipe closes, quickly.
        const double t = seconds();
        delete client;
        CHECK(seconds() - t < 1.5);
        CHECK(discordApp.readFrame(op, payload, 2000) && op == Op::Frame);
        CHECK(payload == discord::setActivity(pid, discord::Activity{}, 3, product, website));
        CHECK(discordApp.gone(2000));
    }

    // Nothing to show: connected, and nothing written after the handshake.
    {
        const std::wstring b = base(L"idle");
        FakeDiscord discordApp(b + L"0");
        auto* client = new PushDiscordClient(veyra::brand::kDiscordApplicationId, b);
        Op op = Op::Frame;
        std::string payload;
        CHECK(discordApp.accept(3000) && discordApp.readFrame(op, payload, 3000) && op == Op::Handshake);
        CHECK(discordApp.writeFrame(Op::Frame, kReady));
        CHECK(waitFor([&] { return client->state() == PushDiscordClient::State::Connected; }, 3));
        Sleep(2500);
        CHECK(discordApp.waiting() == 0 && client->updates() == 0);
        delete client;
        CHECK(discordApp.gone(2000));   // closed without a SET_ACTIVITY: there was nothing to clear
    }

    // An application id Discord does not know: it closes, saying so, and nothing is tried again.
    {
        const std::wstring b = base(L"refused");
        FakeDiscord discordApp(b + L"0");
        auto* client = new PushDiscordClient("1", b);
        Op op = Op::Frame;
        std::string payload;
        CHECK(discordApp.accept(3000) && discordApp.readFrame(op, payload, 3000) && op == Op::Handshake);
        CHECK(discordApp.writeFrame(Op::Close, "{\"code\":4000,\"message\":\"Invalid Client ID\"}"));
        CHECK(waitFor([&] { return client->state() == PushDiscordClient::State::Refused; }, 3));
        const double t = seconds();
        delete client;
        CHECK(seconds() - t < 1.0);
    }

    // A pipe that does not speak Discord's frames is dropped, and Discord is found again a little later.
    {
        const std::wstring b = base(L"garbage");
        auto first = std::make_unique<FakeDiscord>(b + L"0");
        auto* client = new PushDiscordClient(veyra::brand::kDiscordApplicationId, b);
        client->set(sample("", 60, 120));
        Op op = Op::Frame;
        std::string payload;
        CHECK(first->accept(3000) && first->readFrame(op, payload, 3000) && op == Op::Handshake);
        CHECK(first->writeFrame(Op::Frame, kReady));
        CHECK(first->readFrame(op, payload, 3000) && op == Op::Frame);
        CHECK(first->writeRaw(std::string("\x01\0\0\0\x70\x11\x01\0", 8)));   // 70000 bytes announced
        CHECK(first->gone(3000));
        CHECK(waitFor([&] { return client->state() == PushDiscordClient::State::Looking && !client->showing(); }, 2));
        first.reset();
        FakeDiscord again(b + L"0");
        CHECK(again.accept(9000));   // looked for again after kRetryLostSeconds
        CHECK(again.readFrame(op, payload, 3000) && op == Op::Handshake);
        CHECK(again.writeFrame(Op::Frame, kReady));
        CHECK(again.readFrame(op, payload, 3000) && op == Op::Frame && payload.find("\"details\":\"PS5\"") != std::string::npos);
        delete client;
    }

    // A program that holds the pipe and never answers does not hold PushFPS.
    {
        const std::wstring b = base(L"silent");
        FakeDiscord discordApp(b + L"0");
        auto* client = new PushDiscordClient(veyra::brand::kDiscordApplicationId, b);
        Op op = Op::Frame;
        std::string payload;
        CHECK(discordApp.accept(3000) && discordApp.readFrame(op, payload, 3000) && op == Op::Handshake);
        Sleep(500);
        CHECK(client->state() == PushDiscordClient::State::Looking);
        const double t = seconds();
        delete client;
        CHECK(seconds() - t < 1.0);
        CHECK(discordApp.gone(2000));
    }

    std::printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
