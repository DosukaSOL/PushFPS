#!/usr/bin/env python3
# PushFPS 4.3 showcase: the old interface beside the new one, from screenshots of the real program.
# Every picture of the interface is a screenshot; the script only moves, crops and labels them.
#   python3 render.py --frame 12.5 out.png     one frame
#   python3 render.py --video out.mp4 [--audio music.mp3] [--jobs N]
import math, os, subprocess, sys
from functools import lru_cache
from multiprocessing import Pool
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
W, H, FPS = 1920, 1080, 60
DURATION = 44.04
BEAT, OFFSET = 60.0 / 124.0, 0.012
SW, SH = 1600, 900                      # a screenshot's size
BG = (11, 15, 20)
LIME = (137, 245, 42)
T1, T2, T3 = (244, 247, 250), (183, 193, 205), (147, 160, 175)
EDGE = (52, 64, 80)
BASE_S = 0.93                           # the window as it stands in the frame
BASE_CX, BASE_CY = 960, 478


def beat(n):
    return OFFSET + n * BEAT


# ---------------------------------------------------------------- easing
def clamp(x, a=0.0, b=1.0):
    return a if x < a else b if x > b else x


def at(t, a, b):
    return clamp((t - a) / (b - a)) if b > a else (1.0 if t >= b else 0.0)


def out_cubic(x):
    return 1 - (1 - x) ** 3


def out_expo(x):
    return 1.0 if x >= 1 else 1 - 2 ** (-10 * x)


def in_out(x):
    return 0.5 - 0.5 * math.cos(math.pi * x)


def in_out_cubic(x):
    return 4 * x * x * x if x < 0.5 else 1 - (-2 * x + 2) ** 3 / 2


def lerp(a, b, k):
    return a + (b - a) * k


# ---------------------------------------------------------------- assets
@lru_cache(maxsize=None)
def shot(name):
    return Image.open(os.path.join(HERE, "assets", name + ".png")).convert("RGB")


@lru_cache(maxsize=None)
def font(kind, size):
    files = {"title": "BarlowSemiCondensed-ExtraBoldItalic.woff2", "semi": "BarlowSemiCondensed-SemiBold.woff2",
             "body": "Barlow-Medium.woff2", "bold": "Barlow-SemiBold.woff2", "regular": "Barlow-Regular.woff2"}
    path = os.path.join(HERE, "fonts", files[kind])
    alt = path[:-6] + ".ttf"
    return ImageFont.truetype(alt if os.path.exists(alt) else path, size)


@lru_cache(maxsize=None)
def background():
    # Graphite with a faint lime light from above and a darker edge: nothing that moves or distracts.
    im = Image.new("RGB", (W, H), BG)
    glow = Image.new("L", (W // 8, H // 8), 0)
    d = ImageDraw.Draw(glow)
    d.ellipse((W // 8 * 0.18, -H // 8 * 0.75, W // 8 * 0.82, H // 8 * 0.42), fill=255)
    glow = glow.filter(ImageFilter.GaussianBlur(28)).resize((W, H), Image.BILINEAR)
    im.paste(Image.new("RGB", (W, H), (30, 52, 22)), (0, 0), glow.point(lambda v: v * 0.55))
    vig = Image.new("L", (W // 8, H // 8), 0)
    ImageDraw.Draw(vig).ellipse((-30, -26, W // 8 + 30, H // 8 + 26), fill=255)
    vig = vig.filter(ImageFilter.GaussianBlur(30)).resize((W, H), Image.BILINEAR)
    dark = Image.new("RGB", (W, H), (5, 7, 10))
    im = Image.composite(im, dark, vig)
    return im.convert("RGBA")


@lru_cache(maxsize=None)
def shadow_sprite(w, h):
    pad = 110
    sm = Image.new("L", ((w + 2 * pad) // 4, (h + 2 * pad) // 4), 0)
    ImageDraw.Draw(sm).rounded_rectangle((pad // 4, pad // 4 + 6, (w + pad) // 4, (h + pad) // 4 + 6), radius=6, fill=255)
    sm = sm.filter(ImageFilter.GaussianBlur(11)).resize((w + 2 * pad, h + 2 * pad), Image.BILINEAR)
    out = Image.new("RGBA", sm.size, (0, 0, 0, 0))
    out.putalpha(sm.point(lambda v: int(v * 0.62)))
    return out, pad


@lru_cache(maxsize=None)
def rim_sprite(w, h, radius):
    # A lime light around a rectangle's edge (the look of a hovered option in the app).
    pad = 60
    sm = Image.new("L", (w + 2 * pad, h + 2 * pad), 0)
    ImageDraw.Draw(sm).rounded_rectangle((pad, pad, pad + w, pad + h), radius=radius, outline=255, width=5)
    blur = sm.filter(ImageFilter.GaussianBlur(14))
    core = Image.new("L", sm.size, 0)
    ImageDraw.Draw(core).rounded_rectangle((pad, pad, pad + w, pad + h), radius=radius, outline=255, width=3)
    a = Image.eval(blur, lambda v: min(255, int(v * 2.2)))
    a = Image.composite(Image.new("L", sm.size, 255), a, core)
    out = Image.new("RGBA", sm.size, LIME + (0,))
    out.putalpha(a)
    return out, pad


def faded(im, alpha):
    if alpha >= 0.999:
        return im
    out = im.copy()
    out.putalpha(im.getchannel("A").point(lambda v: int(v * alpha)))
    return out


def put(canvas, im, x, y, alpha=1.0):
    if alpha <= 0.003:
        return
    canvas.alpha_composite(faded(im, alpha), (int(round(x)), int(round(y)))) if 0 <= x and 0 <= y and x + im.width <= W and y + im.height <= H \
        else put_clipped(canvas, faded(im, alpha), int(round(x)), int(round(y)))


def put_clipped(canvas, im, x, y):
    x0, y0, x1, y1 = max(0, x), max(0, y), min(W, x + im.width), min(H, y + im.height)
    if x1 <= x0 or y1 <= y0:
        return
    canvas.alpha_composite(im, (x0, y0), (x0 - x, y0 - y, x1 - x, y1 - y))


# ---------------------------------------------------------------- the window
def cam_base():
    return BASE_S, BASE_CX - SW * BASE_S / 2, BASE_CY - SH * BASE_S / 2


def cam_region(rx, ry, rw, rh, cy=470, max_s=2.3, fit_w=1660, fit_h=760):
    s = min(fit_w / rw, fit_h / rh, max_s)
    return s, 960 - (rx + rw / 2) * s, cy - (ry + rh / 2) * s


def cam_mix(a, b, k):
    s = math.exp(lerp(math.log(a[0]), math.log(b[0]), k))
    return s, lerp(a[1], b[1], k), lerp(a[2], b[2], k)


def window(canvas, name, cam, alpha=1.0, clip=None, rim=0.0, shadow=1.0):
    """Draw a screenshot as the app's window: scale s, top left corner at (X, Y); clip = (x0, x1) on screen."""
    s, X, Y = cam
    w, h = SW * s, SH * s
    radius = 13 * s
    cx0, cx1 = (0, W) if clip is None else (max(0, clip[0]), min(W, clip[1]))
    if shadow > 0.01 and clip is None and s < 1.6:
        base, pad = shadow_sprite(int(SW * BASE_S), int(SH * BASE_S))
        k = s / BASE_S
        sp = base if abs(k - 1) < 0.002 else base.resize((max(1, int(base.width * k)), max(1, int(base.height * k))), Image.BILINEAR)
        put(canvas, sp, X - pad * k, Y - pad * k, alpha * shadow)
    dx0, dy0 = max(cx0, math.floor(X)), max(0, math.floor(Y))
    dx1, dy1 = min(cx1, math.ceil(X + w)), min(H, math.ceil(Y + h))
    if dx1 - dx0 < 1 or dy1 - dy0 < 1:
        return
    box = ((dx0 - X) / s, (dy0 - Y) / s, (dx1 - X) / s, (dy1 - Y) / s)
    box = (max(0, box[0]), max(0, box[1]), min(SW, box[2]), min(SH, box[3]))
    part = shot(name).resize((dx1 - dx0, dy1 - dy0), Image.LANCZOS, box=box)
    if s > 1.12:                                    # an enlarged screenshot: bring the letters' edges back
        part = part.filter(ImageFilter.UnsharpMask(radius=1.1 + 0.5 * (s - 1), percent=int(min(95, 55 + 45 * (s - 1))), threshold=2))
    part = part.convert("RGBA")
    # Rounded corners, drawn at twice the size so the curve is smooth.
    mask = Image.new("L", ((dx1 - dx0) * 2, (dy1 - dy0) * 2), 0)
    ImageDraw.Draw(mask).rounded_rectangle(((X - dx0) * 2, (Y - dy0) * 2, (X - dx0 + w) * 2 - 1, (Y - dy0 + h) * 2 - 1), radius=radius * 2, fill=255)
    mask = mask.resize((dx1 - dx0, dy1 - dy0), Image.BILINEAR)
    if alpha < 0.999:
        mask = mask.point(lambda v: int(v * alpha))
    part.putalpha(mask)
    canvas.alpha_composite(part, (dx0, dy0))
    if clip is None:
        edge = Image.new("RGBA", (dx1 - dx0, dy1 - dy0), (0, 0, 0, 0))
        ImageDraw.Draw(edge).rounded_rectangle((X - dx0, Y - dy0, X - dx0 + w - 1, Y - dy0 + h - 1), radius=radius, outline=EDGE + (int(255 * alpha),), width=2)
        canvas.alpha_composite(edge, (dx0, dy0))
    if rim > 0.01 and clip is None:
        sp, pad = rim_sprite(int(round(w)), int(round(h)), int(round(radius)))
        put(canvas, sp, X - pad, Y - pad, rim * alpha)


def focus(canvas, cam, rect, alpha=1.0, radius=10, grow=8, blur=6.5, dim=0.40):
    """Everything but one part of the screenshot goes soft and dark, so the eye has one place to go."""
    if alpha <= 0.01:
        return
    s, X, Y = cam
    x, y, w, h = rect
    box = (X + x * s - grow, Y + y * s - grow, X + (x + w) * s + grow, Y + (y + h) * s + grow)
    soft = canvas.filter(ImageFilter.GaussianBlur(blur * alpha))
    soft.alpha_composite(Image.new("RGBA", canvas.size, BG + (int(255 * dim * alpha),)))
    mask = Image.new("L", canvas.size, 255)
    ImageDraw.Draw(mask).rounded_rectangle(box, radius=radius * s + grow, fill=0)
    canvas.paste(soft, (0, 0), mask.filter(ImageFilter.GaussianBlur(2.5)))


def highlight(canvas, cam, rect, alpha=1.0, radius=10, grow=6):
    """A lime outline around a part of the screenshot (rect in the screenshot's own pixels)."""
    if alpha <= 0.01:
        return
    s, X, Y = cam
    x, y, w, h = rect
    w2, h2 = int(round(w * s + 2 * grow)), int(round(h * s + 2 * grow))
    sp, pad = rim_sprite(w2, h2, int(radius * s) if radius * s < min(w2, h2) / 2 else min(w2, h2) // 2)
    put(canvas, sp, X + x * s - grow - pad, Y + y * s - grow - pad, alpha)


# ---------------------------------------------------------------- words
def text_sprite(parts, kind, size, tracking=0):
    """parts: [(text, colour)] drawn on one line."""
    f = font(kind, size)
    widths = [f.getlength(t) for t, _ in parts]
    asc, desc = f.getmetrics()
    im = Image.new("RGBA", (int(sum(widths)) + 16, asc + desc + 12), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    x = 4
    for (t, colour), wd in zip(parts, widths):
        d.text((x, 4), t, font=f, fill=colour + (255,))
        x += wd
    return im


@lru_cache(maxsize=None)
def words(text, kind, size, colour):
    return text_sprite([(text, colour)], kind, size)


@lru_cache(maxsize=None)
def scrim_sprite():
    im = Image.new("RGBA", (W, 300), BG + (0,))
    a = Image.new("L", (1, 300), 0)
    for yy in range(300):
        a.putpixel((0, yy), int(246 * in_out(clamp(yy / 130))))
    im.putalpha(a.resize((W, 300)))
    return im


def scrim(canvas, cam, alpha=1.0):
    """Darkens the foot of the frame under the words while the window is zoomed in over it."""
    k = clamp((cam[0] - BASE_S - 0.03) / 0.25) * alpha
    if k > 0.01:
        put(canvas, scrim_sprite(), 0, H - 300, k)


def caption(canvas, t, t0, t1, title, sub=None, x=216, y=930):
    """A line of words under the window: up and in at t0, out before t1."""
    k_in, k_out = out_cubic(at(t, t0, t0 + 0.32)), at(t, t1 - 0.22, t1)
    a = k_in * (1 - k_out)
    if a <= 0.01:
        return
    dy = (1 - k_in) * 26
    bar = Image.new("RGBA", (7, 66 if sub else 58), LIME + (255,))
    put(canvas, bar, x, y + 6 + dy, a)
    put(canvas, words(title, "title", 58, T1), x + 24, y - 6 + dy, a)
    if sub:
        put(canvas, words(sub, "body", 29, T2), x + 26, y + 60 + dy, a * at(t, t0 + 0.12, t0 + 0.45))


@lru_cache(maxsize=None)
def chip(text, lime):
    f = font("semi", 30)
    wd = int(f.getlength(text)) + 40
    im = Image.new("RGBA", (wd, 52), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((0, 0, wd - 1, 51), radius=26, fill=(LIME if lime else (20, 26, 33)) + (255,), outline=(LIME if lime else EDGE) + (255,), width=2)
    d.text((20, 7), text, font=f, fill=((13, 26, 4) if lime else T1) + (255,))
    return im


# ---------------------------------------------------------------- the reveal line (the app's own before/after line)
@lru_cache(maxsize=None)
def line_sprite(h):
    pad = 46
    im = Image.new("L", (2 * pad + 6, h + 2 * pad), 0)
    ImageDraw.Draw(im).rectangle((pad - 3, pad, pad + 8, pad + h), fill=255)
    glow = im.filter(ImageFilter.GaussianBlur(15))
    out = Image.new("RGBA", im.size, LIME + (0,))
    out.putalpha(Image.eval(glow, lambda v: min(255, int(v * 1.8))))
    core = Image.new("RGBA", im.size, (0, 0, 0, 0))
    ImageDraw.Draw(core).rectangle((pad, pad, pad + 5, pad + h), fill=(255, 255, 255, 255))
    out.alpha_composite(core)
    # the handle
    d = ImageDraw.Draw(out)
    cy = pad + h // 2
    d.ellipse((pad + 3 - 26, cy - 26, pad + 3 + 26, cy + 26), fill=(20, 26, 33, 255), outline=LIME + (255,), width=3)
    for sx in (-1, 1):
        d.line([(pad + 3 + sx * 7, cy - 9), (pad + 3 + sx * 15, cy), (pad + 3 + sx * 7, cy + 9)], fill=(255, 255, 255, 255), width=3)
    return out, pad


def reveal(canvas, old, new, cam, k, label_old="4.2", label_new="4.3", alpha=1.0):
    """The new picture uncovers the old one from the left as k goes 0 -> 1."""
    s, X, Y = cam
    w, h = SW * s, SH * s
    x = X - 30 + (w + 60) * k
    window(canvas, old, cam, alpha=alpha)
    if k > 0:
        window(canvas, new, cam, alpha=alpha, clip=(int(X), int(min(X + w, x))), shadow=0)
        edge = Image.new("RGBA", (int(w) + 4, int(h) + 4), (0, 0, 0, 0))
        ImageDraw.Draw(edge).rounded_rectangle((0, 0, int(w) - 1, int(h) - 1), radius=13 * s, outline=EDGE + (int(255 * alpha),), width=2)
        put(canvas, edge, X, Y)
    if 0.0 < k < 1.0:
        sp, pad = line_sprite(int(h) - 8)
        put(canvas, sp, x - pad - 3, Y + 4 - pad, alpha)
    # which side is which: one label at the right of the caption row, changing as the line passes the middle
    c_old, c_new = chip(label_old, False), chip(label_new, True)
    mix = at(k, 0.42, 0.58)
    put(canvas, c_old, X + w - c_old.width, 944, alpha * (1 - mix))
    put(canvas, c_new, X + w - c_new.width, 944, alpha * mix)


# ---------------------------------------------------------------- scenes
def scene_intro(canvas, t):
    # The app's own start-up animation: a lime line opens from the centre, the mark rises over it, the
    # wordmark comes up from behind the line, one bright frame runs along the line and the edge lights up.
    away = in_out(at(t, beat(7.2), beat(8)))
    a = 1 - away
    zoom = 1 + 0.07 * away
    line_on = out_expo(at(t, 0.15, 1.05))
    mark_on = out_expo(at(t, 0.55, 1.55))
    word_on = out_cubic(at(t, beat(4) - 0.22, beat(4) + 0.16))
    sweep = in_out(at(t, beat(4), beat(4) + 0.75))
    p = (t - beat(4)) / 1.3
    edge = 0 if p <= 0 or p >= 1 else (out_cubic(p / 0.12) if p < 0.12 else (1 - (p - 0.12) / 0.88) ** 2)
    cx, cy = 960, 500
    lw = 760 * line_on * zoom
    if lw > 2:
        glow = Image.new("L", (int(lw) + 120, 120), 0)
        ImageDraw.Draw(glow).rounded_rectangle((60, 56, 60 + int(lw), 63), radius=3, fill=255)
        gl = glow.filter(ImageFilter.GaussianBlur(16))
        g = Image.new("RGBA", glow.size, LIME + (0,))
        g.putalpha(Image.eval(gl, lambda v: min(255, int(v * (1.1 + 1.6 * edge)))))
        ImageDraw.Draw(g).rounded_rectangle((60, 57, 60 + int(lw), 62), radius=3, fill=LIME + (255,))
        put(canvas, g, cx - lw / 2 - 60, cy - 60, a)
        if 0 < sweep < 1:
            rw = 150
            run = Image.new("RGBA", (rw, 12), (255, 255, 255, 0))
            ra = Image.new("L", (rw, 12), 0)
            for i in range(rw):
                v = int(255 * math.sin(math.pi * i / (rw - 1)) ** 2 * math.sin(math.pi * sweep))
                ImageDraw.Draw(ra).line([(i, 2), (i, 9)], fill=v)
            run.putalpha(ra)
            put(canvas, run, cx - lw / 2 + (lw - rw) * sweep, cy - 6, a)
    if mark_on > 0.01:
        mw = int(230 * zoom * (0.86 + 0.14 * mark_on))
        m = Image.open(os.path.join(HERE, "assets", "logo-mark.png")).convert("RGBA")
        m = m.resize((mw, int(mw * m.height / m.width)), Image.LANCZOS)
        put(canvas, m, cx - mw / 2, cy - 44 * zoom - m.height + (1 - mark_on) * 40, a * mark_on)
    if word_on > 0.01:
        sp = text_sprite([("Push", (255, 255, 255)), ("FPS", LIME)], "title", int(118 * zoom))
        hh = int(sp.height * word_on)
        if hh > 2:
            part = sp.crop((0, sp.height - hh, sp.width, sp.height))
            put(canvas, part, cx - sp.width / 2, cy + 26 * zoom, a * word_on)
    k = out_cubic(at(t, beat(6), beat(6) + 0.3))
    if k > 0.01:
        sp = text_sprite([("4.3", LIME), ("   a new design", T1)], "title", 64)
        put(canvas, sp, cx - sp.width / 2, cy + 176 + (1 - k) * 22, a * k)
    if edge > 0.01:
        fr = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        ImageDraw.Draw(fr).rounded_rectangle((5, 5, W - 6, H - 6), radius=18, outline=LIME + (int(235 * edge * a),), width=4)
        canvas.alpha_composite(fr)


def scene_compare(canvas, t, b0, old, new, cap_old, cap_new):
    cam = cam_base()
    enter = out_cubic(at(t, beat(b0), beat(b0) + 0.35))
    s, X, Y = cam
    cam = (s, X, Y + (1 - enter) * 40)
    k = in_out_cubic(at(t, beat(b0 + 3.6), beat(b0 + 5.6)))
    leave = at(t, beat(b0 + 8) - 0.2, beat(b0 + 8))
    reveal(canvas, old, new, cam, k, alpha=enter * (1 - leave))
    caption(canvas, t, beat(b0) + 0.1, beat(b0 + 4.4), *cap_old)
    caption(canvas, t, beat(b0 + 4.6), beat(b0 + 8), *cap_new)


def scene_tour(canvas, t):
    b0 = 24
    stops = [  # (from beat, region in the screenshot, title, sub, what lights up)
        (24.0, (0, 40, 232, 345), "Every mode, by name", "Home, Pro, Streaming, Video playback, Export, Minimal, Settings", (10, 58, 190, 306)),
        (27.0, (514, 50, 660, 570), "The picture, on the home page", "With the tools right under it", (526, 60, 636, 360)),
        (29.5, (1160, 50, 436, 400), "All of quick setup beside it", "Change a setting while you watch what it does", (1174, 62, 410, 392)),
    ]
    cams = [cam_base()] + [cam_region(*r[1], max_s=2.0) for r in stops]
    starts = [r[0] for r in stops] + [32.0]
    cam = cams[0]
    for i in range(3):
        k = in_out_cubic(at(t, beat(starts[i]), beat(starts[i]) + 0.62))
        cam = cam_mix(cam, cams[i + 1], k) if t >= beat(starts[i]) else cam
    enter = out_cubic(at(t, beat(b0), beat(b0) + 0.3))
    leave = at(t, beat(32) - 0.18, beat(32))
    window(canvas, "Vn02-home-playing", cam, alpha=enter * (1 - leave), shadow=clamp(1.7 - cam[0]))
    for i, (b, _, title, sub, lit) in enumerate(stops):
        a = at(t, beat(b) + 0.5, beat(b) + 0.8) * (1 - at(t, beat(starts[i + 1]) - 0.15, beat(starts[i + 1])))
        focus(canvas, cam, lit, a)
        highlight(canvas, cam, lit, a * 0.9)
    scrim(canvas, cam, enter * (1 - leave))
    for i, (b, _, title, sub, lit) in enumerate(stops):
        caption(canvas, t, beat(b) + 0.25, beat(starts[i + 1]), title, sub)


def scene_modes(canvas, t):
    cam = cam_base()
    for i, (name, b) in enumerate((("Vn10-studio", 32), ("Vn11-watch", 34))):
        k = out_cubic(at(t, beat(b), beat(b) + 0.34))
        out = at(t, beat(b + 2) - 0.16, beat(b + 2))
        if k <= 0 or out >= 1:
            continue
        s, X, Y = cam
        window(canvas, name, (s, X + (1 - k) * 90, Y), alpha=k * (1 - out))
    caption(canvas, t, beat(32) + 0.1, beat(36), "Streaming, video playback, export", "Every mode in the same window")


def scene_hidden(canvas, t):
    top = cam_region(1166, 54, 426, 300, max_s=2.2)
    cam = cam_mix(cam_base(), top, in_out_cubic(at(t, beat(36) + 0.1, beat(36) + 0.75)))
    cam = cam_mix(cam, (cam[0] * 1.06, cam[1] - 1375 * cam[0] * 0.06, cam[2] - 150 * cam[0] * 0.06), in_out(at(t, beat(37), beat(44))))
    enter = out_cubic(at(t, beat(36), beat(36) + 0.3))
    leave = at(t, beat(44) - 0.18, beat(44))
    window(canvas, "Vn01-home", cam, alpha=enter * (1 - leave), shadow=clamp(1.7 - cam[0]))
    focus(canvas, cam, (1178, 62, 404, 138), at(t, beat(37.4), beat(37.8)) * (1 - at(t, beat(44) - 0.2, beat(44))), radius=12, grow=2)
    highlight(canvas, cam, (1184, 97, 268, 98), at(t, beat(37.4), beat(37.8)) * (1 - at(t, beat(40) - 0.1, beat(40) + 0.1)), radius=12, grow=4)
    highlight(canvas, cam, (1190, 133, 146, 28), at(t, beat(40.2), beat(40.6)) * (1 - at(t, beat(44) - 0.2, beat(44))), radius=14, grow=4)
    scrim(canvas, cam, enter * (1 - leave))
    caption(canvas, t, beat(36) + 0.3, beat(40), "Nothing hidden", "An index over quick setup: every part, one click away")
    caption(canvas, t, beat(40) + 0.25, beat(44), "AI picture enhancement", "By name, at the top, one click away")


def scene_pro(canvas, t):
    s, X, Y = cam_base()
    k = 1.06                                        # a little nearer, growing from the window's bottom right corner
    near = (s * k, X + SW * s - SW * s * k, Y + SH * s - SH * s * k)
    cam = cam_mix((s, X, Y), near, in_out_cubic(at(t, beat(47), beat(47) + 0.9)))
    enter = out_cubic(at(t, beat(44), beat(44) + 0.3))
    leave = at(t, beat(52) - 0.18, beat(52))
    window(canvas, "Vn08-pro", cam, alpha=enter * (1 - leave))
    lit = at(t, beat(48.2), beat(48.6)) * (1 - at(t, beat(52) - 0.2, beat(52)))
    focus(canvas, cam, (1166, 100, 426, 772), lit, radius=12, grow=2, blur=4.5, dim=0.34)
    highlight(canvas, cam, (1172, 108, 412, 35), lit, radius=10, grow=4)
    scrim(canvas, cam, enter * (1 - leave))
    caption(canvas, t, beat(44) + 0.2, beat(48), "Pro mode, same power", "Every tab is still there")
    caption(canvas, t, beat(48) + 0.1, beat(52), "New: the All tab", "Every setting of every tab in one list")


def scene_themes(canvas, t):
    names = ["Vn01-home", "Vn03-home-light", "Vn04-home-black", "Vn05-home-midnight-ice", "Vn06-home-plum-violet", "Vn07-home-forest-amber", "Vn01-home", "Vn01-home"]
    i = min(7, max(0, int((t - beat(52)) / BEAT)))
    local = t - beat(52 + i)
    pop = 1 + 0.018 * (1 - out_cubic(clamp(local / 0.22)))
    s, X, Y = cam_base()
    cam = (s * pop, X - SW * s * (pop - 1) / 2, Y - SH * s * (pop - 1) / 2)
    leave = at(t, beat(60) - 0.18, beat(60))
    window(canvas, names[i], cam, alpha=1 - leave, rim=(1 - clamp(local / 0.3)) * 0.8 if i < 6 else 0)
    caption(canvas, t, beat(52) + 0.05, beat(60), "Dark, light, black, three tinted themes", "And seven accent colours")


def scene_legacy(canvas, t):
    dialog = cam_region(470, 215, 660, 470, max_s=1.75)
    cam = cam_mix(cam_base(), dialog, in_out_cubic(at(t, beat(60) + 0.15, beat(60) + 0.8)))
    enter = out_cubic(at(t, beat(60), beat(60) + 0.25))
    if t < beat(64):
        window(canvas, "Vn13-appearance", cam, alpha=enter, shadow=clamp(1.7 - cam[0]))
        focus(canvas, cam, (490, 232, 620, 438), at(t, beat(60) + 0.5, beat(60) + 0.9), radius=14, grow=0, blur=5, dim=0.25)
        highlight(canvas, cam, (512, 320, 566, 46), at(t, beat(61.6), beat(62.0)) * (1 - at(t, beat(64) - 0.15, beat(64))), radius=10, grow=3)
        scrim(canvas, cam, enter)
    else:
        k = in_out_cubic(at(t, beat(64), beat(64) + 0.7))
        leave = at(t, beat(68) - 0.18, beat(68))
        reveal(canvas, "Vn01-home", "Vn14-legacy-home", cam_base(), k, label_old="New", label_new="Legacy", alpha=1 - leave)
    caption(canvas, t, beat(60) + 0.2, beat(64), "Prefer the design you know?", "The palette opens Appearance: Design, New or Legacy")
    caption(canvas, t, beat(64) + 0.3, beat(68), "Legacy is one click away", "The same features, the same settings")


ALSO = [("Upscale to my screen", "One switch for a sharp picture in full screen"),
        ("Before/after line", "Drag it in full screen, swap its sides"),
        ("Colour above 100%", "AI picture enhancement that keeps its colours"),
        ("A wider panel", "Stretch Pro mode's settings over the picture"),
        ("All of quick setup at home", "Smooth pacing, low latency, auto connect"),
        ("Fixed", "The enhancement switch in Pro mode, the update buttons")]


@lru_cache(maxsize=None)
def also_card(i):
    title, sub = ALSO[i]
    w, h = 540, 190
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((0, 0, w - 1, h - 1), radius=18, fill=(20, 26, 33, 255), outline=EDGE + (255,), width=2)
    d.rounded_rectangle((26, 30, 32, 86), radius=3, fill=LIME + (255,))
    d.text((50, 22), title, font=font("title", 46), fill=T1 + (255,))
    f = font("body", 27)
    line, y = "", 100
    for wd in sub.split():
        if f.getlength(line + " " + wd) > w - 80 and line:
            d.text((50, y), line, font=f, fill=T2 + (255,))
            line, y = wd, y + 36
        else:
            line = (line + " " + wd).strip()
    d.text((50, y), line, font=f, fill=T2 + (255,))
    return im


def scene_also(canvas, t):
    enter = out_cubic(at(t, beat(68), beat(68) + 0.3))
    leave = at(t, beat(80) - 0.2, beat(80))
    a = enter * (1 - leave)
    head = text_sprite([("Also new ", T1), ("since 4.2", LIME)], "title", 78)
    put(canvas, head, 960 - head.width / 2, 212 + (1 - enter) * 24, a)
    for i in range(6):
        b = 69 + i * 1.75
        k = out_cubic(at(t, beat(b), beat(b) + 0.3))
        if k <= 0.01:
            continue
        card = also_card(i)
        col, rowi = i % 3, i // 3
        x = 960 - (3 * 540 + 2 * 36) / 2 + col * (540 + 36)
        y = 392 + rowi * (190 + 36) + (1 - k) * 30
        put(canvas, card, x, y, a * k)
        lit = (1 - clamp((t - beat(b)) / 0.5)) * 0.9
        if lit > 0.02:
            sp, pad = rim_sprite(540, 190, 18)
            put(canvas, sp, x - pad, y - pad, a * lit)


def scene_outro(canvas, t):
    t0 = beat(80)
    k = out_expo(at(t, t0, t0 + 0.7))
    fade = 1 - at(t, DURATION - 0.9, DURATION - 0.15)
    logo = Image.open(os.path.join(HERE, "assets", "logo-full.png")).convert("RGBA")
    lw = int(880 * (1.1 - 0.1 * k))
    logo = logo.resize((lw, int(lw * logo.height / logo.width)), Image.LANCZOS)
    put(canvas, logo, 960 - lw / 2, 430 - logo.height / 2 - 40, k * fade)
    line_on = out_expo(at(t, t0 + 0.25, t0 + 1.1))
    lwid = int(760 * line_on)
    if lwid > 4:
        ln = Image.new("RGBA", (lwid, 5), LIME + (255,))
        put(canvas, ln, 960 - lwid / 2, 590, fade)
    k2 = out_cubic(at(t, beat(84), beat(84) + 0.35))
    if k2 > 0.01:
        sp = text_sprite([("4.3", LIME), ("  is out", T1)], "title", 92)
        put(canvas, sp, 960 - sp.width / 2, 630 + (1 - k2) * 24, k2 * fade)
        k3 = out_cubic(at(t, beat(85.5), beat(85.5) + 0.35))
        sp2 = words("Free and open source   github.com/DosukaSOL/PushFPS", "body", 36, T2)
        put(canvas, sp2, 960 - sp2.width / 2, 770 + (1 - k3) * 18, k3 * fade)
    p = (t - beat(84)) / 1.2
    edge = 0 if p <= 0 or p >= 1 else (out_cubic(p / 0.12) if p < 0.12 else (1 - (p - 0.12) / 0.88) ** 2)
    if edge > 0.01:
        fr = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        ImageDraw.Draw(fr).rounded_rectangle((5, 5, W - 6, H - 6), radius=18, outline=LIME + (int(235 * edge * fade),), width=4)
        canvas.alpha_composite(fr)


def frame(t):
    canvas = background().copy()
    if t < beat(8):
        scene_intro(canvas, t)
    if beat(8) - 0.05 <= t < beat(16):
        scene_compare(canvas, t, 8, "Vo1-home", "Vn01-home", ("The design you know", "PushFPS 4.2"), ("The new design", "Everything in one window"))
    if beat(16) <= t < beat(24):
        scene_compare(canvas, t, 16, "Vo2-pro", "Vn08-pro", ("Pro mode, before", None), ("Pro mode, now", "Navigation by name, live figures at the bottom"))
    if beat(24) <= t < beat(32):
        scene_tour(canvas, t)
    if beat(32) <= t < beat(36):
        scene_modes(canvas, t)
    if beat(36) <= t < beat(44):
        scene_hidden(canvas, t)
    if beat(44) <= t < beat(52):
        scene_pro(canvas, t)
    if beat(52) <= t < beat(60):
        scene_themes(canvas, t)
    if beat(60) <= t < beat(68):
        scene_legacy(canvas, t)
    if beat(68) <= t < beat(80):
        scene_also(canvas, t)
    if t >= beat(80):
        scene_outro(canvas, t)
    return canvas.convert("RGB")


def frame_bytes(i):
    return frame(i / FPS).tobytes()


# The film in parts, each its own file with the same encoder settings: a change to one scene is one part
# rendered again, and the parts are joined without encoding anything twice. No scene fades into another
# part's pictures, so a cut between two files is a cut in the film.
PARTS = [("intro", 0, 8), ("compare", 8, 24), ("tour", 24, 36), ("index", 36, 52), ("themes", 52, 68), ("also", 68, 80), ("outro", 80, None)]
ENCODE = ["-c:v", "libx264", "-preset", "slow", "-crf", "14", "-pix_fmt", "yuv420p", "-profile:v", "high", "-level", "4.2",
          "-color_primaries", "bt709", "-color_trc", "bt709", "-colorspace", "bt709", "-color_range", "tv",
          "-g", "60", "-bf", "2", "-movflags", "+faststart"]


def part_frames(i):
    total = int(round(DURATION * FPS))
    first = lambda b: total if b is None else int(math.ceil(beat(b) * FPS - 1e-6))
    name, b0, b1 = PARTS[i]
    return name, (0 if i == 0 else first(b0)), first(b1)


def render_part(i, folder, jobs):
    name, f0, f1 = part_frames(i)
    out = os.path.join(folder, f"part{i}-{name}.mp4")
    tmp = os.path.join(folder, f"part{i}-{name}.tmp.mp4")
    cmd = ["ffmpeg", "-v", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{W}x{H}", "-r", str(FPS), "-i", "-",
           "-vf", "scale=in_range=full:out_range=tv:out_color_matrix=bt709,format=yuv420p"] + ENCODE + [tmp]
    enc = subprocess.Popen(cmd, stdin=subprocess.PIPE)
    with Pool(jobs) as pool:
        for n, data in enumerate(pool.imap(frame_bytes, range(f0, f1), chunksize=4)):
            enc.stdin.write(data)
    enc.stdin.close()
    if enc.wait() != 0:
        sys.exit(f"{name}: the encoder failed")
    os.replace(tmp, out)
    print(f"{name}: frames {f0}..{f1 - 1} -> {out}", flush=True)
    return out


def join(folder, audio, out):
    files = []
    for i in range(len(PARTS)):
        f = os.path.join(folder, f"part{i}-{PARTS[i][0]}.mp4")
        if not os.path.exists(f):
            sys.exit(f"missing {f}")
        files.append(f)
    lst = os.path.join(folder, "parts.txt")
    with open(lst, "w") as fh:
        for f in files:
            fh.write(f"file '{os.path.abspath(f)}'\n")
    silent = os.path.join(folder, "silent.tmp.mp4")
    subprocess.check_call(["ffmpeg", "-v", "error", "-y", "-f", "concat", "-safe", "0", "-i", lst, "-c", "copy", "-movflags", "+faststart", silent])
    if audio:
        tmp = out + ".tmp.mp4"
        subprocess.check_call(["ffmpeg", "-v", "error", "-y", "-i", silent, "-i", audio, "-map", "0:v", "-map", "1:a", "-c:v", "copy",
                               "-c:a", "aac", "-b:a", "320k", "-ar", "48000", "-t", f"{DURATION:.3f}", "-movflags", "+faststart", tmp])
        os.replace(tmp, out)
    else:
        os.replace(silent, out)
    print("joined ->", out, flush=True)


def main():
    args = sys.argv[1:]
    jobs = int(args[args.index("--jobs") + 1]) if "--jobs" in args else (os.cpu_count() or 2)
    if args and args[0] == "--frame":
        frame(float(args[1])).save(args[2])
    elif args and args[0] == "--parts":            # --parts <folder> [numbers...]
        folder = args[1]
        os.makedirs(folder, exist_ok=True)
        rest = args[2:args.index("--jobs")] if "--jobs" in args else args[2:]
        which = [int(a) for a in rest] or list(range(len(PARTS)))
        for i in which:
            render_part(i, folder, jobs)
    elif args and args[0] == "--join":             # --join <folder> <out.mp4> [--audio music]
        join(args[1], args[args.index("--audio") + 1] if "--audio" in args else None, args[2])
    elif args and args[0] == "--list":
        for i in range(len(PARTS)):
            print(i, *part_frames(i))


if __name__ == "__main__":
    main()
