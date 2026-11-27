// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// Mouse and keyboard as a controller (remoteplay/PushKbm.h), with synthesized input. Also builds with g++.
#include "veyra/remoteplay/PushKbm.h"

#include <cstdio>
#include <set>

using namespace veyra::remoteplay;
using namespace veyra::remoteplay::kbm;
static int failures = 0;
static void check(bool ok, const char* label) { std::printf("%s %s\n", ok ? "PASS" : "FAIL", label); if (!ok) ++failures; }

static Settings with(Preset p) { Settings s; s.on = true; s.map = presetMap(p); return clean(s); }
static ControllerState pad() { ControllerState s; s.inputActive = true; return s; }
static bool has(const ControllerState& s, std::uint32_t bit) { return (s.buttons & bit) != 0; }

int main() {
    // ---- presets ----
    for (std::size_t p = 0; p < std::size_t(Preset::Count); ++p) {
        const Map m = presetMap(Preset(p));
        std::set<std::uint16_t> seen; bool unique = true, valid = true;
        for (const auto& inputs : m) for (auto code : inputs) {
            if (code == kNone) continue;
            if (!validCode(code)) valid = false;
            if (!seen.insert(code).second) unique = false;
        }
        check(valid, "every input of a preset is a valid code");
        check(unique, "no input presses two controls in a preset");
        check(seen.count(vk::F12) == 0, "no preset uses the release key");
        check(m[std::size_t(Control::MoveUp)][0] == 'W' && m[std::size_t(Control::MoveLeft)][0] == 'A' &&
              m[std::size_t(Control::MoveDown)][0] == 'S' && m[std::size_t(Control::MoveRight)][0] == 'D', "WASD moves in every preset");
        check(m[std::size_t(Control::R2)][0] == kMouseLeft && m[std::size_t(Control::L2)][0] == kMouseRight, "left button shoots, right button aims");
        check(m[std::size_t(Control::Home)][0] == kNone && m[std::size_t(Control::Create)][0] == kNone, "the PS / Xbox button and Create are never bound by a preset");
        check(presetIndex(kPresetIds[p]) == int(p), "a preset is found by its id");
    }
    check(presetIndex("nope") == -1, "an unknown preset id is none");
    {
        const Map gta6 = presetMap(Preset::Gta6), classic = presetMap(Preset::Gta6Classic), rdr2 = presetMap(Preset::Rdr2);
        check(gta6[std::size_t(Control::L3)][0] == vk::LShift && gta6[std::size_t(Control::Cross)][0] == vk::Space, "GTA 6: Shift runs on L3, Space jumps on Cross");
        check(gta6[std::size_t(Control::Circle)][0] == vk::LControl && gta6[std::size_t(Control::Square)][0] == 'R', "GTA 6: Ctrl crouches on Circle, R is melee on Square");
        check(gta6[std::size_t(Control::View)][0] == 'M', "GTA 6: M opens the map on the touchpad");
        check(classic[std::size_t(Control::Cross)][0] == vk::LShift && classic[std::size_t(Control::Square)][0] == vk::Space, "Classic / GTA V: Shift runs on Cross, Space jumps on Square");
        check(classic[std::size_t(Control::L3)][0] == vk::LControl && classic[std::size_t(Control::Circle)][0] == 'R', "Classic / GTA V: Ctrl is stealth on L3, R on Circle");
        check(rdr2[std::size_t(Control::Triangle)][0] == 'E' && rdr2[std::size_t(Control::Up)][0] == 'H', "RDR2: E interacts on Triangle, H whistles on Up");
    }

    // ---- clean ----
    {
        Settings s; s.sensitivity = 0; s.verticalPercent = 9999; s.deadzonePercent = -5; s.curvePercent = 10; s.release = kMouseLeft;
        s.map[0] = {0x05 /* VK_XBUTTON1: not a code here */, 0x200};
        const Settings c = clean(s);
        check(c.sensitivity == kSensitivityMin && c.verticalPercent == kVerticalMax && c.deadzonePercent == kDeadzoneMin && c.curvePercent == kCurveMin, "clean puts every number back in its range");
        check(c.release == vk::F12, "a release that is not a key becomes F12");
        check(c.map[0][0] == kNone && c.map[0][1] == kNone, "codes that are not inputs are dropped");
        Settings b; b.release = vk::Space;   // Space jumps in GTA 6
        check(clean(b).map[std::size_t(Control::Cross)][0] == kNone, "the release key is taken out of the control it was bound to");
    }

    // ---- keys ----
    {
        Mixer m; m.configure(with(Preset::Gta6));
        check(m.bound(vk::Space) && m.bound(kMouseLeft) && !m.bound(vk::F12) && !m.bound('Z'), "bound says which inputs press something");
        auto out = m.apply(pad(), 0, Console::Ps5);
        check(out.inputActive && out.buttons == 0 && out.leftX == 0 && out.rightX == 0, "nothing held: a still controller");
        m.press(vk::Space, true);
        out = m.apply(pad(), 1000, Console::Ps5);
        check(has(out, ControllerState::Cross), "Space presses Cross in GTA 6");
        m.press(vk::Space, false);
        out = m.apply(pad(), 2000, Console::Ps5);
        check(!has(out, ControllerState::Cross), "and lets go of it");
        m.press(kMouseLeft, true); m.press(kMouseRight, true);
        out = m.apply(pad(), 3000, Console::Ps5);
        check(out.r2 == 255 && out.l2 == 255, "mouse buttons pull the triggers all the way");
        m.press('M', true);
        check(has(m.apply(pad(), 4000, Console::Ps5), ControllerState::Touchpad), "View is the touchpad's press on a PS5");
        check(has(m.apply(pad(), 4000, Console::Xbox), ControllerState::Share) && !has(m.apply(pad(), 4000, Console::Xbox), ControllerState::Touchpad), "and View on an Xbox");
        m.clear();
        out = m.apply(pad(), 5000, Console::Ps5);
        check(out.buttons == 0 && out.l2 == 0 && out.r2 == 0 && !m.anyHeld(), "clear lets go of everything");
        Mixer two; two.configure(with(Preset::Gta6));
        two.press('F', true); two.press('E', true); two.press('F', false);
        check(has(two.apply(pad(), 0, Console::Ps5), ControllerState::Triangle), "a control stays pressed while its second input is held");
    }

    // ---- movement keys ----
    {
        Mixer m; m.configure(with(Preset::Gta6));
        m.press('W', true);
        auto out = m.apply(pad(), 0, Console::Ps5);
        check(out.leftY == -32767 && out.leftX == 0, "W pushes the left stick all the way up");
        m.press('D', true);
        out = m.apply(pad(), 0, Console::Ps5);
        check(out.leftX > 23000 && out.leftX < 23300 && out.leftY < -23000 && out.leftY > -23300, "W and D push it up and right on the circle, not to the corner");
        m.press(vk::LMenu, true);
        out = m.apply(pad(), 0, Console::Ps5);
        check(out.leftX > 11500 && out.leftX < 11700, "walk pushes it half way");
        m.press('D', false); m.press(vk::LMenu, false); m.press('S', true);
        out = m.apply(pad(), 0, Console::Ps5);
        check(out.leftX == 0 && out.leftY == 0, "W and S together leave it in the middle");
        ControllerState p = pad(); p.leftX = 20000; p.leftY = 20000; m.press('S', false);
        out = m.apply(p, 0, Console::Ps5);
        check(out.leftY == -32767 && out.leftX == 0, "the keys win over a controller stick pushed less far");
        p.leftX = 32767; p.leftY = 32767;
        m.press(vk::LMenu, true);
        out = m.apply(p, 0, Console::Ps5);
        check(out.leftX == 32767 && out.leftY == 32767, "a controller stick pushed further wins over the keys");
    }

    // ---- the mouse ----
    {
        Settings s = with(Preset::Gta6); s.deadzonePercent = 0; s.sensitivity = 50;   // full push at 5000 counts a second
        Mixer m; m.configure(s);
        std::int64_t t = 0;
        // 1000 Hz, 3 counts a report to the right: 3000 counts a second = 60 % of the full speed.
        for (int i = 0; i < 40; ++i) { t += 1000; m.move(3, 0, t); }
        auto out = m.apply(pad(), t, Console::Ps5);
        check(out.rightX > 0.58 * 32767 && out.rightX < 0.62 * 32767 && out.rightY == 0, "the right stick follows the mouse's speed");
        out = m.apply(pad(), t + 9000, Console::Ps5);
        check(out.rightX == 0 && out.rightY == 0, "and is back in the middle one window after the mouse stopped");
        ControllerState p = pad(); p.rightX = -20000;
        check(m.apply(p, t + 9000, Console::Ps5).rightX == -20000, "with the mouse still, the controller's right stick comes through");
        for (int i = 0; i < 20; ++i) { t += 1000; m.move(0, 50, t); }
        out = m.apply(p, t, Console::Ps5);
        check(out.rightY == 32767 && out.rightX == 0, "a fast move down pushes the stick all the way down, over the controller's");
    }
    {
        Settings s = with(Preset::Gta6); s.deadzonePercent = 20; s.invertY = true; s.verticalPercent = 50;
        Mixer m; m.configure(s);
        m.move(0, 1, 1000);
        auto out = m.apply(pad(), 1000, Console::Ps5);
        check(out.rightY < 0 && out.rightY <= -int(0.20 * 32767) + 2, "inverted, a move down looks up, and starts at the dead zone offset");
        MouseStick slow; Settings plain = with(Preset::Gta6);
        std::int64_t t = 0;
        for (int i = 0; i < 20; ++i) { t += 8000; slow.move(10, 0, t); }   // a 125 Hz mouse
        check(slow.window() >= 19000 && slow.window() <= 21000, "a 125 Hz mouse gets a window of about 20 ms");
        bool flicker = false;
        for (std::int64_t at = t; at < t + 7900; at += 500) if (slow.push(at, plain).x == 0) flicker = true;
        check(!flicker, "and its stick does not drop to the middle between reports");
        MouseStick fast;
        for (int i = 0; i < 50; ++i) fast.move(1, 0, 1000 * (i + 1));
        check(fast.window() == MouseStick::kMinWindowUs, "a 1000 Hz mouse gets the shortest window");
    }
    {
        Settings a = with(Preset::Gta6); a.deadzonePercent = 0; a.curvePercent = 200;
        Settings b = a; b.curvePercent = 100;
        MouseStick ma, mb;
        for (int i = 1; i <= 10; ++i) { ma.move(2, 0, i * 1000); mb.move(2, 0, i * 1000); }
        check(ma.push(10000, a).x < mb.push(10000, b).x, "a curve above 100 makes small moves gentler");
    }

    // ---- the wheel ----
    {
        Settings s = with(Preset::Gta6); s.map[std::size_t(Control::Right)] = {kWheelUp, kNone};
        Mixer m; m.configure(s);
        m.wheel(2);
        int presses = 0; bool was = false;
        for (std::int64_t t = 0; t < 400000; t += 4000) {
            const bool now = has(m.apply(pad(), t, Console::Ps5), ControllerState::Right);
            if (now && !was) ++presses;
            was = now;
        }
        check(presses == 2 && !was, "two notches are two separate presses, then nothing is held");
        Mixer w; w.configure(s); w.wheel(-3);
        check(!has(w.apply(pad(), 0, Console::Ps5), ControllerState::Right), "a notch the other way presses nothing that is not bound to it");
    }

    // ---- the merge with the controller ----
    {
        Mixer m; m.configure(with(Preset::Gta6));
        ControllerState p = pad(); p.buttons = ControllerState::Circle; p.l2 = 100; p.r2 = 30; p.motionValid = true; p.gyro = {1, 2, 3};
        m.press(kMouseLeft, true);
        auto out = m.apply(p, 0, Console::Ps5);
        check(has(out, ControllerState::Circle) && out.r2 == 255 && out.l2 == 100, "buttons from either, the trigger pressed further");
        check(out.motionValid && out.gyro[1] == 2, "the controller's motion comes through");
        ControllerState none;   // no controller, or PushFPS not in front
        none.buttons = ControllerState::Cross;
        out = m.apply(none, 0, Console::Ps5);
        check(out.inputActive && !has(out, ControllerState::Cross) && out.r2 == 255, "without a controller only the keys and mouse count");
    }

    std::printf("%s (%d failed)\n", failures ? "FAILED" : "ALL PASSED", failures);
    return failures ? 1 : 0;
}
