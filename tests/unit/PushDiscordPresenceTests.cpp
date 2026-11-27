// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// The Discord status (ui/PushDiscordPresence.h): what is shown, every byte written, how Discord's answers
// are read and how often anything is sent. Pure; also builds with g++.
#include "veyra/ui/PushDiscordPresence.h"
#include <cstdio>
#include <string>
using namespace veyra::ui::discord;
static int failures = 0, checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #x); } } while (0)

static const std::string kArrow = " \xE2\x86\x92 ";   // " → "
static const std::string kDot = " \xC2\xB7 ";         // " · "

static Facts playing(std::string_view kind, std::string_view console, std::string_view game, int fps, int shown) {
    Facts f;
    f.playing = true;
    f.kind = kind;
    f.console = console;
    f.game = game;
    f.gameFps = fps;
    f.shownFps = shown;
    f.startUnix = 1764250000;
    return f;
}

int main() {
    // --- frames ---------------------------------------------------------------------------------------
    {
        const std::string f = frame(Op::Handshake, "{}");
        CHECK(f.size() == 10);
        CHECK(f.substr(0, 8) == std::string("\0\0\0\0\2\0\0\0", 8));
        CHECK(f.substr(8) == "{}");
        const std::string g = frame(Op::Frame, std::string(300, 'x'));
        CHECK(uint8_t(g[0]) == 1 && uint8_t(g[4]) == 0x2C && uint8_t(g[5]) == 0x01 && g.size() == 308);
        const auto h = header(g);
        CHECK(h && h->op == Op::Frame && h->length == 300);
        CHECK(!header(g.substr(0, 7)));
        CHECK(!header(std::string("\5\0\0\0\0\0\0\0", 8)));                         // no opcode 5
        CHECK(!header(std::string("\x01\0\0\0\x01\0\x01\0", 8)));                   // 65537 bytes: not Discord's
        CHECK(header(std::string("\x03\0\0\0\0\0\x01\0", 8)) && header(std::string("\x03\0\0\0\0\0\x01\0", 8))->length == kMaxFrame);
        CHECK(!header(std::string("\xff\xff\xff\xff\0\0\0\0", 8)));                 // a negative opcode
    }
    // --- text the user typed ----------------------------------------------------------------------------
    {
        CHECK(field("GTA 6") == "GTA 6");
        CHECK(field("  GTA\t\n 6  ") == "GTA 6");
        CHECK(field("X").empty());                     // Discord refuses fewer than two characters
        CHECK(field("   ").empty() && field("").empty());
        CHECK(field("Ab") == "Ab");
        CHECK(field("a\x01" "b") == "a b");
        CHECK(field("\xC3\xA9t\xC3\xA9") == "\xC3\xA9t\xC3\xA9");                      // été
        CHECK(field("a\xFF" "b") == "a\xEF\xBF\xBD" "b");                              // not UTF-8: U+FFFD
        CHECK(field("a\xC0\xAF" "b") == "a\xEF\xBF\xBD\xEF\xBF\xBD" "b");               // an overlong slash
        CHECK(field("a\xED\xA0\x80" "b").find("\xED\xA0\x80") == std::string::npos);  // a surrogate
        CHECK(field("ab\xE2\x80\xA8" "cd") == "ab cd");                                // a line separator
        const std::string longName(200, 'g');
        CHECK(field(longName).size() == size_t(kMaxField));
        // Cut between characters, counted as Discord counts: an emoji is two units.
        std::string emoji;
        for (int i = 0; i < 70; ++i) emoji += "\xF0\x9F\x8E\xAE";                     // 🎮
        const std::string cut = field(emoji);
        CHECK(cut.size() == 60 * 4);
        std::string mixed = std::string(119, 'a') + "\xF0\x9F\x8E\xAE";
        CHECK(field(mixed) == std::string(119, 'a'));
        CHECK(field("abc  ", 3) == "abc");
        CHECK(field("ab cd", 3) == "ab");                // never a trailing space
    }
    // --- JSON strings -----------------------------------------------------------------------------------
    {
        CHECK(jsonString("a\"b\\c") == "\"a\\\"b\\\\c\"");
        CHECK(jsonString(std::string("\x01\x7F", 2)) == "\"\\u0001\\u007f\"");
        CHECK(jsonString("PS5" + kDot + "30" + kArrow + "120 FPS") == "\"PS5" + kDot + "30" + kArrow + "120 FPS\"");
    }
    // --- frame rates ------------------------------------------------------------------------------------
    {
        CHECK(snapFps(29.6) == 30 && snapFps(59.94) == 60 && snapFps(119.4) == 120 && snapFps(143.0) == 144);
        CHECK(snapFps(37.0) == 37 && snapFps(0.2) == 0 && snapFps(-5) == 0 && snapFps(5000) == 1000);
        CHECK(snapFps(std::nan("")) == 0);
        CHECK(shownFps(30, 4, 0, 0, 144) == 120);
        CHECK(shownFps(30, 4, 1, 0, 60) == 60);           // follows a 60 Hz screen
        CHECK(shownFps(30, 3, 2, 60.0, 144) == 60);       // a fixed 60 FPS target
        CHECK(shownFps(30, 1, 0, 0, 144) == 30);          // no frame generation
        CHECK(shownFps(60, 2, 1, 0, 59.94) == 60);        // never under the game's own rate
        CHECK(shownFps(0, 2, 0, 0, 144) == 0);
        CHECK(shownFps(30, 99, 0, 0, 0) == 240);          // a multiplier past 8 is 8
        CHECK(rates(30, 120) == "30" + kArrow + "120 FPS");
        CHECK(rates(60, 60) == "60 FPS" && rates(60, 0) == "60 FPS" && rates(0, 120).empty() && rates(5000, 0).empty());
    }
    // --- what is shown ----------------------------------------------------------------------------------
    {
        Activity a = activityFor(playing("capture", "ps5", "GTA 6", 30, 120));
        CHECK(a.show && a.details == "GTA 6" && a.state == "PS5" + kDot + "30" + kArrow + "120 FPS" && a.start == 1764250000);
        a = activityFor(playing("capture", "ps5", "", 30, 120));
        CHECK(a.details == "PS5" && a.state == "30" + kArrow + "120 FPS");
        // A capture card with no console chosen: never a guess.
        a = activityFor(playing("capture", "", "", 60, 120));
        CHECK(a.show && a.details == "60" + kArrow + "120 FPS" && a.state.empty());
        a = activityFor(playing("capture", "other", "Elden Ring", 60, 60));
        CHECK(a.details == "Elden Ring" && a.state == "60 FPS");
        a = activityFor(playing("capture", "", "", 0, 0));
        CHECK(a.show && a.details.empty() && a.state.empty());
        // PS5 Remote Play is a PS5 whatever is chosen; a PS5 Pro when that is chosen.
        CHECK(activityFor(playing("ps5", "xbox", "", 60, 120)).details == "PS5");
        CHECK(activityFor(playing("ps5", "ps5pro", "", 60, 120)).details == "PS5 Pro");
        CHECK(activityFor(playing("xbox", "", "Forza", 30, 60)).state == "Xbox" + kDot + "30" + kArrow + "60 FPS");
        CHECK(activityFor(playing("moonlight", "", "", 60, 120)).details == "PC");
        CHECK(activityFor(playing("capture", "switch2", "", 30, 60)).details == "Switch 2");
        // Never for a video, a file, a window, the frame input or a still picture, nor when nothing plays.
        for (const char* kind : {"file", "url", "screen", "frameinput", "image", "", "something-new"})
            CHECK(!activityFor(playing(kind, "ps5", "GTA 6", 30, 120)).show);
        Facts stopped = playing("capture", "ps5", "GTA 6", 30, 120);
        stopped.playing = false;
        CHECK(!activityFor(stopped).show && activityFor(stopped) == Activity{});
        // A game profile name made safe; one that is too short is left out.
        a = activityFor(playing("capture", "ps5", "  my\nGTA  ", 30, 120));
        CHECK(a.details == "my GTA");
        a = activityFor(playing("capture", "ps5", "X", 30, 120));
        CHECK(a.details == "PS5");
        Facts noStart = playing("capture", "ps5", "", 30, 120);
        noStart.startUnix = -4;
        CHECK(activityFor(noStart).start == 0);
    }
    // --- the bytes written --------------------------------------------------------------------------------
    {
        CHECK(handshake("1575866445412433962") == "{\"v\":1,\"client_id\":\"1575866445412433962\"}");
        const Activity a = activityFor(playing("capture", "ps5", "GTA \"6\"", 30, 120));
        const std::string json = setActivity(4242, a, 7, "PushFPS", "https://dosukasol.github.io/PushFPS/");
        CHECK(json == "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":4242,\"activity\":{\"details\":\"GTA \\\"6\\\"\",\"state\":\"PS5" + kDot + "30" + kArrow +
                          "120 FPS\",\"timestamps\":{\"start\":1764250000},\"assets\":{\"large_image\":\"pushfps\",\"large_text\":\"PushFPS\"},"
                          "\"buttons\":[{\"label\":\"Get PushFPS\",\"url\":\"https://dosukasol.github.io/PushFPS/\"}],\"instance\":false}},\"nonce\":\"7\"}");
        CHECK(setActivity(1, Activity{}, 2, "PushFPS", "x") == "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":1,\"activity\":null},\"nonce\":\"2\"}");
        Activity bare;
        bare.show = true;
        CHECK(setActivity(1, bare, 3, "PushFPS", "") ==
              "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":1,\"activity\":{\"assets\":{\"large_image\":\"pushfps\",\"large_text\":\"PushFPS\"},\"instance\":false}},\"nonce\":\"3\"}");
        CHECK(kButtonLabel.size() <= 32);   // Discord's limit for a button's label
    }
    // --- Discord's answers ----------------------------------------------------------------------------------
    {
        CHECK(eventOf("{\"cmd\":\"DISPATCH\",\"data\":{\"v\":1,\"user\":{\"username\":\"evt\"}},\"evt\":\"READY\",\"nonce\":null}") == "READY");
        CHECK(eventOf("{\"cmd\":\"SET_ACTIVITY\",\"data\":{\"code\":4000,\"message\":\"bad\"},\"evt\":\"ERROR\",\"nonce\":\"1\"}") == "ERROR");
        CHECK(eventOf("{\"cmd\":\"SET_ACTIVITY\",\"data\":{},\"evt\":null,\"nonce\":\"1\"}").empty());
        CHECK(eventOf("{\"evt\" : \"READY\"}") == "READY");
        // A user name cannot pose as the event: inside a JSON string every quote is escaped.
        CHECK(eventOf("{\"data\":{\"user\":{\"username\":\"\\\"evt\\\":\\\"READY\"}},\"evt\":null}").empty());
        CHECK(eventOf("{\"evt\":\"ready\"}").empty() && eventOf("{\"evt\":\"READY").empty() && eventOf("").empty());
        CHECK(eventOf("{\"evt\":\"" + std::string(40, 'A') + "\"}").empty());
        CHECK(codeOf("{\"code\":4000,\"message\":\"Invalid Client ID\"}") == kInvalidClientId);
        CHECK(codeOf("{\"data\":{\"code\": 4002}}") == 4002);
        CHECK(codeOf("{\"message\":\"no code\"}") == -1 && codeOf("{\"code\":\"x\"}") == -1 && codeOf("{\"code\":12345678}") == -1);
    }
    // --- how often ------------------------------------------------------------------------------------------
    {
        Pacer p;
        Activity off;
        CHECK(!p.due(off, 0) && !p.due(off, 100));          // nothing to clear on a new connection
        Activity a = activityFor(playing("capture", "ps5", "", 30, 120));
        CHECK(!p.due(a, 100));                              // a picture that starts waits until it has stood
        CHECK(!p.due(a, 101.9));
        CHECK(p.due(a, 102.0));
        p.sent(a, 102.0);
        CHECK(p.showing());
        CHECK(!p.due(a, 200));                              // already shown
        // A change waits for the settle time and for five seconds after the last send.
        Activity b = activityFor(playing("capture", "ps5", "", 60, 120));
        CHECK(!p.due(b, 103.0) && !p.due(b, 105.5) && !p.due(b, 106.9));
        CHECK(p.due(b, 107.0));
        p.sent(b, 107.0);
        // A frame rate that wobbles every second is still sent within the longest wait.
        Activity c = activityFor(playing("capture", "ps5", "", 30, 60)), d = activityFor(playing("capture", "ps5", "", 30, 90));
        int sends = 0;
        double firstSend = -1;
        for (int t = 0; t < 60; ++t) {
            const Activity& w = t % 2 ? c : d;
            const double now = 200.0 + t;
            if (p.due(w, now)) { p.sent(w, now); ++sends; if (firstSend < 0) firstSend = now; }
        }
        CHECK(sends > 0 && firstSend <= 200.0 + kMaxWaitSeconds);
        CHECK(sends <= 60 / int(kMinSendSeconds));          // never more than Discord takes
        // Going back to what is shown cancels a pending change.
        Pacer q;
        q.due(a, 0);
        q.due(a, 3);
        q.sent(a, 3);
        CHECK(!q.due(b, 20));
        CHECK(!q.due(a, 21));
        CHECK(!q.due(a, 40));
        // Clearing goes the same way, and a new connection starts empty.
        CHECK(!q.due(off, 50) && q.due(off, 52));
        q.sent(off, 52);
        CHECK(!q.showing());
        q.reset();
        CHECK(!q.due(off, 60));                             // a new connection: nothing to clear
        q.reset();
        CHECK(q.due(a, 61));                                // what is wanted when a connection comes goes at once
        Pacer fresh;
        CHECK(fresh.due(a, 5));                             // the first connection too
    }
    std::printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
