#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#
# The 4.5.0 video: what is new and where. The 4.3 film's machinery (make_release_video.py) and the tour
# film's drawn street (make_tour_video.py), with this release's scenes on the same beats, so the same music
# fits. Same commands:
#
#   python3 scripts/brand/make_v45_video.py --frame 12.5 frame.png
#   python3 scripts/brand/make_v45_video.py --parts out/v45 [n ...]
#   python3 scripts/brand/make_v45_video.py --join out/v45 film.mp4 --audio music.m4a
#
# The screenshots are not in the repository. They are the V-* entries of the screenshots workflow on the
# scratch branch video/v45 (1600 x 900), into out/v45-shots (or the folder PUSHFPS_VIDEO_SHOTS names):
#   V-02-home-hud (--scroll-to pushfps-subtitle-row), V-03-ps5.
# As for the tour film, that branch's EffectSupport.js answers "available" for everything, because the build
# machine has no graphics card: the rows are the interface as a PC with a card shows it. What cannot be
# photographed there (subtitles over a game, a sound wave, an AMD display) is drawn and, where it shows what
# a picture looks like, says that it is an illustration.
import math, os, sys
from functools import lru_cache
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_release_video as V
import make_tour_video as TV
from make_release_video import (W, H, FPS, LIME, T1, T2, T3, EDGE, beat, at, clamp, out_cubic, in_out, in_out_cubic,
                                put, window, focus, highlight, caption, scrim, chip, text_sprite, words, font,
                                rim_sprite, cam_base, cam_region, cam_mix)
from make_tour_video import street_view, panel, heading

V.SHOTS = os.environ.get("PUSHFPS_VIDEO_SHOTS") or os.path.join(V.ROOT, "out", "v45-shots")
V.INTRO_TAG = [("4.5", LIME), ("   what's new", T1)]
V.ALSO_HEAD = [("Also in ", T1), ("4.5", LIME)]
V.OUTRO_TAG = [("4.5", LIME), ("  is out", T1)]
V.ALSO = [("Xbox", "Decoding on the processor uses the stream's real size"),
          ("In English", "A warning that showed in Chinese is translated"),
          ("Tooltips", "No longer pop up through an open window"),
          ("Network streams", "The address box is ready to paste into"),
          ("Performance orb", "Its glow is no longer cut off"),
          ("Crash notes", "Say more about where, still nothing personal")]

# The home page's source panel ends with a line of credits; the owner wanted the film about PushFPS alone, so
# that one line is covered with the panel's own colour where a screenshot of the home page shows it.
_shot = V.shot


@lru_cache(maxsize=None)
def shot_without_credit_line(name):
    im = _shot(name).copy()
    if name.startswith("V-0") and name != "V-05-pro-fg":
        box = (234, 708, 456, 732)
        fill = im.getpixel((box[0] + 4, box[1] - 6))
        ImageDraw.Draw(im).rectangle(box, fill=fill)
    return im


V.shot = shot_without_credit_line

# Rectangles in a 1600 x 900 screenshot (x, y, w, h), measured on the V-* pictures.
SUBTITLE_ROW = (1180, 232, 396, 52)
PS5_ACCOUNT_BUTTON = (971, 343, 123, 31)


# ---------------------------------------------------------------- drawn pictures
@lru_cache(maxsize=None)
def subtitle_line(text, w, alpha):
    f = font("bold", 40)
    tw = int(f.getlength(text))
    im = Image.new("RGBA", (w, 70), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    x = (w - tw) // 2
    for dx, dy in ((-2, 0), (2, 0), (0, -2), (0, 2), (-2, -2), (2, 2), (-2, 2), (2, -2)):
        d.text((x + dx, 12 + dy), text, font=f, fill=(0, 0, 0, alpha))
    d.text((x, 12), text, font=f, fill=(250, 250, 246, alpha))
    return im


def scene_subtitle_demo(canvas, t):
    # What Subtitle Hold is for, drawn: the road runs towards the camera, so frame generation drags the line of
    # subtitles up and down with it in every frame it makes. Each frame is held four film frames.
    t0, t1 = beat(8), beat(16)
    enter = out_cubic(at(t, t0, t0 + 0.3))
    a = enter * (1 - at(t, t1 - 0.2, t1))
    heading(canvas, [("Subtitle ", T1), ("Hold", LIME)], 104, enter, a)
    pw, ph = 800, 450
    n = int((t - t0) * FPS + 1e-6)
    made = (n // 4) % 2 == 1
    offset = (t - t0) * 520
    text = "Hold on, we're almost there."
    for i, held in enumerate((False, True)):
        k = out_cubic(at(t, t0 + 0.12 + 0.16 * i, t0 + 0.5 + 0.16 * i))
        if k <= 0.01:
            continue
        pic = street_view(pw, ph, 11, 2, offset).convert("RGBA")
        y0 = ph - 96
        if held or not made:
            pic.alpha_composite(subtitle_line(text, pw, 255), (0, y0))
        else:
            ghost = subtitle_line(text, pw, 120)
            for dy in (-26, 26):
                pic.alpha_composite(ghost, (0, y0 + dy))
        x, y = 140 + i * (pw + 40), 262 + (1 - k) * 30
        panel(canvas, pic, x, y, a * k, lime=held)
        put(canvas, chip("Subtitle Hold on" if held else "Frame generation", held), x + 22, y + 20, a * k)
    caption(canvas, t, t0 + 0.3, t1, "Subtitles stay sharp at speed", "Illustration, slowed down", y=806)


def scene_subtitle_where(canvas, t):
    b0, b1 = 16, 24
    row = SUBTITLE_ROW
    rx, ry, rw, rh = row
    cam = cam_mix(cam_base(), cam_region(rx - 14, ry - 176, rw + 30, 392, max_s=2.05), in_out_cubic(at(t, beat(17.4), beat(17.4) + 0.7)))
    enter = out_cubic(at(t, beat(b0), beat(b0) + 0.3))
    leave = at(t, beat(b1) - 0.18, beat(b1))
    window(canvas, "V-02-home-hud", cam, alpha=enter * (1 - leave), shadow=clamp(1.7 - cam[0]))
    lit = at(t, beat(19), beat(19) + 0.3) * (1 - at(t, beat(b1) - 0.2, beat(b1)))
    focus(canvas, cam, row, lit, radius=8, grow=5, blur=5.0, dim=0.36)
    highlight(canvas, cam, row, lit * 0.9, radius=8, grow=5)
    scrim(canvas, cam, enter * (1 - leave))
    caption(canvas, t, beat(b0) + 0.2, beat(19.6), "Where: quick setup, under HUD", "Right under Crosshair Hold")
    caption(canvas, t, beat(19.6) + 0.1, beat(b1), "Turn it on if you play with subtitles", "Off until you switch it on")


def scene_ps5_dialog(canvas, t):
    b0, b1 = 24, 30
    bx, by, bw, bh = PS5_ACCOUNT_BUTTON
    cam = cam_mix(cam_base(), cam_region(bx - 300, by - 170, bw + 600, 400, max_s=1.8), in_out_cubic(at(t, beat(25), beat(25) + 0.7)))
    enter = out_cubic(at(t, beat(b0), beat(b0) + 0.3))
    leave = at(t, beat(b1) - 0.18, beat(b1))
    window(canvas, "V-03-ps5", cam, alpha=enter * (1 - leave), shadow=clamp(1.7 - cam[0]))
    lit = at(t, beat(26.2), beat(26.2) + 0.3) * (1 - at(t, beat(b1) - 0.2, beat(b1)))
    focus(canvas, cam, PS5_ACCOUNT_BUTTON, lit, radius=8, grow=6, blur=5.0, dim=0.36)
    highlight(canvas, cam, PS5_ACCOUNT_BUTTON, lit * 0.9, radius=8, grow=6)
    scrim(canvas, cam, enter * (1 - leave))
    caption(canvas, t, beat(b0) + 0.2, beat(b1), "PS5 setup, made easy", "Get Account ID now signs you in on Sony's own page")


@lru_cache(maxsize=None)
def step_card(n, title, sub, w=500, h=230):
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((0, 0, w - 1, h - 1), radius=20, fill=(20, 26, 33, 255), outline=EDGE + (255,), width=2)
    d.ellipse((30, 30, 92, 92), fill=LIME + (255,))
    f = font("title", 44)
    s = str(n)
    d.text((61 - f.getlength(s) / 2, 30), s, font=f, fill=(13, 26, 4, 255))
    d.text((30, 112), title, font=font("title", 44), fill=T1 + (255,))
    d.text((30, 168), sub, font=font("body", 27), fill=T2 + (255,))
    return im


@lru_cache(maxsize=None)
def chevron():
    im = Image.new("RGBA", (22, 36), (0, 0, 0, 0))
    ImageDraw.Draw(im).line([(4, 4), (18, 18), (4, 32)], fill=LIME + (255,), width=5, joint="curve")
    return im


def scene_ps5_steps(canvas, t):
    t0, t1 = beat(30), beat(36)
    enter = out_cubic(at(t, t0, t0 + 0.3))
    a = enter * (1 - at(t, t1 - 0.2, t1))
    heading(canvas, [("Your Account ID, ", T1), ("filled in", LIME)], 150, enter, a)
    steps = [("Get Account ID", "One button in PushFPS"), ("Sign in", "On Sony's own page"), ("Fill in", "Paste the address, done")]
    gap = 40
    total = 3 * 500 + 2 * gap
    for i, (title, sub) in enumerate(steps):
        b = 30.6 + i * 1.1
        k = out_cubic(at(t, beat(b), beat(b) + 0.3))
        if k <= 0.01:
            continue
        x = 960 - total / 2 + i * (500 + gap)
        y = 360 + (1 - k) * 30
        put(canvas, step_card(i + 1, title, sub), x, y, a * k)
        lit = (1 - clamp((t - beat(b)) / 0.5)) * 0.9
        if lit > 0.02:
            sp, pad = rim_sprite(500, 230, 20)
            put(canvas, sp, x - pad, y - pad, a * lit)
        if i < 2 and k > 0.5:
            put(canvas, chevron(), x + 500 + gap / 2 - 11, y + 115 - 18, a * (k - 0.5) * 2)
    caption(canvas, t, t0 + 0.3, t1, "No lookup website, no session code", "The 8 digits can be typed the way the PS5 shows them", y=806)


@lru_cache(maxsize=None)
def fact_card(title, sub, w=820, h=150):
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((0, 0, w - 1, h - 1), radius=18, fill=(20, 26, 33, 255), outline=EDGE + (255,), width=2)
    d.rounded_rectangle((26, 28, 32, 82), radius=3, fill=LIME + (255,))
    d.text((52, 18), title, font=font("title", 44), fill=T1 + (255,))
    d.text((54, 88), sub, font=font("body", 27), fill=T2 + (255,))
    return im


def facts(canvas, t, b0, b1, head, items, cap):
    enter = out_cubic(at(t, beat(b0), beat(b0) + 0.3))
    a = enter * (1 - at(t, beat(b1) - 0.2, beat(b1)))
    sp = text_sprite(head, "title", 78)
    put(canvas, sp, 960 - sp.width / 2, 150 + (1 - enter) * 24, a)
    for i, (title, sub) in enumerate(items):
        b = b0 + 0.6 + i * 1.3
        k = out_cubic(at(t, beat(b), beat(b) + 0.3))
        if k <= 0.01:
            continue
        col, row = i % 2, i // 2
        x = 960 - (2 * 820 + 40) / 2 + col * (820 + 40)
        y = 330 + row * (150 + 36) + (1 - k) * 30
        put(canvas, fact_card(title, sub), x, y, a * k)
        lit = (1 - clamp((t - beat(b)) / 0.5)) * 0.9
        if lit > 0.02:
            r, pad = rim_sprite(820, 150, 18)
            put(canvas, r, x - pad, y - pad, a * lit)
    if cap:
        caption(canvas, t, beat(b0) + 0.3, beat(b1), *cap, y=806)


def scene_ps5_facts(canvas, t):
    facts(canvas, t, 36, 44, [("PS5 Remote Play ", T1), ("holds on", LIME)],
          [("Reconnects after a drop", "Waits until the PS5 lets go of the old session"),
           ("The controller you press", "is the one PushFPS reads"),
           ("Fewer stutters", "A larger network buffer at high bitrates"),
           ("AMD hardware decoding", "No longer stops after a few seconds")],
          None)


def scene_amd(canvas, t):
    facts(canvas, t, 44, 52, [("Better on ", T1), ("AMD", LIME)],
          [("FreeSync detected", "The variable refresh switch works on AMD"),
           ("FSR upscaling", "Gets motion at the right size"),
           ("AMD FRC", "Stronger motion search on fast camera turns"),
           ("Set it in AMD Software", "PushFPS reads it, never changes it")],
          None)


def scene_capture(canvas, t):
    facts(canvas, t, 52, 60, [("Capture cards, ", T1), ("cleaner", LIME)],
          [("Sharper colour edges", "For YUY2 and NV12 / P010 capture"),
           ("MJPEG and a manual range", "Full or Limited no longer stops it connecting"),
           ("60 to 30 original", "The earlier frame selection, as its own mode"),
           ("Any window size records", "Odd sizes are no longer refused")],
          None)


def wave(w, h, gain, limited, phase):
    """A loud passage drawn as a wave: cut flat at full scale, or turned down so its peaks stay round."""
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    mid, amp = h / 2, h * 0.42
    d.line((0, mid - amp, w, mid - amp), fill=(90, 100, 112, 255), width=2)
    d.line((0, mid + amp, w, mid + amp), fill=(90, 100, 112, 255), width=2)
    pts = []
    for x in range(w):
        u = x / w
        env = 0.45 + 0.55 * math.exp(-((u - 0.5) / 0.22) ** 2)
        v = gain * env * math.sin(2 * math.pi * (u * 9 + phase)) * (0.85 + 0.15 * math.sin(2 * math.pi * u * 31))
        if limited:
            peak = gain * env
            if peak > 0.98:
                v *= 0.98 / peak
        else:
            v = max(-1.0, min(1.0, v))
        pts.append((x, mid - v * amp))
    d.line(pts, fill=LIME + (255,) if limited else (240, 120, 96, 255), width=4)
    return im


def scene_volume(canvas, t):
    t0, t1 = beat(60), beat(68)
    enter = out_cubic(at(t, t0, t0 + 0.3))
    a = enter * (1 - at(t, t1 - 0.2, t1))
    heading(canvas, [("Volume above 100% ", T1), ("without crackles", LIME)], 120, enter, a)
    pw, ph = 800, 380
    phase = (t - t0) * 0.6
    for i, limited in enumerate((False, True)):
        k = out_cubic(at(t, t0 + 0.12 + 0.16 * i, t0 + 0.5 + 0.16 * i))
        if k <= 0.01:
            continue
        bg = Image.new("RGB", (pw, ph), (16, 21, 27))
        x, y = 140 + i * (pw + 40), 300 + (1 - k) * 30
        panel(canvas, bg, x, y, a * k, lime=limited)
        put(canvas, wave(pw - 60, ph - 120, 1.6, limited, phase), x + 30, y + 90, a * k)
        put(canvas, chip("Turned down smoothly" if limited else "Cut off at full scale", limited), x + 22, y + 20, a * k)
    caption(canvas, t, t0 + 0.3, t1, "Loud moments stay clean", "At 300% the peaks are turned down instead of cut off. Illustration", y=806)


def frame(t):
    canvas = V.background().copy()
    if t < beat(8):
        V.scene_intro(canvas, t)
    if beat(8) - 0.05 <= t < beat(16):
        scene_subtitle_demo(canvas, t)
    if beat(16) <= t < beat(24):
        scene_subtitle_where(canvas, t)
    if beat(24) <= t < beat(30):
        scene_ps5_dialog(canvas, t)
    if beat(30) <= t < beat(36):
        scene_ps5_steps(canvas, t)
    if beat(36) <= t < beat(44):
        scene_ps5_facts(canvas, t)
    if beat(44) <= t < beat(52):
        scene_amd(canvas, t)
    if beat(52) <= t < beat(60):
        scene_capture(canvas, t)
    if beat(60) <= t < beat(68):
        scene_volume(canvas, t)
    if beat(68) <= t < beat(80):
        V.scene_also(canvas, t)
    if t >= beat(80):
        V.scene_outro(canvas, t)
    return canvas.convert("RGB")


V.frame = frame
V.PARTS = [("intro", 0, 8), ("subtitle", 8, 24), ("ps5", 24, 44), ("amd", 44, 52), ("more", 52, 68), ("also", 68, 80), ("outro", 80, None)]

if __name__ == "__main__":
    V.main()
