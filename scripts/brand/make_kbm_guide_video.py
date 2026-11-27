#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#
# The mouse and keyboard setup video (4.5.7): a step-by-step guide in the showcase films' style, on their
# machinery (make_release_video.py) and beat grid. The music is the 4.5.0 film's track with one phrase of
# four bars played twice (film beats 24..56 again after 56), so the film is 32 beats longer; the outro is
# the same music as before, 32 beats later. Same commands as the other films:
#
#   python3 scripts/brand/make_kbm_guide_video.py --frame 12.5 frame.png
#   python3 scripts/brand/make_kbm_guide_video.py --parts out/guide [n ...]
#   python3 scripts/brand/make_kbm_guide_video.py --join out/guide film.mp4 --audio music-long.m4a
#
# The screenshots are not in the repository: the G-* entries of the screenshots workflow on the scratch
# branch video/kbm-guide, taken from the 4.5.7 package (1600 x 900), into out/guide-shots (or the folder
# PUSHFPS_VIDEO_SHOTS names). The "ready" state and a paired PS5 cannot exist on the build machine (no
# console): those two shots show them through the scratch branch's switches; the capture card's state is
# the program's own, with its test switch that counts the test pattern as a capture card.
# The controller is drawn here and is no console maker's design: a plain gamepad with the usual layout.
import math, os, sys
from functools import lru_cache
from PIL import Image, ImageDraw, ImageFilter, ImageChops

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_release_video as V
import make_v456_video as V456
from make_release_video import (W, H, FPS, LIME, T1, T2, T3, EDGE, BG, beat, at, clamp, out_cubic, in_out, in_out_cubic,
                                put, window, focus, highlight, scrim, chip, text_sprite, words, font, rim_sprite,
                                cam_base, cam_region, cam_mix)
from make_tour_video import panel, heading
from make_v45_video import facts
from make_v456_video import key_sprite, mouse_sprite, KEY

V.SHOTS = os.environ.get("PUSHFPS_VIDEO_SHOTS") or os.path.join(V.ROOT, "out", "guide-shots")
V.INTRO_TAG = [("Mouse and keyboard", LIME), ("   setup guide", T1)]
V.OUTRO_TAG = [("Play your way", T1), ("  ·  4.5.7", LIME)]

SHIFT = 32                      # beats the film is longer than the 4.5.0 track
MUSIC_SECONDS = 44.04           # the track as it was
V.DURATION = MUSIC_SECONDS + SHIFT * V.BEAT

# Rectangles in a 1600 x 900 screenshot (x, y, w, h), measured on the G-* pictures.
SOURCES = (236, 102, 266, 478)          # G-02: the source panel
SRC_CAPTURE = (236, 102, 266, 126)
SRC_PS5 = (236, 354, 266, 52)
SRC_XBOX = (236, 470, 266, 52)
KBM_ROW = (1180, 232, 392, 60)          # G-01 / G-02: quick setup's Mouse and keyboard row
KBM_SWITCH = (1528, 240, 34, 20)
KBM_BINDINGS = (1425, 236, 95, 30)
CAP_ROW = (1180, 232, 392, 84)          # G-03 / G-04: the row and what it says is missing
CAP_FIX = (1180, 326, 392, 40)          # ... and the row with the button under it
DLG_LAYOUT = (522, 242, 545, 52)        # G-05: the layout row
DLG_MOUSE = (522, 366, 545, 232)        # ... the four mouse settings and invert
DLG_KEYS = (522, 318, 545, 425)         # G-06: the key rows
DLG_RELEASE = (522, 608, 545, 40)       # G-05: the release key


# ---------------------------------------------------------------- the controller (drawn, no maker's design)
PAD_W, PAD_H = 900, 600
SS = 2                                  # drawn at twice the size, then made smaller: smooth edges
PAD_BODY = (34, 42, 53)
PAD_PART = (74, 88, 108)
PAD_DARK = (20, 25, 32)
# Where each part sits in the 900 x 600 picture (also where the labels point).
PAD_AT = {
    "L2": (240, 52), "R2": (660, 52), "L1": (240, 98), "R1": (660, 98),
    "dpad": (238, 236), "Up": (238, 182), "Down": (238, 290), "Left": (184, 236), "Right": (292, 236),
    "Triangle": (662, 180), "Cross": (662, 292), "Square": (606, 236), "Circle": (718, 236),
    "L3": (352, 372), "R3": (548, 372), "Options": (548, 168), "View": (352, 168), "Home": (450, 300),
}


@lru_cache(maxsize=None)
def pad_body():
    """The body: a wide top joined to two grips, graphite with a soft light from above and a thin edge."""
    w, h = PAD_W * SS, PAD_H * SS
    m = Image.new("L", (w, h), 0)
    d = ImageDraw.Draw(m)
    d.rounded_rectangle((110 * SS, 110 * SS, 790 * SS, 360 * SS), radius=120 * SS, fill=255)
    for cx, ang in ((236, 24), (664, -24)):
        g = Image.new("L", (w, h), 0)
        ImageDraw.Draw(g).ellipse(((cx - 118) * SS, (250) * SS, (cx + 118) * SS, (560) * SS), fill=255)
        g = g.rotate(ang, center=(cx * SS, 300 * SS), resample=Image.BICUBIC)
        m = ImageChops.lighter(m, g)
    m = m.filter(ImageFilter.GaussianBlur(1.2 * SS)).point(lambda v: 255 if v > 127 else 0)
    shade = Image.new("RGB", (w, h), PAD_BODY)
    light = Image.new("L", (w, h), 0)
    ImageDraw.Draw(light).ellipse((140 * SS, -260 * SS, 760 * SS, 300 * SS), fill=255)
    light = light.filter(ImageFilter.GaussianBlur(70 * SS))
    shade.paste(Image.new("RGB", (w, h), (52, 63, 78)), (0, 0), light.point(lambda v: int(v * 0.8)))
    body = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    body.paste(shade, (0, 0), m)
    edge = ImageChops.subtract(m, m.filter(ImageFilter.MinFilter(5)))
    body.paste(Image.new("RGBA", (w, h), EDGE + (255,)), (0, 0), edge)
    return body.resize((PAD_W, PAD_H), Image.LANCZOS)


def pad_sprite(lit=(), stick_l=(0.0, 0.0), stick_r=(0.0, 0.0)):
    im = Image.new("RGBA", (PAD_W * SS, PAD_H * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    s = SS
    on = lambda name: name in lit
    # shoulders: the triggers behind, the bumpers in front
    for name in ("L2", "R2"):
        x, y = PAD_AT[name]
        d.rounded_rectangle(((x - 78) * s, (y - 24) * s, (x + 78) * s, (y + 28) * s), radius=24 * s,
                            fill=(LIME if on(name) else PAD_DARK) + (255,), outline=EDGE + (255,), width=2 * s)
    for name in ("L1", "R1"):
        x, y = PAD_AT[name]
        d.rounded_rectangle(((x - 96) * s, (y - 16) * s, (x + 96) * s, (y + 18) * s), radius=17 * s,
                            fill=(LIME if on(name) else PAD_PART) + (255,), outline=EDGE + (255,), width=2 * s)
    body = pad_body().resize((PAD_W * s, PAD_H * s), Image.LANCZOS)
    im.alpha_composite(body)
    d = ImageDraw.Draw(im)
    # d-pad: four arms
    cx, cy = PAD_AT["dpad"]
    for name, (dx, dy) in (("Up", (0, -1)), ("Down", (0, 1)), ("Left", (-1, 0)), ("Right", (1, 0))):
        x0, y0 = cx + dx * 36, cy + dy * 36
        hw, hh = (20, 26) if dx == 0 else (26, 20)
        d.rounded_rectangle(((x0 - hw) * s, (y0 - hh) * s, (x0 + hw) * s, (y0 + hh) * s), radius=7 * s,
                            fill=(LIME if on(name) else PAD_PART) + (255,))
    d.ellipse(((cx - 18) * s, (cy - 18) * s, (cx + 18) * s, (cy + 18) * s), fill=PAD_PART + (255,))
    # face buttons: four plain round buttons (no symbols; the labels name them)
    for name in ("Triangle", "Cross", "Square", "Circle"):
        x, y = PAD_AT[name]
        d.ellipse(((x - 33) * s, (y - 33) * s, (x + 33) * s, (y + 33) * s),
                  fill=(LIME if on(name) else PAD_PART) + (255,), outline=((96, 160, 40) if on(name) else (104, 120, 142)) + (255,), width=2 * s)
    # the small middle buttons
    for name in ("View", "Options"):
        x, y = PAD_AT[name]
        d.rounded_rectangle(((x - 22) * s, (y - 11) * s, (x + 22) * s, (y + 11) * s), radius=11 * s,
                            fill=(LIME if on(name) else PAD_PART) + (255,))
    x, y = PAD_AT["Home"]
    d.ellipse(((x - 17) * s, (y - 17) * s, (x + 17) * s, (y + 17) * s), fill=PAD_PART + (255,), outline=EDGE + (255,), width=2 * s)
    # the sticks: a well, and the cap moved by the push (-1..1 each way)
    for name, (vx, vy) in (("L3", stick_l), ("R3", stick_r)):
        x, y = PAD_AT[name]
        d.ellipse(((x - 66) * s, (y - 66) * s, (x + 66) * s, (y + 66) * s), fill=PAD_DARK + (255,), outline=EDGE + (255,), width=2 * s)
        moved = abs(vx) + abs(vy) > 0.05
        px, py = x + vx * 26, y + vy * 26
        cap = LIME if (moved or on(name)) else (64, 76, 92)
        d.ellipse(((px - 44) * s, (py - 44) * s, (px + 44) * s, (py + 44) * s), fill=cap + (255,))
        d.ellipse(((px - 30) * s, (py - 30) * s, (px + 30) * s, (py + 30) * s), outline=((96, 160, 40) if (moved or on(name)) else (80, 94, 112)) + (255,), width=3 * s)
    return im.resize((PAD_W, PAD_H), Image.LANCZOS)


@lru_cache(maxsize=None)
def pad_cached(lit, stick_l, stick_r):
    return pad_sprite(lit, stick_l, stick_r)


@lru_cache(maxsize=None)
def tag_sprite(text, lime):
    f = font("semi", 30)
    wd = int(f.getlength(text)) + 32
    im = Image.new("RGBA", (wd, 46), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((0, 0, wd - 1, 45), radius=12, fill=(LIME if lime else (20, 26, 33)) + (255,), outline=(LIME if lime else EDGE) + (255,), width=2)
    d.text((16, 5), text, font=f, fill=((13, 26, 4) if lime else T1) + (255,))
    return im


def label(canvas, text, part, px, py, scale, side, alpha, lime=False):
    """A label beside the pad with a thin line to the part it names."""
    if alpha <= 0.01:
        return
    ax, ay = PAD_AT[part]
    ax, ay = px + ax * scale, py + ay * scale
    sp = tag_sprite(text, lime)
    lx = ax + (90 if side > 0 else -90 - sp.width)
    ly = ay - sp.height / 2
    ln = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(ln).line([(ax, ay), (lx if side > 0 else lx + sp.width, ay)], fill=(LIME if lime else (120, 134, 152)) + (int(255 * alpha),), width=2)
    canvas.alpha_composite(ln)
    put(canvas, sp, lx, ly, alpha)


# ---------------------------------------------------------------- step captions
@lru_cache(maxsize=None)
def step_dot(n):
    im = Image.new("RGBA", (76, 76), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.ellipse((2, 2, 73, 73), fill=LIME + (255,))
    f = font("title", 48)
    s = str(n)
    d.text((38 - f.getlength(s) / 2 - 2, 6), s, font=f, fill=(13, 26, 4, 255))
    return im


def step_caption(canvas, t, t0, t1, n, title, sub=None, y=930, x=216):
    """The step's number in a lime dot, its title and one line under it: up and in at t0, out before t1."""
    k_in, k_out = out_cubic(at(t, t0, t0 + 0.32)), at(t, t1 - 0.22, t1)
    a = k_in * (1 - k_out)
    if a <= 0.01:
        return
    dy = (1 - k_in) * 26
    if n is not None:
        put(canvas, step_dot(n), x, y - 4 + dy, a)
        tx = x + 96
    else:
        put(canvas, Image.new("RGBA", (7, 66 if sub else 58), LIME + (255,)), x, y + 6 + dy, a)
        tx = x + 24
    put(canvas, words(title, "title", 58, T1), tx, y - 6 + dy, a)
    if sub:
        put(canvas, words(sub, "body", 29, T2), tx + 2, y + 60 + dy, a * at(t, t0 + 0.12, t0 + 0.45))


def shot_scene(canvas, t, b0, b1, shot, rect, cam_rect=None, max_s=1.7, light_at=None, second=None):
    """One screenshot as the app's window: the camera moves in on a part, the part lights up.
    second = (beat, shot): the same window changes to another screenshot (a switch turned on)."""
    enter = out_cubic(at(t, beat(b0), beat(b0) + 0.3))
    leave = at(t, beat(b1) - 0.18, beat(b1))
    alpha = enter * (1 - leave)
    if cam_rect:
        cam = cam_mix(cam_base(), cam_region(*cam_rect, max_s=max_s), in_out_cubic(at(t, beat(b0 + 1), beat(b0 + 1) + 0.7)))
    else:
        cam = cam_base()
    window(canvas, shot, cam, alpha=alpha, shadow=clamp(1.7 - cam[0]))
    if second:
        k = in_out_cubic(at(t, beat(second[0]), beat(second[0]) + 0.25))
        if k > 0.001:
            window(canvas, second[1], cam, alpha=alpha * k, shadow=0.0)
    if rect:
        lit = at(t, beat(light_at if light_at is not None else b0 + 2.2), beat(light_at if light_at is not None else b0 + 2.2) + 0.3) * (1 - at(t, beat(b1) - 0.2, beat(b1)))
        focus(canvas, cam, rect, lit, radius=8, grow=6, blur=5.0, dim=0.36)
        highlight(canvas, cam, rect, lit * 0.9, radius=8, grow=6)
    scrim(canvas, cam, alpha)
    return cam, alpha


# ---------------------------------------------------------------- scenes
# What the keys press in the first scene: (beat, keys lit, mouse left/right, pad lit, left stick, right stick, words)
DEMO = [
    (8.8, ("W",), (False, False), (), (0, -1), (0, 0), "W  moves"),
    (10.2, ("Space",), (False, False), ("Cross",), (0, 0), (0, 0), "Space  jumps"),
    (11.6, (), (False, False), (), (0, 0), (0.8, -0.1), "The mouse  turns the camera"),
    (13.0, (), (False, True), ("L2",), (0, 0), (0.3, 0), "Right button  aims"),
    (14.4, (), (True, True), ("L2", "R2"), (0, 0), (0, 0), "Left button  shoots"),
]


def demo_at(rows, t):
    current = None
    for row in rows:
        if t >= beat(row[0]):
            current = row
    return current


@lru_cache(maxsize=None)
def motion_marks(phase):
    """Two short lime strokes either side of the mouse: it is moving."""
    im = Image.new("RGBA", (260, 80), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    for side in (-1, 1):
        for i in range(3):
            x0 = 130 + side * (84 + i * 11 + phase * 3)
            d.line([(x0, 20 + i * 6), (x0, 60 - i * 6)], fill=LIME + (255 - i * 70,), width=5)
    return im


def keys_panel(canvas, x, y, keys_lit, mouse_lit, a, extra_rows=None, moving=False, phase=0):
    pw, ph = 700, 400
    bg = Image.new("RGB", (pw, ph), (16, 21, 27))
    panel(canvas, bg, x, y, a, lime=False)
    rows = extra_rows or [[("Tab", 92), ("Q", KEY), ("W", KEY), ("E", KEY)],
                          [("A", KEY), ("S", KEY), ("D", KEY), ("F", KEY)],
                          [("Shift", 132), ("C", KEY)],
                          [("Space", 300)]]
    offsets = [36, 70, 36, 120]
    for r, keys in enumerate(rows):
        kx = x + offsets[r % len(offsets)]
        for lab, kw in keys:
            put(canvas, key_sprite(lab, kw, lab in keys_lit), kx, y + 52 + r * (KEY + 14), a)
            kx += kw + 12
    put(canvas, mouse_sprite(*mouse_lit), x + pw - 190, y + 100, a)
    if moving:
        put(canvas, motion_marks(phase), x + pw - 190 + 75 - 130, y + 100 + 110 - 40, a)


def arrow(canvas, x, y, a):
    sp = Image.new("RGBA", (90, 60), (0, 0, 0, 0))
    ImageDraw.Draw(sp).line([(6, 30), (78, 30)], fill=LIME + (255,), width=6)
    ImageDraw.Draw(sp).line([(58, 10), (80, 30), (58, 50)], fill=LIME + (255,), width=6, joint="curve")
    put(canvas, sp, x, y, a)


def scene_how(canvas, t):
    t0, t1 = beat(8), beat(16)
    enter = out_cubic(at(t, t0, t0 + 0.3))
    a = enter * (1 - at(t, t1 - 0.2, t1))
    heading(canvas, [("Your mouse and keyboard ", T1), ("become a controller", LIME)], 96, enter, a)
    st = demo_at(DEMO, t)
    keys_lit, mouse_lit, pad_lit, sl, sr = (st[1], st[2], st[3], st[4], st[5]) if st else ((), (False, False), (), (0, 0), (0, 0))
    if sr != (0, 0):
        sr = (round(clamp(sr[0] + math.sin((t - t0) * 7) * 0.2, -1, 1), 2), sr[1])
    k = out_cubic(at(t, t0 + 0.12, t0 + 0.5))
    keys_panel(canvas, 110, 290 + (1 - k) * 30, keys_lit, mouse_lit, a * k, moving=sr != (0, 0), phase=int((t * 8) % 3))
    arrow(canvas, 838, 460, a * k)
    k2 = out_cubic(at(t, t0 + 0.28, t0 + 0.66))
    scale = 0.86
    pad = pad_cached(tuple(pad_lit), tuple(sl), tuple(sr))
    pw = int(PAD_W * scale)
    padim = pad.resize((pw, int(PAD_H * scale)), Image.LANCZOS)
    px, py = 960, 250 + (1 - k2) * 30
    put(canvas, padim, px, py, a * k2)
    if st:
        lab = text_sprite([(st[6], T1)], "semi", 40)
        fade = out_cubic(at(t, beat(st[0]), beat(st[0]) + 0.25))
        put(canvas, lab, 960 - lab.width / 2, 760 + (1 - fade) * 12, a * fade)
    step_caption(canvas, t, t0 + 0.3, t1, None, "The console sees a controller", "Your own controller keeps working too   ·   Illustration", y=860)


def scene_connect(canvas, t):
    b0, b1 = 16, 28
    cam, alpha = shot_scene(canvas, t, b0, b1, "G-02-home-ready", None, cam_rect=(SOURCES[0] - 20, SOURCES[1] - 20, SOURCES[2] + 40, SOURCES[3] + 40), max_s=1.5)
    # the three ways in, lit one after another
    for i, (rect, b) in enumerate(((SRC_PS5, 18.2), (SRC_XBOX, 20.2), (SRC_CAPTURE, 22.2))):
        lit = at(t, beat(b), beat(b) + 0.3) * (1 - at(t, beat(b + 2), beat(b + 2) + 0.3) if i < 2 else 1 - at(t, beat(b1) - 0.2, beat(b1)))
        focus(canvas, cam, rect, lit, radius=8, grow=6, blur=5.0, dim=0.36)
        highlight(canvas, cam, rect, lit * 0.9, radius=8, grow=6)
    step_caption(canvas, t, beat(b0) + 0.2, beat(22.0), 1, "Connect your console", "PS5 streaming or Xbox streaming, over your home network")
    step_caption(canvas, t, beat(22.0) + 0.1, beat(b1), 1, "Or your capture card", "It needs your PS5 paired once for Remote Play (step 2 shows how)")


def scene_capture(canvas, t):
    b0, b1 = 36, 48
    union = (CAP_ROW[0] - 16, CAP_ROW[1] - 16, CAP_ROW[2] + 32, CAP_FIX[1] + CAP_FIX[3] - CAP_ROW[1] + 32)
    cam, alpha = shot_scene(canvas, t, b0, b1, "G-04-capture-pair", None, cam_rect=union, max_s=1.9, second=(42.0, "G-03-capture-turnon"))
    lit1 = at(t, beat(38), beat(38) + 0.3) * (1 - at(t, beat(39.8), beat(39.8) + 0.25))
    focus(canvas, cam, CAP_ROW, lit1, radius=8, grow=6, blur=5.0, dim=0.36)
    highlight(canvas, cam, CAP_ROW, lit1 * 0.9, radius=8, grow=6)
    lit2 = at(t, beat(40), beat(40) + 0.3) * (1 - at(t, beat(b1) - 0.2, beat(b1)))
    focus(canvas, cam, CAP_FIX, lit2, radius=8, grow=6, blur=5.0, dim=0.36)
    highlight(canvas, cam, CAP_FIX, lit2 * 0.9, radius=8, grow=6)
    step_caption(canvas, t, beat(b0) + 0.2, beat(42.0), 2, "Capture card? It says what's missing", "A capture card only brings the picture in: keys go through Remote Play")
    step_caption(canvas, t, beat(42.0) + 0.1, beat(b1), 2, "Pair your PS5 once, then press Turn on", "That switches on Control the PS5 through Remote Play")


def scene_turn_on(canvas, t):
    b0, b1 = 28, 36
    rect = (KBM_ROW[0] - 10, KBM_ROW[1] - 10, KBM_ROW[2] + 20, KBM_ROW[3] + 60)
    cam, alpha = shot_scene(canvas, t, b0, b1, "G-01-home-off", KBM_ROW, cam_rect=rect, max_s=2.0, light_at=30.0, second=(31.6, "G-02-home-ready"))
    # the pointer pressing the switch
    k = at(t, beat(31.0), beat(31.0) + 0.4) * (1 - at(t, beat(32.6), beat(32.6) + 0.3))
    if k > 0.01:
        s, X, Y = cam
        x, y, w, h = KBM_SWITCH
        cx, cy = X + (x + w / 2) * s, Y + (y + h / 2) * s
        r = 26 + 10 * (1 - k)
        ring = Image.new("RGBA", (120, 120), (0, 0, 0, 0))
        ImageDraw.Draw(ring).ellipse((60 - r, 60 - r, 60 + r, 60 + r), outline=LIME + (int(220 * k),), width=4)
        put(canvas, ring, cx - 60, cy - 60, alpha)
    step_caption(canvas, t, beat(b0) + 0.2, beat(b1), 2, "Turn on Mouse and keyboard", "Quick setup, under Console and game")


def scene_layout(canvas, t):
    b0, b1 = 48, 56
    cam, alpha = shot_scene(canvas, t, b0, b1, "G-05-kbm", DLG_LAYOUT, cam_rect=(500, 160, 600, 240), max_s=2.0, light_at=50.0)
    step_caption(canvas, t, beat(b0) + 0.2, beat(b1), 3, "Pick a layout", "Key bindings: GTA 6, GTA 6 Classic / GTA V, or Red Dead Redemption 2")


def scene_keys(canvas, t):
    b0, b1 = 56, 64
    cam, alpha = shot_scene(canvas, t, b0, b1, "G-06-kbm-keys", DLG_KEYS, cam_rect=(500, 300, 600, 460), max_s=1.8, light_at=58.0)
    step_caption(canvas, t, beat(b0) + 0.2, beat(b1), 4, "Change any key (optional)", "Click a box, press the key you want. Two keys per button")


def scene_mouse(canvas, t):
    b0, b1 = 64, 72
    cam, alpha = shot_scene(canvas, t, b0, b1, "G-05-kbm", DLG_MOUSE, cam_rect=(500, 300, 600, 400), max_s=1.8, light_at=66.0)
    step_caption(canvas, t, beat(b0) + 0.2, beat(b1), 5, "Tune the mouse", "Camera too slow? Raise Mouse sensitivity (the game's own counts too)")


PLAY = [
    (73.6, ("F12",), (False, False), (), (0, 0), (0, 0)),
    (75.0, ("W",), (False, False), (), (0, -1), (0, 0)),
    (76.2, (), (False, True), ("L2",), (0, 0), (0.7, -0.2)),
    (77.4, (), (True, True), ("L2", "R2"), (0, 0), (0.2, 0)),
    (78.8, (), (False, False), (), (0, 0), (0, 0)),
]


def scene_play(canvas, t):
    t0, t1 = beat(72), beat(80)
    enter = out_cubic(at(t, t0, t0 + 0.3))
    a = enter * (1 - at(t, t1 - 0.2, t1))
    heading(canvas, [("Click the picture ", T1), ("or press F12", LIME)], 96, enter, a)
    st = demo_at(PLAY, t)
    keys_lit, mouse_lit, pad_lit, sl, sr = (st[1], st[2], st[3], st[4], st[5]) if st else ((), (False, False), (), (0, 0), (0, 0))
    if sr != (0, 0):
        sr = (round(clamp(sr[0] + math.sin((t - t0) * 7) * 0.2, -1, 1), 2), sr[1])
    k = out_cubic(at(t, t0 + 0.12, t0 + 0.5))
    rows = [[("F12", 92), ("Tab", 92), ("Q", KEY), ("W", KEY)], [("A", KEY), ("S", KEY), ("D", KEY), ("F", KEY)],
            [("Shift", 132), ("C", KEY)], [("Space", 300)]]
    keys_panel(canvas, 110, 290 + (1 - k) * 30, keys_lit, mouse_lit, a * k, rows, moving=sr != (0, 0), phase=int((t * 8) % 3))
    arrow(canvas, 838, 460, a * k)
    k2 = out_cubic(at(t, t0 + 0.28, t0 + 0.66))
    scale = 0.86
    padim = pad_cached(tuple(pad_lit), tuple(sl), tuple(sr)).resize((int(PAD_W * scale), int(PAD_H * scale)), Image.LANCZOS)
    put(canvas, padim, 960, 250 + (1 - k2) * 30, a * k2)
    # "In use" once F12 is pressed: the console has them
    k3 = out_cubic(at(t, beat(74.2), beat(74.2) + 0.3))
    if k3 > 0.01:
        sp = chip("The console has your mouse and keyboard", True)
        put(canvas, sp, 960 - sp.width / 2, 770 + (1 - k3) * 12, a * k3)
    step_caption(canvas, t, t0 + 0.3, t1, 6, "Play", "Drive with your controller, aim with the mouse. Both work at once", y=860)


@lru_cache(maxsize=None)
def big_key(lab, w, lit, h=124):
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((0, 0, w - 1, h - 1), radius=22, fill=(LIME if lit else (28, 35, 44)) + (255,),
                        outline=(LIME if lit else EDGE) + (255,), width=3)
    f = font("semi", 56)
    d.text(((w - f.getlength(lab)) / 2, 26), lab, font=f, fill=((13, 26, 4) if lit else T1) + (255,))
    return im


def scene_release(canvas, t):
    t0, t1 = beat(80), beat(88)
    enter = out_cubic(at(t, t0, t0 + 0.3))
    a = enter * (1 - at(t, t1 - 0.2, t1))
    heading(canvas, [("Done? ", T1), ("Take them back", LIME)], 150, enter, a)
    # F12 / Alt + Tab, big keys pressed in turn
    pressed1 = at(t, beat(81.6), beat(81.6) + 0.1) * (1 - at(t, beat(82.6), beat(82.6) + 0.1))
    pressed2 = at(t, beat(84.0), beat(84.0) + 0.1) * (1 - at(t, beat(85.2), beat(85.2) + 0.1))
    k = out_cubic(at(t, t0 + 0.2, t0 + 0.6))
    y = 380 + (1 - k) * 24
    put(canvas, big_key("F12", 300, pressed1 > 0.5), 960 - 520, y, a * k)
    sp = words("or", "title", 64, T2)
    put(canvas, sp, 960 - sp.width / 2 - 60, y + 22, a * k)
    put(canvas, big_key("Alt", 220, pressed2 > 0.5), 960 + 30, y, a * k)
    sp2 = words("+", "title", 64, T2)
    put(canvas, sp2, 960 + 270, y + 22, a * k)
    put(canvas, big_key("Tab", 220, pressed2 > 0.5), 960 + 330, y, a * k)
    k2 = out_cubic(at(t, beat(83.0), beat(83.0) + 0.3))
    if k2 > 0.01:
        sp = words("Your mouse and keyboard are your PC's again", "semi", 44, T1)
        put(canvas, sp, 960 - sp.width / 2, 600 + (1 - k2) * 12, a * k2)
    step_caption(canvas, t, t0 + 0.3, t1, None, "F-keys you haven't bound still work", "Screenshots, recording and instant replay keep their keys", y=860)


def scene_tips(canvas, t):
    facts(canvas, t, 88, 100, [("Good ", T1), ("to know", LIME)],
          [("Won't start?", "The Mouse and keyboard row says what's missing"),
           ("Capture card", "Needs your PS5 paired once, for Remote Play"),
           ("PS5 voice chat", "Turn on PC microphone in the PS5 window"),
           ("F2 mutes it", "Also while the console has the keyboard")],
          None)


@lru_cache(maxsize=None)
def recap_card(i, title, sub):
    w, h = 820, 120
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((0, 0, w - 1, h - 1), radius=18, fill=(20, 26, 33, 255), outline=EDGE + (255,), width=2)
    dot = step_dot(i).resize((60, 60), Image.LANCZOS)
    im.alpha_composite(dot, (26, 30))
    d.text((112, 14), title, font=font("title", 44), fill=T1 + (255,))
    d.text((114, 70), sub, font=font("body", 26), fill=T2 + (255,))
    return im


def scene_recap(canvas, t):
    t0, t1 = beat(100), beat(112)
    enter = out_cubic(at(t, t0, t0 + 0.3))
    a = enter * (1 - at(t, t1 - 0.2, t1))
    sp = text_sprite([("Mouse and keyboard ", T1), ("in four steps", LIME)], "title", 78)
    put(canvas, sp, 960 - sp.width / 2, 120 + (1 - enter) * 24, a)
    steps = [("Connect your console", "PS5 or Xbox streaming, or your capture card"),
             ("Turn on Mouse and keyboard", "Quick setup, under Console and game"),
             ("Pick a layout", "Key bindings: GTA 6, GTA V or RDR2"),
             ("Click the picture or press F12", "F12 or Alt+Tab gives them back")]
    for i, (title, sub) in enumerate(steps):
        b = 100.6 + i * 1.6
        k = out_cubic(at(t, beat(b), beat(b) + 0.3))
        if k <= 0.01:
            continue
        x = 960 - 410
        y = 290 + i * 140 + (1 - k) * 24
        put(canvas, recap_card(i + 1, title, sub), x, y, a * k)
        lit = (1 - clamp((t - beat(b)) / 0.5)) * 0.9
        if lit > 0.02:
            r, pad = rim_sprite(820, 120, 18)
            put(canvas, r, x - pad, y - pad, a * lit)


def scene_outro(canvas, t):
    # The 4.5.0 film's outro, SHIFT beats later (the same music); its fade is timed by its own length.
    keep = V.DURATION
    V.DURATION = MUSIC_SECONDS
    try:
        V.scene_outro(canvas, t - SHIFT * V.BEAT)
    finally:
        V.DURATION = keep


def frame(t):
    canvas = V.background().copy()
    if t < beat(8):
        V.scene_intro(canvas, t)
    if beat(8) - 0.05 <= t < beat(16):
        scene_how(canvas, t)
    if beat(16) <= t < beat(28):
        scene_connect(canvas, t)
    if beat(28) <= t < beat(36):
        scene_turn_on(canvas, t)
    if beat(36) <= t < beat(48):
        scene_capture(canvas, t)
    if beat(48) <= t < beat(56):
        scene_layout(canvas, t)
    if beat(56) <= t < beat(64):
        scene_keys(canvas, t)
    if beat(64) <= t < beat(72):
        scene_mouse(canvas, t)
    if beat(72) <= t < beat(80):
        scene_play(canvas, t)
    if beat(80) <= t < beat(88):
        scene_release(canvas, t)
    if beat(88) <= t < beat(100):
        scene_tips(canvas, t)
    if beat(100) <= t < beat(112):
        scene_recap(canvas, t)
    if t >= beat(112):
        scene_outro(canvas, t)
    return canvas.convert("RGB")


V.frame = frame
V.PARTS = [("intro", 0, 8), ("how", 8, 16), ("connect", 16, 28), ("turnon", 28, 36), ("capture", 36, 48),
           ("layout", 48, 56), ("keys", 56, 64), ("mouse", 64, 72), ("play", 72, 80), ("release", 80, 88),
           ("tips", 88, 100), ("recap", 100, 112), ("outro", 112, None)]

if __name__ == "__main__":
    V.main()
