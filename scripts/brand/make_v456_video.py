#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#
# The 4.5.6 video: what is new since the 4.5.0 film. The same machinery (make_release_video.py, the 4.5.0
# film's fact cards) and the same beats, so the same music fits. Same commands:
#
#   python3 scripts/brand/make_v456_video.py --frame 12.5 frame.png
#   python3 scripts/brand/make_v456_video.py --parts out/v456 [n ...]
#   python3 scripts/brand/make_v456_video.py --join out/v456 film.mp4 --audio music.m4a
#
# The screenshots are not in the repository. They are the V-* entries of the screenshots workflow on the
# scratch branch video/v456 (1600 x 900), into out/v456-shots (or the folder PUSHFPS_VIDEO_SHOTS names):
#   V-01-kbm (the mouse and keyboard window), V-03-ps5 (the PS5 window, scrolled to PC microphone).
# Nothing here needs a console on the build machine; what would (keys pressing a controller's buttons) is
# drawn and says that it is an illustration.
import math, os, sys
from functools import lru_cache
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_release_video as V
import make_v45_video as V45
from make_release_video import (FPS, LIME, T1, T2, T3, EDGE, beat, at, clamp, out_cubic, in_out_cubic,
                                put, window, focus, highlight, caption, scrim, chip, text_sprite, font,
                                rim_sprite, cam_base, cam_region, cam_mix)
from make_tour_video import panel, heading
from make_v45_video import facts

V.SHOTS = os.environ.get("PUSHFPS_VIDEO_SHOTS") or os.path.join(V.ROOT, "out", "v456-shots")
V.shot = V45._shot           # these shots show no home page, so nothing to cover
V.INTRO_TAG = [("4.5.6", LIME), ("   what's new", T1)]
V.ALSO_HEAD = [("Also new ", T1), ("since 4.5", LIME)]
V.OUTRO_TAG = [("4.5.6", LIME), ("  is out", T1)]
V.ALSO = [("Lighter start", "Windows you haven't opened aren't made at start"),
          ("Lowest latency", "Goes full screen at once while a picture plays"),
          ("Frame quality test", "Works with capture cards"),
          ("Virtual camera", "Installing its component no longer freezes PushFPS"),
          ("Capture cards", "Magewell Pro Capture 4K Plus LT and Monster reported working"),
          ("Linux and macOS", "Profiles, upscaling, crop, Check my setup")]

# Rectangles in a 1600 x 900 screenshot (x, y, w, h), measured on the V-* pictures (set_rects() reads them
# from the pictures' own rows when the shots are taken again; these are the values used for the film).
KBM_PRESET = (560, 250, 480, 52)
KBM_KEYS = (560, 380, 480, 400)
PS5_MIC = (560, 420, 480, 56)


# ---------------------------------------------------------------- drawn pictures
KEY = 62        # one key's size on the drawn keyboard


@lru_cache(maxsize=None)
def key_sprite(label, w, lit):
    im = Image.new("RGBA", (w, KEY), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((0, 0, w - 1, KEY - 1), radius=12, fill=(LIME if lit else (28, 35, 44)) + (255,),
                        outline=(LIME if lit else EDGE) + (255,), width=2)
    f = font("semi", 26 if len(label) > 2 else 30)
    tw = f.getlength(label)
    d.text(((w - tw) / 2, 13), label, font=f, fill=((13, 26, 4) if lit else T1) + (255,))
    return im


@lru_cache(maxsize=None)
def mouse_sprite(lit_left, lit_right, w=150, h=220):
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((0, 0, w - 1, h - 1), radius=70, fill=(28, 35, 44, 255), outline=EDGE + (255,), width=2)
    # The lit buttons, cut to the inside of the body.
    inside = Image.new("L", (w, h), 0)
    ImageDraw.Draw(inside).rounded_rectangle((3, 3, w - 4, h - 4), radius=67, fill=255)
    lit = Image.new("L", (w, h), 0)
    dl = ImageDraw.Draw(lit)
    if lit_left:
        dl.rectangle((0, 0, w // 2 - 2, 96), fill=255)
    if lit_right:
        dl.rectangle((w // 2 + 2, 0, w, 96), fill=255)
    from PIL import ImageChops
    im.paste(Image.new("RGBA", (w, h), LIME + (255,)), (0, 0), ImageChops.multiply(lit, inside))
    d.line((w // 2, 4, w // 2, 96), fill=EDGE + (255,), width=3)
    d.line((4, 98, w - 5, 98), fill=EDGE + (255,), width=2)
    d.rounded_rectangle((w // 2 - 9, 30, w // 2 + 9, 70), radius=9, fill=(60, 72, 88, 255))
    return im


def controller_sprite(lit, stick_l, stick_r, w=640, h=380):
    """A controller drawn plainly, no console maker's shape: sticks, face buttons, shoulders, d-pad."""
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    body = (28, 35, 44, 255)
    d.rounded_rectangle((40, 70, w - 41, h - 61), radius=110, fill=body, outline=EDGE + (255,), width=2)
    for x0, name in ((70, "L2"), (w - 230, "R2")):
        d.rounded_rectangle((x0, 4, x0 + 160, 30), radius=12, fill=(LIME if name in lit else (34, 42, 52)) + (255,), outline=EDGE + (255,), width=2)
    for x0, name in ((70, "L1"), (w - 230, "R1")):
        d.rounded_rectangle((x0, 38, x0 + 160, 62), radius=12, fill=(LIME if name in lit else (34, 42, 52)) + (255,), outline=EDGE + (255,), width=2)
    f = font("semi", 22)
    for x0, name in ((70, "L2"), (w - 230, "R2"), (70, "L1"), (w - 230, "R1")):
        y0 = 4 if name.endswith("2") else 38
        d.text((x0 + 80 - f.getlength(name) / 2, y0 + 1), name, font=f, fill=((13, 26, 4) if name in lit else T2) + (255,))
    # d-pad
    cx, cy = 150, 170
    for dx, dy in ((0, -34), (0, 34), (-34, 0), (34, 0)):
        d.rounded_rectangle((cx + dx - 16, cy + dy - 16, cx + dx + 16, cy + dy + 16), radius=6, fill=(40, 49, 60, 255))
    # face buttons
    fx, fy = w - 150, 170
    faces = {"Triangle": (0, -40), "Cross": (0, 40), "Square": (-40, 0), "Circle": (40, 0)}
    for name, (dx, dy) in faces.items():
        on = name in lit
        d.ellipse((fx + dx - 20, fy + dy - 20, fx + dx + 20, fy + dy + 20), fill=(LIME if on else (40, 49, 60)) + (255,),
                  outline=EDGE + (255,), width=2)
    # sticks: the well, then the cap moved by (stick_x, stick_y) in -1..1
    for (sx, sy), (vx, vy), name in (((240, 250), stick_l, "L"), ((w - 240, 250), stick_r, "R")):
        d.ellipse((sx - 46, sy - 46, sx + 46, sy + 46), fill=(18, 23, 29, 255), outline=EDGE + (255,), width=2)
        on = abs(vx) + abs(vy) > 0.05
        px, py = sx + vx * 22, sy + vy * 22
        d.ellipse((px - 30, py - 30, px + 30, py + 30), fill=(LIME if on else (52, 62, 76)) + (255,))
    return im


# What the keys press, in order: (beat, keys lit, mouse left/right, controller lit, left stick, right stick, label)
DEMO = [
    (8.6, ("W",), (False, False), (), (0, -1), (0, 0), "W  moves you forward"),
    (10.0, ("Shift",), (False, False), ("L3",), (0, -1), (0, 0), "Shift  runs"),
    (11.4, ("Space",), (False, False), ("Cross",), (0, 0), (0, 0), "Space  jumps"),
    (12.8, (), (False, True), ("L2",), (0, 0), (0.6, -0.2), "Right mouse button  aims"),
    (14.2, (), (True, True), ("L2", "R2"), (0, 0), (-0.5, 0.1), "Left mouse button  shoots"),
]


def demo_state(t):
    current = None
    for row in DEMO:
        if t >= beat(row[0]):
            current = row
    return current


def scene_kbm_demo(canvas, t):
    t0, t1 = beat(8), beat(16)
    enter = out_cubic(at(t, t0, t0 + 0.3))
    a = enter * (1 - at(t, t1 - 0.2, t1))
    heading(canvas, [("Mouse and keyboard ", T1), ("on PS5 and Xbox", LIME)], 100, enter, a)
    state = demo_state(t)
    keys_lit = state[1] if state else ()
    mouse_lit = state[2] if state else (False, False)
    pad_lit = state[3] if state else ()
    stick_l = state[4] if state else (0, 0)
    stick_r = state[5] if state else (0, 0)
    if stick_r != (0, 0):
        wob = math.sin((t - t0) * 7.0) * 0.25          # the mouse keeps moving the camera
        stick_r = (clamp(stick_r[0] + wob, -1, 1), stick_r[1])
    # Left panel: keys and the mouse.
    pw, ph = 800, 460
    k = out_cubic(at(t, t0 + 0.12, t0 + 0.5))
    x, y = 140, 250 + (1 - k) * 30
    bg = Image.new("RGB", (pw, ph), (16, 21, 27))
    panel(canvas, bg, x, y, a * k, lime=False)
    rows = [[("Tab", 92), ("Q", KEY), ("W", KEY), ("E", KEY)],
            [("A", KEY), ("S", KEY), ("D", KEY), ("F", KEY)],
            [("Shift", 132), ("C", KEY)],
            [("Space", 300)]]
    offsets = [40, 76, 40, 130]
    for r, keys in enumerate(rows):
        kx = x + offsets[r]
        for label, kw in keys:
            put(canvas, key_sprite(label, kw, label in keys_lit), kx, y + 78 + r * (KEY + 14), a * k)
            kx += kw + 12
    put(canvas, mouse_sprite(*mouse_lit), x + pw - 210, y + 120, a * k)
    put(canvas, chip("Keyboard and mouse", False), x + 22, y + 20 - 70, a * k)
    # Right panel: the controller the console sees.
    k2 = out_cubic(at(t, t0 + 0.28, t0 + 0.66))
    x2, y2 = 980, 250 + (1 - k2) * 30
    bg2 = Image.new("RGB", (pw, ph), (16, 21, 27))
    panel(canvas, bg2, x2, y2, a * k2, lime=True)
    put(canvas, controller_sprite(pad_lit, stick_l, stick_r), x2 + 80, y2 + 50, a * k2)
    put(canvas, chip("What the console sees", True), x2 + 22, y2 + 20 - 70, a * k2)
    if state:
        lab = text_sprite([(state[6], T1)], "semi", 40)
        fade = out_cubic(at(t, beat(state[0]), beat(state[0]) + 0.25))
        put(canvas, lab, 960 - lab.width / 2, 722 + (1 - fade) * 12, a * fade)
    caption(canvas, t, t0 + 0.3, t1, "The console sees a controller", "Your controller keeps working beside them. Illustration", y=806)


def scene_kbm_window(canvas, t):
    b0, b1 = 16, 24
    rx, ry, rw, rh = KBM_PRESET
    cam = cam_mix(cam_base(), cam_region(rx - 40, ry - 80, rw + 80, 560, max_s=1.9),
                  in_out_cubic(at(t, beat(17.2), beat(17.2) + 0.7)))
    enter = out_cubic(at(t, beat(b0), beat(b0) + 0.3))
    leave = at(t, beat(b1) - 0.18, beat(b1))
    window(canvas, "V-01-kbm", cam, alpha=enter * (1 - leave), shadow=clamp(1.7 - cam[0]))
    lit1 = at(t, beat(18), beat(18) + 0.3) * (1 - at(t, beat(20.4), beat(20.4) + 0.3))
    focus(canvas, cam, KBM_PRESET, lit1, radius=8, grow=6, blur=5.0, dim=0.36)
    highlight(canvas, cam, KBM_PRESET, lit1 * 0.9, radius=8, grow=6)
    lit2 = at(t, beat(20.6), beat(20.6) + 0.3) * (1 - at(t, beat(b1) - 0.2, beat(b1)))
    focus(canvas, cam, KBM_KEYS, lit2, radius=8, grow=6, blur=5.0, dim=0.36)
    highlight(canvas, cam, KBM_KEYS, lit2 * 0.9, radius=8, grow=6)
    scrim(canvas, cam, enter * (1 - leave))
    caption(canvas, t, beat(b0) + 0.2, beat(20.4), "GTA 6, GTA V and RDR2 layouts", "After Rockstar's own PC keys")
    caption(canvas, t, beat(20.4) + 0.1, beat(b1), "Every key can be changed", "Two keys per button, mouse buttons and the wheel too")


def scene_kbm_facts(canvas, t):
    facts(canvas, t, 24, 32, [("Hand them over, ", T1), ("take them back", LIME)],
          [("Click the picture or F12", "The mouse and keyboard go to the console"),
           ("F12 or Alt+Tab", "And they're your PC's again"),
           ("Your shortcuts stay", "Unbound F-keys still record and screenshot"),
           ("Tune the mouse", "Sensitivity, curve, stick start, invert")],
          None)


def scene_voice(canvas, t):
    b0, b1 = 32, 40
    rx, ry, rw, rh = PS5_MIC
    cam = cam_mix(cam_base(), cam_region(rx - 60, ry - 140, rw + 120, 360, max_s=1.9),
                  in_out_cubic(at(t, beat(33), beat(33) + 0.7)))
    enter = out_cubic(at(t, beat(b0), beat(b0) + 0.3))
    leave = at(t, beat(b1) - 0.18, beat(b1))
    window(canvas, "V-03-ps5", cam, alpha=enter * (1 - leave), shadow=clamp(1.7 - cam[0]))
    lit = at(t, beat(34.2), beat(34.2) + 0.3) * (1 - at(t, beat(b1) - 0.2, beat(b1)))
    focus(canvas, cam, PS5_MIC, lit, radius=8, grow=6, blur=5.0, dim=0.36)
    highlight(canvas, cam, PS5_MIC, lit * 0.9, radius=8, grow=6)
    scrim(canvas, cam, enter * (1 - leave))
    caption(canvas, t, beat(b0) + 0.2, beat(36.2), "PS5 voice chat from your PC", "Party and game chat with your PC's microphone")
    caption(canvas, t, beat(36.2) + 0.1, beat(b1), "F2 mutes it", "Also while the console has the keyboard")


def scene_settings(canvas, t):
    facts(canvas, t, 40, 48, [("Your settings, ", T1), ("kept", LIME)],
          [("Profiles remember", "Changes after choosing one are saved into it"),
           ("Settings always save", "A damaged file is set aside, not the end"),
           ("AI enhancement later", "Install its files with one button"),
           ("Record window", "Shows what you set in Settings")],
          None)


def scene_latency(canvas, t):
    facts(canvas, t, 48, 56, [("Less ", T1), ("waiting", LIME)],
          [("Steadier delay", "No jump back up after menus and pauses"),
           ("Fewer stutters", "A late generated frame gets its time"),
           ("Check my setup", "Finds what adds delay, with Undo"),
           ("Keys for full screen", "Latency readout, clips, frame generation")],
          None)


def scene_remote(canvas, t):
    facts(canvas, t, 56, 64, [("Remote Play, ", T1), ("steadier", LIME)],
          [("Long PS5 sessions", "Stay on the graphics card"),
           ("PS5 sound", "Stays with the picture"),
           ("Xbox at 1440p", "A new picture resolution row"),
           ("One bad frame", "No longer moves decoding to the processor")],
          None)


def scene_bridge(canvas, t):
    # 64..68: one line, a breath before the list.
    t0, t1 = beat(64), beat(68)
    enter = out_cubic(at(t, t0, t0 + 0.3))
    a = enter * (1 - at(t, t1 - 0.2, t1))
    sp = text_sprite([("Drive with the controller,", T1)], "title", 92)
    put(canvas, sp, 960 - sp.width / 2, 380 + (1 - enter) * 24, a)
    k = out_cubic(at(t, t0 + 0.5, t0 + 0.8))
    sp2 = text_sprite([("aim with the mouse.", LIME)], "title", 92)
    put(canvas, sp2, 960 - sp2.width / 2, 500 + (1 - k) * 24, a * k)


def frame(t):
    canvas = V.background().copy()
    if t < beat(8):
        V.scene_intro(canvas, t)
    if beat(8) - 0.05 <= t < beat(16):
        scene_kbm_demo(canvas, t)
    if beat(16) <= t < beat(24):
        scene_kbm_window(canvas, t)
    if beat(24) <= t < beat(32):
        scene_kbm_facts(canvas, t)
    if beat(32) <= t < beat(40):
        scene_voice(canvas, t)
    if beat(40) <= t < beat(48):
        scene_settings(canvas, t)
    if beat(48) <= t < beat(56):
        scene_latency(canvas, t)
    if beat(56) <= t < beat(64):
        scene_remote(canvas, t)
    if beat(64) <= t < beat(68):
        scene_bridge(canvas, t)
    if beat(68) <= t < beat(80):
        V.scene_also(canvas, t)
    if t >= beat(80):
        V.scene_outro(canvas, t)
    return canvas.convert("RGB")


V.frame = frame
V.PARTS = [("intro", 0, 8), ("kbm", 8, 32), ("voice", 32, 40), ("facts", 40, 64), ("bridge", 64, 68), ("also", 68, 80), ("outro", 80, None)]

if __name__ == "__main__":
    V.main()
