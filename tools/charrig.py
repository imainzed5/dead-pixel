#!/usr/bin/env python3
"""
Character rig for Dead Pixel Survival.

Every character frame is drawn from one small 3D skeleton: a pose places the
joints, the rig projects them for one of five facings and rasterises the body
as shaded capsules with a depth buffer. So one description of a walk gives
every facing and every frame, and they all agree with each other.

Character space: +x is the character's right, +y is forward, +z is up, in
pixels, with the origin on the ground between the feet.

Frames are 64x64 with the soles on row 49, the same as the old hand-placed
sprites, so the renderer's foot pivot does not change.

Views are the five facings that are drawn; the game mirrors se, e and ne for
the three left-hand facings:
    s  toward the viewer        se  down-right      e  screen right
    ne up-right                 n   away
"""

import math

from PIL import Image

SIZE = 64
CX = 32.0          # screen x of the ground point
GROUND = 50.0      # screen y of the ground line (bottom edge of row 49)
TILT = math.radians(28.0)
COS_T, SIN_T = math.cos(TILT), math.sin(TILT)

VIEWS = {
    "s": (0.0, 1.0),
    "se": (0.7071, 0.7071),
    "e": (0.985, 0.174),   # a hair short of side-on, so the far limbs show
    "ne": (0.7071, -0.7071),
    "n": (0.0, -1.0),
}
VIEW_ORDER = ("s", "se", "e", "ne", "n")

OUTLINE = (30, 24, 26)
SHADOW_RGBA = (0, 0, 0, 72)
BLOOD = ((158, 42, 36), (124, 28, 26), (86, 18, 18))

WOOD = ((176, 138, 92), (142, 106, 68), (102, 74, 48))
STEEL = ((206, 212, 218), (160, 168, 178), (108, 116, 128))
GUNMETAL = ((92, 96, 104), (62, 66, 74), (40, 42, 50))


# ---------------------------------------------------------------------------
# Looks: palettes and a few switches
# ---------------------------------------------------------------------------

def _look(**kw):
    base = dict(zombie=False, pack=False, hair_style="short", sleeves="long")
    base.update(kw)
    return base


PLAYER_LOOK = _look(
    skin=((236, 204, 172), (214, 178, 144), (172, 134, 108)),
    hair=((104, 76, 54), (70, 50, 36), (44, 32, 24)),
    top=((112, 134, 150), (82, 102, 120), (56, 70, 88)),
    legs=((84, 94, 124), (60, 68, 94), (42, 48, 68)),
    shoes=((82, 66, 52), (56, 44, 36), (36, 28, 22)),
    pack=((152, 128, 86), (118, 98, 64), (84, 70, 46)),
    detail=(40, 50, 62),
    eye=(36, 30, 28),
    pack_on=True,
)

ZOMBIE_LOOKS = [
    # A: the office shirt gone grey-brown.
    _look(zombie=True, hair_style="patchy",
          skin=((160, 176, 134), (128, 148, 110), (92, 110, 84)),
          hair=((74, 70, 60), (54, 50, 44), (38, 36, 32)),
          top=((136, 122, 102), (106, 94, 80), (76, 66, 58)),
          legs=((86, 84, 98), (66, 64, 78), (48, 46, 58)),
          shoes=((66, 60, 54), (48, 44, 40), (34, 30, 28)),
          detail=(60, 52, 46), eye=(236, 224, 150), mouth=(52, 18, 22)),
    # B: a red work shirt, bald, greyer skin.
    _look(zombie=True, hair_style="bald", sleeves="short",
          skin=((150, 160, 144), (120, 130, 118), (86, 96, 90)),
          hair=((60, 58, 54), (46, 44, 42), (34, 32, 32)),
          top=((138, 78, 70), (108, 56, 52), (76, 38, 40)),
          legs=((96, 88, 74), (74, 68, 58), (54, 50, 44)),
          shoes=((58, 52, 48), (42, 38, 36), (30, 26, 26)),
          detail=(54, 30, 30), eye=(240, 232, 170), mouth=(52, 18, 22)),
    # C: a blue-green jumper, long dark hair.
    _look(zombie=True, hair_style="long",
          skin=((170, 172, 128), (138, 142, 104), (100, 106, 80)),
          hair=((58, 48, 44), (42, 34, 32), (28, 24, 24)),
          top=((92, 122, 124), (68, 94, 98), (48, 68, 74)),
          legs=((70, 72, 76), (52, 54, 58), (38, 40, 44)),
          shoes=((70, 58, 50), (50, 42, 36), (34, 28, 24)),
          detail=(40, 58, 62), eye=(232, 216, 140), mouth=(52, 18, 22)),
]


# ---------------------------------------------------------------------------
# Small vector helpers
# ---------------------------------------------------------------------------

def add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def mul(a, s):
    return (a[0] * s, a[1] * s, a[2] * s)


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def length(a):
    return math.sqrt(dot(a, a))


def norm(a):
    n = length(a)
    return (a[0] / n, a[1] / n, a[2] / n) if n > 1e-9 else (0.0, 0.0, 1.0)


def lerp(a, b, t):
    return a + (b - a) * t


def lerp3(a, b, t):
    return (lerp(a[0], b[0], t), lerp(a[1], b[1], t), lerp(a[2], b[2], t))


def rot_z(p, ang):
    c, s = math.cos(ang), math.sin(ang)
    return (p[0] * c - p[1] * s, p[0] * s + p[1] * c, p[2])


def ik(root, target, l1, l2, hint):
    """Middle joint of a two-bone limb from root to target, bent toward hint.
    Returns (joint, end): end is target pulled in if it was out of reach."""
    d = sub(target, root)
    dist = length(d)
    reach = (l1 + l2) * 0.995
    if dist > reach:
        d = mul(d, reach / dist)
        dist = reach
        target = add(root, d)
    if dist < 1e-6:
        return add(root, mul(norm(hint), l1)), target
    along = mul(d, 1.0 / dist)
    a1 = (l1 * l1 - l2 * l2 + dist * dist) / (2.0 * dist)
    h = math.sqrt(max(0.0, l1 * l1 - a1 * a1))
    side = sub(hint, mul(along, dot(hint, along)))
    side = norm(side) if length(side) > 1e-6 else (0.0, 0.0, 0.0)
    return add(add(root, mul(along, a1)), mul(side, h)), target


# ---------------------------------------------------------------------------
# The rasteriser
# ---------------------------------------------------------------------------

class Frame:
    """One 64x64 frame: colour, depth and part group per pixel."""

    def __init__(self, facing, pitch=0.0, pivot_y=0.0, lift=0.0):
        self.F = facing
        self.pitch = pitch      # radians; the whole body tips backward about the heels
        self.pivot_y = pivot_y
        self.lift = lift
        self.cp, self.sp = math.cos(pitch), math.sin(pitch)
        n = SIZE * SIZE
        self.col = [None] * n
        self.dep = [-1e9] * n
        self.grp = [0] * n
        self.under = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))

    # -- transforms ---------------------------------------------------------

    def tip(self, p):
        """Apply the fall (a pitch backward about the heels) to a body point."""
        if self.pitch == 0.0:
            return p
        y, z = p[1] - self.pivot_y, p[2]
        return (p[0], self.pivot_y + y * self.cp - z * self.sp, y * self.sp + z * self.cp + self.lift)

    def untip_dir(self, d):
        if self.pitch == 0.0:
            return d
        return (d[0], d[1] * self.cp + d[2] * self.sp, -d[1] * self.sp + d[2] * self.cp)

    def project(self, p):
        x, y, z = self.tip(p)
        fx, fy = self.F
        gx = -fy * x + fx * y
        gy = fx * x + fy * y
        return (CX + gx, GROUND - z + gy * 0.5, gy * COS_T + z * SIN_T)

    # -- primitives ---------------------------------------------------------

    def capsule(self, a, b, r, ramp, group, r_end=None, flat=False):
        """A shaded capsule from a to b. ramp is (light, base, dark) or a
        function (t, tone) -> rgb. flat skips the round shading (decals)."""
        ax, ay, ad = self.project(a)
        bx, by, bd = self.project(b)
        r_end = r if r_end is None else r_end
        rmax = max(r, r_end)
        dx, dy = bx - ax, by - ay
        l2 = dx * dx + dy * dy
        x0 = max(0, int(math.floor(min(ax, bx) - rmax)))
        x1 = min(SIZE - 1, int(math.ceil(max(ax, bx) + rmax)))
        y0 = max(0, int(math.floor(min(ay, by) - rmax)))
        y1 = min(SIZE - 1, int(math.ceil(max(ay, by) + rmax)))
        for py in range(y0, y1 + 1):
            sy = py + 0.5
            for px in range(x0, x1 + 1):
                sx = px + 0.5
                t = 0.0
                if l2 > 1e-9:
                    t = ((sx - ax) * dx + (sy - ay) * dy) / l2
                    t = 0.0 if t < 0.0 else (1.0 if t > 1.0 else t)
                rr = r + (r_end - r) * t
                ox, oy = sx - (ax + dx * t), sy - (ay + dy * t)
                d2 = ox * ox + oy * oy
                if d2 > rr * rr:
                    continue
                h = math.sqrt(rr * rr - d2)
                depth = ad + (bd - ad) * t + h
                i = py * SIZE + px
                if depth <= self.dep[i]:
                    continue
                if flat:
                    tone = 1
                else:
                    lit = (-0.62 * ox - 0.66 * oy) / rr + 0.30 * h / rr
                    tone = 0 if lit > 0.50 else (2 if lit < -0.22 else 1)
                self.dep[i] = depth
                self.grp[i] = group
                self.col[i] = ramp(t, tone) if callable(ramp) else ramp[tone]

    def ball(self, c, r, ramp, group, flat=False):
        self.capsule(c, c, r, ramp, group, flat=flat)

    def pixel(self, x, y, rgb, depth, group):
        x, y = int(math.floor(x)), int(math.floor(y))
        if 0 <= x < SIZE and 0 <= y < SIZE:
            i = y * SIZE + x
            if depth >= self.dep[i]:
                self.col[i] = rgb
                self.dep[i] = depth
                self.grp[i] = group

    def head(self, centre, look, group, yaw=0.0, mouth_open=False, nod=0.0):
        """A 9x10 head. yaw turns it (radians, + toward the character's left);
        nod drops the face forward (zombies hang their heads)."""
        cxs, cys, cd = self.project(centre)
        cx = math.floor(cxs) + 0.5
        cy = float(round(cys))
        rx, ry = 4.5, 5.0
        fx, fy = self.F
        skin, hair = look["skin"], look["hair"]
        zombie = look["zombie"]
        style = look["hair_style"]
        cyaw, syaw = math.cos(yaw), math.sin(yaw)
        cn, sn = math.cos(nod), math.sin(nod)

        def to_head(gx, gyv, up):
            # screen-ground normal -> body space -> head space
            n = (-fy * gx + fx * gyv, fx * gx + fy * gyv, up)
            n = self.untip_dir(n)
            n = (n[0] * cyaw + n[1] * syaw, -n[0] * syaw + n[1] * cyaw, n[2])
            return (n[0], n[1] * cn - n[2] * sn, n[1] * sn + n[2] * cn)

        def from_head(d):
            d = (d[0], d[1] * cn + d[2] * sn, -d[1] * sn + d[2] * cn)
            d = (d[0] * cyaw - d[1] * syaw, d[0] * syaw + d[1] * cyaw, d[2])
            if self.pitch != 0.0:
                d = (d[0], d[1] * self.cp - d[2] * self.sp, d[1] * self.sp + d[2] * self.cp)
            gx = -fy * d[0] + fx * d[1]
            gyv = fx * d[0] + fy * d[1]
            toward = gyv * COS_T + d[2] * SIN_T
            up = d[2] * COS_T - gyv * SIN_T
            return gx, up, toward

        for py in range(int(cy - ry), int(cy + ry)):
            for px in range(int(cx - rx), int(cx + rx) + 1):
                u = (px + 0.5 - cx) / rx
                v = (py + 0.5 - cy) / ry
                q = u * u + v * v
                if q > 1.0:
                    continue
                w = math.sqrt(1.0 - q)
                up = -v * COS_T + w * SIN_T
                gyv = w * COS_T + v * SIN_T
                nx, ny, nz = to_head(u, gyv, up)
                lit = -0.55 * u - 0.60 * v + 0.35 * w
                tone = 0 if lit > 0.62 else (2 if lit < -0.05 else 1)

                is_hair = False
                if style == "bald":
                    is_hair = False
                elif style == "patchy":
                    is_hair = nz > 0.55 and ((px * 7 + py * 13) % 5 != 0) or (ny < -0.5 and nz > 0.1 and (px + py) % 3 != 0)
                elif style == "long":
                    is_hair = nz > 0.34 + 0.34 * max(0.0, ny) or ny < -0.05 or (abs(nx) > 0.62 and ny < 0.55)
                else:
                    is_hair = (nz > 0.36 + 0.30 * max(0.0, ny)) or (ny < -0.30 and nz > -0.55) \
                              or (abs(nx) > 0.80 and nz > 0.05 and ny < 0.35)
                ramp = hair if is_hair else skin
                i = py * SIZE + px
                depth = cd + w * 4.5
                if 0 <= px < SIZE and 0 <= py < SIZE and depth > self.dep[i]:
                    self.col[i] = ramp[tone]
                    self.dep[i] = depth
                    self.grp[i] = group

        def feature(d, rgb, min_toward=0.18):
            gx, up, toward = from_head(norm(d))
            if toward < min_toward:
                return
            self.pixel(cx + gx * rx, cy - up * ry, rgb, cd + 6.0, group)

        eye = look["eye"]
        feature((-0.40, 0.90, 0.02), eye)
        feature((0.40, 0.90, 0.02), eye)
        if zombie:
            mouth = look["mouth"]
            feature((-0.14, 0.86, -0.52), mouth)
            feature((0.14, 0.86, -0.52), mouth)
            if mouth_open:
                feature((-0.14, 0.78, -0.72), mouth)
                feature((0.14, 0.78, -0.72), mouth)
                feature((0.0, 0.70, -0.86), BLOOD[1])
            feature((0.30, 0.74, -0.74), BLOOD[1], 0.3)
            feature((-0.62, 0.30, 0.74), BLOOD[2], 0.3)
        else:
            feature((0.0, 0.98, -0.30), skin[2], 0.30)       # nose and its shadow
            feature((-0.98, 0.05, -0.05), skin[2], 0.25)     # ears
            feature((0.98, 0.05, -0.05), skin[2], 0.25)

    # -- compositing --------------------------------------------------------

    def ground_ellipse(self, cx, cy, rx, ry, rgba):
        for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
            for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
                if 0 <= x < SIZE and 0 <= y < SIZE:
                    if ((x + 0.5 - cx) / rx) ** 2 + ((y + 0.5 - cy) / ry) ** 2 <= 1.0:
                        self.under.putpixel((x, y), rgba)

    def finish(self):
        """Contact shading between overlapping parts, a dark outline, and the
        figure laid over whatever is on the ground layer."""
        col, dep, grp = self.col, self.dep, self.grp
        out = self.under.copy()
        px = out.load()
        shaded = list(col)
        for y in range(SIZE):
            for x in range(SIZE):
                i = y * SIZE + x
                if col[i] is None:
                    continue
                # A nearer part to the upper left drops a line of shade on this one.
                for nx, ny in ((x - 1, y), (x, y - 1)):
                    if nx < 0 or ny < 0:
                        continue
                    j = ny * SIZE + nx
                    if col[j] is not None and grp[j] != grp[i] and dep[j] > dep[i] + 2.2:
                        c = col[i]
                        shaded[i] = (int(c[0] * 0.62), int(c[1] * 0.60), int(c[2] * 0.66))
                        break
        for y in range(SIZE):
            for x in range(SIZE):
                i = y * SIZE + x
                if col[i] is not None:
                    c = shaded[i]
                    px[x, y] = (c[0], c[1], c[2], 255)
                    continue
                for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                    if 0 <= nx < SIZE and 0 <= ny < SIZE and col[ny * SIZE + nx] is not None:
                        c = col[ny * SIZE + nx]
                        # The outline takes a little of the colour it borders.
                        px[x, y] = (int(OUTLINE[0] * 0.75 + c[0] * 0.10),
                                    int(OUTLINE[1] * 0.75 + c[1] * 0.10),
                                    int(OUTLINE[2] * 0.75 + c[2] * 0.10), 255)
                        break
        return out


# ---------------------------------------------------------------------------
# Poses
# ---------------------------------------------------------------------------

G_LEG_FAR, G_LEG_NEAR, G_TORSO, G_HEAD, G_ARM_L, G_ARM_R, G_PACK, G_WEAPON = range(1, 9)


def base_pose(zombie=False):
    """The neutral standing pose. Everything a pose can change lives here."""
    p = dict(
        hip=(0.0, 0.0, 14.0),
        chest=(0.0, 0.4, 25.0),
        head=(0.0, 0.6, 32.0),
        twist=0.0,            # shoulders turn about the spine (+ brings the right one forward)
        foot_l=(-2.6, 0.0, 0.0),
        foot_r=(2.6, 0.0, 0.0),
        hand_l=(-5.4, 0.6, 14.6),
        hand_r=(5.4, 0.6, 14.6),
        head_yaw=0.0,
        nod=0.0,
        mouth_open=False,
        weapon=None,          # None, "club", "blade", "gun", "long"
        weapon_dir=(0.0, 0.3, 1.0),
        flash=False,          # muzzle flash
        pitch=0.0,            # the whole body falling backward (radians)
        pool=0.0,             # blood pool size, 0..1
    )
    if zombie:
        p.update(
            chest=(0.5, 2.4, 24.0),
            head=(0.9, 4.6, 30.2),
            nod=0.35,
            hand_l=(-3.4, 9.0, 21.5),
            hand_r=(3.6, 10.0, 23.5),
            foot_l=(-2.8, 0.4, 0.0),
            foot_r=(2.6, -0.6, 0.0),
        )
    return p


def _gait(p, ph, stride, lift, bob, arm, twist, drag=1.0):
    """Legs and arms for one point in a stride. ph: 0..2pi. The left foot is
    furthest forward at pi/2. drag < 1 shortens and flattens the right leg's step."""
    s, c = math.sin(ph), math.cos(ph)
    fl, fr = p["foot_l"], p["foot_r"]
    p["foot_l"] = (fl[0], fl[1] + stride * s, lift * max(0.0, c))
    p["foot_r"] = (fr[0], fr[1] - stride * s * drag, lift * drag * max(0.0, -c))
    drop = bob * (1.0 - abs(c))
    for k in ("hip", "chest", "head"):
        p[k] = (p[k][0], p[k][1], p[k][2] - drop)
    hl, hr = p["hand_l"], p["hand_r"]
    p["hand_l"] = (hl[0], hl[1] - arm * s, hl[2] - drop + 0.9 * abs(s))
    p["hand_r"] = (hr[0], hr[1] + arm * s, hr[2] - drop + 0.9 * abs(s))
    p["twist"] += twist * s
    return p


def _crouch(p):
    p["hip"] = (0.0, -1.2, 9.4)
    p["chest"] = (0.0, 3.2, 18.6)
    p["head"] = (0.0, 4.8, 25.2)
    p["foot_l"] = (-3.0, 1.6, 0.0)
    p["foot_r"] = (3.0, -1.4, 0.0)
    p["hand_l"] = (-4.8, 5.0, 11.0)
    p["hand_r"] = (4.8, 5.0, 11.0)
    return p


def _carry(p, weapon, sway=0.0):
    """Hold a weapon at the ready in the right hand."""
    p["weapon"] = weapon
    dz = p["chest"][2] - 25.0
    dy = p["chest"][1] - 0.4
    if weapon == "club":
        p["hand_r"] = (5.6, 2.4 + dy + sway * 0.4, 17.0 + dz)
        p["weapon_dir"] = (0.10, 0.30 + sway * 0.05, 1.0)
    elif weapon == "blade":
        p["hand_r"] = (5.4, 2.8 + dy + sway * 0.6, 15.6 + dz)
        p["weapon_dir"] = (0.0, 1.0, 0.25)
    elif weapon == "gun":
        p["hand_r"] = (4.8, 3.6 + dy + sway * 0.4, 16.4 + dz)
        p["weapon_dir"] = (0.0, 1.0, -0.45)
    return p


def pose_idle(f, n, weapon=None):
    p = base_pose()
    breathe = 0.5 * (1.0 - math.cos(2.0 * math.pi * f / n))   # 0..1
    p["chest"] = (0.0, 0.4, 25.0 - 0.7 * breathe)
    p["head"] = (0.0, 0.6, 32.0 - 0.9 * breathe)
    p["hand_l"] = (-5.4, 0.6, 14.6 - 0.6 * breathe)
    p["hand_r"] = (5.4, 0.6, 14.6 - 0.6 * breathe)
    return _carry(p, weapon) if weapon else p


def pose_walk(f, n, weapon=None):
    ph = 2.0 * math.pi * f / n
    p = _gait(base_pose(), ph, stride=4.2, lift=2.8, bob=1.0, arm=3.4, twist=0.20)
    return _carry(p, weapon, math.sin(ph)) if weapon else p


def pose_run(f, n, weapon=None):
    ph = 2.0 * math.pi * f / n
    p = base_pose()
    p["hip"] = (0.0, 0.0, 13.2)
    p["chest"] = (0.0, 2.6, 24.0)
    p["head"] = (0.0, 4.0, 30.8)
    p["hand_l"] = (-5.0, 2.5, 18.5)
    p["hand_r"] = (5.0, 2.5, 18.5)
    p = _gait(p, ph, stride=6.4, lift=4.6, bob=1.3, arm=5.2, twist=0.32)
    return _carry(p, weapon, math.sin(ph) * 1.6) if weapon else p


def pose_crouch(f, n, weapon=None):
    p = _crouch(base_pose())
    return _carry(p, weapon) if weapon else p


def pose_crouchwalk(f, n, weapon=None):
    ph = 2.0 * math.pi * f / n
    p = _gait(_crouch(base_pose()), ph, stride=3.0, lift=1.8, bob=0.5, arm=1.6, twist=0.12)
    return _carry(p, weapon, math.sin(ph)) if weapon else p


def pose_hurt(f, n, weapon=None):
    p = base_pose()
    p["hip"] = (0.0, -0.8, 13.0)
    p["chest"] = (0.0, -2.0, 23.6)
    p["head"] = (0.0, -3.2, 30.2)
    p["head_yaw"] = 0.5
    p["foot_l"] = (-3.0, -1.6, 0.0)
    p["foot_r"] = (2.8, 1.2, 0.0)
    p["hand_l"] = (-7.2, 0.5, 19.0)
    p["hand_r"] = (7.0, 1.5, 17.5)
    p["twist"] = -0.25
    if weapon:
        p["weapon"] = weapon
        p["weapon_dir"] = (0.5, 0.4, 0.8)
    return p


def _keys(f, frames):
    """Pick the keyframe dict for frame f and lay it over the base pose."""
    p = base_pose()
    p.update(frames[min(f, len(frames) - 1)])
    return p


def pose_swing(f, n, weapon="club"):
    """Overhand swing. Frames 0-1 wind up, 2-3 strike, 4-5 recover."""
    frames = [
        dict(hand_r=(6.4, -1.0, 23.5), weapon_dir=(0.25, -0.45, 1.0), twist=-0.35,
             hand_l=(-4.6, 3.0, 17.0), chest=(0.0, 0.0, 25.0), foot_l=(-2.6, 1.0, 0.0)),
        dict(hand_r=(5.8, -3.4, 28.5), weapon_dir=(0.30, -0.95, 0.55), twist=-0.62,
             hand_l=(-4.2, 4.5, 18.5), chest=(0.0, -0.6, 25.0), head=(0.0, -0.2, 32.0),
             foot_l=(-2.6, 2.0, 0.0), foot_r=(2.6, -1.4, 0.0)),
        dict(hand_r=(5.0, 5.0, 28.0), weapon_dir=(-0.05, 0.75, 0.85), twist=0.15,
             hand_l=(-5.4, -0.5, 17.0), chest=(0.0, 1.8, 24.6), head=(0.0, 2.6, 31.4),
             foot_l=(-2.6, 3.6, 0.0), foot_r=(2.6, -1.8, 0.0), hip=(0.0, 0.6, 13.4)),
        dict(hand_r=(0.5, 10.0, 18.0), weapon_dir=(-0.35, 1.0, -0.35), twist=0.60,
             hand_l=(-5.8, -2.4, 16.0), chest=(0.0, 3.4, 23.6), head=(0.0, 5.0, 30.2),
             foot_l=(-2.6, 4.2, 0.0), foot_r=(2.6, -2.2, 0.0), hip=(0.0, 1.2, 12.8)),
        dict(hand_r=(-3.2, 7.4, 13.5), weapon_dir=(-0.70, 0.60, -0.55), twist=0.66,
             hand_l=(-5.8, -2.0, 15.6), chest=(0.0, 3.0, 23.8), head=(0.0, 4.4, 30.4),
             foot_l=(-2.6, 4.0, 0.0), foot_r=(2.6, -2.0, 0.0), hip=(0.0, 1.0, 13.0)),
        dict(hand_r=(3.4, 4.6, 15.6), weapon_dir=(-0.1, 0.7, 0.5), twist=0.22,
             hand_l=(-5.4, 0.0, 15.0), chest=(0.0, 1.4, 24.6), head=(0.0, 2.0, 31.4),
             foot_l=(-2.6, 2.4, 0.0), foot_r=(2.6, -1.0, 0.0)),
    ]
    p = _keys(f, frames)
    p["weapon"] = weapon
    return p


def pose_stab(f, n, weapon="blade"):
    """A short thrust (knives, and fists when weapon is None).
    Frames 0-1 wind up, 2 strikes, 3-4 recover."""
    high = 3.0 if weapon is None else 0.0
    frames = [
        dict(hand_r=(5.6, -1.0, 18.0 + high), twist=-0.30, hand_l=(-4.4, 4.0, 19.0),
             foot_l=(-2.6, 1.0, 0.0)),
        dict(hand_r=(5.4, -3.2, 18.6 + high), twist=-0.55, hand_l=(-4.0, 5.2, 20.0),
             chest=(0.0, -0.4, 25.0), foot_l=(-2.6, 1.8, 0.0), foot_r=(2.6, -1.2, 0.0)),
        dict(hand_r=(2.6, 10.6, 20.0 + high), twist=0.58, hand_l=(-5.6, -2.0, 17.0),
             chest=(0.0, 3.0, 24.0), head=(0.0, 4.4, 30.8), hip=(0.0, 1.0, 13.2),
             foot_l=(-2.6, 4.0, 0.0), foot_r=(2.6, -2.0, 0.0)),
        dict(hand_r=(3.2, 8.4, 19.4 + high), twist=0.42, hand_l=(-5.6, -1.4, 16.4),
             chest=(0.0, 2.4, 24.2), head=(0.0, 3.6, 31.0), hip=(0.0, 0.8, 13.4),
             foot_l=(-2.6, 3.6, 0.0), foot_r=(2.6, -1.8, 0.0)),
        dict(hand_r=(5.0, 3.4, 16.4 + high * 0.5), twist=0.12, hand_l=(-5.2, 0.4, 15.4),
             chest=(0.0, 1.0, 24.8), head=(0.0, 1.4, 31.6), foot_l=(-2.6, 2.0, 0.0),
             foot_r=(2.6, -0.8, 0.0)),
    ]
    p = _keys(f, frames)
    p["weapon"] = weapon
    p["weapon_dir"] = (0.0, 1.0, 0.1)
    return p


def pose_shoot(f, n, weapon="gun"):
    """Two-handed aim. Frame 0 raises the gun, 1 fires, 2 settles."""
    frames = [
        dict(hand_r=(1.6, 8.6, 22.6), hand_l=(-0.6, 8.2, 22.0), twist=0.25,
             foot_l=(-3.0, 1.6, 0.0), foot_r=(3.0, -1.4, 0.0)),
        dict(hand_r=(1.6, 7.4, 24.0), hand_l=(-0.6, 7.0, 23.2), twist=0.25,
             chest=(0.0, -0.4, 25.0), head=(0.0, -0.2, 32.0),
             foot_l=(-3.0, 1.6, 0.0), foot_r=(3.0, -1.4, 0.0), flash=True,
             weapon_dir=(0.0, 1.0, 0.22)),
        dict(hand_r=(1.6, 8.2, 23.0), hand_l=(-0.6, 7.8, 22.4), twist=0.25,
             foot_l=(-3.0, 1.6, 0.0), foot_r=(3.0, -1.4, 0.0)),
    ]
    p = _keys(f, frames)
    p["weapon"] = weapon
    p.setdefault("weapon_dir", (0.0, 1.0, 0.0))
    if "weapon_dir" not in frames[min(f, 2)]:
        p["weapon_dir"] = (0.0, 1.0, 0.0)
    return p


# -- zombies ---------------------------------------------------------------

def zpose_idle(f, n, weapon=None):
    p = base_pose(True)
    s = math.sin(2.0 * math.pi * f / n)
    p["chest"] = (0.5 + 0.7 * s, 2.4, 24.0)
    p["head"] = (0.9 + 1.2 * s, 4.6, 30.2 - 0.4 * abs(s))
    p["hand_l"] = (-3.4, 9.0, 21.5 + 0.8 * s)
    p["hand_r"] = (3.6, 10.0, 23.5 - 0.8 * s)
    return p


def zpose_walk(f, n, weapon=None):
    ph = 2.0 * math.pi * f / n
    s, c = math.sin(ph), math.cos(ph)
    p = base_pose(True)
    sway = 1.0 * s
    p["hip"] = (sway * 0.6, 0.0, 14.0)
    p["chest"] = (0.5 + sway, 2.4, 24.0)
    p["head"] = (0.9 + sway * 1.5, 4.6, 30.2)
    p = _gait(p, ph, stride=3.4, lift=2.0, bob=1.2, arm=0.0, twist=0.10, drag=0.55)
    p["hand_l"] = (-3.4 + sway * 0.5, 9.0, p["chest"][2] - 2.5 + 0.9 * c)
    p["hand_r"] = (3.6 + sway * 0.5, 10.0, p["chest"][2] - 0.5 - 0.9 * c)
    return p


def zpose_lunge(f, n, weapon=None):
    """Frames 0-1 coil back (the telegraph), 2-3 lunge with the mouth open, 4 recovers."""
    frames = [
        dict(chest=(0.3, 0.6, 24.2), head=(0.6, 1.4, 30.8), nod=0.1,
             hand_l=(-5.0, 4.0, 24.0), hand_r=(5.2, 4.5, 25.5), hip=(0.0, -0.6, 13.4),
             foot_l=(-2.8, 1.2, 0.0), foot_r=(2.6, -1.4, 0.0)),
        dict(chest=(0.2, -0.8, 24.0), head=(0.4, -1.0, 31.0), nod=-0.1,
             hand_l=(-6.0, 1.0, 26.0), hand_r=(6.2, 1.5, 27.0), hip=(0.0, -1.0, 12.8),
             foot_l=(-2.8, 1.6, 0.0), foot_r=(2.6, -1.8, 0.0), mouth_open=True),
        dict(chest=(0.4, 5.6, 22.6), head=(0.6, 9.0, 28.0), nod=0.15,
             hand_l=(-2.6, 13.5, 22.5), hand_r=(2.8, 14.0, 24.0), hip=(0.0, 1.6, 12.8),
             foot_l=(-2.8, 5.0, 0.0), foot_r=(2.6, -2.6, 0.0), mouth_open=True),
        dict(chest=(0.4, 6.0, 22.0), head=(0.6, 9.4, 27.2), nod=0.3,
             hand_l=(-2.2, 13.0, 20.5), hand_r=(2.4, 13.6, 21.5), hip=(0.0, 1.8, 12.4),
             foot_l=(-2.8, 5.2, 0.0), foot_r=(2.6, -2.6, 0.0), mouth_open=True),
        dict(chest=(0.5, 3.6, 23.4), head=(0.9, 6.0, 29.4),
             hand_l=(-3.2, 10.5, 21.0), hand_r=(3.4, 11.2, 22.6),
             foot_l=(-2.8, 2.6, 0.0), foot_r=(2.6, -1.4, 0.0)),
    ]
    p = base_pose(True)
    p.update(frames[min(f, len(frames) - 1)])
    return p


def _reel(amount):
    """Knocked back on the heels: amount 0..1."""
    p = base_pose(True)
    a = amount
    p["hip"] = (0.0, -0.6 * a, 14.0 - 0.8 * a)
    p["chest"] = (0.5 - 0.5 * a, 2.4 - 4.6 * a, 24.0 - 0.6 * a)
    p["head"] = (0.9 - 0.9 * a, 4.6 - 8.6 * a, 30.2 + 0.6 * a)
    p["nod"] = 0.35 - 0.9 * a
    p["hand_l"] = (-3.4 - 4.0 * a, 9.0 - 8.0 * a, 21.5 + 3.5 * a)
    p["hand_r"] = (3.6 + 4.0 * a, 10.0 - 8.0 * a, 23.5 + 2.5 * a)
    p["foot_l"] = (-2.8 - 0.4 * a, 0.4 - 2.4 * a, 0.0)
    p["foot_r"] = (2.6 + 0.4 * a, -0.6 + 1.6 * a, 1.2 * a)
    p["mouth_open"] = a > 0.5
    return p


def zpose_stagger(f, n, weapon=None):
    return _reel(1.0 if f == 0 else 0.55)


def _fall(p, f, splay_hands):
    """Six frames of going over backward and lying still."""
    pitch = (18, 40, 66, 86, 90, 90)[min(f, 5)]
    p["pitch"] = math.radians(pitch)
    t = min(f, 4) / 4.0
    hl, hr = p["hand_l"], p["hand_r"]
    p["hand_l"] = lerp3(hl, splay_hands[0], t)
    p["hand_r"] = lerp3(hr, splay_hands[1], t)
    fl, fr = p["foot_l"], p["foot_r"]
    p["foot_l"] = lerp3(fl, (-3.6, 0.0, 0.0), t)
    p["foot_r"] = lerp3(fr, (3.4, 0.0, 0.0), t)
    p["pool"] = (0.0, 0.0, 0.0, 0.25, 0.65, 1.0)[min(f, 5)]
    return p


def zpose_death(f, n, weapon=None):
    p = _reel(1.0)
    p["mouth_open"] = True
    return _fall(p, f, ((-9.0, -1.0, 21.0), (8.0, -1.5, 26.0)))


def pose_death(f, n, weapon=None):
    p = pose_hurt(0, 1)
    p["head_yaw"] = 0.0
    return _fall(p, f, ((-8.5, -1.0, 20.0), (8.5, -1.5, 23.0)))


# ---------------------------------------------------------------------------
# Drawing a posed figure
# ---------------------------------------------------------------------------

def draw(look, pose, view):
    """Render one frame of a posed character. Returns a 64x64 RGBA image."""
    F = VIEWS[view]
    pitch = pose["pitch"]
    fr = Frame(F, pitch=pitch, pivot_y=-1.5, lift=2.2 * math.sin(pitch))
    zombie = look["zombie"]
    skin, top, legs, shoes = look["skin"], look["top"], look["legs"], look["shoes"]
    tw = pose["twist"]

    hip, chest, head = pose["hip"], pose["chest"], pose["head"]
    sh_l = add(chest, rot_z((-3.9, 0.0, 0.4), tw))
    sh_r = add(chest, rot_z((3.9, 0.0, 0.4), tw))
    hp_l = add(hip, (-2.2, 0.0, 0.0))
    hp_r = add(hip, (2.2, 0.0, 0.0))

    # Which side is nearer the viewer decides the group, for contact shading.
    def leg(side_hip, foot, group):
        ankle = add(foot, (0.0, 0.0, 1.6))
        knee, ankle = ik(side_hip, ankle, 6.5, 6.5, (0.0, 1.0, 0.15))
        fr.capsule(side_hip, knee, 2.0, legs, group, r_end=1.7)
        fr.capsule(knee, ankle, 1.7, legs, group, r_end=1.5)
        toe = add(ankle, (0.0, 2.4, -0.5))
        fr.capsule(add(ankle, (0.0, -0.3, -0.4)), toe, 1.45, shoes, group)

    lx = fr.project(hp_l)[2]
    rx = fr.project(hp_r)[2]
    leg(hp_l, pose["foot_l"], G_LEG_NEAR if lx >= rx else G_LEG_FAR)
    leg(hp_r, pose["foot_r"], G_LEG_NEAR if rx > lx else G_LEG_FAR)

    # Torso: two flanks and a spine, so it is wide from the front and slim side-on.
    belt = add(hip, (0.0, 0.0, 0.6))
    fr.capsule(hp_l, hp_r, 2.3, legs, G_TORSO)
    fr.capsule(sh_l, add(belt, (-1.9, 0.0, 0.6)), 2.5, top, G_TORSO, r_end=2.3)
    fr.capsule(sh_r, add(belt, (1.9, 0.0, 0.6)), 2.5, top, G_TORSO, r_end=2.3)
    fr.capsule(add(chest, (0.0, 0.0, 0.2)), add(belt, (0.0, 0.0, 0.8)), 3.1, top, G_TORSO, r_end=2.8)

    detail = look["detail"]
    front = rot_z((0.0, 3.0, 0.0), tw)
    if zombie:
        # Blood down the front, a wound in the back, a ragged hem.
        fr.ball(add(chest, add(front, (0.8, 0.2, -1.2))), 1.5, BLOOD, G_TORSO, flat=False)
        fr.ball(add(chest, add(front, (1.4, 0.1, -4.0))), 1.0, (BLOOD[1], BLOOD[1], BLOOD[2]), G_TORSO)
        fr.ball(add(chest, rot_z((-1.2, -3.0, -3.0), tw)), 1.3, (BLOOD[1], BLOOD[2], BLOOD[2]), G_TORSO)
        fr.ball(add(belt, rot_z((-2.4, 2.2, 1.4), tw)), 1.0, skin, G_TORSO)
    else:
        # Collar, zip and a belt line.
        fr.ball(add(chest, (0.0, 0.6, 2.2)), 1.6, skin, G_TORSO)
        if not look.get("pack_on"):
            fr.capsule(add(chest, add(front, (0.0, 0.25, 0.6))), add(belt, add(front, (0.0, -0.1, 1.6))),
                       0.55, (detail, detail, detail), G_TORSO, flat=True)

    if look.get("pack_on"):
        pack = look["pack"]
        back = rot_z((0.0, -3.7, 0.0), tw)
        fr.capsule(add(chest, add(back, (0.0, 0.0, -0.6))), add(chest, add(back, (0.0, -0.2, -6.4))),
                   3.3, pack, G_PACK, r_end=3.0)
        # The flap and the straps over the shoulders.
        fr.capsule(add(chest, add(back, (-2.2, -0.9, -3.6))), add(chest, add(back, (2.2, -0.9, -3.6))),
                   0.6, (pack[2], pack[2], pack[2]), G_PACK, flat=True)
        for sx in (-2.3, 2.3):
            fr.capsule(add(chest, rot_z((sx, 2.75, 1.0), tw)), add(chest, rot_z((sx, 2.9, -5.5), tw)),
                       0.55, (pack[1], pack[2], pack[2]), G_TORSO, flat=True)

    fr.head(head, look, G_HEAD, yaw=pose["head_yaw"], mouth_open=pose["mouth_open"], nod=pose["nod"])

    def arm(shoulder, hand, group, side):
        elbow, hand = ik(shoulder, hand, 5.4, 5.4, (0.5 * side, -1.0, -0.45))
        if zombie and look["sleeves"] == "short":
            fr.capsule(shoulder, lerp3(shoulder, elbow, 0.6), 1.6, top, group)
            fr.capsule(lerp3(shoulder, elbow, 0.6), elbow, 1.4, skin, group)
            fr.capsule(elbow, hand, 1.35, skin, group, r_end=1.2)
        elif zombie:
            fr.capsule(shoulder, elbow, 1.6, top, group, r_end=1.45)
            fr.capsule(elbow, lerp3(elbow, hand, 0.45), 1.4, top, group)
            fr.capsule(lerp3(elbow, hand, 0.45), hand, 1.3, skin, group, r_end=1.2)
        else:
            fr.capsule(shoulder, elbow, 1.6, top, group, r_end=1.45)
            fr.capsule(elbow, hand, 1.4, top, group, r_end=1.3)
        fr.ball(hand, 1.45, skin, group)
        return hand

    arm(sh_l, pose["hand_l"], G_ARM_L, -1.0)
    hand_r = arm(sh_r, pose["hand_r"], G_ARM_R, 1.0)

    weapon = pose["weapon"]
    if weapon:
        d = norm(pose["weapon_dir"])
        if weapon == "club":
            fr.capsule(add(hand_r, mul(d, -1.6)), add(hand_r, mul(d, 10.5)), 0.8, WOOD, G_WEAPON, r_end=1.25)
        elif weapon == "blade":
            fr.capsule(add(hand_r, mul(d, -0.8)), add(hand_r, mul(d, 1.4)), 0.8, GUNMETAL, G_WEAPON)
            fr.capsule(add(hand_r, mul(d, 1.6)), add(hand_r, mul(d, 6.4)), 0.75, STEEL, G_WEAPON, r_end=0.5)
        elif weapon == "long":
            # A shotgun at the shoulder: the stock back into it, the barrels
            # well out past the hands.
            fr.capsule(add(hand_r, mul(d, -5.5)), add(hand_r, mul(d, -0.4)), 0.95, WOOD, G_WEAPON)
            fr.capsule(add(hand_r, mul(d, -0.4)), add(hand_r, mul(d, 9.5)), 0.7, GUNMETAL, G_WEAPON)
            if pose["flash"]:
                muzzle = add(hand_r, mul(d, 11.2))
                fr.ball(muzzle, 2.8, ((255, 236, 150),) * 3, G_WEAPON, flat=True)
                fr.ball(add(muzzle, mul(d, 2.4)), 1.6, ((255, 250, 220),) * 3, G_WEAPON, flat=True)
        elif weapon == "gun":
            fr.capsule(add(hand_r, (0.0, 0.0, -1.6)), add(hand_r, (0.0, 0.0, 0.6)), 0.8, GUNMETAL, G_WEAPON)
            fr.capsule(add(hand_r, (0.0, 0.0, 0.9)), add(add(hand_r, (0.0, 0.0, 0.9)), mul(d, 4.6)),
                       0.85, GUNMETAL, G_WEAPON)
            if pose["flash"]:
                muzzle = add(add(hand_r, (0.0, 0.0, 0.9)), mul(d, 6.4))
                fr.ball(muzzle, 2.3, ((255, 236, 150),) * 3, G_WEAPON, flat=True)
                fr.ball(add(muzzle, mul(d, 2.2)), 1.3, ((255, 250, 220),) * 3, G_WEAPON, flat=True)

    # Ground layer: the shadow, and the pool under the fallen.
    fall = math.sin(pitch)
    mid = fr.project(lerp3((0.0, 0.0, 0.0), (hip[0], hip[1], hip[2]), fall))
    foot_mid = fr.project((0.0, 0.0, 0.0))
    scx = lerp(foot_mid[0], mid[0], fall)
    scy = lerp(GROUND - 0.5, mid[1], fall)
    if pose["pool"] > 0.0:
        k = pose["pool"]
        cpx, cpy, _ = fr.project((chest[0], chest[1], chest[2] * 0.85))
        fr.ground_ellipse(cpx, cpy + 1.0, 4.0 + 9.0 * k, 2.0 + 3.6 * k, BLOOD[2] + (215,))
        fr.ground_ellipse(cpx - 1.0, cpy + 1.0, 2.0 + 6.5 * k, 1.0 + 2.4 * k, BLOOD[1] + (225,))
    else:
        fr.ground_ellipse(scx, scy, 9.0 + 3.0 * fall, 2.6 + 0.8 * fall, SHADOW_RGBA)
    return fr.finish()


# ---------------------------------------------------------------------------
# The animation tables the atlas is built from
# ---------------------------------------------------------------------------

# name -> (pose function, frame count)
PLAYER_MOVES = {
    "idle": (pose_idle, 2),
    "walk": (pose_walk, 8),
    "run": (pose_run, 6),
    "crouch": (pose_crouch, 1),
    "crouchwalk": (pose_crouchwalk, 6),
    "hurt": (pose_hurt, 1),
}
PLAYER_LOADOUTS = (("none", None), ("club", "club"), ("blade", "blade"), ("gun", "gun"))
# name -> (pose function, frame count, weapon)
PLAYER_ATTACKS = {
    "swing": (pose_swing, 6, "club"),
    "stab": (pose_stab, 5, "blade"),
    "punch": (pose_stab, 5, None),
    "shoot": (pose_shoot, 3, "gun"),
    "shootlong": (pose_shoot, 3, "long"),
}
ZOMBIE_MOVES = {
    "idle": (zpose_idle, 2),
    "walk": (zpose_walk, 8),
    "lunge": (zpose_lunge, 5),
    "stagger": (zpose_stagger, 2),
    "death": (zpose_death, 6),
}


def character_frames():
    """Yield (sprite name, [frames]) for every character animation."""
    for view in VIEW_ORDER:
        for load, weapon in PLAYER_LOADOUTS:
            for move, (fn, count) in PLAYER_MOVES.items():
                yield (f"player_{load}_{move}_{view}",
                       [draw(PLAYER_LOOK, fn(f, count, weapon), view) for f in range(count)])
        for move, (fn, count, weapon) in PLAYER_ATTACKS.items():
            yield (f"player_{move}_{view}",
                   [draw(PLAYER_LOOK, fn(f, count, weapon), view) for f in range(count)])
        yield (f"player_death_{view}",
               [draw(PLAYER_LOOK, pose_death(f, 6), view) for f in range(6)])
        for i, look in enumerate(ZOMBIE_LOOKS):
            for move, (fn, count) in ZOMBIE_MOVES.items():
                yield (f"zombie{i}_{move}_{view}",
                       [draw(look, fn(f, count), view) for f in range(count)])


# ---------------------------------------------------------------------------
# Preview: python tools/charrig.py out.png [scale]
# ---------------------------------------------------------------------------

def preview(path, scale=4):
    rows = []
    looks = [("player", PLAYER_LOOK)] + [(f"z{i}", l) for i, l in enumerate(ZOMBIE_LOOKS)]
    rows.append([draw(l, (zpose_idle if l["zombie"] else pose_idle)(0, 2), v) for _, l in looks for v in VIEW_ORDER][:16])
    for v in VIEW_ORDER:
        rows.append([draw(PLAYER_LOOK, pose_walk(f, 8), v) for f in range(8)] +
                    [draw(PLAYER_LOOK, pose_run(f, 6, "club"), v) for f in range(6)])
    for v in VIEW_ORDER:
        rows.append([draw(PLAYER_LOOK, pose_swing(f, 6), v) for f in range(6)] +
                    [draw(PLAYER_LOOK, pose_stab(f, 5), v) for f in range(5)] +
                    [draw(PLAYER_LOOK, pose_shoot(f, 3), v) for f in range(3)] +
                    [draw(PLAYER_LOOK, pose_crouchwalk(f, 6, "blade"), v) for f in (0, 2)])
    for v in VIEW_ORDER:
        rows.append([draw(ZOMBIE_LOOKS[0], zpose_walk(f, 8), v) for f in range(8)] +
                    [draw(ZOMBIE_LOOKS[1], zpose_lunge(f, 5), v) for f in range(5)] +
                    [draw(ZOMBIE_LOOKS[2], zpose_stagger(f, 2), v) for f in range(2)])
    for v in VIEW_ORDER:
        rows.append([draw(ZOMBIE_LOOKS[2], zpose_death(f, 6), v) for f in range(6)] +
                    [draw(PLAYER_LOOK, pose_death(f, 6), v) for f in range(6)] +
                    [draw(PLAYER_LOOK, pose_hurt(0, 1, "club"), v), draw(PLAYER_LOOK, pose_crouch(0, 1), v)])
    w = max(len(r) for r in rows) * SIZE
    sheet = Image.new("RGBA", (w, len(rows) * SIZE), (74, 98, 62, 255))
    for j, r in enumerate(rows):
        for i, frame in enumerate(r):
            sheet.alpha_composite(frame, (i * SIZE, j * SIZE))
    sheet = sheet.resize((sheet.width * scale, sheet.height * scale), Image.NEAREST)
    sheet.save(path)


if __name__ == "__main__":
    import sys
    preview(sys.argv[1] if len(sys.argv) > 1 else "charrig_preview.png",
            int(sys.argv[2]) if len(sys.argv) > 2 else 4)
