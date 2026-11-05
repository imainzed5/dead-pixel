#!/usr/bin/env python3
"""
Isometric sprite generator for Dead Pixel Survival.
Generates all game sprites as isometric pixel art using Pillow.
Output: spritesheet.png + spritesheet.json in assets/sprites/
"""

import json
import math
import os
import random
import sys
from pathlib import Path

from PIL import Image, ImageDraw

# Atlas dimensions
ATLAS_W = 1024
ATLAS_H = 1024

# Tile sizes
ISO_TILE_W = 64
ISO_TILE_H = 32
ENTITY_SIZE = 64   # Characters/zombies
ITEM_SIZE = 32      # Items, env, UI, FX, solids

# Deterministic seed for reproducible results
random.seed(42)


# --- Isometric Diamond Helpers ---

def in_diamond(px, py, w=ISO_TILE_W, h=ISO_TILE_H):
    cx = (w - 1) / 2.0
    cy = (h - 1) / 2.0
    return abs(px - cx) / (w / 2.0) + abs(py - cy) / (h / 2.0) <= 1.0


def diamond_edge(px, py, w=ISO_TILE_W, h=ISO_TILE_H, thickness=1.5):
    cx = (w - 1) / 2.0
    cy = (h - 1) / 2.0
    d = abs(px - cx) / (w / 2.0) + abs(py - cy) / (h / 2.0)
    return 1.0 - thickness / min(w, h) <= d <= 1.0


def fill_diamond(img, ox, oy, base_color, noise_range=15, w=ISO_TILE_W, h=ISO_TILE_H):
    r0, g0, b0 = base_color
    for py in range(h):
        for px in range(w):
            if in_diamond(px, py, w, h):
                nr = random.randint(-noise_range, noise_range)
                r = max(0, min(255, r0 + nr))
                g = max(0, min(255, g0 + nr))
                b = max(0, min(255, b0 + nr))
                img.putpixel((ox + px, oy + py), (r, g, b, 255))


def fill_diamond_edge(img, ox, oy, edge_color, w=ISO_TILE_W, h=ISO_TILE_H):
    for py in range(h):
        for px in range(w):
            if diamond_edge(px, py, w, h):
                img.putpixel((ox + px, oy + py), edge_color)


# --- Tile Generation ---

def gen_tile_grass(img, ox, oy):
    fill_diamond(img, ox, oy, (58, 107, 45), noise_range=18)
    for _ in range(40):
        px = random.randint(0, ISO_TILE_W - 1)
        py = random.randint(0, ISO_TILE_H - 1)
        if in_diamond(px, py):
            shade = random.choice([(72, 128, 55, 255), (50, 95, 38, 255), (65, 115, 48, 255)])
            img.putpixel((ox + px, oy + py), shade)


def gen_tile_grass2(img, ox, oy):
    fill_diamond(img, ox, oy, (52, 98, 40), noise_range=20)
    for _ in range(30):
        px = random.randint(0, ISO_TILE_W - 1)
        py = random.randint(0, ISO_TILE_H - 1)
        if in_diamond(px, py):
            img.putpixel((ox + px, oy + py), (44, 85, 35, 255))


def gen_tile_dirt(img, ox, oy):
    fill_diamond(img, ox, oy, (120, 90, 60), noise_range=15)
    for _ in range(20):
        px = random.randint(2, ISO_TILE_W - 3)
        py = random.randint(2, ISO_TILE_H - 3)
        if in_diamond(px, py):
            img.putpixel((ox + px, oy + py), (95, 72, 50, 255))


def gen_tile_concrete(img, ox, oy):
    fill_diamond(img, ox, oy, (140, 140, 135), noise_range=10)
    for _ in range(3):
        sx = random.randint(16, 48)
        sy = random.randint(4, 28)
        for step in range(random.randint(4, 12)):
            cx = sx + step + random.randint(-1, 1)
            cy = sy + random.randint(-1, 1)
            if 0 <= cx < ISO_TILE_W and 0 <= cy < ISO_TILE_H and in_diamond(cx, cy):
                img.putpixel((ox + cx, oy + cy), (110, 110, 105, 255))


def gen_tile_carpet(img, ox, oy):
    fill_diamond(img, ox, oy, (130, 85, 70), noise_range=8)
    for py in range(ISO_TILE_H):
        for px in range(ISO_TILE_W):
            if in_diamond(px, py) and (px + py) % 4 == 0:
                r, g, b, a = img.getpixel((ox + px, oy + py))
                img.putpixel((ox + px, oy + py), (max(0, r - 15), max(0, g - 15), max(0, b - 15), 255))


def gen_tile_gravel(img, ox, oy):
    fill_diamond(img, ox, oy, (125, 115, 100), noise_range=20)
    for _ in range(35):
        px = random.randint(2, ISO_TILE_W - 3)
        py = random.randint(2, ISO_TILE_H - 3)
        if in_diamond(px, py):
            shade = random.randint(80, 140)
            img.putpixel((ox + px, oy + py), (shade, shade - 5, shade - 10, 255))


def gen_tile_metal(img, ox, oy):
    fill_diamond(img, ox, oy, (90, 95, 100), noise_range=8)
    for py in range(ISO_TILE_H):
        for px in range(ISO_TILE_W):
            if in_diamond(px, py) and py < 10 and abs(px - 32) < 8:
                r, g, b, a = img.getpixel((ox + px, oy + py))
                img.putpixel((ox + px, oy + py), (min(255, r + 20), min(255, g + 20), min(255, b + 25), 255))


def gen_tile_water(img, ox, oy):
    fill_diamond(img, ox, oy, (35, 85, 145), noise_range=15)
    for _ in range(25):
        px = random.randint(4, ISO_TILE_W - 5)
        py = random.randint(2, ISO_TILE_H - 3)
        if in_diamond(px, py):
            img.putpixel((ox + px, oy + py), (80, 150, 210, 255))


def gen_tile_wall_top(img, ox, oy):
    for py in range(ISO_TILE_H):
        for px in range(ISO_TILE_W):
            if in_diamond(px, py):
                if py < ISO_TILE_H // 2:
                    n = random.randint(-5, 5)
                    img.putpixel((ox + px, oy + py), (145 + n, 130 + n, 115 + n, 255))
                else:
                    if px < ISO_TILE_W // 2:
                        n = random.randint(-5, 5)
                        img.putpixel((ox + px, oy + py), (100 + n, 90 + n, 80 + n, 255))
                    else:
                        n = random.randint(-5, 5)
                        img.putpixel((ox + px, oy + py), (120 + n, 108 + n, 95 + n, 255))
    fill_diamond_edge(img, ox, oy, (70, 65, 58, 255))


def gen_tile_wall_face(img, ox, oy):
    fill_diamond(img, ox, oy, (110, 100, 88), noise_range=8)
    for line_y in [8, 16, 24]:
        for px in range(ISO_TILE_W):
            if in_diamond(px, line_y):
                img.putpixel((ox + px, oy + line_y), (85, 78, 68, 255))
    fill_diamond_edge(img, ox, oy, (65, 60, 52, 255))


def gen_tile_door_closed(img, ox, oy):
    fill_diamond(img, ox, oy, (100, 70, 45), noise_range=10)
    for py in range(4, ISO_TILE_H - 4):
        for px in range(20, 44):
            if in_diamond(px, py):
                n = random.randint(-3, 3)
                img.putpixel((ox + px, oy + py), (85 + n, 55 + n, 30 + n, 255))
    if in_diamond(38, 16):
        img.putpixel((ox + 38, oy + 16), (180, 170, 80, 255))
    fill_diamond_edge(img, ox, oy, (60, 42, 25, 255))


def gen_tile_door_open(img, ox, oy):
    fill_diamond(img, ox, oy, (55, 55, 50), noise_range=5)
    for py in range(4, ISO_TILE_H - 4):
        if in_diamond(10, py):
            img.putpixel((ox + 10, oy + py), (100, 70, 45, 255))
            img.putpixel((ox + 11, oy + py), (90, 62, 38, 255))


def gen_tile_solid_white(img, ox, oy):
    for py in range(ISO_TILE_H):
        for px in range(ISO_TILE_W):
            if in_diamond(px, py):
                img.putpixel((ox + px, oy + py), (255, 255, 255, 255))


def gen_tile_solid_dark(img, ox, oy):
    for py in range(ISO_TILE_H):
        for px in range(ISO_TILE_W):
            if in_diamond(px, py):
                img.putpixel((ox + px, oy + py), (20, 20, 20, 255))


def gen_tile_window(img, ox, oy):
    fill_diamond(img, ox, oy, (110, 100, 88), noise_range=5)
    for py in range(8, 24):
        for px in range(22, 42):
            if in_diamond(px, py):
                img.putpixel((ox + px, oy + py), (130, 180, 220, 180))
    fill_diamond_edge(img, ox, oy, (65, 60, 52, 255))


def gen_tile_fence(img, ox, oy):
    for py in range(ISO_TILE_H):
        for px in range(ISO_TILE_W):
            if in_diamond(px, py):
                if px in (16, 32, 48) and py > 4:
                    img.putpixel((ox + px, oy + py), (120, 95, 60, 255))
                elif py in (10, 20) and 14 <= px <= 50:
                    img.putpixel((ox + px, oy + py), (110, 85, 50, 255))


# --- Character Generation ---
#
# Characters are 64x64 frames whose feet rest on row 49 (the renderer pins the
# bottom of that row to the ground point). Each is drawn in two views, both
# turned to the viewer's right: "front" faces down-right on screen and "back"
# faces up-right. The game mirrors them for the two left-hand directions.
#
# Flat-shaded pixel art (highlight, base, shadow) with a dark outline, so the
# figures read against any floor, and a soft shadow on the ground.

CHAR_FEET = 49
CHAR_OUTLINE = (30, 24, 26, 255)
CHAR_SHADOW = (0, 0, 0, 72)
BLOOD = ((150, 40, 34), (122, 28, 26), (86, 18, 18))

PLAYER_PAL = {
    "skin": ((232, 200, 168), (214, 178, 144), (178, 142, 112)),
    "hair": ((96, 70, 50), (66, 48, 34), (44, 32, 24)),
    "top": ((104, 124, 140), (80, 98, 114), (58, 72, 86)),
    "legs": ((76, 86, 112), (58, 66, 90), (42, 48, 68)),
    "shoes": ((74, 60, 48), (54, 42, 34), (36, 28, 22)),
    "pack": ((140, 118, 80), (112, 94, 62), (84, 70, 46)),
    "detail": (44, 54, 64),
    "eye": (36, 30, 28),
}

ZOMBIE_PAL = {
    "skin": ((156, 172, 130), (128, 148, 110), (96, 114, 86)),
    "hair": ((70, 68, 58), (52, 50, 44), (38, 36, 32)),
    "top": ((128, 114, 96), (104, 92, 80), (78, 68, 60)),
    "legs": ((82, 80, 92), (66, 64, 76), (50, 48, 58)),
    "shoes": ((66, 60, 54), (48, 44, 40), (34, 30, 28)),
    "detail": (60, 52, 46),
    "eye": (232, 220, 150),
    "mouth": (52, 18, 22),
}


class CharCanvas:
    """A 64x64 RGBA frame with the few primitives the characters need."""

    def __init__(self):
        self.img = Image.new("RGBA", (ENTITY_SIZE, ENTITY_SIZE), (0, 0, 0, 0))
        self.under = Image.new("RGBA", (ENTITY_SIZE, ENTITY_SIZE), (0, 0, 0, 0))

    def set(self, x, y, c, layer=None):
        target = self.img if layer is None else layer
        x, y = int(round(x)), int(round(y))
        if 0 <= x < ENTITY_SIZE and 0 <= y < ENTITY_SIZE:
            target.putpixel((x, y), tuple(c[:3]) + ((c[3],) if len(c) > 3 else (255,)))

    def opaque(self, x, y):
        return 0 <= x < ENTITY_SIZE and 0 <= y < ENTITY_SIZE and self.img.getpixel((x, y))[3] > 0

    def rect(self, x0, y0, x1, y1, c):
        for y in range(int(y0), int(y1) + 1):
            for x in range(int(x0), int(x1) + 1):
                self.set(x, y, c)

    def ellipse(self, x0, y0, x1, y1, c, layer=None):
        cx, cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0
        rx, ry = (x1 - x0) / 2.0 + 0.5, (y1 - y0) / 2.0 + 0.5
        for y in range(int(y0), int(y1) + 1):
            for x in range(int(x0), int(x1) + 1):
                if ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0:
                    self.set(x, y, c, layer)

    def limb(self, x0, y0, x1, y1, c, width=2, outline=True, colors=None):
        """A thick line from (x0, y0) to (x1, y1), outlined so it reads over the body.
        colors, when given, is a function t -> colour along the limb."""
        n = max(abs(x1 - x0), abs(y1 - y0), 1)
        points = []
        for i in range(n + 1):
            t = i / n
            points.append((round(x0 + (x1 - x0) * t), round(y0 + (y1 - y0) * t), t))
        if outline:
            for x, y, _ in points:
                for dy in range(-1, width + 1):
                    for dx in range(-1, width + 1):
                        if (dx in (-1, width)) and (dy in (-1, width)):
                            continue
                        self.set(x + dx, y + dy, CHAR_OUTLINE)
        for x, y, t in points:
            col = colors(t) if colors else c
            for dy in range(width):
                for dx in range(width):
                    self.set(x + dx, y + dy, col)

    def finish(self):
        """Outline the figure, lay it over its shadow and return the frame."""
        src = self.img
        out = self.under.copy()
        outline = Image.new("RGBA", src.size, (0, 0, 0, 0))
        for y in range(ENTITY_SIZE):
            for x in range(ENTITY_SIZE):
                if src.getpixel((x, y))[3]:
                    continue
                for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                    if 0 <= nx < ENTITY_SIZE and 0 <= ny < ENTITY_SIZE and src.getpixel((nx, ny))[3]:
                        outline.putpixel((x, y), CHAR_OUTLINE)
                        break
        out.alpha_composite(outline)
        out.alpha_composite(src)
        return out


def _leg(cv, pal, x, hip_y, fdx, fdy, tone, toe, knee=0):
    """A trouser leg from the hip down to its shoe. fdx/fdy move the foot
    (positive fdy lowers it); toe is +1 when the toes point right, 0 for heels;
    knee pushes the middle of the leg out to the right (a bent knee)."""
    legs, shoes = pal["legs"], pal["shoes"]
    shoe_top = CHAR_FEET + fdy - 2
    span = max(1, shoe_top - hip_y)
    for y in range(hip_y, shoe_top):
        t = (y - hip_y) / span
        xo = x + round(fdx * t + knee * (1.0 - abs(2.0 * t - 1.0)))
        cv.rect(xo, y, xo + 3, y, legs[tone])
        cv.set(xo, y, legs[max(0, tone - 1)] if tone == 1 else legs[tone])
        cv.set(xo + 3, y, legs[2])
    sx = x + fdx
    cv.rect(sx, shoe_top, sx + 3, shoe_top + 2, shoes[1])
    cv.rect(sx, shoe_top + 2, sx + 3 + (1 if toe > 0 else 0), shoe_top + 2, shoes[2])
    if toe > 0:
        cv.set(sx + 4, shoe_top + 1, shoes[1])
    cv.set(sx, shoe_top, shoes[0])


def _torso(cv, pal, view, top, ox, zombie, player_pack):
    """Jacket or shirt from the shoulders (row top) to the belt (top + 13)."""
    t = pal["top"]
    mid = tuple((a + b) // 2 for a, b in zip(t[1], t[2]))
    for i in range(14):
        if i == 0:
            x0, x1 = 28, 36
        elif i == 1:
            x0, x1 = 27, 37
        elif i <= 8:
            x0, x1 = 26, 38
        else:
            x0, x1 = 27, 37
        y = top + i
        for x in range(x0, x1 + 1):
            if zombie and i == 13 and (x % 3 == 0):
                continue  # a ragged hem
            if x <= x0 + 1:
                c = mid  # the flank turned away from the viewer
            elif x == x1:
                c = t[2]
            elif i <= 1 and x < 32:
                c = t[0]
            else:
                c = t[1]
            cv.set(x + ox, y, c)
        if not zombie and i == 13:
            for x in range(x0 + 2, x1):
                cv.set(x + ox, y, t[2])

    if zombie:
        skin, blood = pal["skin"], BLOOD
        if view == "front":
            for (x, y) in ((29, 7), (30, 7), (30, 8), (35, 3), (36, 4)):
                cv.set(x + ox, top + y, skin[1])
            cv.rect(31 + ox, top + 1, 34 + ox, top + 2, blood[1])
            cv.set(33 + ox, top + 3, blood[2])
            cv.rect(34 + ox, top + 3, 34 + ox, top + 6, blood[1])
            cv.rect(28 + ox, top + 10, 30 + ox, top + 11, blood[2])
        else:
            for i in range(5):
                cv.set(29 + i + ox, top + 4 + i, blood[1])
                cv.set(30 + i + ox, top + 4 + i, blood[2])
            cv.set(35 + ox, top + 10, skin[1])
            cv.set(36 + ox, top + 10, skin[2])
        return

    d = pal["detail"]
    if view == "front":
        cv.set(31 + ox, top, pal["skin"][2])
        cv.set(32 + ox, top, pal["skin"][2])
        cv.rect(33 + ox, top, 33 + ox, top + 12, d)  # zip, turned toward the right
        cv.set(30 + ox, top + 10, d)
        cv.set(36 + ox, top + 10, d)
        if player_pack:
            p = pal["pack"]
            cv.rect(29 + ox, top + 1, 29 + ox, top + 8, p[2])
            cv.rect(36 + ox, top + 1, 36 + ox, top + 7, p[2])
    elif player_pack:
        p = pal["pack"]
        cv.rect(28 + ox, top + 1, 37 + ox, top + 11, p[1])
        cv.rect(28 + ox, top + 1, 37 + ox, top + 3, p[0])
        cv.rect(28 + ox, top + 4, 37 + ox, top + 4, p[2])
        cv.rect(37 + ox, top + 1, 37 + ox, top + 11, p[2])
        cv.rect(28 + ox, top + 11, 37 + ox, top + 11, p[2])
        cv.rect(30 + ox, top + 6, 35 + ox, top + 6, p[2])
        cv.rect(30 + ox, top + 6, 30 + ox, top + 9, p[2])
        cv.rect(35 + ox, top + 6, 35 + ox, top + 9, p[2])
        cv.set(32 + ox, top + 4, d)
        cv.set(33 + ox, top + 4, d)
    else:
        cv.rect(32 + ox, top + 2, 32 + ox, top + 12, t[2])


def _head(cv, pal, view, hx, hy, zombie, mouth_open=False):
    """A 9x10 head with its top-left at (hx, hy)."""
    skin, hair = pal["skin"], pal["hair"]
    cx, cy, rx, ry = hx + 4, hy + 4.5, 4.5, 5.0

    def inside(x, y):
        return ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0

    def put(x, y, c):
        if inside(x, y):
            cv.set(x, y, c)

    for y in range(hy, hy + 10):
        for x in range(hx, hx + 9):
            if not inside(x, y):
                continue
            if zombie or view == "front":
                c = skin[2] if x >= hx + 7 else skin[1]
                if x <= hx + 3 and y <= hy + 3:
                    c = skin[0]
            else:
                c = hair[2] if (x >= hx + 7 or y >= hy + 8) else hair[1]
                if hx + 2 <= x <= hx + 4 and hy + 1 <= y <= hy + 2:
                    c = hair[0]
            cv.set(x, y, c)

    if view == "front":
        if zombie:
            for (x, y) in ((2, 0), (3, 0), (4, 0), (5, 1), (1, 2), (1, 3), (2, 1)):
                put(hx + x, hy + y, hair[1])
            put(hx + 4, hy + 4, skin[2])
            put(hx + 7, hy + 4, skin[2])
            put(hx + 4, hy + 5, pal["eye"])
            put(hx + 7, hy + 5, pal["eye"])
            put(hx + 5, hy + 7, pal["mouth"])
            put(hx + 6, hy + 7, pal["mouth"])
            if mouth_open:
                for (x, y) in ((5, 8), (6, 8), (7, 8), (6, 9)):
                    put(hx + x, hy + y, pal["mouth"])
            put(hx + 6, hy + 9, BLOOD[1])
        else:
            for y in range(hy, hy + 3):
                for x in range(hx, hx + 9):
                    put(x, y, hair[1] if y > hy else hair[0])
            for x in range(hx, hx + 6):
                put(x, hy + 3, hair[1])
            for y in range(hy + 3, hy + 7):
                for x in range(hx, hx + 3):
                    put(x, y, hair[2] if x == hx else hair[1])
            put(hx + 3, hy + 6, skin[2])  # ear
            put(hx + 4, hy + 5, pal["eye"])
            put(hx + 7, hy + 5, pal["eye"])
            put(hx + 6, hy + 6, skin[2])  # nose
            put(hx + 5, hy + 8, skin[2])
            put(hx + 6, hy + 8, skin[2])
    else:
        if zombie:
            for (x, y) in ((1, 1), (2, 1), (3, 1), (2, 2), (3, 2), (4, 2), (1, 3), (5, 4), (6, 4)):
                put(hx + x, hy + y, hair[1])
            put(hx + 6, hy + 2, BLOOD[1])
            put(hx + 7, hy + 3, BLOOD[2])
        else:
            put(hx + 8, hy + 5, skin[2])  # the cheek turned away
            put(hx + 8, hy + 6, skin[2])
            put(hx + 7, hy + 5, skin[1])  # ear
            for x in range(hx + 3, hx + 6):
                put(x, hy + 9, skin[2])  # nape


def _arm(cv, pal, shoulder, hand, zombie, tone=1):
    """An arm from the shoulder to the hand, with the hand in skin."""
    sx, sy = shoulder
    hx, hy = hand
    skin, top = pal["skin"], pal["top"]
    if zombie:
        cv.limb(sx, sy, hx, hy, None, colors=lambda t: top[tone] if t < 0.35 else skin[tone])
    else:
        cv.limb(sx, sy, hx, hy, None, colors=lambda t: top[tone] if t < 0.85 else skin[1])
    cv.set(hx, hy + 1, skin[1])
    cv.set(hx + 1, hy + 1, skin[2])


def draw_character(pal, view, zombie=False, pose="idle", frame=0):
    """One 64x64 character frame. view: "front" (faces down-right) or "back"
    (faces up-right). pose: idle, walk, crouch, crouchwalk, attack, dead."""
    cv = CharCanvas()
    front = view == "front"

    if pose == "dead":
        _draw_dead(cv, pal, zombie)
        return cv.finish()

    cv.ellipse(23, 47, 41, 51, CHAR_SHADOW, layer=cv.under)

    # Pose: feet offsets (dx, dy) for the near (left) and far (right) leg,
    # body bob, forward lean, and hand offsets for the swinging arms.
    near, far = (0, 0), (0, 0)
    bob, lean = 0, 0
    swing_near, swing_far = (0, 0), (0, 0)
    hip_y = 36

    crouched = pose in ("crouch", "crouchwalk")
    knee = 0
    if crouched:
        hip_y = 40
        bob, lean = 6, 2
        knee = 3 if front else 1

    if pose in ("walk", "crouchwalk"):
        f = frame % 4
        if zombie:
            # A shamble: the right leg drags and the body sags on each step.
            steps_front = [((1, 1), (0, -1), 0, 0), ((0, 0), (0, 0), 1, 1),
                           ((0, -1), (1, 0), 0, 1), ((0, 0), (-1, 0), 1, 0)]
            steps_back = [((1, -1), (0, 0), 0, 0), ((0, 0), (0, -1), 1, 1),
                          ((0, 0), (1, -1), 0, 1), ((-1, 0), (0, 0), 1, 0)]
            near, far, sag, tilt = (steps_front if front else steps_back)[f]
            bob += sag
            lean += tilt
        else:
            if front:
                steps = [((1, 1), (-1, -2)), ((0, 0), (0, -1)), ((-1, -2), (1, 1)), ((0, -1), (0, 0))]
            else:
                steps = [((1, -2), (-1, 0)), ((0, 0), (0, -1)), ((-1, 0), (1, -2)), ((0, -1), (0, 0))]
            near, far = steps[f]
            if crouched:
                near = (near[0], min(0, near[1]))
                far = (far[0], min(0, far[1]))
            if f in (1, 3):
                bob -= 1
            fwd = (2, -1) if front else (2, -2)
            bwd = (-2, -1) if front else (-2, 0)
            swing_near, swing_far = [(bwd, fwd), ((0, 0), (0, 0)), (fwd, bwd), ((0, 0), (0, 0))][f]
    elif pose == "attack":
        lean = 3 if zombie else 1
        bob = 1 if zombie else 0
        if not zombie:
            near, far = ((1, 1), (-1, 0)) if front else ((1, -2), (-1, 0))

    toe = 1 if front else 0
    top = 23 + bob
    head_x, head_y = 28 + lean, 13 + bob
    if zombie:
        head_x += 1
        head_y += 2 if front else 1

    # Shoulders and resting hands.
    sh_near, sh_far = (24 + lean, top + 2), (39 + lean, top + 2)
    hand_near = (24 + lean + swing_near[0], top + 10 + swing_near[1])
    hand_far = (39 + lean + swing_far[0], top + 10 + swing_far[1])
    if crouched:
        hand_near = (hand_near[0] + 1, hand_near[1] - 1)
        hand_far = (hand_far[0] + 1, hand_far[1] - 1)

    # Arms reaching out ahead (zombies) or swinging (the player's attack).
    reach_front = reach_back = None
    if zombie:
        bump = [0, 1, 1, 0][frame % 4] if pose == "walk" else 0
        # Both arms held out ahead: toward the right in either view, the near
        # one lower (front) or hidden behind the body (back).
        out = 3 if pose == "attack" else 0
        if front:
            reach_front = (((28 + lean, top + 3), (41 + lean + out, top + 8 + bump)),
                           ((37 + lean, top + 2), (46 + lean + out, top + 4 - bump)))
        else:
            reach_back = (((28 + lean, top + 3), (41 + lean + out, top + 1 + bump)),
                          ((37 + lean, top + 2), (46 + lean + out, top - 1 - bump)))
    elif pose == "attack":
        if front:
            hand_far = (45 + lean, top + 6)
            hand_near = (27 + lean, top + 8)
        else:
            hand_far = (44 + lean, top - 4)

    pack = not zombie

    if front:
        _leg(cv, pal, 33, hip_y, far[0], far[1], 2, toe, knee)
        _leg(cv, pal, 28, hip_y, near[0], near[1], 1, toe, knee)
        _torso(cv, pal, view, top, lean, zombie, pack)
        _head(cv, pal, view, head_x, head_y, zombie, mouth_open=pose == "attack")
        if reach_front:
            (s1, h1), (s2, h2) = reach_front
            _arm(cv, pal, s2, h2, zombie, tone=2)
            _arm(cv, pal, s1, h1, zombie, tone=1)
        else:
            _arm(cv, pal, sh_far, hand_far, zombie, tone=2)
            _arm(cv, pal, sh_near, hand_near, zombie, tone=1)
    else:
        if reach_back:
            (s1, h1), _ = reach_back
            _arm(cv, pal, s1, h1, zombie, tone=2)
        _leg(cv, pal, 33, hip_y, far[0], far[1], 2, toe, knee)
        _leg(cv, pal, 28, hip_y, near[0], near[1], 1, toe, knee)
        _torso(cv, pal, view, top, lean, zombie, pack)
        _head(cv, pal, view, head_x, head_y, zombie)
        if reach_back:
            _, (s2, h2) = reach_back
            _arm(cv, pal, s2, h2, zombie, tone=1)
        else:
            _arm(cv, pal, sh_far, hand_far, zombie, tone=2)
            _arm(cv, pal, sh_near, hand_near, zombie, tone=1)

    return cv.finish()


def _draw_dead(cv, pal, zombie):
    """Lying on the back, head to the left, in a pool of blood."""
    cv.ellipse(15, 44, 47, 52, BLOOD[2] + (210,), layer=cv.under)
    cv.ellipse(19, 45, 40, 50, BLOOD[1] + (220,), layer=cv.under)
    t, legs, shoes, skin, hair = pal["top"], pal["legs"], pal["shoes"], pal["skin"], pal["hair"]
    # Far arm flung up and out.
    cv.limb(27, 41, 31, 36, None, colors=lambda s: t[2] if s < 0.7 else skin[1])
    # Legs, then shoes.
    cv.rect(36, 41, 46, 43, legs[2])
    cv.rect(36, 44, 46, 46, legs[1])
    cv.rect(36, 44, 46, 44, legs[0])
    cv.rect(47, 40, 49, 43, shoes[1])
    cv.rect(47, 44, 49, 47, shoes[1])
    # Torso.
    cv.rect(22, 40, 36, 47, t[1])
    cv.rect(22, 40, 36, 41, t[0])
    cv.rect(22, 47, 36, 47, t[2])
    if zombie:
        cv.rect(27, 42, 31, 44, BLOOD[1])
        cv.set(30, 45, BLOOD[2])
    elif "pack" in pal:
        cv.rect(23, 47, 33, 48, pal["pack"][2])
    # Near arm along the body.
    cv.limb(24, 47, 33, 47, None, colors=lambda s: t[2] if s < 0.7 else skin[1])
    # Head.
    cv.ellipse(13, 39, 21, 48, skin[1])
    if zombie:
        for (x, y) in ((14, 41), (15, 40), (14, 43), (16, 42)):
            cv.set(x, y, hair[1])
        cv.set(18, 45, pal["mouth"])
        cv.set(19, 45, pal["mouth"])
    else:
        for y in range(39, 49):
            for x in range(13, 16):
                if cv.opaque(x, y):
                    cv.set(x, y, hair[1])
        cv.set(18, 42, pal["eye"])
        cv.set(18, 45, pal["eye"])


def _char_strip(img, atlas_sprites, name, row_y, slot, frames_drawn):
    """Paste frames into consecutive 64 px slots and register them."""
    frames = []
    for i, frame in enumerate(frames_drawn):
        x = (slot + i) * ENTITY_SIZE
        img.alpha_composite(frame, (x, row_y))
        frames.append({"x": x, "y": row_y, "w": ENTITY_SIZE, "h": ENTITY_SIZE})
    entry = dict(frames[0])
    if len(frames) > 1:
        entry["frames"] = frames
    atlas_sprites[name] = entry
    return slot + len(frames_drawn)


def gen_player_sprites(img, atlas_sprites, row_y, extra_row_y):
    pal = PLAYER_PAL
    slot = 0
    for view in ("front", "back"):
        slot = _char_strip(img, atlas_sprites, f"player_walk_{view}", row_y, slot,
                           [draw_character(pal, view, pose="walk", frame=f) for f in range(4)])
    for view in ("front", "back"):
        slot = _char_strip(img, atlas_sprites, f"player_idle_{view}", row_y, slot, [draw_character(pal, view)])
    for view in ("front", "back"):
        slot = _char_strip(img, atlas_sprites, f"player_crouch_{view}", row_y, slot,
                           [draw_character(pal, view, pose="crouch")])
    for view in ("front", "back"):
        slot = _char_strip(img, atlas_sprites, f"player_attack_{view}", row_y, slot,
                           [draw_character(pal, view, pose="attack")])
    slot = _char_strip(img, atlas_sprites, "player_hurt", row_y, slot,
                       [_tint_frame(draw_character(pal, "front"), (210, 40, 36), 0.45)])
    slot = _char_strip(img, atlas_sprites, "player_dead", row_y, slot, [draw_character(pal, "front", pose="dead")])

    slot = 0
    for view in ("front", "back"):
        slot = _char_strip(img, atlas_sprites, f"player_crouchwalk_{view}", extra_row_y, slot,
                           [draw_character(pal, view, pose="crouchwalk", frame=f) for f in range(4)])

    # The old one-view names, for anything still asking for them.
    atlas_sprites["player_idle"] = dict(atlas_sprites["player_idle_front"])
    atlas_sprites["player_crouch"] = dict(atlas_sprites["player_crouch_front"])
    atlas_sprites["player_walk_right"] = dict(atlas_sprites["player_walk_front"])
    atlas_sprites["player_walk_down"] = dict(atlas_sprites["player_walk_front"])
    atlas_sprites["player_walk_up"] = dict(atlas_sprites["player_walk_back"])


def gen_zombie_sprites(img, atlas_sprites, row_y):
    pal = ZOMBIE_PAL
    slot = 0
    for view in ("front", "back"):
        slot = _char_strip(img, atlas_sprites, f"zombie_walk_{view}", row_y, slot,
                           [draw_character(pal, view, zombie=True, pose="walk", frame=f) for f in range(4)])
    for view in ("front", "back"):
        slot = _char_strip(img, atlas_sprites, f"zombie_idle_{view}", row_y, slot,
                           [draw_character(pal, view, zombie=True)])
    for view in ("front", "back"):
        slot = _char_strip(img, atlas_sprites, f"zombie_attack_{view}", row_y, slot,
                           [draw_character(pal, view, zombie=True, pose="attack")])
    slot = _char_strip(img, atlas_sprites, "zombie_dead", row_y, slot,
                       [draw_character(pal, "front", zombie=True, pose="dead")])

    atlas_sprites["zombie_idle"] = dict(atlas_sprites["zombie_idle_front"])
    atlas_sprites["zombie_attack"] = dict(atlas_sprites["zombie_attack_front"])
    atlas_sprites["zombie_walk_right"] = dict(atlas_sprites["zombie_walk_front"])
    atlas_sprites["zombie_walk_down"] = dict(atlas_sprites["zombie_walk_front"])
    atlas_sprites["zombie_walk_up"] = dict(atlas_sprites["zombie_walk_back"])


def _tint_frame(frame, colour, amount):
    out = frame.copy()
    tr, tg, tb = colour
    for y in range(ENTITY_SIZE):
        for x in range(ENTITY_SIZE):
            r, g, b, a = out.getpixel((x, y))
            if a == 255:
                out.putpixel((x, y), (int(r + (tr - r) * amount), int(g + (tg - g) * amount),
                                      int(b + (tb - b) * amount), a))
    return out


# --- Item Generation ---

def draw_small_rect(img, ox, oy, x1, y1, x2, y2, color):
    for py in range(y1, y2):
        for px in range(x1, x2):
            n = random.randint(-3, 3)
            r, g, b = color
            img.putpixel((ox + px, oy + py), (max(0, min(255, r + n)), max(0, min(255, g + n)), max(0, min(255, b + n)), 255))


def gen_item_sprite(img, ox, oy, name):
    if name == "item_canned_food":
        draw_small_rect(img, ox, oy, 10, 8, 22, 24, (160, 50, 40))
        draw_small_rect(img, ox, oy, 10, 8, 22, 11, (180, 180, 180))
        draw_small_rect(img, ox, oy, 12, 14, 20, 18, (200, 180, 60))
    elif name == "item_apple":
        for py in range(8, 24):
            for px in range(10, 22):
                dy = py - 16
                dx = px - 16
                if dx * dx + dy * dy < 50:
                    n = random.randint(-8, 8)
                    img.putpixel((ox + px, oy + py), (max(0, 180 + n), max(0, 30 + n), max(0, 30 + n), 255))
        img.putpixel((ox + 16, oy + 8), (80, 50, 20, 255))
        img.putpixel((ox + 16, oy + 7), (80, 50, 20, 255))
    elif name == "item_bread":
        draw_small_rect(img, ox, oy, 8, 14, 24, 22, (190, 160, 90))
        draw_small_rect(img, ox, oy, 9, 12, 23, 15, (210, 180, 110))
    elif name == "item_water_bottle":
        draw_small_rect(img, ox, oy, 12, 6, 20, 26, (60, 120, 200))
        draw_small_rect(img, ox, oy, 13, 4, 19, 7, (180, 180, 180))
        for py in range(10, 20):
            img.putpixel((ox + 14, oy + py), (100, 160, 230, 255))
    elif name == "item_bandage":
        draw_small_rect(img, ox, oy, 10, 10, 22, 22, (230, 225, 215))
        draw_small_rect(img, ox, oy, 14, 10, 18, 22, (200, 50, 40))
        draw_small_rect(img, ox, oy, 10, 14, 22, 18, (200, 50, 40))
    elif name == "item_knife":
        for i in range(14):
            img.putpixel((ox + 16 + i, oy + 15 - i // 2), (180, 185, 190, 255))
            img.putpixel((ox + 16 + i, oy + 16 - i // 2), (160, 165, 170, 255))
        draw_small_rect(img, ox, oy, 10, 16, 17, 20, (90, 55, 30))
    elif name == "item_bat":
        for i in range(20):
            img.putpixel((ox + 6 + i, oy + 22 - i), (130, 95, 55, 255))
            img.putpixel((ox + 7 + i, oy + 22 - i), (120, 85, 45, 255))
        draw_small_rect(img, ox, oy, 24, 2, 28, 8, (130, 95, 55))
    elif name == "item_pipe":
        for i in range(22):
            img.putpixel((ox + 5 + i, oy + 20 - i), (120, 120, 125, 255))
            img.putpixel((ox + 6 + i, oy + 20 - i), (100, 100, 105, 255))
    elif name == "item_plank":
        draw_small_rect(img, ox, oy, 6, 10, 26, 16, (160, 130, 80))
        draw_small_rect(img, ox, oy, 6, 16, 26, 22, (140, 110, 65))
    elif name == "item_axe":
        for i in range(18):
            img.putpixel((ox + 8 + i, oy + 22 - i), (120, 85, 45, 255))
        draw_small_rect(img, ox, oy, 24, 2, 30, 10, (150, 155, 160))
    elif name == "item_sleeping_bag":
        draw_small_rect(img, ox, oy, 6, 12, 26, 22, (60, 80, 130))
        draw_small_rect(img, ox, oy, 6, 12, 10, 22, (50, 70, 115))
    elif name == "item_campfire":
        for angle in range(0, 360, 45):
            px = int(16 + 8 * math.cos(math.radians(angle)))
            py = int(16 + 6 * math.sin(math.radians(angle)))
            draw_small_rect(img, ox, oy, px - 1, py - 1, px + 2, py + 2, (100, 95, 85))
        for offset in [(15, 14), (16, 12), (17, 14), (16, 16)]:
            img.putpixel((ox + offset[0], oy + offset[1]), (220, 150, 40, 255))
        img.putpixel((ox + 16, oy + 13), (240, 80, 30, 255))
    elif name == "item_noise_trap":
        draw_small_rect(img, ox, oy, 10, 12, 22, 22, (140, 140, 145))
        draw_small_rect(img, ox, oy, 12, 8, 20, 13, (180, 175, 50))
    elif name == "item_barricade":
        for i in range(24):
            y1 = 4 + i
            if y1 < 28:
                img.putpixel((ox + 4 + i, oy + y1), (140, 110, 65, 255))
                img.putpixel((ox + 27 - i, oy + y1), (130, 100, 55, 255))
    elif name == "item_ammo":
        draw_small_rect(img, ox, oy, 10, 12, 22, 22, (150, 130, 50))
        for px in range(12, 20, 3):
            draw_small_rect(img, ox, oy, px, 8, px + 2, 13, (180, 160, 80))
    elif name == "item_medkit":
        draw_small_rect(img, ox, oy, 8, 10, 24, 24, (220, 220, 220))
        draw_small_rect(img, ox, oy, 14, 12, 18, 22, (200, 40, 40))
        draw_small_rect(img, ox, oy, 10, 15, 22, 19, (200, 40, 40))
    elif name == "item_wood":
        draw_small_rect(img, ox, oy, 8, 8, 14, 26, (145, 115, 70))
        draw_small_rect(img, ox, oy, 18, 10, 24, 24, (135, 105, 60))
    elif name == "item_scrap":
        for _ in range(8):
            px = random.randint(8, 24)
            py = random.randint(8, 24)
            draw_small_rect(img, ox, oy, px, py, px + 4, py + 3, (130, 130, 135))
    elif name == "item_rope":
        for angle in range(0, 360, 15):
            px = int(16 + 7 * math.cos(math.radians(angle)))
            py = int(16 + 5 * math.sin(math.radians(angle)))
            img.putpixel((ox + px, oy + py), (160, 140, 90, 255))
    elif name == "item_fuel":
        draw_small_rect(img, ox, oy, 10, 8, 22, 24, (180, 50, 45))
        draw_small_rect(img, ox, oy, 14, 4, 18, 9, (120, 120, 125))
    elif name == "item_glass":
        draw_small_rect(img, ox, oy, 10, 10, 22, 24, (180, 210, 230))
        for py in range(12, 22):
            img.putpixel((ox + 12, oy + py), (200, 230, 245, 200))
    else:
        draw_small_rect(img, ox, oy, 8, 8, 24, 24, (150, 150, 150))


# --- Environment Sprites ---

def gen_env_sprite(img, ox, oy, name):
    if name == "env_grave":
        draw_small_rect(img, ox, oy, 12, 8, 20, 12, (140, 140, 135))
        draw_small_rect(img, ox, oy, 10, 12, 22, 24, (130, 130, 125))
        draw_small_rect(img, ox, oy, 14, 14, 18, 20, (110, 110, 105))
    elif name == "env_grave2":
        draw_small_rect(img, ox, oy, 11, 10, 21, 24, (120, 120, 115))
        draw_small_rect(img, ox, oy, 13, 6, 19, 11, (130, 130, 125))
    elif name == "env_blood_small":
        for _ in range(12):
            px = random.randint(10, 22)
            py = random.randint(12, 24)
            img.putpixel((ox + px, oy + py), (150, 20, 20, 200))
    elif name == "env_blood_large":
        for _ in range(40):
            px = random.randint(6, 26)
            py = random.randint(8, 26)
            a = random.randint(140, 220)
            img.putpixel((ox + px, oy + py), (155 + random.randint(-20, 20), 15, 15, a))
    elif name == "env_crate":
        draw_small_rect(img, ox, oy, 8, 10, 24, 24, (150, 120, 70))
        for y in [14, 18, 22]:
            for px in range(8, 24):
                img.putpixel((ox + px, oy + y), (120, 95, 55, 255))
    elif name == "env_shelf":
        draw_small_rect(img, ox, oy, 8, 4, 24, 28, (120, 90, 50))
        for y in [8, 14, 20]:
            draw_small_rect(img, ox, oy, 6, y, 26, y + 2, (100, 75, 40))
        # Add random clutter on shelves
        for y in [8, 14, 20]:
            if random.random() > 0.3:
                cx = random.randint(10, 20)
                cc = random.choice([(180, 50, 40), (60, 120, 200), (180, 180, 180), (220, 220, 220)])
                draw_small_rect(img, ox, oy, cx, y - 4, cx + 4, y, cc)
    elif name == "env_bed":
        # Wooden frame
        draw_small_rect(img, ox, oy, 4, 8, 28, 28, (110, 85, 45))
        # Mattress
        draw_small_rect(img, ox, oy, 6, 12, 26, 26, (220, 220, 215))
        # Blanket
        draw_small_rect(img, ox, oy, 6, 16, 26, 26, (80, 100, 160))
        # Pillow
        draw_small_rect(img, ox, oy, 8, 10, 24, 14, (240, 240, 240))
    elif name == "env_chair":
        # Seat
        draw_small_rect(img, ox, oy, 10, 16, 22, 20, (130, 105, 65))
        # Backrest
        draw_small_rect(img, ox, oy, 10, 10, 22, 16, (110, 88, 50))
        # Legs
        draw_small_rect(img, ox, oy, 10, 20, 12, 26, (90, 70, 40))
        draw_small_rect(img, ox, oy, 20, 20, 22, 26, (90, 70, 40))
    elif name == "env_table":
        # Table top
        draw_small_rect(img, ox, oy, 6, 12, 26, 18, (140, 115, 75))
        # Legs
        draw_small_rect(img, ox, oy, 8, 18, 10, 26, (100, 80, 50))
        draw_small_rect(img, ox, oy, 22, 18, 24, 26, (100, 80, 50))
    elif name == "env_cabinet":
        # Frame
        draw_small_rect(img, ox, oy, 8, 6, 24, 26, (120, 95, 55))
        # Doors
        draw_small_rect(img, ox, oy, 10, 8, 15, 24, (100, 75, 40))
        draw_small_rect(img, ox, oy, 17, 8, 22, 24, (100, 75, 40))
        # Handles
        draw_small_rect(img, ox, oy, 14, 14, 15, 18, (200, 200, 200))
        draw_small_rect(img, ox, oy, 17, 14, 18, 18, (200, 200, 200))
    elif name == "env_couch":
        # Frame/Base
        draw_small_rect(img, ox, oy, 4, 14, 28, 22, (90, 120, 90))
        # Backrest
        draw_small_rect(img, ox, oy, 6, 8, 26, 14, (70, 100, 70))
        # Armrests
        draw_small_rect(img, ox, oy, 4, 12, 8, 20, (80, 110, 80))
        draw_small_rect(img, ox, oy, 24, 12, 28, 20, (80, 110, 80))
        # Cushions
        draw_small_rect(img, ox, oy, 8, 14, 16, 20, (100, 130, 100))
        draw_small_rect(img, ox, oy, 16, 14, 24, 20, (100, 130, 100))
    elif name == "env_lamp":
        # Base
        draw_small_rect(img, ox, oy, 14, 24, 18, 26, (80, 80, 80))
        # Pole
        draw_small_rect(img, ox, oy, 15, 10, 17, 24, (100, 100, 100))
        # Shade
        draw_small_rect(img, ox, oy, 12, 6, 20, 12, (240, 230, 200))
        # Light glow (if not off)
        img.putpixel((ox + 16, oy + 11), (255, 255, 200, 255))
        img.putpixel((ox + 15, oy + 11), (255, 255, 200, 255))
    elif name == "env_campfire_lit":
        gen_item_sprite(img, ox, oy, "item_campfire")
        for _ in range(12):
            px = random.randint(13, 19)
            py = random.randint(10, 16)
            c = random.choice([(255, 200, 50), (255, 140, 30), (255, 80, 20)])
            img.putpixel((ox + px, oy + py), (*c, 255))
    elif name == "env_barricade":
        gen_item_sprite(img, ox, oy, "item_barricade")
    elif name == "env_noise_trap":
        gen_item_sprite(img, ox, oy, "item_noise_trap")
    elif name == "env_campfire":
        gen_item_sprite(img, ox, oy, "item_campfire")
    elif name == "env_sleeping_area":
        draw_small_rect(img, ox, oy, 6, 12, 26, 24, (55, 70, 120))
        draw_small_rect(img, ox, oy, 6, 12, 10, 18, (190, 185, 175))
    else:
        draw_small_rect(img, ox, oy, 8, 8, 24, 24, (100, 100, 100))


# --- UI Icons ---

def gen_ui_sprite(img, ox, oy, name):
    if name == "ui_heart":
        for py in range(8, 24):
            for px in range(8, 24):
                dx = px - 16
                dy = py - 16
                in_heart = (dx * dx + (dy - 2) * (dy - 2) - 36) ** 3 - dx * dx * (dy - 2) ** 3 < 0
                if in_heart:
                    img.putpixel((ox + px, oy + py), (200, 50, 50, 255))
    elif name == "ui_stamina":
        points = [(18, 6), (14, 14), (18, 14), (14, 26), (20, 16), (16, 16), (20, 6)]
        draw = ImageDraw.Draw(img)
        poly = [(ox + p[0], oy + p[1]) for p in points]
        draw.polygon(poly, fill=(50, 200, 80, 255))
    elif name == "ui_hunger":
        for py in range(6, 26):
            img.putpixel((ox + 16, oy + py), (200, 180, 60, 255))
        for px in [12, 14, 16, 18, 20]:
            for py in range(6, 12):
                img.putpixel((ox + px, oy + py), (200, 180, 60, 255))
    elif name == "ui_thirst":
        for py in range(8, 24):
            width = max(0, min(6, (py - 8) // 2))
            for px in range(16 - width, 16 + width + 1):
                img.putpixel((ox + px, oy + py), (60, 140, 220, 255))
    elif name == "ui_sleep":
        for sx, sy in [(10, 10), (16, 14), (22, 18)]:
            for dx in range(6):
                img.putpixel((ox + sx + dx, oy + sy), (150, 130, 200, 255))
                img.putpixel((ox + sx + dx, oy + sy + 4), (150, 130, 200, 255))
            img.putpixel((ox + sx + 5, oy + sy + 1), (150, 130, 200, 255))
            img.putpixel((ox + sx, oy + sy + 3), (150, 130, 200, 255))
    elif name == "ui_clock":
        for angle in range(0, 360, 10):
            px = int(16 + 8 * math.cos(math.radians(angle)))
            py = int(16 + 8 * math.sin(math.radians(angle)))
            img.putpixel((ox + px, oy + py), (200, 200, 180, 255))
        for px, py in [(16, 12), (16, 14), (20, 16), (18, 16)]:
            img.putpixel((ox + px, oy + py), (200, 200, 180, 255))
    elif name == "ui_skull":
        for py in range(8, 20):
            for px in range(10, 22):
                dy = py - 14
                dx = px - 16
                if dx * dx / 40.0 + dy * dy / 35.0 <= 1.0:
                    img.putpixel((ox + px, oy + py), (220, 215, 200, 255))
        for px, py in [(13, 13), (14, 13), (18, 13), (19, 13)]:
            img.putpixel((ox + px, oy + py), (40, 40, 40, 255))
        for px in range(13, 20):
            img.putpixel((ox + px, oy + 20), (200, 195, 180, 255))
    else:
        draw_small_rect(img, ox, oy, 8, 8, 24, 24, (200, 200, 200))


# --- FX Sprites ---

def gen_fx_sprite(img, ox, oy, name, frame=0):
    if name == "fx_slash":
        for angle in range(-30 + frame * 10, 30 + frame * 10, 5):
            r = 4 + frame * 2
            px = int(16 + r * math.cos(math.radians(angle)))
            py = int(16 + r * math.sin(math.radians(angle)))
            if 0 <= px < 32 and 0 <= py < 32:
                alpha = max(60, 255 - frame * 50)
                img.putpixel((ox + px, oy + py), (255, 255, 220, alpha))
    elif name == "fx_impact":
        for angle in range(0, 360, 30):
            r = 3 + frame * 4
            px = int(16 + r * math.cos(math.radians(angle)))
            py = int(16 + r * math.sin(math.radians(angle)))
            if 0 <= px < 32 and 0 <= py < 32:
                img.putpixel((ox + px, oy + py), (255, 230, 150, 200))
    elif name == "fx_blood":
        count = 8 + frame * 6
        for _ in range(count):
            spread = 4 + frame * 4
            px = 16 + random.randint(-spread, spread)
            py = 16 + random.randint(-spread, spread)
            if 0 <= px < 32 and 0 <= py < 32:
                img.putpixel((ox + px, oy + py), (180 + random.randint(-30, 30), 15, 15, 220))
    elif name == "fx_footstep":
        count = 3 + frame * 3
        for _ in range(count):
            px = 16 + random.randint(-3 - frame * 2, 3 + frame * 2)
            py = 20 + random.randint(-2 - frame, 2 + frame)
            if 0 <= px < 32 and 0 <= py < 32:
                g = 140 + random.randint(-20, 20)
                img.putpixel((ox + px, oy + py), (g, g - 10, g - 20, max(10, 150 - frame * 40)))


# --- Solid/Utility Sprites ---

def gen_solid(img, ox, oy, color):
    for py in range(32):
        for px in range(32):
            img.putpixel((ox + px, oy + py), (*color, 255))


# --- Main Atlas Assembly ---

def generate_atlas():
    img = Image.new("RGBA", (ATLAS_W, ATLAS_H), (0, 0, 0, 0))
    sprites = {}

    # Row 0: Tiles (y=0, h=32, each 64x32)
    tile_funcs = [
        ("tile_grass", gen_tile_grass),
        ("tile_grass2", gen_tile_grass2),
        ("tile_dirt", gen_tile_dirt),
        ("tile_concrete", gen_tile_concrete),
        ("tile_carpet", gen_tile_carpet),
        ("tile_gravel", gen_tile_gravel),
        ("tile_metal", gen_tile_metal),
        ("tile_water", gen_tile_water),
        ("tile_wall_top", gen_tile_wall_top),
        ("tile_wall_face", gen_tile_wall_face),
        ("tile_door_closed", gen_tile_door_closed),
        ("tile_door_open", gen_tile_door_open),
        ("tile_solid_white", gen_tile_solid_white),
        ("tile_solid_dark", gen_tile_solid_dark),
        ("tile_window", gen_tile_window),
        ("tile_fence", gen_tile_fence),
    ]

    for i, (name, func) in enumerate(tile_funcs):
        x = i * ISO_TILE_W
        func(img, x, 0)
        sprites[name] = {"x": x, "y": 0, "w": ISO_TILE_W, "h": ISO_TILE_H}

    # Row 1: Player (y=32, h=64); crouch-walk frames overflow to y=848
    gen_player_sprites(img, sprites, 32, 848)

    # Row 2: Zombies (y=96, h=64)
    gen_zombie_sprites(img, sprites, 96)

    # Row 3: Items (y=160, h=32)
    items_row = [
        "item_canned_food", "item_apple", "item_bread", "item_water_bottle",
        "item_bandage", "item_knife", "item_bat", "item_pipe",
        "item_plank", "item_axe", "item_sleeping_bag", "item_campfire",
        "item_noise_trap", "item_barricade", "item_ammo", "item_medkit",
    ]
    for i, name in enumerate(items_row):
        x = i * ITEM_SIZE
        gen_item_sprite(img, x, 160, name)
        sprites[name] = {"x": x, "y": 160, "w": ITEM_SIZE, "h": ITEM_SIZE}

    # Row 4: Environment + extra items (y=192, h=32)
    env_row = [
        "env_grave", "env_grave2", "env_blood_small", "env_blood_large",
        "env_crate", "env_shelf", "env_bed", "env_campfire_lit",
        "env_barricade", "env_noise_trap", "env_campfire", "env_sleeping_area",
        "item_wood", "item_scrap", "item_rope", "item_fuel",
        "env_chair", "env_table", "env_cabinet", "env_couch", "env_lamp",
    ]
    for i, name in enumerate(env_row):
        x = i * ITEM_SIZE
        if name.startswith("env_"):
            gen_env_sprite(img, x, 192, name)
        else:
            gen_item_sprite(img, x, 192, name)
        sprites[name] = {"x": x, "y": 192, "w": ITEM_SIZE, "h": ITEM_SIZE}

    # Row 5: UI icons + misc (y=224, h=32)
    ui_row = [
        "ui_heart", "ui_stamina", "ui_hunger", "ui_thirst",
        "ui_sleep", "ui_clock", "ui_skull", "item_glass",
    ]
    for i, name in enumerate(ui_row):
        x = i * ITEM_SIZE
        if name.startswith("ui_"):
            gen_ui_sprite(img, x, 224, name)
        else:
            gen_item_sprite(img, x, 224, name)
        sprites[name] = {"x": x, "y": 224, "w": ITEM_SIZE, "h": ITEM_SIZE}

    # Row 6: FX (y=256, h=32)
    fx_defs = [
        ("fx_slash", 4),
        ("fx_impact", 2),
        ("fx_blood", 2),
        ("fx_footstep", 2),
    ]
    fx_x = 0
    for name, frame_count in fx_defs:
        frames_list = []
        start_x = fx_x
        for f in range(frame_count):
            gen_fx_sprite(img, fx_x, 256, name, f)
            frames_list.append({"x": fx_x, "y": 256, "w": ITEM_SIZE, "h": ITEM_SIZE})
            fx_x += ITEM_SIZE
        entry = {"x": start_x, "y": 256, "w": ITEM_SIZE, "h": ITEM_SIZE}
        if frame_count > 1:
            entry["frames"] = frames_list
        sprites[name] = entry

    # Row 7: Solids + tints (y=288, h=32)
    solids = [
        ("solid_white", (255, 255, 255)),
        ("solid_black", (0, 0, 0)),
        ("solid_red", (200, 40, 40)),
        ("solid_green", (40, 180, 60)),
        ("solid_blue", (40, 80, 200)),
        ("solid_yellow", (220, 200, 50)),
        ("solid_orange", (220, 140, 40)),
        ("solid_purple", (140, 60, 180)),
        ("tint_night", (10, 15, 40)),
        ("tint_dusk", (60, 40, 50)),
        ("tint_dark", (5, 5, 10)),
    ]
    for i, (name, color) in enumerate(solids):
        x = i * ITEM_SIZE
        gen_solid(img, x, 288, color)
        sprites[name] = {"x": x, "y": 288, "w": ITEM_SIZE, "h": ITEM_SIZE}

    # Rows 8+: structures (walls, doorways, roofs).
    add_structure_sprites(img, sprites)
    add_legacy_props(img, sprites)
    add_item_icons(img, sprites)
    add_furniture_sprites(img, sprites)

    return img, sprites


# ---------------------------------------------------------------------------
# Exact 2:1 diamonds, ground textures and structures (walls, doorways, roofs).
#
# Everything below uses its own random.Random instances, so the older atlas
# content above stays byte-identical when this file changes.
# ---------------------------------------------------------------------------

WALL_H = 56       # full wall height in pixels at zoom 1 (a little taller than a person)
WALL_LOW_H = 8    # cut-away stub drawn when a wall would hide the player
TILE_VARIANTS = 4 # rows of variants per ground tile in the tileset


def diamond_span(y):
    """Inclusive x range of row y (0..31) in an exact 64x32 diamond.

    Rows are 4k+2 pixels wide, so diamonds placed at (+-32, +-16) offsets
    tile with no gaps and no overlap.
    """
    k = y if y < 16 else 31 - y
    return 31 - 2 * k, 32 + 2 * k


def diamond_bottom(x):
    """Lowest diamond row that contains column x (1..62)."""
    xm = x if x <= 31 else 63 - x
    k = (32 - xm) // 2
    return 31 - k


def tile_world(px, py):
    """Local world coordinates (0..32) of diamond pixel (px, py)."""
    wx = py + (px - 31.5) / 2.0
    wy = py - (px - 31.5) / 2.0
    return wx, wy


def clamp8(v):
    return max(0, min(255, int(round(v))))


def mix(a, b, t):
    return tuple(clamp8(a[i] + (b[i] - a[i]) * t) for i in range(3))


def shade(c, d):
    return (clamp8(c[0] + d), clamp8(c[1] + d), clamp8(c[2] + d))


def put(img, x, y, rgb, a=255):
    img.putpixel((x, y), (rgb[0], rgb[1], rgb[2], a))


def for_diamond(fn):
    for py in range(32):
        x0, x1 = diamond_span(py)
        for px in range(x0, x1 + 1):
            fn(px, py)


# --- Ground textures -------------------------------------------------------
# Each takes (img, ox, oy, rng) and paints one 64x32 diamond. Patterns are laid
# out in tile-local world coordinates so they continue across tile borders.

def tex_grass(img, ox, oy, rng, base=(64, 112, 48)):
    def px_fn(px, py):
        n = rng.randint(-10, 10)
        put(img, ox + px, oy + py, shade(base, n))
    for_diamond(px_fn)
    # Blades: short vertical strokes, lighter on top.
    for _ in range(70):
        py = rng.randint(2, 29)
        x0, x1 = diamond_span(py)
        px = rng.randint(x0, x1)
        light = rng.random() < 0.55
        col = shade(base, 22 if light else -18)
        put(img, ox + px, oy + py, col)
        if py > 0:
            a0, a1 = diamond_span(py - 1)
            if a0 <= px <= a1:
                put(img, ox + px, oy + py - 1, shade(col, 10 if light else -6))
    # A few flowers/weeds.
    for _ in range(rng.randint(0, 2)):
        py = rng.randint(4, 27)
        x0, x1 = diamond_span(py)
        px = rng.randint(x0 + 2, x1 - 2) if x1 - x0 > 4 else x0
        put(img, ox + px, oy + py, rng.choice([(196, 188, 92), (180, 150, 190), (210, 210, 200)]))


def tex_grass_dark(img, ox, oy, rng):
    tex_grass(img, ox, oy, rng, base=(58, 104, 45))


def tex_dirt(img, ox, oy, rng):
    base = (116, 88, 62)
    def px_fn(px, py):
        put(img, ox + px, oy + py, shade(base, rng.randint(-9, 9)))
    for_diamond(px_fn)
    for _ in range(26):
        py = rng.randint(1, 30)
        x0, x1 = diamond_span(py)
        px = rng.randint(x0, x1)
        put(img, ox + px, oy + py, shade(base, rng.choice([-24, -16, 14])))


def tex_gravel(img, ox, oy, rng):
    base = (122, 116, 104)
    def px_fn(px, py):
        put(img, ox + px, oy + py, shade(base, rng.randint(-14, 14)))
    for_diamond(px_fn)
    for _ in range(40):
        py = rng.randint(1, 30)
        x0, x1 = diamond_span(py)
        px = rng.randint(x0, x1)
        v = rng.randint(-34, 26)
        put(img, ox + px, oy + py, shade(base, v))


def tex_asphalt(img, ox, oy, rng):
    base = (70, 72, 76)
    def px_fn(px, py):
        put(img, ox + px, oy + py, shade(base, rng.randint(-6, 6)))
    for_diamond(px_fn)
    for _ in range(22):
        py = rng.randint(1, 30)
        x0, x1 = diamond_span(py)
        px = rng.randint(x0, x1)
        put(img, ox + px, oy + py, shade(base, rng.choice([-12, 10, 16])))
    # Occasional crack.
    if rng.random() < 0.35:
        py = rng.randint(6, 24)
        x0, x1 = diamond_span(py)
        px = rng.randint(x0 + 4, max(x0 + 4, x1 - 10))
        for step in range(rng.randint(5, 11)):
            cx = px + step
            cy = py + (step // 3) * rng.choice([-1, 0, 1])
            if 0 <= cy < 32:
                a0, a1 = diamond_span(cy)
                if a0 <= cx <= a1:
                    put(img, ox + cx, oy + cy, shade(base, -22))


def tex_sidewalk(img, ox, oy, rng):
    base = (152, 150, 142)
    slab = 16.0
    def px_fn(px, py):
        wx, wy = tile_world(px, py)
        c = shade(base, rng.randint(-5, 5))
        fx = wx % slab
        fy = wy % slab
        if fx < 0.75 or fy < 0.75:
            c = shade(base, -26)
        elif fx < 1.5 or fy < 1.5:
            c = shade(base, 8)
        put(img, ox + px, oy + py, c)
    for_diamond(px_fn)


def tex_concrete(img, ox, oy, rng):
    base = (138, 138, 132)
    def px_fn(px, py):
        wx, wy = tile_world(px, py)
        c = shade(base, rng.randint(-5, 5))
        if wx < 0.6 or wy < 0.6:
            c = shade(base, -14)
        put(img, ox + px, oy + py, c)
    for_diamond(px_fn)
    for _ in range(rng.randint(0, 3)):
        py = rng.randint(4, 27)
        x0, x1 = diamond_span(py)
        put(img, ox + rng.randint(x0, x1), oy + py, shade(base, -18))


def tex_tile_floor(img, ox, oy, rng):
    light = (196, 192, 178)
    dark = (150, 146, 136)
    cell = 8.0
    def px_fn(px, py):
        wx, wy = tile_world(px, py)
        checker = (int(wx // cell) + int(wy // cell)) % 2
        c = light if checker == 0 else dark
        c = shade(c, rng.randint(-3, 3))
        if wx % cell < 0.6 or wy % cell < 0.6:
            c = shade(c, -18)
        put(img, ox + px, oy + py, c)
    for_diamond(px_fn)


def tex_wood_floor(img, ox, oy, rng):
    plank_w = 6.4  # five planks per tile, running along world x
    tones = [(128, 88, 56), (138, 96, 60), (120, 82, 52), (134, 92, 58), (124, 86, 54)]
    rng.shuffle(tones)
    joints = [rng.uniform(4.0, 28.0) for _ in tones]
    def px_fn(px, py):
        wx, wy = tile_world(px, py)
        i = min(len(tones) - 1, max(0, int(wy // plank_w)))
        c = shade(tones[i], rng.randint(-4, 4))
        if wy % plank_w < 0.7:
            c = shade(c, -30)
        elif abs(wx - joints[i]) < 0.6:
            c = shade(c, -24)
        elif (int(wx * 1.7 + i * 5) % 9) == 0:
            c = shade(c, -8)  # grain
        put(img, ox + px, oy + py, c)
    for_diamond(px_fn)


def tex_carpet(img, ox, oy, rng):
    base = (112, 62, 58)
    def px_fn(px, py):
        wx, wy = tile_world(px, py)
        c = shade(base, rng.randint(-5, 5))
        if (int(wx) + int(wy)) % 4 == 0:
            c = shade(c, -10)
        put(img, ox + px, oy + py, c)
    for_diamond(px_fn)


def tex_metal_floor(img, ox, oy, rng):
    base = (102, 108, 114)
    def px_fn(px, py):
        wx, wy = tile_world(px, py)
        c = shade(base, rng.randint(-3, 3))
        # Diamond plate: short raised dashes on a grid.
        gx = wx % 6.0
        gy = wy % 6.0
        if 1.0 < gx < 3.4 and 1.4 < gy < 2.4:
            c = shade(base, 22)
        elif 1.0 < gx < 3.4 and 2.4 <= gy < 3.0:
            c = shade(base, -16)
        put(img, ox + px, oy + py, c)
    for_diamond(px_fn)


def tex_water(img, ox, oy, rng):
    base = (40, 84, 128)
    phase = rng.uniform(0, 6.28)
    def px_fn(px, py):
        wx, wy = tile_world(px, py)
        wave = math.sin((wx + wy) * 0.55 + phase) + 0.5 * math.sin(wx * 0.9 - phase)
        c = shade(base, int(wave * 7) + rng.randint(-3, 3))
        put(img, ox + px, oy + py, c)
    for_diamond(px_fn)
    for _ in range(6):
        py = rng.randint(4, 27)
        x0, x1 = diamond_span(py)
        px = rng.randint(x0 + 2, max(x0 + 2, x1 - 4))
        for d in range(rng.randint(2, 4)):
            if px + d <= x1:
                put(img, ox + px + d, oy + py, (120, 168, 200))


def tex_wall_base(img, ox, oy, rng):
    # Ground under wall blocks; covered by the block, kept for the minimap.
    base = (88, 82, 74)
    def px_fn(px, py):
        put(img, ox + px, oy + py, shade(base, rng.randint(-4, 4)))
    for_diamond(px_fn)


def tex_solid(color):
    def fn(img, ox, oy, rng):
        def px_fn(px, py):
            put(img, ox + px, oy + py, color)
        for_diamond(px_fn)
    return fn


# GID -> texture. GIDs 11-14 were unused by the map and now hold new floors.
GROUND_TEXTURES = {
    2: tex_concrete,          # kGidFloor
    3: tex_gravel,            # kGidGravel
    4: tex_dirt,              # kGidDirt
    5: tex_water,             # kGidWater
    6: tex_carpet,            # kGidCarpet
    7: tex_wall_base,         # kGidWall (under wall blocks)
    8: tex_metal_floor,       # kGidMetal
    9: tex_grass,             # kGidGrass
    10: tex_grass_dark,       # kGidGrass2
    11: tex_wood_floor,       # kGidWood
    12: tex_tile_floor,       # kGidTileFloor
    13: tex_asphalt,          # kGidAsphalt
    14: tex_sidewalk,         # kGidSidewalk
    15: tex_solid((255, 255, 255)),
    16: tex_solid((20, 20, 20)),
}


def generate_tileset():
    """Ground tileset (placeholder.png): 16 columns (GID 1..16) x 4 variant rows.

    GID g, variant v lives at column g-1, row v, i.e. local id v*16 + g-1.
    The renderer picks a variant per tile from a hash of its position.
    """
    cols = 16
    img = Image.new("RGBA", (cols * ISO_TILE_W, TILE_VARIANTS * ISO_TILE_H), (0, 0, 0, 0))
    for gid, fn in GROUND_TEXTURES.items():
        for v in range(TILE_VARIANTS):
            rng = random.Random(gid * 1009 + v * 7919)
            fn(img, (gid - 1) * ISO_TILE_W, v * ISO_TILE_H, rng)
    return img


# --- Structures ------------------------------------------------------------

WALL_MATERIALS = {
    # name: (left face, right face, top cap, accent)
    "plaster": ((184, 168, 140), (146, 132, 108), (92, 86, 80), (120, 108, 90)),
    "brick": ((156, 82, 62), (122, 64, 50), (88, 82, 78), (176, 160, 140)),
    "metal": ((128, 138, 146), (98, 106, 114), (150, 156, 160), (84, 90, 96)),
}

ROOF_COLORS = {
    "residential": (98, 66, 58),
    "commercial": (92, 92, 96),
    "industrial": (128, 136, 142),
}


def face_color(material, left, u, h, height, rng):
    """Color of a wall face pixel. u runs along the face (0..31), h is the
    height above the ground (0 at the bottom)."""
    lc, rc, cap, accent = WALL_MATERIALS[material]
    c = lc if left else rc
    c = shade(c, rng.randint(-3, 3))
    if material == "plaster":
        # Horizontal siding boards with a dark baseboard and light top trim.
        if h < 3:
            return shade(c, -34)
        if h >= height - 2:
            return shade(c, 16)
        if h % 7 == 0:
            return shade(c, -16)
        if h % 7 == 1:
            return shade(c, 6)
        return c
    if material == "brick":
        course = h // 4
        if h < 2:
            return shade(c, -30)
        if h % 4 == 0:
            return accent if left else shade(accent, -26)
        offset = 4 if course % 2 else 0
        if (u + offset) % 8 == 0:
            return accent if left else shade(accent, -26)
        return shade(c, (course * 7 % 5) - 2)
    # metal: vertical corrugation
    if h < 2:
        return shade(c, -30)
    if u % 4 == 0:
        return shade(c, 16)
    if u % 4 == 2:
        return shade(c, -14)
    return c


DOOR_WOOD = (122, 84, 50)


def door_panel(u, h, left):
    """A closed wooden door filling the doorway (u 10..21, h 0..43)."""
    c = DOOR_WOOD
    if u % 4 == 1:
        c = shade(c, -16)  # gaps between vertical planks
    elif u % 4 == 3:
        c = shade(c, 8)
    if h in (10, 11, 32, 33):
        c = shade(DOOR_WOOD, -24)  # cross rails
    if u in (19, 20) and 20 <= h <= 22:
        c = (204, 172, 86)  # handle
    if not left:
        c = shade(c, -18)
    return c


def door_remains(u, h, left):
    """Splintered boards left on the hinges; None where it is broken through."""
    jag = (u * 7 + h * 3) % 5
    keep = u <= 11 + (1 if jag == 0 else 0)          # hinge-side stiles
    keep = keep or h <= 3 + (u * 5) % 4               # stump of the bottom rail
    keep = keep or (31 <= h <= 33 and u <= 14 + jag)  # a rail snapped off mid-way
    if not keep:
        return None
    c = DOOR_WOOD if jag else shade(DOOR_WOOD, 26)    # pale fresh splinters
    if u % 4 == 1:
        c = shade(c, -16)
    if not left:
        c = shade(c, -18)
    return c


# Boards nailed across a closed door, in the door face's (u, h) space: centre
# height and slope of each plank, outermost last.
DOOR_BOARDS = ((13, -0.12), (27, 0.14), (38, -0.08))
BOARD_WOOD = (178, 136, 88)


def make_door_boards(count, left, height=WALL_H, boards=DOOR_BOARDS):
    """An overlay for a closed door with `count` planks nailed across it, on the
    left (south) or right (east) face. Same frame as a wall block of `height`
    (the cut-away stub takes one plank low down)."""
    img = Image.new("RGBA", (ISO_TILE_W, ISO_TILE_H + height), (0, 0, 0, 0))
    for px in range(1, 63):
        if (px <= 31) != left:
            continue
        u = px - 1 if left else 62 - px
        if not 6 <= u <= 25:
            continue
        yb = diamond_bottom(px)
        for centre, slope in boards[:count]:
            mid = centre + round((u - 15.5) * slope)
            # A soft shadow on the door just under the plank.
            put(img, px, yb + height - (mid - 2), (40, 26, 16), 120)
            for dh in range(-1, 3):
                h = mid + dh
                c = BOARD_WOOD
                if dh == 2:
                    c = shade(BOARD_WOOD, 22)   # top edge catches the light
                elif dh == -1:
                    c = shade(BOARD_WOOD, -40)  # underside
                elif (u * 5 + centre) % 7 == 0:
                    c = shade(BOARD_WOOD, -16)  # grain
                if u in (7, 24) and dh in (0, 1):
                    c = (54, 50, 48) if dh == 0 else (150, 146, 140)  # nail heads
                if u in (6, 25) and dh == -1:
                    continue  # ragged sawn ends
                if not left:
                    c = shade(c, -18)
                put(img, px, yb + height - h, c)
    return img


def make_block(material, height, feature=None, seed=0):
    """A wall block: top cap diamond plus the two faces the camera sees.

    feature: None, "window_l", "window_r", "door_l", "door_r", "doorclosed_l",
    "doorclosed_r", "doorbroken_l", "doorbroken_r". The suffix says which face
    carries it (l = left/south face, r = right/east face). "door" is an open
    doorway; "doorclosed" holds a wooden door; "doorbroken" is what is left of
    one after the dead got through.
    """
    rng = random.Random(seed)
    lc, rc, cap, accent = WALL_MATERIALS[material]
    img = Image.new("RGBA", (ISO_TILE_W, ISO_TILE_H + height), (0, 0, 0, 0))

    for px in range(1, 63):
        left = px <= 31
        u = px - 1 if left else 62 - px
        yb = diamond_bottom(px)
        for h in range(height):
            y = yb + height - h
            c = face_color(material, left, u, h, height, rng)
            alpha = 255
            if feature is not None and feature.endswith("_l" if left else "_r"):
                if feature.startswith("window"):
                    if 9 <= u <= 22 and 20 <= h <= 42 and height >= 40:
                        if u in (9, 22) or h in (20, 42) or u == 15 or h == 31:
                            c = (210, 204, 190) if material != "metal" else (170, 176, 180)
                        else:
                            glass = (58, 76, 92) if left else (46, 60, 74)
                            # Simple reflection streak.
                            if (u - h // 2) % 9 in (0, 1):
                                glass = shade(glass, 30)
                            c = glass
                elif feature.startswith("door"):
                    if 9 <= u <= 22 and h <= 44:
                        if u in (9, 22) or h == 44:
                            c = shade(accent, -10) if material != "brick" else (92, 86, 80)
                        elif feature.startswith("doorclosed"):
                            c = door_panel(u, h, left)
                        elif feature.startswith("doorbroken"):
                            c = door_remains(u, h, left)
                            if c is None:
                                alpha = 0
                        else:
                            alpha = 0  # open doorway: see straight through
            if alpha:
                put(img, px, y, c, alpha)

    # Top cap (drawn last so it sits over the faces' top rows).
    for py in range(32):
        x0, x1 = diamond_span(py)
        for px in range(x0, x1 + 1):
            # No outline: neighbouring caps merge into one continuous wall top.
            put(img, px, py, shade(cap, rng.randint(-3, 3)))
    return img


def make_roof(kind, seed=0):
    rng = random.Random(seed)
    base = ROOF_COLORS[kind]
    img = Image.new("RGBA", (ISO_TILE_W, ISO_TILE_H), (0, 0, 0, 0))
    for py in range(32):
        x0, x1 = diamond_span(py)
        for px in range(x0, x1 + 1):
            wx, wy = tile_world(px, py)
            c = shade(base, rng.randint(-4, 4))
            if kind == "residential":
                # Shingle rows along world x, staggered tabs.
                row = int(wy // 4)
                if wy % 4 < 0.8:
                    c = shade(base, -18)
                elif (int(wx + (row % 2) * 3) % 6) == 0:
                    c = shade(base, -10)
                else:
                    c = shade(c, (row % 3) * 3 - 3)
            elif kind == "commercial":
                if rng.random() < 0.08:
                    c = shade(base, rng.choice([-16, 14]))
                if wx % 16 < 0.6 or wy % 16 < 0.6:
                    c = shade(base, -10)  # seams in the membrane
            else:
                if int(wy) % 4 == 0:
                    c = shade(base, 14)
                elif int(wy) % 4 == 2:
                    c = shade(base, -12)
            put(img, px, py, c)
    return img


def add_structure_sprites(img, sprites):
    """Wall blocks, doorways and roofs, placed below the older atlas rows."""
    y_full = 320
    x = 0
    seed = 1
    for material in ("plaster", "brick", "metal"):
        for feature, suffix in ((None, ""), ("window_l", "_window_l"), ("window_r", "_window_r"),
                                ("door_l", "_door_l"), ("door_r", "_door_r")):
            block = make_block(material, WALL_H, feature, seed)
            seed += 1
            img.paste(block, (x, y_full), block)
            sprites["wall_" + material + suffix] = {"x": x, "y": y_full, "w": ISO_TILE_W, "h": ISO_TILE_H + WALL_H}
            x += ISO_TILE_W

    y_low = y_full + ISO_TILE_H + WALL_H
    x = 0
    for material in ("plaster", "brick", "metal"):
        block = make_block(material, WALL_LOW_H, None, seed)
        seed += 1
        img.paste(block, (x, y_low), block)
        sprites["wall_" + material + "_low"] = {"x": x, "y": y_low, "w": ISO_TILE_W, "h": ISO_TILE_H + WALL_LOW_H}
        x += ISO_TILE_W
    for kind in ("residential", "commercial", "industrial"):
        roof = make_roof(kind, seed)
        seed += 1
        img.paste(roof, (x, y_low), roof)
        sprites["roof_" + kind] = {"x": x, "y": y_low, "w": ISO_TILE_W, "h": ISO_TILE_H}
        x += ISO_TILE_W

    # Doors: closed and broken, per material and face. Below the legacy props.
    y_doors = 512
    x = 0
    for material in ("plaster", "brick", "metal"):
        for feature in ("doorclosed_l", "doorclosed_r", "doorbroken_l", "doorbroken_r"):
            block = make_block(material, WALL_H, feature, seed)
            seed += 1
            img.paste(block, (x, y_doors), block)
            sprites["wall_" + material + "_" + feature] = {"x": x, "y": y_doors, "w": ISO_TILE_W, "h": ISO_TILE_H + WALL_H}
            x += ISO_TILE_W

    # Boards over a closed door: overlays drawn on top of it, one per face and
    # count. Beside the player's crouch-walk frames.
    y_boards = 848
    x = 512
    for face in ("l", "r"):
        for count in (1, 2, 3):
            overlay = make_door_boards(count, face == "l")
            img.paste(overlay, (x, y_boards), overlay)
            sprites["door_boards%d_%s" % (count, face)] = {"x": x, "y": y_boards, "w": ISO_TILE_W, "h": ISO_TILE_H + WALL_H}
            x += ISO_TILE_W
    for face in ("l", "r"):
        overlay = make_door_boards(1, face == "l", WALL_LOW_H, ((4, 0.0),))
        img.paste(overlay, (x, y_boards), overlay)
        sprites["door_boardslow_%s" % face] = {"x": x, "y": y_boards, "w": ISO_TILE_W, "h": ISO_TILE_H + WALL_LOW_H}
        x += ISO_TILE_W


# --- Legacy props ------------------------------------------------------------
# 64x64 cells drawn like characters: the renderer puts the bottom centre of the
# cell on the tile centre, so each prop's footprint sits along the bottom.

PROP = 64


def ground_ellipse(img, cx, cy, rx, ry, color_fn):
    """Fill an iso-flattened ellipse; color_fn(dx, dy, t) -> (rgb, alpha) or None."""
    for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
        for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
            dx = (x + 0.5 - cx) / rx
            dy = (y + 0.5 - cy) / ry
            t = dx * dx + dy * dy
            if t <= 1.0 and 0 <= x < PROP and 0 <= y < PROP:
                c = color_fn(dx, dy, t)
                if c is not None:
                    put(img, x, y, c[0], c[1])


def soft_shadow(img, cx, cy, rx, ry, alpha=70):
    def fn(dx, dy, t):
        return ((10, 12, 10), int(alpha * (1.0 - t) ** 0.6))
    ground_ellipse(img, cx, cy, rx, ry, fn)


def dirt_mound(img, rng, cx, cy, rx, ry, base, height=5):
    """A low mound: iso ellipse with a lit top-left and a darker front lip."""
    soft_shadow(img, cx + 2, cy + 2, rx + 3, ry + 2)

    def fn(dx, dy, t):
        light = -dx * 10 - dy * 16
        n = rng.randint(-9, 9)
        c = shade(base, light + n)
        if rng.random() < 0.07:
            c = shade(c, 22)  # pebbles
        return (c, 255)
    # body (raised): draw the front lip first, then the top shifted up
    def lip(dx, dy, t):
        if dy < 0.1:
            return None
        return (shade(base, -26 + rng.randint(-6, 6)), 255)
    ground_ellipse(img, cx, cy, rx, ry, lip)
    ground_ellipse(img, cx, cy - height * 0.5, rx, ry, fn)


def headstone(img, rng, x0, y_base, w, h, stone=(132, 134, 136)):
    """Upright slab with a rounded top, a lit front and a darker east side."""
    side = 4
    top_r = w // 2
    for y in range(y_base - h, y_base):
        for x in range(x0, x0 + w + side):
            # rounded top
            ry = y - (y_base - h)
            if ry < top_r:
                cxr = x0 + w / 2.0
                if (x + 0.5 - cxr) ** 2 + (top_r - ry - 0.5) ** 2 > top_r ** 2 and x < x0 + w:
                    continue
                if x >= x0 + w and ry < top_r - 2:
                    continue
            if x < x0 + w:
                c = shade(stone, 10 - ry * 0.5 + rng.randint(-5, 5))
                if x == x0:
                    c = shade(c, 14)
            else:
                # side face, sheared up by the iso angle
                if y - (x - x0 - w) // 2 < y_base - h + top_r - 1:
                    continue
                c = shade(stone, -34 + rng.randint(-4, 4))
            put(img, x, y, c)
    # carved lines
    for i, yy in enumerate((y_base - h + top_r + 3, y_base - h + top_r + 6, y_base - h + top_r + 9)):
        if yy >= y_base - 3:
            break
        inset = 3 if i else 2
        for x in range(x0 + inset, x0 + w - inset):
            put(img, x, yy, shade(stone, -30))
    # moss
    for _ in range(6):
        x = x0 + rng.randint(0, w - 1)
        y = y_base - rng.randint(1, 5)
        put(img, x, y, (78, 104, 58))


def wooden_cross(img, x_mid, y_base, h):
    wood = (118, 86, 54)
    dark = shade(wood, -32)
    for y in range(y_base - h, y_base):
        put(img, x_mid, y, wood)
        put(img, x_mid + 1, y, wood)
        put(img, x_mid + 2, y, dark)
    arm_y = y_base - h + 5
    for x in range(x_mid - 6, x_mid + 9):
        put(img, x, arm_y, shade(wood, 10))
        put(img, x, arm_y + 1, wood)
        put(img, x, arm_y + 2, dark)


def outline_opaque(img, color, alpha_min=200):
    """Darken opaque pixels that touch transparency, for a readable silhouette."""
    w, h = img.size
    src = img.copy()
    for y in range(h):
        for x in range(w):
            r, g, b, a = src.getpixel((x, y))
            if a < alpha_min:
                continue
            for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                if not (0 <= nx < w and 0 <= ny < h) or src.getpixel((nx, ny))[3] < 60:
                    img.putpixel((x, y), (color[0], color[1], color[2], 255))
                    break


def make_prop(kind, seed):
    rng = random.Random(9000 + seed)
    img = Image.new("RGBA", (PROP, PROP), (0, 0, 0, 0))
    cx = PROP / 2.0
    if kind == "grave":
        dirt_mound(img, rng, cx + 1, 54, 17, 7, (92, 72, 50), height=4)
        headstone(img, rng, 22, 50, 15, 22)
        # a few grass tufts at the base
        for _ in range(10):
            x = rng.randint(14, 48)
            y = rng.randint(55, 61)
            put(img, x, y, (70, 112, 52))
            put(img, x, y - 1, (84, 128, 60))
    elif kind == "grave_cross":
        dirt_mound(img, rng, cx + 1, 54, 17, 7, (70, 52, 36), height=5)
        wooden_cross(img, 30, 52, 24)
        # a small stone or two on the fresh soil
        for (x, y) in ((24, 55), (38, 53)):
            put(img, x, y, (150, 150, 146))
            put(img, x + 1, y, (128, 128, 124))
    elif kind == "pack":
        soft_shadow(img, cx + 2, 57, 15, 5, 90)
        body = (138, 100, 62)
        # main bag lying on its back, seen from the iso angle
        for y in range(40, 58):
            for x in range(19, 45):
                fx = (x - 32) / 13.0
                fy = (y - 49) / 9.0
                if fx * fx + fy * fy > 1.0:
                    continue
                c = shade(body, -fx * 10 - fy * 18 + rng.randint(-5, 5))
                put(img, x, y, c)
        # top flap
        for y in range(38, 46):
            for x in range(23, 41):
                fx = (x - 32) / 9.0
                fy = (y - 42) / 4.5
                if fx * fx + fy * fy <= 1.0:
                    put(img, x, y, shade(body, 14 - fy * 8 + rng.randint(-4, 4)))
        # straps and buckle
        for y in range(43, 57):
            put(img, 27, y, (58, 50, 36))
            put(img, 37, y, (58, 50, 36))
        for x in (26, 27, 28):
            put(img, x, 50, (176, 160, 104))
        for x in (36, 37, 38):
            put(img, x, 50, (176, 160, 104))
        # side pocket
        for y in range(48, 55):
            for x in range(42, 47):
                put(img, x, y, shade(body, -18 + rng.randint(-3, 3)))
        outline_opaque(img, (52, 38, 26))
        # a red tag so it reads as someone's belongings, not a rock
        for (x, y) in ((40, 41), (41, 42), (41, 41), (42, 43)):
            put(img, x, y, (186, 54, 44))
    elif kind == "cairn":
        soft_shadow(img, cx + 2, 56, 14, 5, 80)
        stones = [(32, 54, 11, 5, (128, 126, 120)), (31, 47, 9, 4, (142, 140, 134)),
                  (33, 41, 7, 3.5, (120, 120, 116)), (32, 35.5, 5, 3, (150, 148, 142))]
        for sx, sy, rx, ry, col in stones:
            def fn(dx, dy, t, col=col):
                return (shade(col, -dx * 12 - dy * 22 + rng.randint(-5, 5)), 255)
            ground_ellipse(img, sx, sy, rx, ry, fn)
        # a strip of cloth tied around the top stone
        for x in range(28, 37):
            put(img, x, 36, (150, 60, 52))
        put(img, 37, 37, (150, 60, 52))
        put(img, 38, 38, (130, 52, 46))
    return img


def add_legacy_props(img, sprites):
    """Graves, a dropped pack and a retirement cairn, below the structure rows."""
    y = 448
    x = 0
    for seed, kind in enumerate(("grave", "grave_cross", "pack", "cairn")):
        prop = make_prop(kind, seed)
        img.paste(prop, (x, y), prop)
        sprites["prop_" + kind] = {"x": x, "y": y, "w": PROP, "h": PROP}
        x += PROP

    # A torn journal page lying flat (32x32, drawn at pickup size).
    note = Image.new("RGBA", (ITEM_SIZE, ITEM_SIZE), (0, 0, 0, 0))
    rng = random.Random(77)
    paper = (214, 204, 176)
    for py in range(10, 26):
        for px in range(4, 28):
            # a parallelogram lying on the iso ground
            if abs((px - 16) * 0.5 + (py - 18)) > 6 or abs(px - 16 - (py - 18) * 0.6) > 11:
                continue
            put(note, px, py, shade(paper, rng.randint(-8, 6)))
    for py in range(12, 25, 3):
        for px in range(6, 26):
            if note.getpixel((px, py))[3] and rng.random() < 0.7:
                put(note, px, py, (110, 104, 96))
    outline_opaque(note, (96, 86, 70))
    img.paste(note, (x, y), note)
    sprites["prop_note"] = {"x": x, "y": y, "w": ITEM_SIZE, "h": ITEM_SIZE}


# ---------------------------------------------------------------------------
# Item icons: one 32x32 icon per item, in a single style (solid shapes, a
# light edge to the top-left, a dark 1px outline). Drawn over the older item
# cells and into free cells of the items row; each icon has its own RNG.
# ---------------------------------------------------------------------------

ICON_OUTLINE = (26, 22, 20)


class Icon:
    def __init__(self, seed):
        self.px = [[None] * ITEM_SIZE for _ in range(ITEM_SIZE)]
        self.rng = random.Random(seed)

    def set(self, x, y, c):
        if 0 <= x < ITEM_SIZE and 0 <= y < ITEM_SIZE:
            self.px[y][x] = c

    def get(self, x, y):
        if 0 <= x < ITEM_SIZE and 0 <= y < ITEM_SIZE:
            return self.px[y][x]
        return None

    def rect(self, x0, y0, x1, y1, c):
        for y in range(y0, y1):
            for x in range(x0, x1):
                self.set(x, y, c)

    def frame(self, x0, y0, x1, y1, c):
        for x in range(x0, x1):
            self.set(x, y0, c)
            self.set(x, y1 - 1, c)
        for y in range(y0, y1):
            self.set(x0, y, c)
            self.set(x1 - 1, y, c)

    def ellipse(self, cx, cy, rx, ry, c):
        for y in range(ITEM_SIZE):
            for x in range(ITEM_SIZE):
                dx = (x + 0.5 - cx) / rx
                dy = (y + 0.5 - cy) / ry
                if dx * dx + dy * dy <= 1.0:
                    self.set(x, y, c)

    def line(self, x0, y0, x1, y1, c, w=1):
        steps = int(max(abs(x1 - x0), abs(y1 - y0)) * 2) + 1
        r = (w - 1) / 2.0
        for i in range(steps + 1):
            t = i / steps
            x = x0 + (x1 - x0) * t
            y = y0 + (y1 - y0) * t
            for yy in range(int(math.floor(y - r)), int(math.floor(y + r)) + 1):
                for xx in range(int(math.floor(x - r)), int(math.floor(x + r)) + 1):
                    self.set(xx, yy, c)

    def poly(self, pts, c):
        n = len(pts)
        for y in range(ITEM_SIZE):
            for x in range(ITEM_SIZE):
                px, py = x + 0.5, y + 0.5
                inside = False
                j = n - 1
                for i in range(n):
                    xi, yi = pts[i]
                    xj, yj = pts[j]
                    if (yi > py) != (yj > py) and px < (xj - xi) * (py - yi) / (yj - yi) + xi:
                        inside = not inside
                    j = i
                if inside:
                    self.set(x, y, c)

    def light(self, amount=26, dark=-30):
        """Light from the top-left: brighten edges facing up/left, darken the rest."""
        src = [row[:] for row in self.px]
        for y in range(ITEM_SIZE):
            for x in range(ITEM_SIZE):
                c = src[y][x]
                if c is None:
                    continue
                up = src[y - 1][x] if y > 0 else None
                left = src[y][x - 1] if x > 0 else None
                down = src[y + 1][x] if y < ITEM_SIZE - 1 else None
                right = src[y][x + 1] if x < ITEM_SIZE - 1 else None
                if up is None or left is None:
                    self.px[y][x] = shade(c, amount)
                elif down is None or right is None:
                    self.px[y][x] = shade(c, dark)

    def grain(self, spread=4):
        for y in range(ITEM_SIZE):
            for x in range(ITEM_SIZE):
                c = self.px[y][x]
                if c is not None:
                    self.px[y][x] = shade(c, self.rng.randint(-spread, spread))

    def outline(self, color=ICON_OUTLINE):
        src = [row[:] for row in self.px]
        for y in range(ITEM_SIZE):
            for x in range(ITEM_SIZE):
                if src[y][x] is not None:
                    continue
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    xx, yy = x + dx, y + dy
                    if 0 <= xx < ITEM_SIZE and 0 <= yy < ITEM_SIZE and src[yy][xx] is not None:
                        self.px[y][x] = color
                        break

    def paste(self, img, ox, oy):
        for y in range(ITEM_SIZE):
            for x in range(ITEM_SIZE):
                c = self.px[y][x]
                img.putpixel((ox + x, oy + y), (c[0], c[1], c[2], 255) if c is not None else (0, 0, 0, 0))


def icon_bandage_roll(ic, cloth, cross):
    ic.rect(7, 11, 20, 23, cloth)
    ic.light()
    ic.ellipse(20, 17, 4.2, 6.2, shade(cloth, 14))
    ic.ellipse(20, 17, 2.2, 3.3, shade(cloth, -22))
    ic.ellipse(20, 17, 1.0, 1.5, shade(cloth, 6))
    ic.rect(20, 21, 27, 24, shade(cloth, -10))   # the loose end
    ic.rect(10, 13, 13, 21, cross)
    ic.rect(8, 15, 15, 18, cross)


def icon_bat(ic, wood):
    ic.line(7, 25, 14, 18, wood, 2)
    ic.line(14, 18, 23, 9, wood, 4)
    ic.ellipse(23.5, 8.5, 2.6, 2.6, wood)
    ic.ellipse(6.5, 25.5, 1.8, 1.8, shade(wood, -20))
    ic.light(30, -34)
    ic.line(8, 24, 12, 20, (62, 52, 48), 2)      # grip tape


def draw_item_icon(name):
    ic = Icon(name)
    if name == "item_canned_food":
        ic.rect(10, 9, 22, 26, (176, 52, 44))
        ic.rect(10, 14, 22, 19, (222, 190, 80))
        ic.light()
        ic.ellipse(16, 9, 6.2, 2.2, (182, 184, 188))
        ic.ellipse(16, 9, 4.5, 1.2, (150, 152, 158))
        ic.rect(10, 24, 22, 26, (150, 152, 158))
    elif name == "item_apple":
        ic.ellipse(16, 18, 7.5, 7.0, (188, 40, 38))
        ic.light(24, -30)
        ic.ellipse(13, 15, 2.0, 2.4, (238, 120, 108))
        ic.set(16, 11, (120, 26, 26))
        ic.line(16, 11, 17, 6, (96, 64, 34), 1)
        ic.ellipse(20, 7.5, 3.0, 1.6, (92, 150, 62))
    elif name == "item_bread":
        ic.ellipse(16, 18, 11, 6.5, (190, 132, 64))
        ic.ellipse(16, 16.5, 9.5, 4.2, (222, 170, 96))
        for x in (10, 15, 20):
            ic.line(x, 18, x + 2, 15, (164, 106, 50), 1)
        ic.light(18, -26)
    elif name == "item_water_bottle":
        ic.rect(12, 10, 21, 27, (112, 170, 226))
        ic.rect(13, 7, 20, 10, (112, 170, 226))
        ic.rect(14, 5, 19, 7, (150, 196, 236))
        ic.rect(14, 3, 19, 5, (44, 92, 172))
        ic.rect(12, 15, 21, 20, (228, 234, 240))
        ic.rect(12, 17, 21, 18, (60, 120, 200))
        ic.light(30, -26)
        ic.line(13, 11, 13, 14, (196, 226, 248), 1)
    elif name == "item_bandage":
        icon_bandage_roll(ic, (234, 228, 216), (200, 50, 44))
    elif name == "item_bandage_clean":
        icon_bandage_roll(ic, (214, 234, 244), (52, 132, 204))
        ic.line(24, 5, 24, 10, (250, 250, 255), 1)
        ic.line(21.5, 7.5, 26.5, 7.5, (250, 250, 255), 1)
    elif name == "item_knife":
        ic.line(11, 21, 24, 8, (192, 196, 202), 2)
        ic.line(12, 22, 25, 9, (150, 154, 160), 1)
        ic.light(30, -20)
        ic.line(9, 19, 13, 23, (70, 70, 74), 1)
        ic.line(6, 26, 10, 22, (102, 62, 34), 3)
    elif name == "item_bat":
        icon_bat(ic, (198, 150, 92))
    elif name == "item_bat_spiked":
        icon_bat(ic, (176, 128, 78))
        for t, side in ((0.25, 1), (0.45, -1), (0.65, 1), (0.85, -1)):
            x = 14 + (23 - 14) * t
            y = 18 + (9 - 18) * t
            ic.line(x + side * 2, y + side * 2, x + side * 4, y + side * 4, (206, 208, 214), 1)
    elif name == "item_pipe":
        ic.line(7, 25, 24, 8, (142, 144, 150), 4)
        ic.light(30, -30)
        ic.ellipse(15.5, 16.5, 3.4, 3.4, (112, 114, 120))
        ic.ellipse(24, 8, 1.4, 1.4, (52, 52, 56))
    elif name == "item_axe":
        ic.line(8, 26, 20, 9, (150, 105, 60), 2)
        ic.poly([(17, 7), (25, 4), (28, 12), (22, 13), (19, 11)], (176, 180, 186))
        ic.light(34, -26)
        ic.line(25, 4, 28, 12, (230, 234, 238), 1)
    elif name == "item_wood":
        ic.poly([(4, 18), (22, 7), (26, 11), (8, 22)], (178, 132, 78))
        ic.poly([(6, 24), (24, 13), (27, 17), (9, 27)], (160, 116, 66))
        ic.light(22, -26)
        ic.line(8, 17, 21, 9, (140, 98, 56), 1)
        ic.line(10, 23, 23, 15, (128, 88, 48), 1)
        ic.set(22, 10, (70, 70, 76))
        ic.set(24, 16, (70, 70, 76))
    elif name == "item_scrap":
        ic.poly([(5, 15), (17, 8), (27, 13), (23, 24), (9, 26)], (128, 132, 138))
        ic.light(28, -28)
        ic.line(17, 9, 15, 25, (98, 102, 108), 1)
        for _ in range(6):
            x, y = ic.rng.randint(8, 23), ic.rng.randint(12, 23)
            if ic.get(x, y) is not None:
                ic.set(x, y, (156, 94, 60))
        ic.ellipse(21.5, 18.5, 1.5, 1.5, (190, 192, 198))
    elif name == "item_rope":
        ic.ellipse(16, 17, 10, 8, (192, 160, 104))
        ic.ellipse(16, 17, 5, 3.6, (0, 0, 0))
        for y in range(ITEM_SIZE):
            for x in range(ITEM_SIZE):
                if ic.get(x, y) == (0, 0, 0):
                    ic.set(x, y, None)
        ic.light(22, -28)
        for a in range(0, 360, 30):
            x = 16 + 7.5 * math.cos(math.radians(a))
            y = 17 + 5.8 * math.sin(math.radians(a))
            ic.set(int(x), int(y), (150, 120, 72))
        ic.line(24, 22, 27, 27, (192, 160, 104), 2)
    elif name == "item_fuel":
        ic.rect(8, 10, 23, 27, (190, 46, 40))
        ic.light(28, -30)
        ic.line(9, 11, 22, 26, (166, 38, 34), 1)
        ic.line(22, 11, 9, 26, (166, 38, 34), 1)
        ic.frame(10, 6, 17, 10, (150, 40, 36))
        ic.line(20, 10, 25, 4, (150, 152, 158), 2)
    elif name == "item_glass":
        for tri in ([(7, 23), (13, 7), (17, 21)], [(16, 25), (24, 11), (27, 26)], [(5, 27), (12, 24), (9, 28)]):
            ic.poly(tri, (188, 222, 238))
        ic.light(40, -24)
    elif name == "item_ammo":
        ic.rect(8, 16, 25, 27, (92, 102, 70))
        ic.rect(8, 19, 25, 22, (206, 182, 92))
        ic.light(24, -26)
        for x in (10, 15, 20):
            ic.rect(x, 10, x + 3, 16, (210, 168, 72))
            ic.ellipse(x + 1.5, 10, 1.6, 2.2, (192, 112, 62))
    elif name == "item_antibiotics":
        ic.rect(11, 11, 22, 27, (214, 128, 40))
        ic.rect(11, 15, 22, 22, (240, 238, 232))
        ic.rect(13, 18, 20, 19, (210, 52, 48))
        ic.rect(10, 6, 23, 11, (236, 236, 236))
        ic.light(26, -30)
        for x in range(11, 23, 2):
            ic.set(x, 9, (200, 200, 204))
    elif name == "item_painkillers":
        ic.rect(5, 9, 27, 24, (198, 202, 210))
        ic.light(26, -30)
        for i, (x, y) in enumerate(((10, 13), (16, 13), (22, 13), (10, 19), (16, 19), (22, 19))):
            if i == 4:
                ic.ellipse(x, y, 2.3, 1.8, (150, 154, 162))
            else:
                ic.ellipse(x, y, 2.3, 1.8, (246, 246, 246))
                ic.set(x, y - 1, (255, 255, 255))
        ic.rect(5, 24, 27, 26, (62, 112, 190))
    elif name == "item_hammer":
        ic.line(17, 11, 14, 27, (160, 114, 64), 3)
        ic.rect(11, 6, 24, 11, (120, 124, 130))
        ic.rect(22, 5, 26, 12, (150, 154, 160))
        ic.poly([(12, 6), (8, 3), (7, 6), (11, 10)], (120, 124, 130))
        ic.light(30, -30)
    elif name == "item_flashlight":
        ic.line(7, 25, 18, 14, (62, 66, 74), 5)
        ic.ellipse(20.5, 11.5, 4.8, 4.8, (92, 96, 106))
        ic.light(28, -24)
        ic.ellipse(22, 10, 2.6, 2.6, (250, 238, 180))
        ic.rect(11, 19, 13, 21, (200, 60, 50))
    elif name == "item_matches":
        for x0, x1, y1 in ((12, 14, 4), (16, 19, 5), (20, 23, 7)):
            ic.line(x0, 12, x1, y1 + 1, (228, 204, 150), 1)
            ic.ellipse(x1 + 0.5, y1 + 0.5, 1.5, 1.6, (182, 42, 40))
        ic.rect(8, 11, 24, 14, (196, 162, 110))
        ic.rect(6, 14, 26, 26, (198, 58, 48))
        ic.rect(9, 17, 23, 23, (222, 190, 82))
        ic.light(24, -30)
    elif name == "item_jacket":
        ic.poly([(10, 8), (22, 8), (27, 14), (26, 25), (6, 25), (5, 14)], (98, 108, 70))
        ic.poly([(5, 14), (10, 9), (10, 26), (3, 25)], (88, 98, 62))
        ic.poly([(27, 14), (22, 9), (22, 26), (29, 25)], (88, 98, 62))
        ic.light(22, -26)
        ic.line(13, 8, 16, 13, (70, 78, 48), 1)
        ic.line(19, 8, 16, 13, (70, 78, 48), 1)
        ic.line(16, 13, 16, 25, (176, 176, 156), 1)
    elif name == "item_rat_meat":
        ic.ellipse(14, 18, 9, 6.5, (186, 84, 86))
        ic.light(24, -30)
        for _ in range(4):
            x, y = ic.rng.randint(8, 19), ic.rng.randint(15, 21)
            ic.line(x, y, x + 3, y - 1, (230, 156, 148), 1)
        ic.line(21, 14, 26, 8, (236, 226, 202), 2)
        ic.ellipse(27, 7, 1.8, 1.8, (236, 226, 202))
        ic.ellipse(25.5, 5.5, 1.6, 1.6, (236, 226, 202))
    elif name == "item_medkit":
        ic.rect(6, 11, 27, 27, (236, 234, 230))
        ic.light(14, -30)
        ic.frame(12, 7, 21, 12, (150, 150, 154))
        ic.rect(14, 13, 19, 25, (200, 44, 40))
        ic.rect(10, 16, 23, 21, (200, 44, 40))
    elif name == "item_notebook":
        ic.rect(21, 7, 25, 27, (232, 222, 196))
        for y in range(9, 26, 2):
            ic.set(23, y, (190, 180, 160))
        ic.rect(8, 6, 22, 28, (122, 78, 50))
        ic.light(24, -26)
        ic.rect(10, 10, 19, 15, (226, 214, 184))
        ic.rect(18, 6, 20, 28, (44, 34, 28))
    elif name == "item_rag":
        ic.poly([(5, 15), (13, 9), (24, 10), (28, 18), (21, 26), (8, 25)], (192, 172, 112))
        ic.light(20, -26)
        for cx, cy, rx, ry in ((12, 18, 3.5, 2.5), (20, 15, 2.5, 2), (19, 22, 3, 2)):
            ic.ellipse(cx, cy, rx, ry, (148, 118, 66))
        ic.line(10, 13, 16, 20, (170, 150, 96), 1)
    elif name == "item_molotov":
        ic.rect(12, 14, 21, 28, (70, 128, 80))
        ic.rect(13, 11, 20, 14, (70, 128, 80))
        ic.rect(14, 7, 19, 11, (70, 128, 80))
        ic.light(34, -28)
        ic.line(13, 15, 13, 26, (130, 186, 136), 1)
        ic.poly([(14, 8), (19, 8), (21, 3), (17, 4)], (204, 184, 124))
        ic.ellipse(21, 3.5, 2.6, 2.4, (240, 140, 40))
        ic.ellipse(21.5, 3, 1.3, 1.3, (252, 222, 92))
    elif name == "item_spear":
        ic.line(5, 27, 21, 11, (152, 110, 64), 2)
        ic.poly([(19, 11), (23, 5), (28, 3), (26, 8), (21, 13)], (186, 190, 196))
        ic.light(34, -26)
        for t in (0.0, 1.5):
            ic.line(17 + t, 12 + t, 20 + t, 15 + t, (192, 160, 104), 1)
    else:
        ic.rect(9, 9, 23, 23, (150, 150, 150))
    ic.grain(3)
    ic.outline()
    return ic


# Older item cells redrawn in the new style, and new items in free cells of
# the items row (y=160, x from 512).
ITEM_ICONS_REDRAWN = [
    "item_canned_food", "item_apple", "item_bread", "item_water_bottle",
    "item_bandage", "item_knife", "item_bat", "item_pipe", "item_axe",
    "item_ammo", "item_medkit", "item_wood", "item_scrap", "item_rope",
    "item_fuel", "item_glass",
]
ITEM_ICONS_NEW = [
    "item_antibiotics", "item_painkillers", "item_hammer", "item_flashlight",
    "item_matches", "item_jacket", "item_rat_meat", "item_notebook",
    "item_bandage_clean", "item_rag", "item_molotov", "item_bat_spiked",
    "item_spear",
]


def add_item_icons(img, sprites):
    for name in ITEM_ICONS_REDRAWN:
        cell = sprites[name]
        draw_item_icon(name).paste(img, cell["x"], cell["y"])
    for i, name in enumerate(ITEM_ICONS_NEW):
        x = 512 + i * ITEM_SIZE
        draw_item_icon(name).paste(img, x, 160)
        sprites[name] = {"x": x, "y": 160, "w": ITEM_SIZE, "h": ITEM_SIZE}


# ---------------------------------------------------------------------------
# Furniture: isometric pieces one tile across, built from shaded boxes.
# Canvas 64x80: the tile's ground diamond fills the bottom 32 rows, leaving
# FURN_UP pixels above it. Oriented pieces come in two views:
#   _r  back against a west wall, front on the right-hand face
#   _l  back against a north wall, front on the left-hand face
# Must match FurnitureView::kSpriteUp in src/rendering/FurnitureView.h.
# ---------------------------------------------------------------------------

FURN_W = 64
FURN_UP = 48
FURN_H = 32 + FURN_UP
FURN_OUTLINE = (34, 28, 24)


class Furn:
    def __init__(self, seed, orient=None):
        self.img = Image.new("RGBA", (FURN_W, FURN_H), (0, 0, 0, 0))
        self.draw = ImageDraw.Draw(self.img)
        self.rng = random.Random(seed)
        self.orient = orient  # None, "r" or "l"

    @staticmethod
    def P(u, v, z):
        return (round(32 + (u - v) * 32), round(FURN_UP + (u + v) * 16 - z))

    def M(self, a, b, z):
        """Canonical (a: back->front, b: lateral) to screen."""
        if self.orient == "l":
            return self.P(b, a, z)
        return self.P(a, b, z)

    def poly(self, pts, col):
        self.draw.polygon(pts, fill=(col[0], col[1], col[2], 255))

    def box(self, u0, v0, u1, v1, z0, z1, col, top=None, south=None, east=None):
        P = self.P
        self.poly([P(u0, v1, z0), P(u1, v1, z0), P(u1, v1, z1), P(u0, v1, z1)], south or col)
        self.poly([P(u1, v0, z0), P(u1, v1, z0), P(u1, v1, z1), P(u1, v0, z1)], east or shade(col, -28))
        self.poly([P(u0, v0, z1), P(u1, v0, z1), P(u1, v1, z1), P(u0, v1, z1)], top or shade(col, 20))

    def cbox(self, a0, b0, a1, b1, z0, z1, col, top=None):
        """A box in canonical coordinates."""
        if self.orient == "l":
            self.box(b0, a0, b1, a1, z0, z1, col, top)
        else:
            self.box(a0, b0, a1, b1, z0, z1, col, top)

    def fline(self, a, b0, b1, z0, z1, col):
        """A line on a front plane a = const, from (b0, z0) to (b1, z1)."""
        self.draw.line([self.M(a, b0, z0), self.M(a, b1, z1)], fill=(col[0], col[1], col[2], 255))

    def ellipse_at(self, pt, rx, ry, col):
        x, y = pt
        self.draw.ellipse([x - rx, y - ry, x + rx, y + ry], fill=(col[0], col[1], col[2], 255))

    def finish(self):
        outline_opaque(self.img, FURN_OUTLINE)
        return self.img


def furn_bed(f):
    wood = (118, 84, 56)
    f.cbox(0.00, 0.10, 0.09, 0.90, 0, 24, shade(wood, -12))      # headboard
    f.cbox(0.06, 0.12, 0.94, 0.88, 0, 7, wood)
    f.cbox(0.08, 0.14, 0.92, 0.86, 7, 12, (226, 224, 214))
    f.cbox(0.12, 0.22, 0.32, 0.78, 12, 15, (242, 240, 234))     # pillow
    f.cbox(0.38, 0.13, 0.93, 0.87, 9, 14, (84, 106, 150))       # blanket
    f.fline(0.93, 0.13, 0.87, 11, 11, (70, 90, 132))


def furn_couch(f):
    cloth = (128, 70, 58)
    cushion = (176, 104, 84)
    f.cbox(0.04, 0.08, 0.18, 0.92, 0, 28, shade(cloth, -16))    # backrest
    f.cbox(0.06, 0.08, 0.84, 0.20, 0, 17, cloth)                 # far arm
    f.cbox(0.16, 0.20, 0.84, 0.80, 0, 9, shade(cloth, -6))       # base
    f.cbox(0.18, 0.21, 0.82, 0.50, 9, 14, cushion)                # cushions
    f.cbox(0.18, 0.50, 0.82, 0.79, 9, 14, cushion)
    f.fline(0.82, 0.50, 0.50, 9, 14, shade(cushion, -40))
    f.cbox(0.06, 0.80, 0.84, 0.92, 0, 17, cloth)                 # near arm


def furn_table(f):
    wood = (150, 108, 70)
    for u, v in ((0.16, 0.16), (0.78, 0.16), (0.16, 0.78), (0.78, 0.78)):
        f.box(u, v, u + 0.06, v + 0.06, 0, 14, shade(wood, -24))
    f.box(0.10, 0.10, 0.90, 0.90, 14, 17, wood)
    f.ellipse_at(f.P(0.45, 0.50, 18), 3, 1.5, (226, 222, 210))  # a plate


def furn_bookshelf(f):
    wood = (110, 78, 52)
    f.cbox(0.00, 0.08, 0.36, 0.92, 0, 44, wood)
    books = [(150, 48, 44), (58, 92, 140), (186, 160, 92), (70, 120, 80), (128, 96, 150), (200, 196, 180)]
    for z in (2, 13, 24, 35):
        f.fline(0.36, 0.10, 0.90, z, z, shade(wood, -34))
        b = 0.14
        while b < 0.86:
            h = f.rng.randint(5, 8)
            col = books[f.rng.randrange(len(books))]
            f.fline(0.36, b, b, z + 1, z + h, col)
            f.fline(0.36, b + 0.03, b + 0.03, z + 1, z + h, shade(col, -16))
            b += f.rng.choice((0.06, 0.08, 0.10))


def furn_wardrobe(f):
    wood = (128, 92, 62)
    f.cbox(0.00, 0.10, 0.42, 0.90, 0, 46, wood)
    f.fline(0.42, 0.50, 0.50, 2, 44, shade(wood, -34))
    for b in (0.44, 0.56):
        f.fline(0.42, b, b, 22, 25, (210, 186, 120))


def furn_dresser(f):
    wood = (140, 100, 66)
    f.cbox(0.00, 0.10, 0.45, 0.90, 0, 20, wood)
    for z in (7, 13):
        f.fline(0.45, 0.12, 0.88, z, z, shade(wood, -34))
    for z in (4, 10, 16):
        f.fline(0.45, 0.48, 0.52, z, z, (210, 186, 120))
    f.cbox(0.10, 0.30, 0.22, 0.42, 20, 30, (220, 210, 180))      # a lamp
    f.cbox(0.08, 0.26, 0.24, 0.46, 30, 34, (238, 218, 150))


def furn_fridge(f):
    white = (222, 224, 226)
    f.cbox(0.00, 0.14, 0.60, 0.86, 0, 42, white)
    f.fline(0.60, 0.16, 0.84, 28, 28, (150, 152, 156))
    f.fline(0.60, 0.78, 0.78, 31, 38, (110, 112, 118))
    f.fline(0.60, 0.78, 0.78, 12, 24, (110, 112, 118))


def furn_stove(f):
    steel = (204, 206, 210)
    f.cbox(0.00, 0.12, 0.08, 0.88, 18, 25, (170, 172, 178))
    f.cbox(0.00, 0.12, 0.60, 0.88, 0, 18, steel)
    for a, b in ((0.22, 0.32), (0.22, 0.68), (0.46, 0.32), (0.46, 0.68)):
        f.ellipse_at(f.M(a, b, 18), 3, 1.5, (52, 52, 56))
    for z in (5, 8, 11):
        f.fline(0.60, 0.28, 0.72, z, z, (60, 62, 68))
    f.fline(0.60, 0.24, 0.76, 14, 14, (120, 122, 128))


def furn_counter(f):
    wood = (196, 176, 146)
    f.cbox(0.00, 0.00, 0.58, 1.00, 0, 17, wood)
    f.cbox(0.00, 0.00, 0.62, 1.00, 17, 20, (96, 96, 102))
    f.fline(0.58, 0.50, 0.50, 1, 16, shade(wood, -38))
    for b in (0.44, 0.56):
        f.fline(0.58, b, b, 11, 13, (90, 86, 80))
    f.cbox(0.10, 0.62, 0.30, 0.82, 20, 26, (210, 120, 60))     # a pan or a kettle


def furn_sink(f):
    white = (232, 232, 228)
    f.cbox(0.05, 0.42, 0.20, 0.58, 0, 12, white)               # pedestal
    f.cbox(0.00, 0.20, 0.50, 0.80, 12, 18, white)
    f.ellipse_at(f.M(0.26, 0.50, 18), 5, 2, (170, 186, 196))
    f.cbox(0.00, 0.28, 0.10, 0.72, 24, 40, (226, 228, 230))    # mirror cabinet
    f.fline(0.10, 0.50, 0.50, 28, 36, (200, 50, 44))
    f.fline(0.10, 0.44, 0.56, 32, 32, (200, 50, 44))


def furn_toilet(f):
    white = (234, 234, 230)
    f.cbox(0.00, 0.30, 0.18, 0.70, 8, 22, white)
    f.cbox(0.12, 0.32, 0.62, 0.68, 0, 11, white)
    f.ellipse_at(f.M(0.40, 0.50, 11), 4, 2, (190, 196, 200))


def furn_tvstand(f):
    wood = (88, 64, 46)
    f.cbox(0.00, 0.08, 0.40, 0.92, 0, 12, wood)
    f.cbox(0.06, 0.16, 0.14, 0.84, 12, 30, (34, 34, 38))
    for z in range(15, 28, 2):
        f.fline(0.14, 0.20, 0.80, z, z, (46, 58, 78))


def furn_plant(f):
    f.box(0.32, 0.32, 0.68, 0.68, 0, 9, (170, 96, 60))
    cx, cy = f.P(0.5, 0.5, 18)
    for dx, dy, r, col in ((0, 0, 8, (64, 124, 58)), (-4, -3, 5, (80, 146, 66)), (4, -5, 4, (92, 160, 74)), (2, 3, 4, (56, 110, 52))):
        f.ellipse_at((cx + dx, cy + dy), r, r * 0.85, col)


def furn_storeshelf(f):
    metal = (150, 156, 164)
    f.cbox(0.00, 0.00, 0.48, 1.00, 0, 40, metal)
    goods = [(200, 60, 50), (230, 190, 70), (70, 120, 190), (90, 160, 80), (230, 230, 220), (180, 110, 60)]
    for z in (2, 13, 24, 35):
        f.fline(0.48, 0.02, 0.98, z, z, shade(metal, -40))
        b = 0.06
        while b < 0.92:
            col = goods[f.rng.randrange(len(goods))]
            h = f.rng.randint(4, 7)
            for k in range(3):
                f.fline(0.48, b + k * 0.02, b + k * 0.02, z + 1, z + h, shade(col, -k * 10))
            b += 0.10


def furn_checkout(f):
    f.cbox(0.00, 0.00, 0.55, 1.00, 0, 20, (118, 122, 132))
    f.cbox(0.00, 0.00, 0.58, 1.00, 20, 22, (70, 72, 78))
    f.cbox(0.10, 0.30, 0.40, 0.70, 22, 30, (48, 50, 56))
    f.fline(0.40, 0.36, 0.64, 27, 27, (120, 200, 140))


def furn_crates(f):
    wood = (160, 120, 70)
    f.box(0.08, 0.10, 0.56, 0.58, 0, 18, wood)
    f.box(0.46, 0.44, 0.92, 0.90, 0, 14, shade(wood, -10))
    f.box(0.14, 0.16, 0.48, 0.50, 18, 30, shade(wood, 8))
    for (u0, v0, u1, v1, z) in ((0.46, 0.44, 0.92, 0.90, 7), (0.08, 0.10, 0.56, 0.58, 9)):
        f.draw.line([f.P(u0, v1, z), f.P(u1, v1, z)], fill=shade(wood, -40) + (255,))
        f.draw.line([f.P(u1, v0, z), f.P(u1, v1, z)], fill=shade(wood, -56) + (255,))


def furn_metalrack(f):
    metal = (132, 138, 146)
    card = (176, 140, 92)
    for b in (0.00, 0.95):
        f.cbox(0.00, b, 0.05, b + 0.05, 0, 46, shade(metal, -20))
    for z in (0, 15, 30, 44):
        f.cbox(0.00, 0.00, 0.50, 1.00, z, z + 2, metal)
        if z < 44:
            f.cbox(0.05, 0.08, 0.42, 0.46, z + 2, z + 2 + f.rng.randint(8, 11), card)
            if f.rng.random() < 0.8:
                f.cbox(0.05, 0.54, 0.40, 0.90, z + 2, z + 2 + f.rng.randint(6, 10), shade(card, -14))
    for b in (0.00, 0.95):
        f.cbox(0.45, b, 0.50, b + 0.05, 0, 46, metal)


def furn_workbench(f):
    wood = (150, 110, 70)
    f.cbox(0.00, 0.00, 0.06, 1.00, 19, 42, (150, 132, 104))    # pegboard
    for b, z0, z1 in ((0.2, 26, 36), (0.35, 28, 38), (0.6, 24, 34), (0.78, 27, 37)):
        f.fline(0.06, b, b, z0, z1, (70, 72, 78))
    for b in (0.04, 0.88):
        f.cbox(0.06, b, 0.14, b + 0.08, 0, 15, shade(wood, -24))
        f.cbox(0.50, b, 0.58, b + 0.08, 0, 15, shade(wood, -24))
    f.cbox(0.00, 0.00, 0.62, 1.00, 15, 19, wood)
    f.cbox(0.32, 0.68, 0.52, 0.90, 19, 24, (120, 124, 132))    # vise


def furn_barrels(f):
    for u, v, col in ((0.32, 0.34, (60, 96, 140)), (0.66, 0.66, (150, 60, 46))):
        gx, gy = f.P(u, v, 0)
        tx, ty = f.P(u, v, 26)
        rx, ry = 10, 5
        f.draw.rectangle([gx - rx, ty, gx + rx, gy], fill=col + (255,))
        f.ellipse_at((gx, gy), rx, ry, col)
        for z in (8, 18):
            y = gy - z
            f.draw.line([(gx - rx, y), (gx + rx, y)], fill=shade(col, -30) + (255,))
        f.draw.rectangle([gx + 4, ty, gx + rx, gy], fill=shade(col, -24) + (255,))
        f.ellipse_at((tx, ty), rx, ry, shade(col, 22))
        f.ellipse_at((tx + 3, ty), 2, 1, shade(col, -30))


FURNITURE = [
    # (name, painter, oriented)
    ("bed", furn_bed, True),
    ("couch", furn_couch, True),
    ("table", furn_table, False),
    ("bookshelf", furn_bookshelf, True),
    ("wardrobe", furn_wardrobe, True),
    ("dresser", furn_dresser, True),
    ("fridge", furn_fridge, True),
    ("stove", furn_stove, True),
    ("counter", furn_counter, True),
    ("sink", furn_sink, True),
    ("toilet", furn_toilet, True),
    ("tvstand", furn_tvstand, True),
    ("plant", furn_plant, False),
    ("storeshelf", furn_storeshelf, True),
    ("checkout", furn_checkout, True),
    ("crates", furn_crates, False),
    ("metalrack", furn_metalrack, True),
    ("workbench", furn_workbench, True),
    ("barrels", furn_barrels, False),
]


def add_furniture_sprites(img, sprites):
    """Furniture pieces from y=608 down, 16 to a row."""
    x, y = 0, 608
    for seed, (name, painter, oriented) in enumerate(FURNITURE):
        for orient in (("r", "l") if oriented else (None,)):
            f = Furn(1000 + seed, orient)
            painter(f)
            piece = f.finish()
            img.paste(piece, (x, y), piece)
            key = "furn_" + name + ("_" + orient if orient else "")
            sprites[key] = {"x": x, "y": y, "w": FURN_W, "h": FURN_H}
            x += FURN_W
            if x + FURN_W > ATLAS_W:
                x = 0
                y += FURN_H


def main():
    script_dir = Path(__file__).resolve().parent
    project_root = script_dir.parent
    sprites_dir = project_root / "assets" / "sprites"
    sprites_dir.mkdir(parents=True, exist_ok=True)

    # Generate spritesheet atlas
    print("Generating isometric sprite atlas...")
    img, sprites = generate_atlas()

    png_path = sprites_dir / "spritesheet.png"
    json_path = sprites_dir / "spritesheet.json"

    img.save(str(png_path))
    print(f"  Saved {png_path} ({img.size[0]}x{img.size[1]})")

    atlas_meta = {
        "imageWidth": ATLAS_W,
        "imageHeight": ATLAS_H,
        "sprites": sprites,
    }

    with open(str(json_path), "w") as f:
        json.dump(atlas_meta, f, indent=2)
    print(f"  Saved {json_path} ({len(sprites)} sprites)")

    # Generate tileset for map rendering
    print("Generating isometric tileset...")
    tileset_img = generate_tileset()
    tileset_path = sprites_dir / "placeholder.png"
    tileset_img.save(str(tileset_path))
    print(f"  Saved {tileset_path} ({tileset_img.size[0]}x{tileset_img.size[1]})")

    print(f"Done! {len(sprites)} sprites + tileset generated.")


if __name__ == "__main__":
    main()
