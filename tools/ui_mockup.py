"""Mockups of a redesigned interface, drawn over shots of the bare world.

Nothing here is in the game: it is a picture of where the interface could go,
to be argued with before any of it is built.

    python tools/ui_mockup.py <dir of bare-world shots> <out dir>

The backdrops come from tools/harness/bare_world.txt.
"""
import json
import os
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
W, H = 1280, 720

INK = (16, 15, 14)
PANEL = (27, 25, 23)
PANEL_HI = (38, 35, 31)
SLOT = (20, 19, 18)
LINE = (78, 70, 58)
LINE_DIM = (52, 47, 40)
TEXT = (230, 222, 204)
DIM = (156, 148, 132)
FAINT = (104, 98, 88)
AMBER = (234, 186, 98)
RED = (216, 86, 70)
ORANGE = (226, 142, 72)
GREEN = (128, 188, 112)
BLUE = (108, 172, 216)
VIOLET = (164, 148, 214)

FONT = ImageFont.load_default()
SHEET = Image.open(os.path.join(ROOT, "assets", "sprites", "spritesheet.png")).convert("RGBA")
ATLAS = json.load(open(os.path.join(ROOT, "assets", "sprites", "spritesheet.json")))["sprites"]


# --- drawing helpers ---------------------------------------------------------

def text_width(s, scale=2):
    return int(FONT.getlength(s)) * scale


def text(img, x, y, s, color=TEXT, scale=2, shadow=True, align="left"):
    w = int(FONT.getlength(s)) + 2
    mask = Image.new("L", (w, 14), 0)
    ImageDraw.Draw(mask).text((0, 0), s, fill=255, font=FONT)
    mask = mask.point(lambda v: 255 if v > 100 else 0).resize((w * scale, 14 * scale), Image.NEAREST)
    if align == "center":
        x -= text_width(s, scale) // 2
    elif align == "right":
        x -= text_width(s, scale)
    if shadow:
        img.paste((0, 0, 0, 255), (int(x) + scale, int(y) + scale), mask)
    img.paste(color + (255,), (int(x), int(y)), mask)
    return text_width(s, scale)


def rect(img, x, y, w, h, color, alpha=255):
    if w <= 0 or h <= 0:
        return
    layer = Image.new("RGBA", (int(w), int(h)), color + (alpha,))
    img.alpha_composite(layer, (int(x), int(y)))


def frame(img, x, y, w, h, color=LINE):
    rect(img, x, y, w, 1, color)
    rect(img, x, y + h - 1, w, 1, color)
    rect(img, x, y, 1, h, color)
    rect(img, x + w - 1, y, 1, h, color)


def panel(img, x, y, w, h, alpha=236, accent=None, fill=PANEL):
    """A plate with clipped corners, a lit top edge and a hard drop shadow."""
    rect(img, x + 3, y + 3, w, h, (0, 0, 0), 90)
    rect(img, x + 2, y, w - 4, h, fill, alpha)
    rect(img, x, y + 2, w, h - 4, fill, alpha)
    rect(img, x + 1, y + 1, w - 2, h - 2, fill, alpha)
    # Border, corners stepped in.
    rect(img, x + 2, y, w - 4, 1, LINE)
    rect(img, x + 2, y + h - 1, w - 4, 1, LINE_DIM)
    rect(img, x, y + 2, 1, h - 4, LINE)
    rect(img, x + w - 1, y + 2, 1, h - 4, LINE_DIM)
    for cx, cy in ((x + 1, y + 1), (x + w - 2, y + 1), (x + 1, y + h - 2), (x + w - 2, y + h - 2)):
        rect(img, cx, cy, 1, 1, LINE)
    rect(img, x + 2, y + 1, w - 4, 1, PANEL_HI, 200)
    if accent:
        rect(img, x + 2, y, w - 4, 2, accent)


def sprite(img, name, x, y, scale=2):
    s = ATLAS[name]
    cell = SHEET.crop((s["x"], s["y"], s["x"] + s["w"], s["y"] + s["h"]))
    cell = cell.resize((s["w"] * scale, s["h"] * scale), Image.NEAREST)
    img.alpha_composite(cell, (int(x), int(y)))


def glyph(img, x, y, rows, color, scale=2):
    """A tiny icon from rows of '#' and '.'."""
    for j, row in enumerate(rows):
        for i, ch in enumerate(row):
            if ch == "#":
                rect(img, x + i * scale, y + j * scale, scale, scale, color)
            elif ch == "+":
                rect(img, x + i * scale, y + j * scale, scale, scale, tuple(int(c * 0.6) for c in color))


ICON = {
    "heart": [".##.##.", "#######", "#######", ".#####.", "..###..", "...#..."],
    "bolt": ["...##", "..##.", ".####", "..##.", ".##..", "#...."],
    "drop": ["...#...", "..###..", ".#####.", ".#####.", ".#####.", "..###.."],
    "food": ["#.#.#..", "#.#.#..", ".###...", "..#....", "..#....", "..#...."],
    "sleep": ["####...", "..#....", ".#..###", "####.#.", "....###", "......."],
    "temp": ["..#..", ".#.#.", ".#.#.", ".###.", "#####", ".###."],
    "skull": [".#####.", "#######", "#.###.#", "#######", ".##.##.", ".#.#.#."],
    "sun": ["#..#..#", ".#####.", ".#####.", "#######", ".#####.", "#..#..#"],
    "moon": ["..###..", ".###...", "###....", "###....", ".###..#", "..####."],
    "cloud": [".......", "..##...", ".####.#", "#######", "#######", "......."],
    "rain": ["..##...", ".####.#", "#######", "#######", ".#.#.#.", "#.#.#.."],
    "check": [".....#", "....##", "#..##.", "####..", ".##...", "......"],
    "lock": [".###.", "#...#", "#####", "##.##", "##.##", "#####"],
    "pack": [".###.", "#####", "#.#.#", "#####", "#####", ".###."],
    "eye": [".......", ".#####.", "##.#.##", "##.#.##", ".#####.", "......."],
    "ear": ["..###..", ".#...#.", ".#.#.#.", ".#..#..", "..#....", "...##.."],
}


def keycap(img, x, y, label, scale=1):
    w = text_width(label, scale) + 8
    h = 12 * scale + 4
    rect(img, x, y + 2, w, h, (0, 0, 0), 140)
    rect(img, x, y, w, h, (58, 54, 48))
    rect(img, x, y, w, 1, (96, 90, 80))
    rect(img, x, y + h - 2, w, 2, (36, 33, 30))
    text(img, x + 4, y + 1, label, TEXT, scale, shadow=False)
    return w


def hint(img, x, y, key, label, scale=1, color=DIM):
    w = keycap(img, x, y, key, scale)
    w += 5 + text(img, x + w + 5, y + 2, label, color, scale)
    return w + 14


def bar(img, x, y, w, h, frac, color, segments=0, back=(44, 40, 36)):
    rect(img, x - 1, y - 1, w + 2, h + 2, INK)
    rect(img, x, y, w, h, back)
    fill = int(round(w * max(0.0, min(1.0, frac))))
    rect(img, x, y, fill, h, color)
    rect(img, x, y, fill, max(1, h // 4), tuple(min(255, c + 36) for c in color))
    if segments:
        for i in range(1, segments):
            rect(img, x + w * i // segments, y, 1, h, INK)


def slot(img, x, y, size=44, selected=False, key=None, item=None, count=None, wear=None, dim=False):
    rect(img, x + 2, y + 2, size, size, (0, 0, 0), 110)
    rect(img, x, y, size, size, SLOT, 240)
    frame(img, x, y, size, size, AMBER if selected else LINE_DIM)
    if selected:
        frame(img, x - 1, y - 1, size + 2, size + 2, AMBER)
        rect(img, x + 1, y + 1, size - 2, size - 2, (60, 48, 26), 120)
    if item:
        s = ATLAS[item]
        scale = 1 if size < 40 else (2 if s["w"] * 2 <= size else 1)
        cell = SHEET.crop((s["x"], s["y"], s["x"] + s["w"], s["y"] + s["h"]))
        cell = cell.resize((s["w"] * scale, s["h"] * scale), Image.NEAREST)
        if dim:
            cell.putalpha(cell.getchannel("A").point(lambda v: v // 3))
        img.alpha_composite(cell, (int(x + (size - cell.width) // 2), int(y + (size - cell.height) // 2)))
    if key:
        text(img, x + 3, y + 1, key, AMBER if selected else FAINT, 1, shadow=False)
    if count:
        text(img, x + size - 3, y + size - 14, str(count), TEXT, 1, align="right")
    if wear is not None:
        c = GREEN if wear > 0.5 else (ORANGE if wear > 0.25 else RED)
        rect(img, x + 4, y + size - 5, size - 8, 2, INK)
        rect(img, x + 4, y + size - 5, int((size - 8) * wear), 2, c)


def backdrop(path, darken=0):
    img = Image.open(path).convert("RGBA").resize((W, H), Image.NEAREST)
    if darken:
        rect(img, 0, 0, W, H, (8, 8, 10), darken)
    return img


def tag(img, x, y, label, color, scale=1):
    w = text_width(label, scale) + 8
    rect(img, x, y, w, 12 * scale + 2, tuple(int(c * 0.28) for c in color), 235)
    rect(img, x, y, 2, 12 * scale + 2, color)
    text(img, x + 5, y, label, color, scale, shadow=False)
    return w


# --- shared pieces -----------------------------------------------------------

HOTBAR = [("item_knife", None, 0.82), ("item_bandage", 3, None), ("item_canned_food", 2, None),
          ("item_water_bottle", 1, None), ("item_bat", None, 0.34), ("item_flashlight", None, None), None, None]


def hotbar(img, selected=0, label="Knife", y=660):
    size, gap = 44, 4
    total = 8 * size + 7 * gap
    x0 = (W - total) // 2
    panel(img, x0 - 8, y - 8, total + 16, size + 16, alpha=200)
    for i, entry in enumerate(HOTBAR):
        x = x0 + i * (size + gap)
        lift = 6 if i == selected else 0
        if entry:
            slot(img, x, y - lift, size, i == selected, str(i + 1), entry[0], entry[1], entry[2])
        else:
            slot(img, x, y, size, False, str(i + 1))
    # What is in hand, named over its slot.
    cx = x0 + selected * (size + gap) + size // 2
    w = text_width(label, 2) + 16
    panel(img, cx - w // 2, y - 46, w, 28, alpha=220, accent=AMBER)
    text(img, cx, y - 43, label, TEXT, 2, align="center")


def vitals(img, x, y, health=1.0, stamina=1.0, needs=None, moods=()):
    """Bottom-left: health and stamina, the four needs as gauges, moods as chips."""
    needs = needs or (("drop", 0.8, BLUE), ("food", 0.7, ORANGE), ("sleep", 0.9, VIOLET), ("temp", 0.6, GREEN))
    panel(img, x, y, 250, 86, alpha=214)
    glyph(img, x + 10, y + 11, ICON["heart"], RED if health < 0.35 else (212, 96, 88), 2)
    bar(img, x + 32, y + 11, 168, 12, health, RED if health < 0.35 else (196, 78, 70), segments=10)
    text(img, x + 240, y + 6, str(int(health * 100)), TEXT, 2, align="right")
    glyph(img, x + 12, y + 31, ICON["bolt"], (226, 200, 96), 2)
    bar(img, x + 32, y + 33, 168, 6, stamina, (212, 184, 88))
    text(img, x + 240, y + 28, str(int(stamina * 100)), DIM, 1, align="right")
    rect(img, x + 8, y + 48, 234, 1, LINE_DIM)
    for i, (icon, frac, color) in enumerate(needs):
        nx = x + 10 + i * 59
        low = frac < 0.3
        c = RED if low else color
        if low:
            rect(img, nx - 3, y + 53, 54, 28, (120, 34, 28), 120)
            frame(img, nx - 3, y + 53, 54, 28, RED)
        glyph(img, nx, y + 59, ICON[icon], c, 2)
        # A short upright gauge that drains downward.
        rect(img, nx + 20, y + 56, 8, 22, INK)
        rect(img, nx + 21, y + 57, 6, 20, (44, 40, 36))
        fill = int(20 * frac)
        rect(img, nx + 21, y + 77 - fill, 6, fill, c)
        text(img, nx + 32, y + 62, "%d" % int(frac * 100), c if low else DIM, 1)
    # Moods stack upward from the plate, worst at the bottom where the eye is.
    my = y - 22
    for label, color in moods:
        tag(img, x, my, label, color, 1)
        my -= 18


def clock(img, x, y, day=1, time="12:25", frac=0.52, weather="rain", weather_text="Storm  13 C", infection=0.02,
          dark_in="Dark in 7h 35m"):
    panel(img, x, y, 236, 92, alpha=214)
    text(img, x + 12, y + 6, "DAY %d" % day, AMBER, 2)
    text(img, x + 224, y + 6, time, TEXT, 2, align="right")
    # The day as a track: sun by day, moon by night, a notch at dusk.
    tx, ty, tw = x + 12, y + 37, 212
    rect(img, tx, ty, tw, 4, (40, 44, 62))
    rect(img, tx + int(tw * 0.25), ty, int(tw * 0.5), 4, (176, 150, 84))
    rect(img, tx + int(tw * 0.22), ty, int(tw * 0.03), 4, (150, 96, 70))
    rect(img, tx + int(tw * 0.75), ty, int(tw * 0.03), 4, (150, 96, 70))
    mx = tx + int(tw * frac)
    rect(img, mx - 1, ty - 4, 3, 12, TEXT)
    glyph(img, tx - 1, ty + 8, ICON["moon"], (128, 136, 176), 1)
    glyph(img, tx + tw // 2 - 3, ty + 8, ICON["sun"], (212, 180, 96), 1)
    glyph(img, tx + tw - 7, ty + 8, ICON["moon"], (128, 136, 176), 1)
    text(img, x + 224, y + 45, dark_in, DIM, 1, align="right")
    rect(img, x + 8, y + 62, 220, 1, LINE_DIM)
    glyph(img, x + 12, y + 70, ICON[weather], BLUE, 2)
    text(img, x + 32, y + 68, weather_text, TEXT, 1)
    glyph(img, x + 128, y + 70, ICON["skull"], (176, 110, 96), 2)
    bar(img, x + 150, y + 73, 46, 5, max(0.04, infection), (176, 80, 66))
    text(img, x + 224, y + 68, "%d%%" % int(infection * 100), DIM, 1, align="right")


def objective(img, x, y, title, sub, steps, color=AMBER):
    h = 44 + 18 * len(steps)
    panel(img, x, y, 286, h, alpha=206)
    rect(img, x, y + 2, 3, h - 4, color)
    text(img, x + 12, y + 5, title, color, 2)
    text(img, x + 12, y + 27, sub, DIM, 1)
    for i, (label, done, progress) in enumerate(steps):
        sy = y + 44 + i * 18
        frame(img, x + 12, sy + 2, 10, 10, GREEN if done else LINE)
        if done:
            glyph(img, x + 13, sy + 3, ICON["check"], GREEN, 1)
        text(img, x + 28, sy, label, FAINT if done else TEXT, 1)
        if progress:
            text(img, x + 274, sy, progress, GREEN if done else DIM, 1, align="right")


def minimap(img, x, y, size=150, label="Residential", sub="2% infected"):
    panel(img, x, y, size + 12, size + 34, alpha=220)
    # A stand-in map: the look of the real one is not the point here.
    rect(img, x + 6, y + 6, size, size, (30, 40, 30))
    for i in range(0, size, 30):
        rect(img, x + 6 + i, y + 6, 6, size, (50, 52, 58))
        rect(img, x + 6, y + 6 + i, size, 6, (50, 52, 58))
    for bx, by, bw, bh in ((12, 12, 14, 10), (44, 40, 12, 14), (72, 14, 16, 10), (104, 72, 14, 14), (40, 100, 18, 12),
                           (100, 104, 12, 12), (74, 70, 12, 10)):
        rect(img, x + 6 + bx, y + 6 + by, bw, bh, (120, 96, 80))
    rect(img, x + 6 + 76, y + 6 + 84, 5, 5, AMBER)
    frame(img, x + 6 + 56, y + 6 + 68, 46, 36, (200, 200, 190))
    frame(img, x + 6, y + 6, size, size, LINE_DIM)
    text(img, x + size - 2, y + 8, "N", DIM, 1, align="right")
    text(img, x + 8, y + size + 9, label, TEXT, 1)
    text(img, x + size + 4, y + size + 9, sub, (200, 130, 110), 1, align="right")
    rect(img, x + 8, y + size + 24, size - 4, 3, (44, 40, 36))
    rect(img, x + 8, y + size + 24, 6, 3, (176, 80, 66))


def prompt(img, x, y, entries):
    """Things you can do to what you are facing, floated beside it."""
    w = sum(text_width(k, 1) + 8 + 5 + text_width(v, 1) + 14 for k, v in entries) + 6
    panel(img, x - w // 2, y, w, 26, alpha=220)
    cx = x - w // 2 + 8
    for key, label in entries:
        cx += hint(img, cx, y + 5, key, label, 1, TEXT)


# --- the three mockups -------------------------------------------------------

def mock_hud(shots, out):
    img = backdrop(os.path.join(shots, "03_street_dead.png"))
    vitals(img, 16, 618, health=0.62, stamina=0.33,
           needs=(("drop", 0.18, BLUE), ("food", 0.42, ORANGE), ("sleep", 0.66, VIOLET), ("temp", 0.24, GREEN)),
           moods=(("Bleeding", RED), ("Panicking", ORANGE), ("Soaked", BLUE)))
    objective(img, 16, 16, "Secure the house", "Day 1  -  before dark",
              (("Search the house", False, "1/3"), ("Fill bottles at the tap", True, "done"),
               ("Board up the doors", False, "0/1")))
    clock(img, W - 252, 16)
    minimap(img, W - 178, 520)
    hotbar(img, 0, "Knife")
    prompt(img, 640, 250, (("E", "Open door"), ("R", "Board up")))
    # What the dead can hear and see of you, by the hotbar where it is glanced at.
    panel(img, 286, 652, 118, 52, alpha=206)
    glyph(img, 296, 662, ICON["ear"], ORANGE, 2)
    bar(img, 318, 665, 76, 5, 0.7, ORANGE)
    glyph(img, 296, 684, ICON["eye"], DIM, 2)
    bar(img, 318, 687, 76, 5, 0.25, DIM)
    text(img, 16, 4, "MOCKUP  -  HUD", FAINT, 1, shadow=True)
    img.convert("RGB").save(os.path.join(out, "ui_hud.png"))


def mock_inventory(shots, out):
    img = backdrop(os.path.join(shots, "01_home_day.png"), 150)
    px, py, pw, ph = 150, 70, 980, 520
    panel(img, px, py, pw, ph, alpha=244, accent=AMBER)
    text(img, px + 20, py + 12, "PACK", AMBER, 3)
    text(img, px + 110, py + 26, "Hayden Stone  -  day 1", DIM, 1)
    # Weight, where the title bar has room for it.
    text(img, px + pw - 20, py + 12, "7.4 / 25 kg", TEXT, 2, align="right")
    bar(img, px + pw - 200, py + 40, 180, 5, 7.4 / 25, AMBER)
    rect(img, px + 16, py + 56, pw - 32, 1, LINE_DIM)

    # Left: what is worn and held.
    lx = px + 20
    text(img, lx, py + 68, "WEARING", FAINT, 1)
    worn = (("Hands", "item_knife", "Knife", 0.82), ("Off hand", "item_flashlight", "Flashlight", None),
            ("Body", "item_jacket", "Jacket", 0.6), ("Back", None, "Backpack", None))
    for i, (where, item, name, wear) in enumerate(worn):
        sy = py + 88 + i * 56
        slot(img, lx, sy, 44, i == 0, None, item, None, wear)
        if item is None:
            glyph(img, lx + 15, sy + 14, ICON["pack"], DIM, 3)
        text(img, lx + 54, sy + 4, where.upper(), FAINT, 1)
        text(img, lx + 54, sy + 18, name, TEXT, 2)
    rect(img, lx, py + 318, 196, 1, LINE_DIM)
    text(img, lx, py + 328, "CONDITION", FAINT, 1)
    rows = (("heart", "Health", 0.62, (196, 78, 70)), ("drop", "Thirst", 0.18, RED), ("food", "Hunger", 0.42, ORANGE),
            ("sleep", "Rest", 0.66, VIOLET), ("temp", "Warmth", 0.24, RED))
    for i, (icon, label, frac, color) in enumerate(rows):
        ry = py + 348 + i * 22
        glyph(img, lx, ry + 2, ICON[icon], color, 2)
        text(img, lx + 22, ry, label, TEXT, 1)
        bar(img, lx + 80, ry + 4, 96, 6, frac, color)
    tag(img, lx, py + 466, "Bleeding", RED)
    tag(img, lx + 70, py + 466, "Soaked", BLUE)
    rect(img, px + 236, py + 66, 1, ph - 120, LINE_DIM)

    # Middle: the pack itself, filtered by kind.
    mx = px + 256
    tabs = ("ALL", "FOOD", "MEDICAL", "TOOLS", "MATERIALS")
    tx = mx
    for i, label in enumerate(tabs):
        w = text_width(label, 1) + 16
        if i == 0:
            rect(img, tx, py + 66, w, 20, (60, 48, 26))
            rect(img, tx, py + 84, w, 2, AMBER)
        text(img, tx + 8, py + 70, label, AMBER if i == 0 else DIM, 1, shadow=False)
        tx += w + 4
    items = [("item_canned_food", 2, None), ("item_water_bottle", 1, None), ("item_bandage", 3, None),
             ("item_bat", None, 0.34), ("item_plank", 4, None), ("item_rope", 1, None), ("item_matches", 1, None),
             ("item_painkillers", 2, None), ("item_rag", 2, None), ("item_scrap", 3, None), ("item_apple", 1, None),
             ("item_empty_bottle", 2, None), ("item_notebook", 1, None)]
    cols, size, gap = 7, 48, 6
    for i in range(cols * 5):
        gx = mx + (i % cols) * (size + gap)
        gy = py + 100 + (i // cols) * (size + gap)
        if i < len(items):
            slot(img, gx, gy, size, i == 3, None, items[i][0], items[i][1], items[i][2])
        else:
            slot(img, gx, gy, size)
    text(img, mx, py + 376, "QUICK SLOTS", FAINT, 1)
    for i, entry in enumerate(HOTBAR):
        gx = mx + i * (size - 4 + gap)
        if entry:
            slot(img, gx, py + 394, 44, False, str(i + 1), entry[0], entry[1], entry[2])
        else:
            slot(img, gx, py + 394, 44, False, str(i + 1))
    rect(img, px + 646, py + 66, 1, ph - 120, LINE_DIM)

    # Right: the thing under the cursor, and what can be done with it.
    rx = px + 666
    rect(img, rx, py + 70, 96, 96, SLOT)
    frame(img, rx, py + 70, 96, 96, LINE_DIM)
    sprite(img, "item_bat", rx + 16, py + 86, 2)
    text(img, rx + 110, py + 72, "Baseball bat", TEXT, 2)
    tag(img, rx + 110, py + 98, "Weapon", AMBER)
    tag(img, rx + 170, py + 98, "Two hands", DIM)
    text(img, rx + 110, py + 122, "1.2 kg", DIM, 1)
    text(img, rx, py + 180, "Ash, split along the grain. It has", DIM, 1)
    text(img, rx, py + 194, "a few more swings in it, not many.", DIM, 1)
    stats = (("Damage", 0.7, "34", TEXT), ("Reach", 0.55, "1.4 m", TEXT), ("Speed", 0.4, "slow", TEXT),
             ("Noise", 0.3, "quiet", TEXT), ("Condition", 0.34, "34%", ORANGE))
    for i, (label, frac, value, color) in enumerate(stats):
        sy = py + 222 + i * 24
        text(img, rx, sy, label, DIM, 1)
        bar(img, rx + 78, sy + 4, 130, 6, frac, ORANGE if label == "Condition" else (150, 142, 122))
        text(img, rx + 292, sy, value, color, 1, align="right")
    # Compared with what is in hand.
    rect(img, rx, py + 348, 292, 34, (34, 31, 28))
    text(img, rx + 8, py + 352, "vs Knife in hand", FAINT, 1)
    text(img, rx + 8, py + 366, "+12 damage", GREEN, 1)
    text(img, rx + 104, py + 366, "+0.5 m reach", GREEN, 1)
    text(img, rx + 212, py + 366, "slower", RED, 1)
    ax = rx
    for key, label in (("F", "Equip"), ("G", "Drop"), ("1-8", "Quick slot")):
        ax += hint(img, ax, py + 398, key, label, 1, TEXT)

    rect(img, px + 16, py + ph - 44, pw - 32, 1, LINE_DIM)
    hx = px + 20
    for key, label in (("WASD", "Move"), ("Q / E", "Kind"), ("Shift+Click", "Split stack"), ("Tab", "Close")):
        hx += hint(img, hx, py + ph - 30, key, label, 1)
    text(img, 16, 4, "MOCKUP  -  PACK", FAINT, 1)
    img.convert("RGB").save(os.path.join(out, "ui_inventory.png"))


def mock_crafting(shots, out):
    img = backdrop(os.path.join(shots, "01_home_day.png"), 150)
    px, py, pw, ph = 150, 70, 980, 520
    panel(img, px, py, pw, ph, alpha=244, accent=AMBER)
    text(img, px + 20, py + 12, "MAKE", AMBER, 3)
    text(img, px + 112, py + 26, "5 known  -  3 you could work out", DIM, 1)
    tabs = ("ALL", "TOOLS", "MEDICAL", "FOOD", "DEFENCES")
    tx = px + pw - 20 - sum(text_width(t, 1) + 20 for t in tabs)
    for i, label in enumerate(tabs):
        w = text_width(label, 1) + 16
        if i == 0:
            rect(img, tx, py + 18, w, 20, (60, 48, 26))
            rect(img, tx, py + 36, w, 2, AMBER)
        text(img, tx + 8, py + 22, label, AMBER if i == 0 else DIM, 1, shadow=False)
        tx += w + 4
    rect(img, px + 16, py + 56, pw - 32, 1, LINE_DIM)

    # Left: recipes, each saying at a glance whether it can be made.
    lx = px + 20
    recipes = (("item_bat_spiked", "Spiked bat", "Ready", GREEN, True),
               ("item_bandage_clean", "Clean bandage", "Ready", GREEN, False),
               ("item_spear", "Spear", "Need 1 rope", ORANGE, False),
               ("item_molotov", "Fire bottle", "Need fuel, rag", ORANGE, False),
               ("item_barricade", "Barricade", "Need 4 planks", ORANGE, False),
               (None, "Bandage + ?", "A hunch", VIOLET, False),
               (None, "Water bottle + ?", "A hunch", VIOLET, False),
               (None, "Knife + ?", "A hunch", VIOLET, False))
    for i, (item, name, status, color, selected) in enumerate(recipes):
        ry = py + 70 + i * 50
        if selected:
            rect(img, lx - 4, ry - 2, 330, 46, (60, 48, 26), 200)
            rect(img, lx - 4, ry - 2, 3, 46, AMBER)
        slot(img, lx + 4, ry, 42, False, None, item)
        if item is None:
            text(img, lx + 18, ry + 6, "?", VIOLET, 2)
        text(img, lx + 56, ry + 2, name, TEXT if item else DIM, 2)
        text(img, lx + 56, ry + 26, status, color, 1)
        if status == "Ready":
            glyph(img, lx + 306, ry + 16, ICON["check"], GREEN, 2)
        elif item is None:
            glyph(img, lx + 306, ry + 14, ICON["lock"], VIOLET, 2)
    rect(img, px + 366, py + 66, 1, ph - 120, LINE_DIM)

    # Right: the recipe laid out as what goes in and what comes out.
    rx = px + 390
    text(img, rx, py + 70, "Spiked bat", TEXT, 3)
    tag(img, rx, py + 110, "Weapon", AMBER)
    tag(img, rx + 60, py + 110, "Two hands", DIM)
    text(img, rx, py + 134, "Nails through the fat end. It bites, and it will not", DIM, 1)
    text(img, rx, py + 148, "come back out of a skull quietly.", DIM, 1)

    text(img, rx, py + 180, "TAKES", FAINT, 1)
    parts = (("item_bat", "Baseball bat", "1 / 1", True), ("item_scrap", "Scrap metal", "3 / 2", True),
             ("item_hammer", "Hammer", "tool, kept", True))
    for i, (item, name, have, ok) in enumerate(parts):
        ix = rx + i * 130
        slot(img, ix, py + 198, 56, False, None, item)
        frame(img, ix, py + 198, 56, 56, GREEN if ok else RED)
        text(img, ix, py + 260, name, TEXT, 1)
        text(img, ix, py + 274, have, GREEN if ok else RED, 1)
    # Arrow to the result.
    ax = rx + 400
    for i in range(5):
        rect(img, ax + i * 6, py + 224, 4, 4, AMBER if i < 4 else AMBER)
    glyph(img, ax + 30, py + 216, ["#....", "##...", "###..", "####.", "###..", "##...", "#...."], AMBER, 2)
    text(img, ax + 56, py + 180, "MAKES", FAINT, 1)
    slot(img, ax + 56, py + 198, 72, True, None, None)
    sprite(img, "item_bat_spiked", ax + 60, py + 202, 2)

    text(img, rx, py + 304, "COMPARED WITH THE BAT", FAINT, 1)
    for i, (label, frac, extra, value) in enumerate((("Damage", 0.7, 0.16, "34 > 46"), ("Speed", 0.4, 0.0, "slow"),
                                                      ("Noise", 0.3, 0.14, "quiet > loud"))):
        sy = py + 324 + i * 22
        text(img, rx, sy, label, DIM, 1)
        bar(img, rx + 78, sy + 4, 200, 6, frac, (150, 142, 122))
        if extra:
            rect(img, rx + 78 + int(200 * frac), sy + 4, int(200 * extra), 6, GREEN if label == "Damage" else RED)
        text(img, rx + 380, sy, value, TEXT, 1, align="right")

    # The act itself: how many, how long, and the button.
    by = py + 404
    rect(img, rx, by, 550, 52, (34, 31, 28))
    text(img, rx + 12, by + 6, "Make", DIM, 1)
    keycap(img, rx + 12, by + 24, "-")
    text(img, rx + 44, by + 18, "1", TEXT, 2)
    keycap(img, rx + 66, by + 24, "+")
    text(img, rx + 120, by + 6, "Takes", DIM, 1)
    text(img, rx + 120, by + 20, "25 min", TEXT, 2)
    text(img, rx + 230, by + 6, "Noise", DIM, 1)
    text(img, rx + 230, by + 20, "hammering", ORANGE, 2)
    rect(img, rx + 400, by + 8, 138, 36, (92, 70, 30))
    frame(img, rx + 400, by + 8, 138, 36, AMBER)
    keycap(img, rx + 410, by + 17, "Enter")
    text(img, rx + 462, by + 14, "Make", TEXT, 2)

    rect(img, px + 16, py + ph - 44, pw - 32, 1, LINE_DIM)
    hx = px + 20
    for key, label in (("W / S", "Choose"), ("A / D", "Kind"), ("X", "Stop"), ("N", "Close")):
        hx += hint(img, hx, py + ph - 30, key, label, 1)
    text(img, 16, 4, "MOCKUP  -  MAKE", FAINT, 1)
    img.convert("RGB").save(os.path.join(out, "ui_crafting.png"))


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return
    shots, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    mock_hud(shots, out)
    mock_inventory(shots, out)
    mock_crafting(shots, out)
    print("wrote", out)


if __name__ == "__main__":
    main()
