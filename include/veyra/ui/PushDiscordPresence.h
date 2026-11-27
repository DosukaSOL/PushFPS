#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Discord status (PushFPS 4.5.8; a user's request: friends on Discord see what is played through PushFPS).
//
// The Discord app on the PC listens on a local named pipe (\\.\pipe\discord-ipc-0 to -9). A program that
// wants a line on its user's profile writes frames to it: a little-endian 32-bit opcode, a little-endian
// 32-bit length, then that many bytes of JSON. The first frame is the handshake with the application's id
// (brand::kDiscordApplicationId: public, made by the owner in Discord's developer portal); after Discord's
// READY, SET_ACTIVITY sets or clears the line. Nothing else is asked for: no sign-in, no permission, no
// user data. Discord's READY carries the user's name and avatar: nothing of it is read, kept or logged.
// When the connection closes, Discord takes the line away by itself.
//
// This header is the whole decision and every byte that is written, pure, so it builds anywhere and
// tests/unit/PushDiscordPresenceTests.cpp holds it: what is shown, the JSON, the frames, how Discord's
// answers are read and how often anything is sent. ui/PushDiscordClient.h only carries the bytes.
//
// What is shown, and only while a console's picture plays (a capture card, PS5 Remote Play, Xbox
// streaming, a PC stream; never a video, a file, a window, the frame input or a still picture):
//   details   the chosen game profile's name (when the user lets it be shown), else the console, else
//             the frame rates;
//   state     what is left of console and frame rates, " · " between them ("PS5 · 30 → 120 FPS");
//   the time since the picture started, the PushFPS logo (art asset "pushfps") and a button to PushFPS's
//   website. The console is named only as engine/PushCompareName.h names it, never guessed. Every word is
//   a name, a number or "FPS", the same in every language, so friends read the same line whatever
//   language PushFPS speaks.
#include "veyra/engine/PushCompareName.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

namespace veyra::ui::discord {

inline constexpr std::string_view kLargeImage = "pushfps";   // the art asset's name in Discord's portal
inline constexpr std::string_view kButtonLabel = "Get PushFPS";
inline constexpr uint32_t kMaxFrame = 64 * 1024;   // a longer frame is not Discord's: the connection is dropped
inline constexpr int kMaxField = 120;              // Discord takes 2..128 characters for details and state
inline constexpr int kPipes = 10;                  // discord-ipc-0 .. discord-ipc-9
// How often: Discord takes five activity updates in twenty seconds. A change waits until it has stood
// kSettleSeconds (a frame rate that wobbles is not sent each time) but never longer than kMaxWaitSeconds,
// and two sends are at least kMinSendSeconds apart. Discord not running: looked for every kRetrySeconds
// (opening a pipe that is not there costs nothing); after a connection was lost, kRetryLostSeconds.
inline constexpr double kSettleSeconds = 2.0;
inline constexpr double kMaxWaitSeconds = 10.0;
inline constexpr double kMinSendSeconds = 5.0;
inline constexpr double kRetrySeconds = 15.0;
inline constexpr double kRetryLostSeconds = 5.0;
// Discord's close and error code for an application id it does not know: looking again cannot help.
inline constexpr int kInvalidClientId = 4000;

enum class Op : int32_t { Handshake = 0, Frame = 1, Close = 2, Ping = 3, Pong = 4 };

// One frame: opcode and length (little endian), then the payload.
inline std::string frame(Op op, std::string_view payload) {
    std::string out(8, '\0');
    const uint32_t o = uint32_t(int32_t(op)), n = uint32_t(payload.size());
    for (int i = 0; i < 4; ++i) {
        out[size_t(i)] = char((o >> (8 * i)) & 0xFFu);
        out[size_t(4 + i)] = char((n >> (8 * i)) & 0xFFu);
    }
    out.append(payload);
    return out;
}

struct Header {
    Op op = Op::Frame;
    uint32_t length = 0;
};
// The first eight bytes of a frame Discord sent; nothing for fewer bytes, an opcode Discord does not send
// or a length past kMaxFrame.
inline std::optional<Header> header(std::string_view bytes) {
    if (bytes.size() < 8) return std::nullopt;
    uint32_t o = 0, n = 0;
    for (int i = 0; i < 4; ++i) {
        o |= uint32_t(uint8_t(bytes[size_t(i)])) << (8 * i);
        n |= uint32_t(uint8_t(bytes[size_t(4 + i)])) << (8 * i);
    }
    if (o > uint32_t(Op::Pong) || n > kMaxFrame) return std::nullopt;
    return Header{Op(int32_t(o)), n};
}

// One code point of UTF-8 at `at`: its length (0 for a byte that does not start a valid sequence: an
// overlong form, a surrogate, past U+10FFFF or cut off) and its value.
inline int utf8At(std::string_view s, size_t at, uint32_t& cp) {
    const auto b = [&](size_t k) { return uint32_t(uint8_t(s[at + k])); };
    const uint32_t c = b(0);
    if (c < 0x80) { cp = c; return 1; }
    int len = 0;
    uint32_t min = 0;
    if ((c & 0xE0) == 0xC0) { len = 2; cp = c & 0x1F; min = 0x80; }
    else if ((c & 0xF0) == 0xE0) { len = 3; cp = c & 0x0F; min = 0x800; }
    else if ((c & 0xF8) == 0xF0) { len = 4; cp = c & 0x07; min = 0x10000; }
    else return 0;
    if (at + size_t(len) > s.size()) return 0;
    for (int k = 1; k < len; ++k) {
        if ((b(size_t(k)) & 0xC0) != 0x80) return 0;
        cp = (cp << 6) | (b(size_t(k)) & 0x3F);
    }
    if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return 0;
    return len;
}

// Text for a line of the status: what the user typed, made safe. Control characters become spaces, runs
// of spaces one, both ends are trimmed, a byte that is not UTF-8 becomes U+FFFD, and it is cut (between
// characters) at kMaxField UTF-16 units, as Discord counts. Shorter than two characters: nothing, because
// Discord refuses a line that short.
inline std::string field(std::string_view s, int maxUnits = kMaxField) {
    std::string out;
    int units = 0, chars = 0;
    bool space = false;
    for (size_t i = 0; i < s.size();) {
        uint32_t cp = 0;
        const int len = utf8At(s, i, cp);
        std::string_view piece = len > 0 ? s.substr(i, size_t(len)) : std::string_view("\xEF\xBF\xBD");
        if (len <= 0) cp = 0xFFFD;
        i += size_t(len > 0 ? len : 1);
        if (cp < 0x20 || cp == 0x7F || (cp >= 0x80 && cp < 0xA0) || cp == 0x2028 || cp == 0x2029 || cp == ' ') {
            space = !out.empty();
            continue;
        }
        const int need = (space ? 1 : 0) + (cp > 0xFFFF ? 2 : 1);
        if (units + need > maxUnits) break;
        if (space) { out += ' '; ++units; ++chars; space = false; }
        out.append(piece);
        units += cp > 0xFFFF ? 2 : 1;
        ++chars;
    }
    return chars >= 2 ? out : std::string();
}

// A JSON string literal. The text is UTF-8 already made safe by field() (or a fixed text of this file);
// quotes, backslashes and any control character are escaped all the same.
inline std::string jsonString(std::string_view s) {
    std::string out = "\"";
    for (char ch : s) {
        const auto c = uint8_t(ch);
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c < 0x20 || c == 0x7F) {
            char buf[8];
            std::snprintf(buf, sizeof buf, "\\u%04x", unsigned(c));
            out += buf;
        } else out += ch;
    }
    out += '"';
    return out;
}

// A measured frame rate as people say it: the nearest of the common rates when it is within 3 % (at
// least 1.5 frames) of one, else rounded. 0 for nothing measured.
inline int snapFps(double fps) {
    if (!std::isfinite(fps) || fps < 0.5) return 0;
    static constexpr int common[] = {24, 25, 30, 40, 45, 48, 50, 60, 72, 75, 90, 100, 120, 144, 165, 180, 200, 240};
    int best = 0;
    double distance = 1e9;
    for (int c : common) {
        const double d = std::abs(fps - double(c));
        if (d < distance) { distance = d; best = c; }
    }
    if (distance <= std::max(1.5, best * 0.03)) return best;
    return int(std::lround(std::min(fps, 1000.0)));
}

// What PushFPS shows: the game's rate times the multiplier in use, never above a fixed output rate
// (capMode 2) or the screen it follows (capMode 1); the comparison clip's label is worked out the same
// way (bridge/PushFps.cpp pushStartCompareClip). Without frame generation the multiplier is 1.
inline int shownFps(int gameFps, int multiplier, int capMode, double capFps, double refreshHz) {
    if (gameFps <= 0) return 0;
    double shown = double(gameFps) * double(std::clamp(multiplier, 1, 8));
    if (capMode == 2 && capFps > 1) shown = std::min(shown, capFps);
    else if (capMode == 1 && refreshHz > 1) shown = std::min(shown, refreshHz);
    return std::max(gameFps, snapFps(shown));
}

// "30 → 120 FPS" with frame generation, "60 FPS" without, nothing when the game's rate is not known.
inline std::string rates(int gameFps, int shown) {
    if (gameFps <= 0 || gameFps > 1000) return {};
    if (shown > gameFps && shown <= 1000) return std::to_string(gameFps) + " \xE2\x86\x92 " + std::to_string(shown) + " FPS";
    return std::to_string(gameFps) + " FPS";
}

// A console's picture: what the status is shown for. A video, a file, a window or screen, the frame input
// and a still picture are never shown.
inline bool consolePicture(std::string_view kind) {
    return kind == "capture" || kind == "ps5" || kind == "xbox" || kind == "moonlight";
}

struct Facts {
    bool playing = false;
    std::string_view kind;      // QmlPlayerBridge::sourceKind(): "capture", "ps5", "xbox", "moonlight", "file", ...
    std::string_view console;   // preference pushConsole; "" when it was never set
    std::string_view game;      // the chosen game profile's name; "" when none or when it is not to be shown
    int gameFps = 0;            // the game's own frame rate; 0 when not known
    int shownFps = 0;           // what PushFPS shows (shownFps()); at most gameFps without frame generation
    int64_t startUnix = 0;      // when this picture started, seconds since 1970; 0 when not known
};

struct Activity {
    bool show = false;
    std::string details, state;
    int64_t start = 0;
    bool operator==(const Activity&) const = default;
};

inline Activity activityFor(const Facts& f) {
    Activity a;
    if (!f.playing || !consolePicture(f.kind)) return a;
    a.show = true;
    const auto name = engine::pushCompareSourceName(f.kind, f.console);
    const std::string console = name.side == engine::PushCompareSide::Named ? std::string(name.name) : std::string();
    const std::string game = field(f.game);
    const std::string fps = rates(f.gameFps, f.shownFps);
    std::string rest;
    const auto add = [&rest](const std::string& part) {
        if (part.empty()) return;
        if (!rest.empty()) rest += " \xC2\xB7 ";
        rest += part;
    };
    if (!game.empty()) { a.details = game; add(console); add(fps); }
    else if (!console.empty()) { a.details = console; add(fps); }
    else a.details = fps;
    a.details = field(a.details);
    a.state = field(rest);
    a.start = f.startUnix > 0 ? f.startUnix : 0;
    return a;
}

inline std::string handshake(std::string_view applicationId) {
    return "{\"v\":1,\"client_id\":" + jsonString(applicationId) + "}";
}

// SET_ACTIVITY for this process; a cleared line (show false) is "activity":null.
inline std::string setActivity(uint32_t pid, const Activity& a, uint64_t nonce, std::string_view largeText, std::string_view buttonUrl) {
    std::string act = "null";
    if (a.show) {
        act = "{";
        if (!a.details.empty()) act += "\"details\":" + jsonString(a.details) + ",";
        if (!a.state.empty()) act += "\"state\":" + jsonString(a.state) + ",";
        if (a.start > 0) act += "\"timestamps\":{\"start\":" + std::to_string(a.start) + "},";
        act += "\"assets\":{\"large_image\":" + jsonString(kLargeImage) + ",\"large_text\":" + jsonString(largeText) + "},";
        if (!buttonUrl.empty()) act += "\"buttons\":[{\"label\":" + jsonString(kButtonLabel) + ",\"url\":" + jsonString(buttonUrl) + "}],";
        act += "\"instance\":false}";
    }
    return "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":" + std::to_string(pid) + ",\"activity\":" + act + "},\"nonce\":\"" + std::to_string(nonce) + "\"}";
}

namespace detail {
inline size_t skipSpace(std::string_view s, size_t at) {
    while (at < s.size() && (s[at] == ' ' || s[at] == '\t' || s[at] == '\r' || s[at] == '\n')) ++at;
    return at;
}
// Where the value of "key" begins: the first place the key stands as a key (followed by a colon). Inside
// a JSON string a quote is escaped, so a name or a message cannot hold one that matches.
inline size_t valueOf(std::string_view s, std::string_view key) {
    const std::string quoted = "\"" + std::string(key) + "\"";
    for (size_t at = s.find(quoted); at != std::string_view::npos; at = s.find(quoted, at + 1)) {
        const size_t colon = skipSpace(s, at + quoted.size());
        if (colon < s.size() && s[colon] == ':') return skipSpace(s, colon + 1);
    }
    return std::string_view::npos;
}
}

// What Discord answered, read only as far as is needed: the event's name ("READY", "ERROR"; "" for none,
// null, or anything but capitals and underscores up to 32) and an error or close code (-1 for none).
inline std::string eventOf(std::string_view json) {
    const size_t at = detail::valueOf(json, "evt");
    if (at == std::string_view::npos || at >= json.size() || json[at] != '"') return {};
    std::string out;
    for (size_t i = at + 1; i < json.size(); ++i) {
        const char c = json[i];
        if (c == '"') return out;
        if (!((c >= 'A' && c <= 'Z') || c == '_') || out.size() >= 32) return {};
        out += c;
    }
    return {};
}
inline int codeOf(std::string_view json) {
    const size_t at = detail::valueOf(json, "code");
    if (at == std::string_view::npos) return -1;
    int value = 0, digits = 0;
    for (size_t i = at; i < json.size() && json[i] >= '0' && json[i] <= '9'; ++i) {
        if (++digits > 6) return -1;
        value = value * 10 + (json[i] - '0');
    }
    return digits > 0 ? value : -1;
}

// When the newest activity goes to Discord (see the constants). `now` is in seconds on any steady clock.
// reset() after connecting: Discord has nothing from this connection yet, so a line to clear is nothing to
// send, and what is wanted at the first call after it goes at once (it was not looked at while there was no
// connection, so how long it stood is not known; a fresh connection is no wobble).
class Pacer {
public:
    bool due(const Activity& wanted, double now) {
        if (fresh_) {
            wanted_ = wanted;
            haveWanted_ = true;
            since_ = now - kSettleSeconds;
            fresh_ = false;
        } else if (!haveWanted_ || !(wanted == wanted_)) {
            wanted_ = wanted;
            haveWanted_ = true;
            since_ = now;
        }
        const bool differs = haveSent_ ? !(wanted == sent_) : wanted.show;
        if (!differs) { differsSince_.reset(); return false; }
        if (!differsSince_) differsSince_ = now;
        if (haveSent_ && now - lastSend_ < kMinSendSeconds) return false;
        return now - since_ >= kSettleSeconds || now - *differsSince_ >= kMaxWaitSeconds;
    }
    void sent(const Activity& a, double now) {
        sent_ = a;
        haveSent_ = true;
        lastSend_ = now;
        differsSince_.reset();
    }
    void reset() {
        haveSent_ = false;
        differsSince_.reset();
        fresh_ = true;
    }
    // The line Discord holds from this connection (nothing when none was sent).
    bool showing() const { return haveSent_ && sent_.show; }

private:
    Activity wanted_, sent_;
    bool haveWanted_ = false, haveSent_ = false, fresh_ = true;
    double since_ = 0, lastSend_ = 0;
    std::optional<double> differsSince_;
};

} // namespace veyra::ui::discord
