#!/usr/bin/env python3
"""
Outdoor scenery for Dead Pixel Survival: trees, bushes, rocks, street
furniture and the small flat things that lie on the ground.

Standing sprites are drawn with their ground contact point FOOT pixels above
the bottom edge, centred left to right; the game puts that point on the
tile. Flat sprites are centred on their point. Light comes from the upper
left, as it does for the characters and the walls.

Preview: python tools/scenery.py out.png
"""

import math
import random

from PIL import Image

FOOT = 6  # must match SceneryView::kFoot

OUTLINE = (30, 26, 28)

# Four tones each: highlight, base, shade, deep.
LEAF_GREEN = ((122, 142, 84), (92, 116, 68), (66, 90, 56), (46, 66, 48))
LEAF_DARK = ((98, 122, 82), (72, 98, 66), (52, 76, 56), (38, 56, 46))
LEAF_RUST = ((190, 138, 72), (158, 102, 58), (122, 74, 50), (88, 54, 44))
LEAF_PINE = ((84, 112, 92), (60, 88, 74), (44, 68, 60), (32, 50, 48))
BARK = ((126, 100, 78), (98, 76, 60), (72, 56, 46), (52, 40, 36))
BARK_GREY = ((132, 124, 114), (104, 96, 88), (78, 72, 68), (56, 52, 50))
STONE = ((160, 160, 156), (128, 128, 126), (98, 98, 100), (72, 72, 78))
METAL = ((150, 156, 160), (112, 118, 124), (80, 86, 92), (56, 60, 66))
DARK_METAL = ((96, 102, 106), (70, 76, 80), (50, 54, 60), (36, 38, 44))


def _hash(x, y, s=0):
    h = (x * 73856093) ^ (y * 19349663) ^ (s * 83492791)
    h &= 0xFFFFFFFF
    h ^= h >> 13
    h = (h * 0x5BD1E995) & 0xFFFFFFFF
    h ^= h >> 15
    return (h & 0xFFFF) / 65535.0


class Cell:
    """An RGBA cell with a ground layer underneath (shadows are not outlined)."""

    def __init__(self, w, h):
        self.w, self.h = w, h
        self.img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        self.under = Image.new("RGBA", (w, h), (0, 0, 0, 0))

    def put(self, x, y, c, a=255):
        x, y = int(x), int(y)
        if 0 <= x < self.w and 0 <= y < self.h:
            self.img.putpixel((x, y), (c[0], c[1], c[2], a))

    def get(self, x, y):
        if 0 <= x < self.w and 0 <= y < self.h:
            return self.img.getpixel((x, y))
        return (0, 0, 0, 0)

    def rect(self, x0, y0, x1, y1, c):
        for y in range(int(y0), int(y1) + 1):
            for x in range(int(x0), int(x1) + 1):
                self.put(x, y, c)

    def shadow(self, cx, cy, rx, ry, alpha=70):
        for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
            for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
                if 0 <= x < self.w and 0 <= y < self.h:
                    if ((x + 0.5 - cx) / rx) ** 2 + ((y + 0.5 - cy) / ry) ** 2 <= 1.0:
                        self.under.putpixel((x, y), (0, 0, 0, alpha))

    def blobs(self, spheres, ramp, seed=0, leafy=0.0, ragged=0.0):
        """Shaded spheres merged by height. spheres: (cx, cy, r[, lift]).
        leafy breaks the shading into clumps; ragged eats into the outline."""
        x0 = int(min(s[0] - s[2] for s in spheres)) - 1
        x1 = int(max(s[0] + s[2] for s in spheres)) + 1
        y0 = int(min(s[1] - s[2] for s in spheres)) - 1
        y1 = int(max(s[1] + s[2] for s in spheres)) + 1
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                best = None
                for s in spheres:
                    dx, dy = x + 0.5 - s[0], y + 0.5 - s[1]
                    d2 = dx * dx + dy * dy
                    r = s[2]
                    if d2 > r * r:
                        continue
                    hgt = math.sqrt(r * r - d2) + (s[3] if len(s) > 3 else 0.0)
                    if best is None or hgt > best[0]:
                        best = (hgt, dx / r, dy / r, math.sqrt(r * r - d2) / r, d2 / (r * r))
                if best is None:
                    continue
                _, nx, ny, nz, edge = best
                if ragged and edge > 0.72 and _hash(x, y, seed + 7) < ragged:
                    continue
                lit = -0.52 * nx - 0.66 * ny + 0.38 * nz
                if leafy:
                    lit += (_hash(x // 2, y // 2, seed) - 0.5) * leafy
                    lit += (_hash(x, y, seed + 3) - 0.5) * leafy * 0.6
                tone = 0 if lit > 0.56 else (1 if lit > 0.16 else (2 if lit > -0.22 else 3))
                self.put(x, y, ramp[tone])

    def line(self, x0, y0, x1, y1, c, width=1):
        n = int(max(abs(x1 - x0), abs(y1 - y0), 1))
        for i in range(n + 1):
            t = i / n
            x, y = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
            for dy in range(width):
                for dx in range(width):
                    self.put(round(x) + dx, round(y) + dy, c)

    def finish(self, outline=True):
        out = self.under.copy()
        if outline:
            edge = Image.new("RGBA", (self.w, self.h), (0, 0, 0, 0))
            for y in range(self.h):
                for x in range(self.w):
                    if self.get(x, y)[3]:
                        continue
                    for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                        c = self.get(nx, ny)
                        if c[3]:
                            edge.putpixel((x, y), (int(OUTLINE[0] * 0.8 + c[0] * 0.1),
                                                   int(OUTLINE[1] * 0.8 + c[1] * 0.1),
                                                   int(OUTLINE[2] * 0.8 + c[2] * 0.1), 255))
                            break
            out.alpha_composite(edge)
        out.alpha_composite(self.img)
        return out


def _trunk(c, cx, base_y, top_y, w_base, w_top, bark, seed):
    for y in range(int(top_y), int(base_y) + 1):
        t = max(0.0, (y - top_y) / max(1.0, base_y - top_y))
        w = w_top + (w_base - w_top) * t ** 2.2
        x0 = cx - w / 2.0
        for i in range(int(round(w))):
            x = int(round(x0)) + i
            f = i / max(1.0, w - 1.0)
            tone = 0 if f < 0.22 else (1 if f < 0.6 else 2)
            if _hash(x, y // 3, seed) < 0.16:
                tone = min(3, tone + 1)  # bark furrows
            c.put(x, y, bark[tone])
    # Roots flare at the foot.
    for side in (-1, 1):
        for i in range(3):
            c.put(cx + side * (w_base / 2.0 + i) - (0 if side > 0 else 1), base_y - (2 - i) // 2, bark[2 if side > 0 else 1])


# ---------------------------------------------------------------------------
# Standing things
# ---------------------------------------------------------------------------

def tree_broadleaf(seed, ramp, size=1.0, bark=BARK):
    rng = random.Random(seed)
    w, h = 96, 118
    c = Cell(w, h)
    cx, base = w // 2, h - FOOT - 1
    c.shadow(cx + 4, base + 1, 26 * size, 6.5 * size, 64)
    crown_y = base - 62 * size
    _trunk(c, cx, base, crown_y + 8, 9 * size, 5 * size, bark, seed)
    # A couple of boughs reaching into the crown.
    c.line(cx - 1, crown_y + 26 * size, cx - 12 * size, crown_y + 10 * size, bark[2], 2)
    c.line(cx + 1, crown_y + 24 * size, cx + 13 * size, crown_y + 8 * size, bark[3], 2)
    spheres = [(cx, crown_y, 21 * size, 6)]
    for i in range(7):
        ang = i / 7.0 * math.tau + rng.uniform(-0.3, 0.3)
        dist = rng.uniform(13, 21) * size
        spheres.append((cx + math.cos(ang) * dist * 1.25, crown_y + math.sin(ang) * dist * 0.72 + 3,
                        rng.uniform(11, 16) * size, rng.uniform(0, 3)))
    c.blobs(spheres, ramp, seed, leafy=0.55, ragged=0.42)
    return c.finish()


def tree_pine(seed):
    rng = random.Random(seed)
    w, h = 72, 124
    c = Cell(w, h)
    cx, base = w // 2, h - FOOT - 1
    c.shadow(cx + 3, base + 1, 18, 5, 64)
    _trunk(c, cx, base, base - 22, 6, 4, BARK, seed)
    tiers = 6
    top = 6
    for i in range(tiers):
        t = i / (tiers - 1.0)
        apex = top + t * 62
        bottom = apex + 26 + t * 10
        half = 7 + t * 20 + rng.uniform(-1.5, 1.5)
        for y in range(int(apex), int(bottom) + 1):
            f = max(0.0, (y - apex) / (bottom - apex))
            hw = half * f ** 0.85
            for x in range(int(cx - hw), int(cx + hw) + 1):
                # Jagged skirt along the bottom of each tier.
                if f > 0.82 and _hash(x // 2, i, seed) < (f - 0.82) * 4.5:
                    continue
                side = (x - cx) / max(1.0, half)
                lit = -0.7 * side + 0.55 - f * 0.95 + (_hash(x, y, seed) - 0.5) * 0.5
                tone = 0 if lit > 0.5 else (1 if lit > 0.05 else (2 if lit > -0.45 else 3))
                c.put(x, y, LEAF_PINE[tone])
    return c.finish()


def tree_dead(seed):
    rng = random.Random(seed)
    w, h = 80, 108
    c = Cell(w, h)
    cx, base = w // 2, h - FOOT - 1
    c.shadow(cx + 3, base + 1, 15, 4, 56)
    _trunk(c, cx, base, base - 40, 8, 5, BARK_GREY, seed)

    def branch(x, y, ang, length, thick, depth):
        x2 = x + math.cos(ang) * length
        y2 = y - math.sin(ang) * length
        tone = BARK_GREY[1] if math.cos(ang) < 0 else BARK_GREY[2]
        c.line(x, y, x2, y2, tone, max(1, int(round(thick))))
        if depth <= 0:
            return
        for _ in range(2 if depth > 1 else rng.randint(1, 2)):
            branch(x2, y2, ang + rng.uniform(-0.75, 0.75), length * rng.uniform(0.6, 0.78),
                   thick * 0.62, depth - 1)

    for ang, ln in ((1.95, 20), (1.25, 22), (2.6, 15), (0.6, 16), (1.6, 26)):
        branch(cx - 1, base - 38, ang + rng.uniform(-0.12, 0.12), ln, 3.2, 3)
    return c.finish()


def bush(seed, ramp, berries=None):
    rng = random.Random(seed)
    w, h = 44, 36
    c = Cell(w, h)
    cx, base = w // 2, h - FOOT - 1
    c.shadow(cx + 1, base, 15, 4, 60)
    spheres = [(cx, base - 9, 10, 2)]
    for i in range(4):
        spheres.append((cx + rng.uniform(-11, 11), base - rng.uniform(5, 13), rng.uniform(5.5, 8), 0))
    c.blobs(spheres, ramp, seed, leafy=0.5, ragged=0.35)
    if berries:
        for _ in range(7):
            x, y = int(cx + rng.uniform(-12, 12)), int(base - rng.uniform(3, 16))
            if c.get(x, y)[3]:
                c.put(x, y, berries)
    return c.finish()


def rock(seed, big=True):
    rng = random.Random(seed)
    w, h = (30, 24) if big else (18, 16)
    c = Cell(w, h)
    cx, base = w // 2, h - FOOT
    r = 8 if big else 4.5
    c.shadow(cx + 1, base - 1, r * 1.3, r * 0.42, 60)
    spheres = [(cx, base - r * 0.6, r, 1), (cx - r * 0.7, base - r * 0.35, r * 0.62, 0),
               (cx + r * 0.75, base - r * 0.3, r * 0.55, 0)]
    c.blobs(spheres, STONE, seed)
    # Flatten the bottom so it sits in the ground, and add a crack.
    for y in range(base, h):
        for x in range(w):
            c.img.putpixel((x, y), (0, 0, 0, 0))
    if big:
        c.line(cx - 1, base - r - 1, cx + 2, base - 4, STONE[3])
        c.put(cx - 5, base - 2, LEAF_GREEN[2])
        c.put(cx - 4, base - 3, LEAF_GREEN[1])
    return c.finish()


def stump(seed):
    w, h = 26, 24
    c = Cell(w, h)
    cx, base = w // 2, h - FOOT
    c.shadow(cx + 1, base - 1, 10, 3.2, 60)
    for y in range(base - 9, base):
        for x in range(cx - 6, cx + 6):
            f = (x - (cx - 6)) / 11.0
            tone = 0 if f < 0.2 else (1 if f < 0.6 else 2)
            if _hash(x, y // 3, seed) < 0.18:
                tone += 1
            c.put(x, y, BARK[min(3, tone)])
    # The cut face: an ellipse of pale wood with rings.
    for y in range(base - 13, base - 6):
        for x in range(cx - 6, cx + 6):
            u, v = (x + 0.5 - cx) / 6.0, (y + 0.5 - (base - 9.5)) / 3.4
            d = u * u + v * v
            if d <= 1.0:
                c.put(x, y, (176, 146, 104) if int(d * 3.2) % 2 == 0 else (150, 120, 86))
    return c.finish()


def grass_tuft(seed, flowers=None):
    rng = random.Random(seed)
    w, h = 20, 20
    c = Cell(w, h)
    cx, base = w // 2, h - FOOT
    for i in range(rng.randint(5, 8)):
        x = cx + rng.randint(-6, 6)
        height = rng.randint(4, 9)
        lean = rng.choice((-1, 0, 0, 1))
        for k in range(height):
            t = k / max(1, height - 1)
            tone = LEAF_GREEN[2] if t < 0.35 else (LEAF_GREEN[1] if t < 0.75 else LEAF_GREEN[0])
            c.put(x + round(lean * t * 2), base - k, tone)
        if flowers and i % 2 == 0:
            fx, fy = x + round(lean * 2), base - height
            c.put(fx, fy, flowers)
            c.put(fx + 1, fy, flowers)
            c.put(fx, fy - 1, tuple(min(255, v + 30) for v in flowers))
    return c.finish(outline=False)


def lamppost(seed):
    w, h = 40, 100
    c = Cell(w, h)
    cx, base = 14, h - FOOT
    c.shadow(cx + 2, base - 1, 7, 2.4, 70)
    # Footing, pole, arm, lamp.
    c.rect(cx - 3, base - 5, cx + 3, base - 1, DARK_METAL[2])
    c.rect(cx - 3, base - 5, cx - 2, base - 1, DARK_METAL[1])
    c.rect(cx - 2, base - 7, cx + 2, base - 6, DARK_METAL[1])
    for y in range(base - 84, base - 6):
        c.put(cx - 1, y, DARK_METAL[0])
        c.put(cx, y, DARK_METAL[1])
        c.put(cx + 1, y, DARK_METAL[2])
    for i in range(16):
        y = base - 84 - round(math.sin(i / 15.0 * math.pi / 2.0) * 5)
        c.put(cx + i, y, DARK_METAL[1])
        c.put(cx + i, y + 1, DARK_METAL[2])
    c.rect(cx + 13, base - 88, cx + 22, base - 86, DARK_METAL[1])
    c.rect(cx + 13, base - 88, cx + 22, base - 88, DARK_METAL[0])
    c.rect(cx + 14, base - 85, cx + 21, base - 84, (206, 196, 150))  # the lens, unlit
    return c.finish()


def hydrant(seed):
    w, h = 18, 26
    c = Cell(w, h)
    cx, base = w // 2, h - FOOT
    red = ((178, 78, 62), (142, 56, 48), (104, 40, 40), (74, 30, 34))
    c.shadow(cx + 1, base - 1, 6, 2.2, 70)
    c.blobs([(cx, base - 12, 3.6, 0)], red)
    for y in range(base - 12, base):
        for x in range(cx - 3, cx + 3):
            f = (x - (cx - 3)) / 5.0
            c.put(x, y, red[0 if f < 0.2 else (1 if f < 0.6 else 2)])
    c.rect(cx - 4, base - 2, cx + 3, base - 1, red[2])
    c.rect(cx - 5, base - 9, cx - 4, base - 7, red[1])
    c.rect(cx + 3, base - 9, cx + 4, base - 7, red[3])
    c.put(cx - 1, base - 16, METAL[1])
    return c.finish()


def trash_can(seed):
    w, h = 22, 30
    c = Cell(w, h)
    cx, base = w // 2, h - FOOT
    c.shadow(cx + 1, base - 1, 8, 2.8, 70)
    for y in range(base - 15, base):
        for x in range(cx - 6, cx + 6):
            f = (x - (cx - 6)) / 11.0
            tone = 0 if f < 0.15 else (1 if f < 0.55 else 2)
            if (x - cx) % 3 == 0:
                tone = min(3, tone + 1)  # ribs
            c.put(x, y, METAL[tone])
    for y in range(base - 19, base - 14):
        for x in range(cx - 7, cx + 7):
            u, v = (x + 0.5 - cx) / 7.0, (y + 0.5 - (base - 16.5)) / 2.6
            if u * u + v * v <= 1.0:
                c.put(x, y, METAL[0] if u < -0.2 else METAL[1])
    c.rect(cx - 1, base - 21, cx, base - 20, METAL[2])
    return c.finish()


# ---------------------------------------------------------------------------
# Flat things (no outline, drawn on the ground)
# ---------------------------------------------------------------------------

def _flat(w, h):
    return Cell(w, h)


def litter_paper(seed):
    c = _flat(12, 8)
    for (x, y) in ((3, 2), (4, 2), (5, 2), (6, 3), (2, 3), (3, 3), (4, 3), (5, 3), (3, 4), (4, 4), (5, 4), (6, 4), (7, 4), (4, 5), (5, 5)):
        c.put(x, y, (206, 200, 184) if (x + y) % 3 else (176, 170, 158))
    return c.finish(outline=False)


def litter_can(seed):
    c = _flat(10, 8)
    c.rect(2, 3, 6, 4, (150, 70, 60))
    c.rect(2, 3, 6, 3, (184, 96, 80))
    c.put(7, 3, (170, 172, 176))
    c.put(7, 4, (120, 122, 128))
    c.put(4, 5, (40, 36, 36), 90)
    c.put(5, 5, (40, 36, 36), 90)
    return c.finish(outline=False)


def leaves(seed):
    rng = random.Random(seed)
    c = _flat(22, 12)
    for _ in range(11):
        x, y = rng.randint(2, 18), rng.randint(2, 9)
        col = rng.choice((LEAF_RUST[1], LEAF_RUST[2], LEAF_RUST[0], (132, 112, 62)))
        c.put(x, y, col)
        c.put(x + 1, y, tuple(int(v * 0.8) for v in col))
    return c.finish(outline=False)


def puddle(seed):
    rng = random.Random(seed)
    c = _flat(34, 18)
    for y in range(18):
        for x in range(34):
            u, v = (x + 0.5 - 17) / 15.0, (y + 0.5 - 9) / 7.0
            d = u * u + v * v + (_hash(x // 3, y // 2, seed) - 0.5) * 0.3
            if d < 1.0:
                col = (64, 84, 100) if d < 0.55 else (52, 66, 80)
                if d < 0.3 and (x + y * 2) % 9 == 0:
                    col = (132, 158, 172)
                c.put(x, y, col, 150 if d < 0.8 else 90)
    return c.finish(outline=False)


def manhole(seed):
    c = _flat(26, 14)
    for y in range(14):
        for x in range(26):
            u, v = (x + 0.5 - 13) / 12.0, (y + 0.5 - 7) / 6.0
            d = u * u + v * v
            if d <= 1.0:
                col = (58, 58, 62) if d > 0.72 else ((84, 84, 88) if (x + y) % 4 else (104, 104, 108))
                if d <= 0.72 and y % 3 == 0:
                    col = (66, 66, 70)
                c.put(x, y, col)
    return c.finish(outline=False)


def weeds(seed):
    rng = random.Random(seed)
    c = _flat(14, 10)
    for _ in range(6):
        x, y = rng.randint(2, 11), rng.randint(4, 8)
        c.put(x, y, LEAF_GREEN[2])
        c.put(x, y - 1, LEAF_GREEN[1])
        if rng.random() < 0.5:
            c.put(x + 1, y - 2, LEAF_GREEN[0])
    return c.finish(outline=False)


def road_crack(seed):
    rng = random.Random(seed)
    c = _flat(30, 16)
    x, y = 2.0, rng.uniform(5, 10)
    for _ in range(26):
        c.put(x, y, (38, 40, 44), 220)
        if rng.random() < 0.3:
            c.put(x, y + 1, (46, 48, 52), 160)
        x += 1.0
        y += rng.choice((-0.5, 0.0, 0.0, 0.5, 0.5)) * (1 if rng.random() < 0.8 else 2)
        y = max(1.0, min(14.0, y))
    return c.finish(outline=False)


# name -> image. Names are the atlas keys the game looks up (SceneryView.cpp).
def soft_blob(w=96, h=48):
    """A soft-edged white ellipse: tinted and stretched into cloud shadows and
    banks of fog."""
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    for y in range(h):
        for x in range(w):
            dx = (x + 0.5 - w / 2.0) / (w / 2.0)
            dy = (y + 0.5 - h / 2.0) / (h / 2.0)
            d = math.sqrt(dx * dx + dy * dy)
            # Lumpy edge, so stretched blobs do not read as perfect ovals.
            d += 0.10 * math.sin(math.atan2(dy, dx) * 5.0 + 1.0) + 0.06 * math.sin(math.atan2(dy, dx) * 9.0)
            a = max(0.0, min(1.0, (1.0 - d) / 0.55))
            a = a * a * (3.0 - 2.0 * a)
            if a > 0.0:
                img.putpixel((x, y), (255, 255, 255, int(255 * a)))
    return img


def scenery_sprites():
    out = {}
    out["fx_cloud"] = soft_blob()
    out["scn_tree_oak_a"] = tree_broadleaf(11, LEAF_GREEN, 1.0)
    out["scn_tree_oak_b"] = tree_broadleaf(23, LEAF_DARK, 0.88)
    out["scn_tree_autumn"] = tree_broadleaf(37, LEAF_RUST, 0.82, BARK_GREY)
    out["scn_tree_pine"] = tree_pine(41)
    out["scn_tree_dead"] = tree_dead(53)
    out["scn_bush_a"] = bush(61, LEAF_GREEN)
    out["scn_bush_b"] = bush(67, LEAF_DARK, berries=(170, 60, 66))
    out["scn_rock_big"] = rock(71, True)
    out["scn_rock_small"] = rock(73, False)
    out["scn_stump"] = stump(79)
    out["scn_tuft_a"] = grass_tuft(83)
    out["scn_tuft_b"] = grass_tuft(89)
    out["scn_flowers_a"] = grass_tuft(97, flowers=(206, 190, 110))
    out["scn_flowers_b"] = grass_tuft(101, flowers=(176, 150, 190))
    out["scn_lamppost"] = lamppost(0)
    out["scn_hydrant"] = hydrant(0)
    out["scn_trash_can"] = trash_can(0)
    out["scn_paper"] = litter_paper(0)
    out["scn_can"] = litter_can(0)
    out["scn_leaves"] = leaves(107)
    out["scn_puddle"] = puddle(109)
    out["scn_manhole"] = manhole(0)
    out["scn_weeds"] = weeds(113)
    out["scn_crack"] = road_crack(127)
    return out


def preview(path, scale=3):
    sprites = scenery_sprites()
    w = sum(im.width + 6 for im in sprites.values()) + 6
    h = max(im.height for im in sprites.values()) + 12
    sheet = Image.new("RGBA", (w, h), (84, 104, 64, 255))
    x = 6
    for im in sprites.values():
        sheet.alpha_composite(im, (x, h - 6 - im.height))
        x += im.width + 6
    sheet.resize((w * scale, h * scale), Image.NEAREST).save(path)


if __name__ == "__main__":
    import sys
    preview(sys.argv[1] if len(sys.argv) > 1 else "scenery_preview.png")
