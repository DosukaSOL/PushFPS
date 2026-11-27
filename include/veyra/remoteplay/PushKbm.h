// SPDX-License-Identifier: GPL-3.0-only
#pragma once
// PushFPS: mouse and keyboard as a controller for a console (PS5 Remote Play, a capture card with
// "Control the PS5 with Remote Play", Xbox streaming). Pure: no Windows and no Qt, so the whole
// translation (presets, the mouse as a stick, what a key presses, the merge with a real controller)
// is tested with synthesized input (tests/unit/PushKbmTests.cpp, also with g++).
//
// The console only ever sees a controller: keys and mouse buttons press its buttons, the movement keys
// push the left stick, and the mouse's speed pushes the right stick. The real controller keeps working
// beside it (buttons from either, triggers the further pressed, each stick from whichever is pushed
// further, the right stick from the mouse while it moves), so a player can drive with the controller and
// walk and aim with the mouse without switching anything.
//
// Input codes: a Windows virtual-key code for a key (1..254; Shift, Ctrl and Alt as their left and right
// codes, never the generic ones), kMouseLeft..kMouseX2 for mouse buttons, kWheelUp / kWheelDown for one
// notch of the wheel, 0 for nothing.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <string_view>

#include "veyra/remoteplay/Types.h"

namespace veyra::remoteplay::kbm {

// ---- input codes -----------------------------------------------------------------------------------
constexpr std::uint16_t kNone = 0;
constexpr std::uint16_t kMouseLeft = 0x101, kMouseRight = 0x102, kMouseMiddle = 0x103, kMouseX1 = 0x104, kMouseX2 = 0x105;
constexpr std::uint16_t kWheelUp = 0x110, kWheelDown = 0x111;
constexpr std::uint16_t kCodes = 0x112;   // every code is below this

// The virtual keys the presets use (winuser.h values, written out so this header needs no Windows).
namespace vk {
constexpr std::uint16_t Back = 0x08, Tab = 0x09, Return = 0x0D, Capital = 0x14, Escape = 0x1B, Space = 0x20;
constexpr std::uint16_t Left = 0x25, Up = 0x26, Right = 0x27, Down = 0x28;
constexpr std::uint16_t LShift = 0xA0, RShift = 0xA1, LControl = 0xA2, RControl = 0xA3, LMenu = 0xA4, RMenu = 0xA5;
constexpr std::uint16_t F1 = 0x70, F12 = 0x7B, F24 = 0x87;
constexpr std::uint16_t letter(char c) { return std::uint16_t(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c); }
}

constexpr bool isKey(std::uint16_t code) { return code >= 0x08 && code <= 0xFE; }
constexpr bool isMouseButton(std::uint16_t code) { return code >= kMouseLeft && code <= kMouseX2; }
constexpr bool isWheel(std::uint16_t code) { return code == kWheelUp || code == kWheelDown; }
constexpr bool validCode(std::uint16_t code) { return code == kNone || isKey(code) || isMouseButton(code) || isWheel(code); }
constexpr bool isFunctionKey(std::uint16_t code) { return code >= vk::F1 && code <= vk::F24; }

// ---- what can be pressed on the controller -----------------------------------------------------------
enum class Control : std::uint8_t {
    Cross, Circle, Square, Triangle, Up, Down, Left, Right, L1, R1, L2, R2, L3, R3,
    Options,   // Options on a PS5, Menu on an Xbox
    View,      // the touchpad's press on a PS5, View on an Xbox
    Create,    // Create on a PS5 (on an Xbox it is View as well)
    Home,      // the PS button, the Xbox button
    MoveUp, MoveDown, MoveLeft, MoveRight,
    Walk,      // held: the movement keys push the stick half way
    Count
};
constexpr std::size_t kControls = std::size_t(Control::Count);
constexpr std::size_t kSlots = 2;   // inputs one control can have

// Names in the saved settings. Never change one that has shipped.
constexpr std::array<std::string_view, kControls> kControlIds{
    "cross", "circle", "square", "triangle", "up", "down", "left", "right", "l1", "r1", "l2", "r2", "l3", "r3",
    "options", "view", "create", "home", "moveUp", "moveDown", "moveLeft", "moveRight", "walk"};

using Map = std::array<std::array<std::uint16_t, kSlots>, kControls>;

enum class Console : std::uint8_t { Ps5, Xbox };

// ---- presets ------------------------------------------------------------------------------------------
// The keyboard is laid out as Rockstar's PC games lay it out (GTA V, Red Dead Redemption 2: WASD, Shift to
// run, Space to jump, F / E to interact, Q for cover, Tab for the weapons, C to look behind, the mouse to aim
// and shoot), and each key presses the button that does the same thing in the console layout named.
//   Gta6        GTA 6's Default layout: run on L3 (press again to sprint), jump Cross, crouch Circle,
//               melee Square, interact Triangle, cover R1, holster / loadout L1, map on the touchpad.
//   Gta6Classic GTA 6's Classic layout, which is GTA V's: run Cross, jump Square, Circle reload / melee,
//               crouch / stealth L3.
//   Rdr2        Red Dead Redemption 2: run Cross, jump Square, reload Circle, interact / mount Triangle,
//               cover R1, stealth L3, weapon wheel L1, dead eye R3, camera on the touchpad, map on Options
//               (held), whistle Up, journal Left, satchel Right.
enum class Preset : std::uint8_t { Gta6, Gta6Classic, Rdr2, Count };
constexpr std::array<std::string_view, std::size_t(Preset::Count)> kPresetIds{"gta6", "gta6classic", "rdr2"};

inline Map presetMap(Preset preset) {
    Map m{};
    const auto set = [&m](Control c, std::uint16_t a, std::uint16_t b = kNone) { m[std::size_t(c)] = {a, b}; };
    using vk::letter;
    // Shared by all three.
    set(Control::MoveUp, letter('W'));
    set(Control::MoveDown, letter('S'));
    set(Control::MoveLeft, letter('A'));
    set(Control::MoveRight, letter('D'));
    set(Control::Walk, vk::LMenu);
    set(Control::R2, kMouseLeft);
    set(Control::L2, kMouseRight);
    set(Control::R1, letter('Q'));
    set(Control::L1, vk::Tab);
    set(Control::R3, letter('C'), kMouseMiddle);
    switch (preset) {
    case Preset::Gta6:
        set(Control::L3, vk::LShift);
        set(Control::Cross, vk::Space);
        set(Control::Circle, vk::LControl);
        set(Control::Square, letter('R'));
        set(Control::Triangle, letter('F'), letter('E'));
        set(Control::View, letter('M'));
        set(Control::Options, vk::Escape, letter('P'));
        set(Control::Up, vk::Up);
        set(Control::Down, vk::Down);
        set(Control::Left, vk::Left);
        set(Control::Right, vk::Right);
        break;
    case Preset::Gta6Classic:
        set(Control::Cross, vk::LShift);
        set(Control::Square, vk::Space);
        set(Control::Circle, letter('R'));
        set(Control::L3, vk::LControl);
        set(Control::Triangle, letter('F'), letter('E'));
        set(Control::View, letter('M'));
        set(Control::Options, vk::Escape, letter('P'));
        set(Control::Up, vk::Up);
        set(Control::Down, vk::Down);
        set(Control::Left, vk::Left);
        set(Control::Right, vk::Right);
        break;
    case Preset::Rdr2:
    case Preset::Count:
        set(Control::Cross, vk::LShift);
        set(Control::Square, vk::Space);
        set(Control::Circle, letter('R'), letter('F'));
        set(Control::Triangle, letter('E'));
        set(Control::L3, vk::LControl);
        set(Control::View, letter('V'));
        set(Control::Options, vk::Escape, letter('M'));
        set(Control::Up, letter('H'), vk::Up);
        set(Control::Down, vk::Down);
        set(Control::Left, letter('J'), vk::Left);
        set(Control::Right, letter('B'), vk::Right);
        break;
    }
    return m;
}

inline int presetIndex(std::string_view id) {
    for (std::size_t i = 0; i < kPresetIds.size(); ++i)
        if (kPresetIds[i] == id) return int(i);
    return -1;
}

// ---- settings -----------------------------------------------------------------------------------------
struct Settings {
    bool on = false;                   // off unless switched on
    Map map = presetMap(Preset::Gta6);
    int sensitivity = 50;              // 1..100: how fast the mouse has to move for the stick's full push
    int verticalPercent = 100;         // 25..400: the up and down movement's share of that
    int deadzonePercent = 15;          // 0..50: where the stick starts once the mouse moves at all
    int curvePercent = 100;            // 50..200: the response's power (100 = straight)
    bool invertY = false;
    std::uint16_t release = vk::F12;   // starts and ends the capture; a key, never bound to a control
};
constexpr int kSensitivityMin = 1, kSensitivityMax = 100;
constexpr int kVerticalMin = 25, kVerticalMax = 400;
constexpr int kDeadzoneMin = 0, kDeadzoneMax = 50;
constexpr int kCurveMin = 50, kCurveMax = 200;

// Everything back inside its range; an input that is not a valid code becomes nothing, the release key is
// a key (F12 otherwise) and is taken out of the controls it was bound to.
inline Settings clean(Settings s) {
    s.sensitivity = std::clamp(s.sensitivity, kSensitivityMin, kSensitivityMax);
    s.verticalPercent = std::clamp(s.verticalPercent, kVerticalMin, kVerticalMax);
    s.deadzonePercent = std::clamp(s.deadzonePercent, kDeadzoneMin, kDeadzoneMax);
    s.curvePercent = std::clamp(s.curvePercent, kCurveMin, kCurveMax);
    if (!isKey(s.release)) s.release = vk::F12;
    for (auto& inputs : s.map)
        for (auto& code : inputs)
            if (!validCode(code) || code == s.release) code = kNone;
    return s;
}

// Counts a second of mouse movement for the stick's full push at this sensitivity (50: 5000 counts a
// second, about 16 cm a second with an 800 DPI mouse).
constexpr double fullSpeed(int sensitivity) { return 250000.0 / double(std::clamp(sensitivity, kSensitivityMin, kSensitivityMax)); }

// ---- the mouse as a stick -----------------------------------------------------------------------------
// The stick follows the mouse's speed: the movement of the last `window()` microseconds divided by that
// time. The window is two and a half report intervals of the mouse, at least 8 ms (a 1000 Hz mouse) and at
// most 30 ms, so a slow mouse does not flicker between pushed and centred; once the mouse stops, the stick
// is back in the middle after one window. Speed -> push: the share of the full speed, to the curve's power,
// starting at the dead zone offset (games ignore a stick pushed less than their own dead zone).
class MouseStick {
public:
    struct Push { double x = 0, y = 0; };
    static constexpr std::int64_t kMinWindowUs = 8000, kMaxWindowUs = 30000;

    void move(int dx, int dy, std::int64_t nowUs) {
        if (lastUs_ >= 0) {
            const std::int64_t gap = nowUs - lastUs_;
            if (gap > 0 && gap < 50000) {
                intervalUs_ = haveInterval_ ? intervalUs_ * 0.8 + double(gap) * 0.2 : double(gap);
                haveInterval_ = true;
            }
        }
        lastUs_ = nowUs;
        samples_.push_back({nowUs, dx, dy});
        while (samples_.size() > 512) samples_.pop_front();
    }
    // The mouse stops counting; how often it reports is kept.
    void clear() { samples_.clear(); lastUs_ = -1; }
    std::int64_t window() const {
        return std::clamp(std::int64_t(std::llround(intervalUs_ * 2.5)), kMinWindowUs, kMaxWindowUs);
    }
    // Whether the mouse moved within the window (the right stick is the mouse's then).
    bool moving(std::int64_t nowUs) { prune(nowUs); return !samples_.empty(); }

    Push push(std::int64_t nowUs, const Settings& s) {
        prune(nowUs);
        if (samples_.empty()) return {};
        double sx = 0, sy = 0;
        for (const auto& sample : samples_) { sx += sample.dx; sy += sample.dy; }
        const double seconds = double(window()) / 1e6;
        const double vx = sx / seconds;
        const double vy = sy / seconds * double(s.verticalPercent) / 100.0 * (s.invertY ? -1.0 : 1.0);
        const double speed = std::hypot(vx, vy);
        if (!(speed > 0)) return {};
        const double share = std::min(1.0, speed / fullSpeed(s.sensitivity));
        const double dz = double(s.deadzonePercent) / 100.0;
        const double shaped = dz + (1.0 - dz) * std::pow(share, double(s.curvePercent) / 100.0);
        return {vx / speed * shaped, vy / speed * shaped};
    }

private:
    struct Sample { std::int64_t t; int dx, dy; };
    void prune(std::int64_t nowUs) {
        const std::int64_t from = nowUs - window();
        while (!samples_.empty() && (samples_.front().t <= from || samples_.front().t > nowUs)) samples_.pop_front();
    }
    std::deque<Sample> samples_;
    std::int64_t lastUs_ = -1;
    double intervalUs_ = 1000.0;   // a 1000 Hz mouse until the reports say otherwise
    bool haveInterval_ = false;
};

inline std::int16_t axis(double v) {
    return std::int16_t(std::lround(std::clamp(v, -1.0, 1.0) * 32767.0));
}

// ---- keys, buttons and the wheel ---------------------------------------------------------------------
class Mixer {
public:
    // One notch of the wheel is a press of 40 ms; further notches follow after a gap of 30 ms (at most 4
    // waiting), so a game sees every one of them.
    static constexpr std::int64_t kPulseUs = 40000, kGapUs = 30000;
    static constexpr int kMaxPendingNotches = 4;

    void configure(const Settings& s) { settings_ = clean(s); }
    const Settings& settings() const { return settings_; }

    // Whether this code presses something.
    bool bound(std::uint16_t code) const {
        if (code == kNone) return false;
        for (const auto& inputs : settings_.map)
            for (auto c : inputs) if (c == code) return true;
        return false;
    }
    void press(std::uint16_t code, bool down) {
        if (code == kNone || code >= kCodes || isWheel(code)) return;
        held_[code] = down;
    }
    void wheel(int notches) {
        if (notches == 0) return;
        auto& w = wheel_[notches > 0 ? 0 : 1];
        w.pending = std::min(kMaxPendingNotches, w.pending + std::abs(notches));
    }
    void move(int dx, int dy, std::int64_t nowUs) { mouse_.move(dx, dy, nowUs); }
    // Everything up and the mouse still (the capture ended).
    void clear() {
        held_.fill(false);
        for (auto& w : wheel_) w = {};
        mouse_.clear();
    }
    bool anyHeld() const {
        for (bool h : held_) if (h) return true;
        for (const auto& w : wheel_) if (w.pressedAt >= 0 || w.pending > 0) return true;
        return false;
    }

    // The controller state to send: the real controller's with the keyboard and mouse added.
    ControllerState apply(const ControllerState& pad, std::int64_t nowUs, Console console) {
        advanceWheel(nowUs);
        ControllerState out = pad;
        out.inputActive = true;
        if (!pad.inputActive) {
            // No controller (or not read now): nothing of it may come through, only what the keys press.
            out = ControllerState{};
            out.inputActive = true;
        }
        using B = ControllerState;
        struct Pair { Control control; std::uint32_t bit; };
        const Pair buttons[] = {
            {Control::Cross, B::Cross}, {Control::Circle, B::Circle}, {Control::Square, B::Square}, {Control::Triangle, B::Triangle},
            {Control::Up, B::Up}, {Control::Down, B::Down}, {Control::Left, B::Left}, {Control::Right, B::Right},
            {Control::L1, B::L1}, {Control::R1, B::R1}, {Control::L3, B::L3}, {Control::R3, B::R3},
            {Control::Options, B::Options}, {Control::Create, B::Share}, {Control::Home, B::PS},
            {Control::View, console == Console::Xbox ? std::uint32_t(B::Share) : std::uint32_t(B::Touchpad)}};
        for (const auto& b : buttons) if (down(b.control)) out.buttons |= b.bit;
        if (down(Control::L2)) out.l2 = 255;
        if (down(Control::R2)) out.r2 = 255;

        // Left stick: the movement keys, unless the controller's stick is pushed further.
        double kx = double(down(Control::MoveRight)) - double(down(Control::MoveLeft));
        double ky = double(down(Control::MoveDown)) - double(down(Control::MoveUp));
        if (kx != 0 && ky != 0) { kx *= 0.70710678118654752; ky *= 0.70710678118654752; }
        if (down(Control::Walk)) { kx *= 0.5; ky *= 0.5; }
        if (kx != 0 || ky != 0) {
            const double keys = std::hypot(kx, ky);
            const double stick = std::hypot(double(out.leftX) / 32767.0, double(out.leftY) / 32767.0);
            if (keys >= stick) { out.leftX = axis(kx); out.leftY = axis(ky); }
        }
        // Right stick: the mouse while it moves, the controller otherwise.
        if (mouse_.moving(nowUs)) {
            const auto p = mouse_.push(nowUs, settings_);
            out.rightX = axis(p.x);
            out.rightY = axis(p.y);
        }
        return out;
    }

    // Whether the control is pressed now (by any of its inputs).
    bool down(Control c) const {
        for (auto code : settings_.map[std::size_t(c)]) {
            if (code == kNone) continue;
            if (isWheel(code)) { if (wheel_[code == kWheelUp ? 0 : 1].pressedAt >= 0) return true; }
            else if (code < kCodes && held_[code]) return true;
        }
        return false;
    }

private:
    struct Wheel { int pending = 0; std::int64_t pressedAt = -1, nextAt = 0; };
    void advanceWheel(std::int64_t nowUs) {
        for (auto& w : wheel_) {
            if (w.pressedAt >= 0 && nowUs - w.pressedAt >= kPulseUs) { w.pressedAt = -1; w.nextAt = nowUs + kGapUs; }
            if (w.pressedAt < 0 && w.pending > 0 && nowUs >= w.nextAt) { w.pressedAt = nowUs; --w.pending; }
        }
    }
    Settings settings_{};
    std::array<bool, kCodes> held_{};
    std::array<Wheel, 2> wheel_{};
    MouseStick mouse_;
};

} // namespace veyra::remoteplay::kbm
