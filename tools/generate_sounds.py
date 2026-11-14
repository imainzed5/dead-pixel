"""Generate every sound in the game by synthesis (phase 12).

    python tools/generate_sounds.py

Writes 16-bit mono WAVs at 22050 Hz to assets/sounds/ and the sound table
assets/data/sounds.json (id, files, range, priority, volume, loop). Nothing is
recorded or downloaded: each sound is built here from noise, sines and
filters, with a fixed seed per sound, so running it twice gives the same
bytes.

Every one-shot gets 3 to 5 variants (pitch +-6 % and fresh noise); a dozen
that are heard through walls get a `_muffled` twin; the guns get a distant
tail. Loops (ambience, music, the body) are crossfaded at their ends so they
repeat without a seam.

`src/audio/SoundId.h` lists the same ids; `tests/test_sounds.cpp` keeps the
two in step.
"""

import array
import json
import math
import os
import random
import wave
import zlib

RATE = 22050
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOUND_DIR = os.path.join(ROOT, "assets", "sounds")
TABLE_PATH = os.path.join(ROOT, "assets", "data", "sounds.json")

TWO_PI = 2.0 * math.pi

# Audible range in tiles, by the noise tier a sound belongs to.
WHISPER, SOFT, MEDIUM, LOUD, EXPLOSIVE = 4, 9, 18, 32, 60


# ---------------------------------------------------------------------------
# Building blocks
# ---------------------------------------------------------------------------

def n_of(seconds):
    return max(1, int(seconds * RATE))


def silence(seconds):
    return [0.0] * n_of(seconds)


def noise(seconds, rng):
    r = rng.random
    return [r() * 2.0 - 1.0 for _ in range(n_of(seconds))]


def lowpass(x, a):
    """One-pole lowpass; a in (0, 1], smaller is darker."""
    out = [0.0] * len(x)
    y = 0.0
    for i, s in enumerate(x):
        y += a * (s - y)
        out[i] = y
    return out


def highpass(x, a):
    low = lowpass(x, a)
    return [s - l for s, l in zip(x, low)]


def bandpass(x, freq, q):
    """RBJ biquad band-pass (constant 0 dB peak)."""
    w = TWO_PI * freq / RATE
    alpha = math.sin(w) / (2.0 * q)
    b0, b2 = alpha, -alpha
    a0, a1, a2 = 1.0 + alpha, -2.0 * math.cos(w), 1.0 - alpha
    b0, b2, a1, a2 = b0 / a0, b2 / a0, a1 / a0, a2 / a0
    out = [0.0] * len(x)
    x1 = x2 = y1 = y2 = 0.0
    for i, s in enumerate(x):
        y = b0 * s + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1 = x1, s
        y2, y1 = y1, y
        out[i] = y
    return out


def gain(x, g):
    return [s * g for s in x]


def decay(x, tau, attack=0.002):
    """Exponential decay with time constant tau seconds after a short attack."""
    na = max(1, int(attack * RATE))
    k = math.exp(-1.0 / max(1.0, tau * RATE))
    out = [0.0] * len(x)
    e = 1.0
    for i, s in enumerate(x):
        if i < na:
            out[i] = s * (i / na)
        else:
            out[i] = s * e
            e *= k
    return out


def fade(x, fin=0.005, fout=0.02):
    n = len(x)
    a, b = max(1, int(fin * RATE)), max(1, int(fout * RATE))
    out = list(x)
    for i in range(min(a, n)):
        out[i] *= i / a
    for i in range(min(b, n)):
        out[n - 1 - i] *= i / b
    return out


def mix(*parts):
    n = max(len(p) for p in parts)
    out = [0.0] * n
    for p in parts:
        for i, s in enumerate(p):
            out[i] += s
    return out


def place(out, x, at_seconds, g=1.0):
    off = int(at_seconds * RATE)
    if off + len(x) > len(out):
        out.extend([0.0] * (off + len(x) - len(out)))
    for i, s in enumerate(x):
        out[off + i] += s * g
    return out


def normalize(x, peak=0.9):
    m = max((abs(s) for s in x), default=0.0)
    if m <= 1e-9:
        return list(x)
    g = peak / m
    return [s * g for s in x]


def resample(x, factor):
    """Play `factor` times faster (pitch up), by linear interpolation."""
    n = max(2, int(len(x) / factor))
    out = [0.0] * n
    last = len(x) - 1
    for i in range(n):
        p = i * factor
        j = int(p)
        if j >= last:
            out[i] = x[last]
        else:
            f = p - j
            out[i] = x[j] * (1.0 - f) + x[j + 1] * f
    return out


def sine(freq, seconds, amp=1.0, phase=0.0):
    w = TWO_PI * freq / RATE
    return [amp * math.sin(w * i + phase) for i in range(n_of(seconds))]


def sweep(f0, f1, seconds, amp=1.0, shape="sine", curve=1.0):
    n = n_of(seconds)
    out = [0.0] * n
    ph = 0.0
    for i in range(n):
        t = (i / n) ** curve
        f = f0 + (f1 - f0) * t
        ph += TWO_PI * f / RATE
        if shape == "sine":
            out[i] = amp * math.sin(ph)
        elif shape == "saw":
            out[i] = amp * (((ph / TWO_PI) % 1.0) * 2.0 - 1.0)
        else:
            out[i] = amp * (1.0 if math.sin(ph) >= 0.0 else -1.0)
    return out


def partials(freqs, seconds, taus, amps):
    out = [0.0] * n_of(seconds)
    for f, tau, a in zip(freqs, taus, amps):
        out = mix(out, decay(sine(f, seconds, a), tau, 0.001))
    return out


def burst(seconds, rng, lp=0.3, hp=0.0, tau=0.05, attack=0.002):
    x = noise(seconds, rng)
    if lp < 1.0:
        x = lowpass(x, lp)
    if hp > 0.0:
        x = highpass(x, hp)
    return decay(x, tau, attack)


def grains(seconds, rng, rate_per_s, length=0.004, lp=0.5, amp_lo=0.3):
    """Sparse clicks: crunch, crackle, gravel, rummage."""
    out = [0.0] * n_of(seconds)
    count = max(1, int(rate_per_s * seconds))
    for _ in range(count):
        at = rng.random() * seconds
        g = burst(length * (0.5 + rng.random()), rng, lp=lp, tau=length * 0.4)
        place(out, g, at, amp_lo + (1.0 - amp_lo) * rng.random())
    return out[:n_of(seconds)]


def voice(f0, f1, seconds, rng, formants=((500, 4), (1100, 5)), rough=0.3, vib=4.0, vib_depth=0.03, breath=0.2,
          curve=1.0):
    """A throat: a saw with a wobble, through two formants, with breath."""
    n = n_of(seconds)
    src = [0.0] * n
    ph = 0.0
    r = rng.random
    for i in range(n):
        t = (i / n) ** curve
        f = (f0 + (f1 - f0) * t) * (1.0 + vib_depth * math.sin(TWO_PI * vib * i / RATE)) * (1.0 + rough * 0.08 * (r() - 0.5))
        ph += TWO_PI * f / RATE
        src[i] = ((ph / TWO_PI) % 1.0) * 2.0 - 1.0
    shaped = mix(*[bandpass(src, fr, q) for fr, q in formants])
    air = gain(bandpass(noise(seconds, rng), formants[0][0], 1.2), breath)
    return mix(shaped, air)


def envelope_shape(x, attack, hold, release):
    n = len(x)
    a, r = max(1, int(attack * RATE)), max(1, int(release * RATE))
    out = list(x)
    for i in range(n):
        if i < a:
            out[i] *= i / a
        elif i > n - r:
            out[i] *= max(0.0, (n - i) / r)
    return out


def am(x, rate_hz, depth, phase=0.0):
    w = TWO_PI * rate_hz / RATE
    return [s * (1.0 - depth + depth * 0.5 * (1.0 + math.sin(w * i + phase))) for i, s in enumerate(x)]


def slow_wobble(n, rng, rate_hz, depth):
    """Smooth random amplitude between 1-depth and 1 (for wind and crowds)."""
    pts = max(4, int(n / RATE * rate_hz) + 2)
    vals = [1.0 - depth * rng.random() for _ in range(pts)]
    out = [0.0] * n
    step = n / (pts - 1)
    for i in range(n):
        p = i / step
        j = int(p)
        f = p - j
        f = f * f * (3 - 2 * f)
        out[i] = vals[j] * (1 - f) + vals[min(j + 1, pts - 1)] * f
    return out


def loopify(x, xfade_seconds=0.6):
    """Crossfade the tail into the head so the loop has no seam."""
    f = min(int(xfade_seconds * RATE), len(x) // 3)
    body = x[:len(x) - f]
    for i in range(f):
        t = i / f
        body[i] = body[i] * t + x[len(x) - f + i] * (1.0 - t)
    return body


def muffle(x):
    """Through a wall: the highs gone, the body left."""
    return normalize(lowpass(lowpass(x, 0.07), 0.12), 0.75)


def distant(x, rng):
    """A gun heard far off: the crack gone, a long low roll."""
    low = lowpass(lowpass(x, 0.05), 0.08)
    out = list(low) + [0.0] * n_of(1.2)
    for k in range(1, 7):
        place(out, gain(low, 0.55 ** k), 0.09 * k + 0.03 * rng.random())
    return normalize(fade(out, 0.03, 0.4), 0.7)


# ---------------------------------------------------------------------------
# Sound recipes. Each takes (rng) and returns samples; variants are made by
# the table below with fresh noise and a pitch shift.
# ---------------------------------------------------------------------------

# Footsteps: (lowpass, tone Hz or 0, tone amp, length s, grit per s, metal)
SURFACES = {
    "grass":   dict(lp=0.22, tone=0,   tone_amp=0.0, length=0.09, grit=120, ring=False),
    "dirt":    dict(lp=0.12, tone=85,  tone_amp=0.5, length=0.08, grit=30,  ring=False),
    "wood":    dict(lp=0.20, tone=150, tone_amp=0.7, length=0.10, grit=0,   ring=False),
    "tile":    dict(lp=0.60, tone=320, tone_amp=0.4, length=0.05, grit=0,   ring=False),
    "carpet":  dict(lp=0.05, tone=70,  tone_amp=0.3, length=0.07, grit=0,   ring=False),
    "asphalt": dict(lp=0.45, tone=110, tone_amp=0.3, length=0.06, grit=60,  ring=False),
    "gravel":  dict(lp=0.50, tone=0,   tone_amp=0.0, length=0.12, grit=320, ring=False),
    "metal":   dict(lp=0.40, tone=420, tone_amp=0.5, length=0.16, grit=0,   ring=True),
    "snow":    dict(lp=0.18, tone=0,   tone_amp=0.0, length=0.15, grit=260, ring=False),
    "mud":     dict(lp=0.10, tone=70,  tone_amp=0.6, length=0.13, grit=0,   ring=False),
}
STANCES = {"walk": (0.7, 1.0), "run": (1.0, 0.8), "crouch": (0.35, 1.25)}


def make_foot(surface, stance):
    s = SURFACES[surface]
    loud, stretch = STANCES[stance]

    def gen(rng):
        length = s["length"] * stretch
        lp = s["lp"] * (0.6 if stance == "crouch" else 1.0)
        x = burst(length, rng, lp=lp, tau=length * 0.35)
        if s["tone"]:
            x = mix(x, decay(sine(s["tone"] * (0.9 + 0.2 * rng.random()), length, s["tone_amp"]), length * 0.3))
        if s["grit"]:
            x = mix(x, gain(grains(length, rng, s["grit"], 0.003, 0.6), 0.6))
        if s["ring"]:
            x = mix(x, partials([980, 1650], length * 1.5, [0.05, 0.03], [0.25, 0.15]))
        if surface == "mud":
            x = mix(x, decay(sweep(140, 60, length, 0.4), length * 0.5))
        if stance == "run":
            x = mix(x, gain(burst(length * 0.6, rng, lp=0.5, tau=0.02), 0.4))
        return gain(normalize(fade(x, 0.002, 0.02), 0.9), loud)
    return gen


def g_breath_out(rng):
    x = bandpass(noise(1.1, rng), 700, 0.8)
    x = envelope_shape(x, 0.15, 0.4, 0.7)
    return normalize(lowpass(x, 0.3), 0.6)


def g_heartbeat(rng):
    lub = decay(sine(55, 0.12, 1.0), 0.04, 0.004)
    dub = decay(sine(48, 0.10, 0.7), 0.035, 0.004)
    out = silence(0.42)
    place(out, lub, 0.0)
    place(out, dub, 0.18)
    return normalize(lowpass(out, 0.2), 0.9)


def g_eat(rng):
    out = silence(0.5)
    for k in range(3):
        place(out, grains(0.08, rng, 300, 0.003, 0.5), 0.05 + k * 0.15)
    return normalize(out, 0.7)


def g_drink(rng):
    out = silence(0.7)
    for k in range(3):
        gulp = decay(sweep(260, 180, 0.09, 0.8), 0.04)
        place(out, mix(gulp, gain(burst(0.09, rng, lp=0.2, tau=0.03), 0.3)), 0.06 + k * 0.2)
    return normalize(out, 0.7)


def g_cough(rng):
    out = silence(0.9)
    for k in range(2 + int(rng.random() * 2)):
        c = voice(180, 140, 0.16, rng, formants=((600, 2), (1500, 3)), rough=1.0, breath=1.2)
        place(out, envelope_shape(c, 0.005, 0.05, 0.1), k * 0.24)
    return normalize(out, 0.8)


def g_grunt(big):
    def gen(rng):
        d = 0.35 if big else 0.2
        x = voice(150 if big else 190, 110, d, rng, formants=((450, 3), (1000, 4)), rough=0.8, breath=0.5)
        return normalize(envelope_shape(x, 0.01, 0.05, d * 0.6), 0.9 if big else 0.7)
    return gen


def g_scream(rng):
    x = voice(420, 520, 0.8, rng, formants=((800, 3), (1800, 4)), rough=0.6, vib=6, vib_depth=0.05, breath=0.4)
    return normalize(envelope_shape(x, 0.03, 0.4, 0.3), 0.9)


def g_door(kind):
    def gen(rng):
        if kind == "open":
            creak = gain(bandpass(sweep(300, 420, 0.35, 1.0, "saw"), 900, 6), 0.25)
            latch = decay(burst(0.04, rng, lp=0.7), 0.01)
            return normalize(mix(latch, place(silence(0.4), creak, 0.03)), 0.7)
        if kind == "close":
            thud = mix(decay(sine(95, 0.25, 1.0), 0.06), burst(0.15, rng, lp=0.3, tau=0.04))
            latch = decay(burst(0.03, rng, lp=0.8), 0.008)
            return normalize(place(thud, latch, 0.06, 0.6), 0.85)
        if kind == "locked":
            out = silence(0.45)
            for k in range(3):
                place(out, mix(decay(burst(0.05, rng, lp=0.8), 0.012), partials([1200, 2300], 0.06, [0.02, 0.01], [0.3, 0.15])), k * 0.12)
            return normalize(out, 0.7)
        # creak: a slow open while crouched
        x = bandpass(sweep(180, 330, 0.9, 1.0, "saw", 0.6), 700, 5)
        x = am(x, 9 + rng.random() * 4, 0.6)
        return normalize(envelope_shape(x, 0.1, 0.5, 0.25), 0.55)
    return gen


def g_furniture_open(kind):
    def gen(rng):
        if kind == "cupboard":
            x = mix(decay(sine(170, 0.2, 0.6), 0.05), bandpass(sweep(260, 360, 0.25, 0.4, "saw"), 800, 5))
        elif kind == "fridge":
            seal = gain(bandpass(burst(0.25, rng, lp=0.9, tau=0.12), 2200, 1.0), 0.6)
            x = mix(seal, gain(sine(118, 0.25), 0.1))
        elif kind == "drawer":
            x = mix(grains(0.3, rng, 160, 0.004, 0.5), decay(sine(140, 0.3, 0.4), 0.25))
            x = place(x, decay(burst(0.05, rng, lp=0.5), 0.015), 0.27)
        else:  # locker
            x = mix(partials([520, 1240, 2100], 0.5, [0.15, 0.08, 0.05], [0.5, 0.3, 0.2]), decay(burst(0.05, rng, lp=0.9), 0.01))
        return normalize(fade(x), 0.7)
    return gen


def g_rummage_loop(rng):
    out = silence(4.0)
    t = 0.0
    while t < 3.8:
        place(out, grains(0.15, rng, 200, 0.003, 0.4), t, 0.5)
        if rng.random() < 0.4:
            place(out, decay(sine(150 + 200 * rng.random(), 0.1, 0.4), 0.03), t + 0.05)
        t += 0.12 + rng.random() * 0.25
    return loopify(normalize(out, 0.6), 0.3)


def g_hammer(rng):
    out = silence(0.4)
    hit = mix(partials([620, 1450, 2900], 0.25, [0.06, 0.03, 0.015], [0.6, 0.3, 0.2]), burst(0.08, rng, lp=0.8, tau=0.015))
    wood = decay(sine(130, 0.25, 0.8), 0.05)
    place(out, mix(hit, wood), 0.0)
    return normalize(out, 0.95)


def g_pry(rng):
    squeal = bandpass(sweep(500, 900, 0.5, 1.0, "saw", 0.7), 1300, 8)
    crack = burst(0.15, rng, lp=0.7, tau=0.04)
    return normalize(mix(gain(squeal, 0.5), place(silence(0.6), crack, 0.42)), 0.85)


def g_split(rng):
    crack = mix(burst(0.25, rng, lp=0.8, tau=0.05), grains(0.25, rng, 200, 0.004, 0.8))
    return normalize(mix(crack, decay(sine(110, 0.3, 0.6), 0.08)), 0.9)


def g_drag(heavy):
    def gen(rng):
        d = 0.9 if heavy else 0.55
        x = grains(d, rng, 500 if heavy else 300, 0.004, 0.25 if heavy else 0.4, 0.5)
        x = mix(x, gain(lowpass(noise(d, rng), 0.05 if heavy else 0.1), 0.6))
        return normalize(envelope_shape(x, 0.05, d * 0.5, 0.2), 0.8 if heavy else 0.6)
    return gen


def g_furniture_break(rng):
    out = mix(burst(0.4, rng, lp=0.6, tau=0.08), grains(0.5, rng, 120, 0.006, 0.7))
    return normalize(mix(out, decay(sine(90, 0.4, 0.8), 0.1)), 0.95)


def g_window_break(rng):
    smash = burst(0.1, rng, lp=1.0, hp=0.2, tau=0.03)
    tinkle = mix(*[place(silence(0.9), partials([2500 + rng.random() * 3000], 0.15, [0.04], [0.4]), 0.05 + rng.random() * 0.7)
                   for _ in range(14)])
    return normalize(mix(smash, tinkle, gain(decay(sine(140, 0.2), 0.05), 0.5)), 0.95)


def g_glass_step(rng):
    x = mix(grains(0.12, rng, 400, 0.002, 0.9), partials([3100 + 800 * rng.random()], 0.12, [0.02], [0.3]))
    return normalize(x, 0.6)


def g_curtain(rng):
    x = bandpass(noise(0.5, rng), 1800, 0.7)
    return normalize(envelope_shape(am(x, 6, 0.6), 0.08, 0.2, 0.2), 0.4)


def g_can_alarm(rng):
    out = silence(1.0)
    for k in range(6):
        place(out, partials([900 + 400 * rng.random(), 2100 + 600 * rng.random()], 0.2, [0.05, 0.03], [0.5, 0.3]),
              k * 0.12 + 0.03 * rng.random())
    return normalize(out, 0.85)


def g_spike(rng):
    thunk = mix(decay(sine(120, 0.2, 1.0), 0.04), burst(0.1, rng, lp=0.4, tau=0.02))
    return normalize(mix(thunk, gain(voice(160, 120, 0.2, rng, rough=1.0), 0.3)), 0.8)


def g_match(rng):
    strike = gain(highpass(burst(0.12, rng, lp=1.0, tau=0.05), 0.3), 0.8)
    flare = gain(envelope_shape(lowpass(noise(0.5, rng), 0.25), 0.03, 0.2, 0.3), 0.5)
    return normalize(mix(strike, place(silence(0.6), flare, 0.08)), 0.6)


def g_fire_loop(large):
    def gen(rng):
        d = 6.0
        bed = gain(lowpass(noise(d, rng), 0.03 if large else 0.02), 1.0)
        bed = [s * w for s, w in zip(bed, slow_wobble(len(bed), rng, 1.5, 0.5))]
        crackle = grains(d, rng, 60 if large else 25, 0.003, 0.9, 0.2)
        roar = gain(lowpass(noise(d, rng), 0.008), 2.5) if large else silence(d)
        return loopify(normalize(mix(bed, gain(crackle, 0.7), roar), 0.7))
    return gen


def g_fire_ignite(rng):
    whump = mix(lowpass(burst(0.6, rng, lp=0.08, tau=0.2, attack=0.03), 0.3), decay(sine(60, 0.5, 0.6), 0.15))
    return normalize(mix(whump, grains(0.6, rng, 80, 0.003, 0.9)), 0.9)


def g_water(kind):
    def gen(rng):
        if kind == "pour":
            x = bandpass(noise(0.9, rng), 900, 0.6)
            x = mix(x, gain(grains(0.9, rng, 90, 0.006, 0.3), 0.6))
            return normalize(envelope_shape(x, 0.05, 0.6, 0.2), 0.55)
        if kind == "tap_run":
            x = bandpass(noise(1.4, rng), 1200, 0.5)
            return normalize(envelope_shape(x, 0.04, 1.0, 0.2), 0.5)
        if kind == "tap_sputter":
            out = silence(1.2)
            t = 0.0
            while t < 1.0:
                place(out, envelope_shape(bandpass(noise(0.12, rng), 1100, 0.6), 0.01, 0.05, 0.05), t)
                t += 0.15 + rng.random() * 0.2
            return normalize(mix(out, gain(bandpass(sweep(90, 70, 1.2, 1.0, "saw"), 200, 3), 0.15)), 0.5)
        if kind == "tap_dry":
            knock = mix(gain(bandpass(sweep(80, 60, 0.6, 1.0, "saw"), 180, 4), 0.6), decay(burst(0.05, rng, lp=0.5), 0.02))
            return normalize(envelope_shape(knock, 0.01, 0.3, 0.2), 0.5)
        # bottle fill: rising pitch as it fills
        out = silence(1.2)
        for k in range(16):
            f = 300 + k * 35
            place(out, decay(sine(f, 0.08, 0.5), 0.03), k * 0.07)
        return normalize(mix(out, gain(bandpass(noise(1.2, rng), 1000, 0.5), 0.3)), 0.55)
    return gen


def g_pack(opening):
    def gen(rng):
        zip_ = grains(0.3, rng, 600 if opening else 400, 0.0015, 0.9, 0.5)
        flap = gain(bandpass(noise(0.2, rng), 900, 0.8), 0.4)
        x = mix(zip_, place(silence(0.3), envelope_shape(flap, 0.02, 0.05, 0.1), 0.15 if opening else 0.0))
        return normalize(fade(x), 0.5)
    return gen


def g_item(heavy, drop):
    def gen(rng):
        if drop:
            thud = mix(decay(sine(110 if heavy else 260, 0.2, 0.8), 0.05 if heavy else 0.03),
                       burst(0.1, rng, lp=0.3 if heavy else 0.6, tau=0.02))
            return normalize(thud, 0.75 if heavy else 0.5)
        rustle = gain(bandpass(noise(0.18, rng), 1500 if not heavy else 900, 0.8), 0.5)
        knock = decay(sine(180 if heavy else 520, 0.1, 0.5), 0.02)
        return normalize(mix(envelope_shape(rustle, 0.01, 0.05, 0.08), knock), 0.6 if heavy else 0.45)
    return gen


def g_craft_done(rng):
    out = silence(0.5)
    place(out, mix(decay(sine(200, 0.2, 0.7), 0.05), burst(0.05, rng, lp=0.5, tau=0.01)), 0.0)
    place(out, mix(decay(sine(240, 0.2, 0.6), 0.05), burst(0.05, rng, lp=0.5, tau=0.01)), 0.18)
    return normalize(out, 0.6)


def g_bandage(rng):
    tear = gain(highpass(noise(0.35, rng), 0.25), 0.6)
    tear = [s * (0.6 + 0.4 * math.sin(i * 0.05)) for i, s in enumerate(tear)]
    return normalize(envelope_shape(tear, 0.01, 0.2, 0.1), 0.5)


def g_splint(rng):
    out = mix(g_bandage(rng), place(silence(0.6), decay(sine(150, 0.2, 0.8), 0.04), 0.4))
    return normalize(out, 0.6)


def g_stitches(rng):
    out = silence(0.8)
    for k in range(4):
        place(out, gain(highpass(burst(0.04, rng, lp=1.0, tau=0.01), 0.4), 0.5), k * 0.18)
    return normalize(out, 0.4)


def g_reload(rng):
    out = silence(0.6)
    place(out, partials([900, 2200], 0.1, [0.02, 0.01], [0.5, 0.3]), 0.0)
    place(out, mix(partials([700, 1600], 0.12, [0.03, 0.015], [0.6, 0.3]), burst(0.03, rng, lp=0.9, tau=0.008)), 0.35)
    return normalize(out, 0.7)


def g_dry_click(rng):
    return normalize(mix(partials([1500, 3200], 0.06, [0.01, 0.006], [0.6, 0.3]), burst(0.02, rng, lp=1.0, tau=0.004)), 0.5)


def g_jam(rng):
    out = mix(g_dry_click(rng), place(silence(0.3), grains(0.15, rng, 300, 0.002, 0.9), 0.08))
    return normalize(out, 0.6)


def g_swing(heavy):
    def gen(rng):
        d = 0.3 if heavy else 0.18
        x = noise(d, rng)
        n = len(x)
        out = [0.0] * n
        # a band that sweeps up and back as the blade passes
        y = 0.0
        for i in range(n):
            t = i / n
            a = 0.05 + 0.4 * math.sin(math.pi * t)
            y += a * (x[i] - y)
            out[i] = y * math.sin(math.pi * t)
        return normalize(out, 0.6 if heavy else 0.45)
    return gen


def g_hit(kind):
    def gen(rng):
        if kind == "blunt":
            x = mix(decay(sine(85, 0.22, 1.0), 0.05), burst(0.12, rng, lp=0.25, tau=0.03))
        elif kind == "blade":
            x = mix(burst(0.15, rng, lp=0.8, hp=0.1, tau=0.04), gain(decay(sine(140, 0.15, 0.5), 0.03), 0.6))
            x = mix(x, gain(lowpass(burst(0.2, rng, lp=0.2, tau=0.08), 0.3), 0.5))
        elif kind == "heavy":
            x = mix(decay(sine(60, 0.35, 1.0), 0.08), burst(0.25, rng, lp=0.4, tau=0.06), grains(0.2, rng, 150, 0.004, 0.6))
        elif kind == "pan":  # the frying pan: a ring
            x = mix(decay(sine(90, 0.2, 0.7), 0.04), partials([480, 1130, 2050], 0.9, [0.3, 0.2, 0.1], [0.5, 0.35, 0.2]))
        else:  # wall
            x = mix(partials([300, 760], 0.15, [0.03, 0.015], [0.5, 0.3]), burst(0.08, rng, lp=0.9, tau=0.015))
        return normalize(fade(x), 0.95)
    return gen


def g_stick_pull(rng):
    wet = lowpass(burst(0.25, rng, lp=0.15, tau=0.08, attack=0.02), 0.5)
    return normalize(mix(wet, gain(decay(sweep(200, 120, 0.25), 0.07), 0.4)), 0.6)


def g_shove(rng):
    x = mix(decay(sine(100, 0.18, 0.8), 0.04), gain(bandpass(burst(0.18, rng, lp=1.0, tau=0.05), 600, 0.7), 0.6))
    return normalize(x, 0.7)


def g_stomp(rng):
    x = mix(decay(sine(55, 0.3, 1.0), 0.07), burst(0.2, rng, lp=0.2, tau=0.05), grains(0.15, rng, 200, 0.004, 0.5))
    return normalize(x, 0.9)


def g_weapon_break(rng):
    x = mix(burst(0.3, rng, lp=0.9, tau=0.06), grains(0.4, rng, 150, 0.005, 0.9),
            partials([700 + 300 * rng.random()], 0.3, [0.05], [0.4]))
    return normalize(x, 0.85)


def g_gun(shotgun):
    def gen(rng):
        d = 0.7 if shotgun else 0.5
        crack = burst(0.05, rng, lp=1.0, tau=0.012, attack=0.0005)
        body = burst(d, rng, lp=0.12 if shotgun else 0.2, tau=0.12 if shotgun else 0.07, attack=0.001)
        boom = decay(sine(50 if shotgun else 70, d, 1.0), 0.09 if shotgun else 0.05)
        tail = gain(lowpass(burst(d, rng, lp=0.05, tau=0.25), 0.3), 2.0)
        return normalize(mix(gain(crack, 0.8), body, boom, tail), 0.98)
    return gen


def g_casing(rng):
    out = silence(0.5)
    for k in range(3):
        place(out, partials([3200 + 600 * rng.random(), 5100], 0.08, [0.02, 0.01], [0.5, 0.25]), k * 0.09 + 0.02 * rng.random(),
              1.0 - 0.3 * k)
    return normalize(out, 0.4)


def g_throw(rng):
    return g_swing(False)(rng)


def g_rock_land(rng):
    x = mix(decay(sine(140, 0.2, 0.7), 0.03), burst(0.12, rng, lp=0.4, tau=0.02), grains(0.15, rng, 100, 0.004, 0.5))
    return normalize(x, 0.8)


def g_bottle_smash(rng):
    return g_window_break(rng)


def g_clock_ring(rng):
    out = silence(1.5)
    t = 0.0
    while t < 1.4:
        place(out, mix(sine(1900, 0.03, 0.6), sine(2850, 0.03, 0.3)), t)
        t += 0.045
    return normalize(out, 0.85)


def g_flare(rng):
    x = gain(highpass(noise(1.5, rng), 0.3), 1.0)
    x = [s * w for s, w in zip(x, slow_wobble(len(x), rng, 8, 0.4))]
    return normalize(envelope_shape(x, 0.04, 1.2, 0.2), 0.5)


# --- The dead ---------------------------------------------------------------

def g_groan(kind):
    def gen(rng):
        if kind == "idle":
            d = 1.1 + 0.4 * rng.random()
            x = voice(70 + 15 * rng.random(), 58, d, rng, formants=((380, 3), (850, 4)), rough=0.8, vib=3, breath=0.5)
            return normalize(envelope_shape(x, 0.25, d * 0.4, d * 0.4), 0.55)
        if kind == "alert":
            x = voice(90, 130, 0.7, rng, formants=((450, 3), (1000, 4)), rough=0.9, vib=5, breath=0.4, curve=0.5)
            return normalize(envelope_shape(x, 0.05, 0.3, 0.3), 0.75)
        x = voice(100, 80, 0.6, rng, formants=((500, 3), (1100, 4)), rough=1.0, vib=7, vib_depth=0.06, breath=0.6)
        return normalize(envelope_shape(x, 0.03, 0.3, 0.2), 0.8)
    return gen


def g_runner_shriek(rng):
    x = voice(320, 620, 0.55, rng, formants=((900, 3), (2200, 4)), rough=1.0, vib=11, vib_depth=0.06, breath=0.8, curve=0.4)
    return normalize(envelope_shape(x, 0.01, 0.3, 0.2), 0.95)


def g_runner_breath(rng):
    out = silence(0.9)
    for k in range(3):
        b = envelope_shape(bandpass(noise(0.16, rng), 900 if k % 2 == 0 else 700, 1.2), 0.02, 0.06, 0.07)
        place(out, b, k * 0.28)
    return normalize(out, 0.5)


def g_brute_growl(rng):
    x = voice(45, 38, 1.3, rng, formants=((260, 2.5), (600, 3)), rough=1.2, vib=2.5, breath=0.4)
    x = mix(x, gain(lowpass(noise(1.3, rng), 0.02), 0.8))
    return normalize(envelope_shape(x, 0.2, 0.6, 0.5), 0.9)


def g_brute_step(rng):
    x = mix(decay(sine(42, 0.4, 1.0), 0.1), lowpass(burst(0.3, rng, lp=0.1, tau=0.06), 0.4))
    return normalize(x, 0.9)


def g_crawler(kind):
    def gen(rng):
        if kind == "scrape":
            x = mix(grains(0.6, rng, 250, 0.004, 0.35, 0.4), gain(lowpass(noise(0.6, rng), 0.06), 0.8))
            return normalize(envelope_shape(x, 0.1, 0.3, 0.2), 0.55)
        wet = lowpass(noise(0.8, rng), 0.05)
        wet = [s * w for s, w in zip(wet, slow_wobble(len(wet), rng, 6, 0.8))]
        return normalize(envelope_shape(mix(wet, gain(grains(0.8, rng, 60, 0.006, 0.2), 0.5)), 0.1, 0.4, 0.3), 0.55)
    return gen


def g_screamer(rng):
    d = 1.3
    a = voice(560, 760, d, rng, formants=((1000, 4), (2600, 5)), rough=0.8, vib=7, vib_depth=0.07, breath=0.5)
    b = voice(380, 450, d, rng, formants=((800, 3), (1900, 4)), rough=1.0, vib=5, vib_depth=0.05, breath=0.3)
    return normalize(envelope_shape(mix(a, gain(b, 0.6)), 0.06, 0.8, 0.4), 0.98)


def g_lunge(rng):
    x = voice(160, 260, 0.35, rng, formants=((700, 3), (1600, 4)), rough=1.3, vib=13, vib_depth=0.08, breath=0.9, curve=0.5)
    return normalize(envelope_shape(x, 0.01, 0.15, 0.15), 0.9)


def g_grab(rng):
    x = mix(gain(bandpass(burst(0.2, rng, lp=1.0, tau=0.06), 800, 0.7), 0.7), g_groan("chase")(rng)[:n_of(0.35)])
    return normalize(x, 0.85)


def g_bite(rng):
    crunch = grains(0.2, rng, 400, 0.003, 0.7)
    wet = lowpass(burst(0.25, rng, lp=0.2, tau=0.07), 0.5)
    return normalize(mix(crunch, wet, gain(decay(sine(110, 0.2), 0.05), 0.5)), 0.9)


def g_body_fall(rng):
    x = mix(decay(sine(65, 0.4, 1.0), 0.09), burst(0.3, rng, lp=0.15, tau=0.07), place(silence(0.4), burst(0.1, rng, lp=0.2, tau=0.03), 0.15, 0.5))
    return normalize(x, 0.85)


def g_dead_door(rng):
    x = mix(decay(sine(75, 0.45, 1.0), 0.09), burst(0.35, rng, lp=0.3, tau=0.07), partials([210, 430], 0.4, [0.08, 0.05], [0.4, 0.2]))
    return normalize(x, 0.98)


def g_board_crack(rng):
    return normalize(mix(g_split(rng), gain(g_dead_door(rng), 0.5)), 0.95)


def g_window_bang(rng):
    x = mix(decay(sine(110, 0.3, 0.8), 0.06), burst(0.2, rng, lp=0.5, tau=0.04), partials([1800, 2900], 0.25, [0.04, 0.02], [0.25, 0.15]))
    return normalize(x, 0.9)


def g_crowd_loop(rng):
    d = 8.0
    out = silence(d)
    for _ in range(26):
        g = g_groan("idle" if rng.random() < 0.6 else "chase")(rng)
        place(out, g, rng.random() * (d - 1.0), 0.3 + 0.5 * rng.random())
    bed = gain(lowpass(noise(d, rng), 0.02), 2.0)
    return loopify(normalize(lowpass(mix(out, bed), 0.4), 0.7), 1.0)


# --- People -----------------------------------------------------------------

def g_hey(rng):
    x = voice(170 + 40 * rng.random(), 230, 0.35, rng, formants=((650, 4), (1700, 5)), rough=0.2, vib=5, breath=0.3)
    return normalize(envelope_shape(x, 0.02, 0.15, 0.15), 0.75)


def g_person_cough(rng):
    return g_cough(rng)


def g_door_slam(rng):
    x = mix(decay(sine(85, 0.35, 1.0), 0.07), burst(0.25, rng, lp=0.5, tau=0.05), partials([260, 540], 0.3, [0.06, 0.04], [0.4, 0.2]))
    return normalize(x, 0.95)


# --- World ------------------------------------------------------------------

def g_thunder(near):
    def gen(rng):
        d = 3.5 if near else 4.5
        x = lowpass(noise(d, rng), 0.03 if near else 0.012)
        x = [s * w for s, w in zip(x, slow_wobble(len(x), rng, 5 if near else 2, 0.7))]
        x = envelope_shape(x, 0.01 if near else 0.4, d * 0.4, d * 0.6)
        if near:
            x = mix(x, gain(burst(0.15, rng, lp=0.8, tau=0.05), 0.4))
        return normalize(x, 0.95 if near else 0.7)
    return gen


def g_dog(rng):
    out = silence(1.6)
    for k in range(2 + int(rng.random() * 2)):
        bark = voice(380, 300, 0.15, rng, formants=((700, 3), (1500, 3)), rough=1.0, breath=0.6)
        place(out, envelope_shape(bark, 0.005, 0.05, 0.08), k * 0.4)
    return normalize(lowpass(out, 0.25), 0.6)


def g_power_down(rng):
    hum = mix(sweep(60, 25, 1.5, 0.6), sweep(120, 50, 1.5, 0.3), sweep(180, 70, 1.5, 0.15))
    return normalize(envelope_shape(hum, 0.01, 0.6, 0.9), 0.6)


def g_engine_loop(drive):
    def gen(rng):
        d = 4.0
        n = n_of(d)
        base = 46.0 if drive else 28.0
        out = [0.0] * n
        ph = 0.0
        jitter = slow_wobble(n, rng, 6, 0.08)
        for i in range(n):
            f = base * (2.0 - jitter[i])
            ph += TWO_PI * f / RATE
            out[i] = 0.6 * math.sin(ph) + 0.3 * math.sin(2 * ph) + 0.2 * (1.0 if math.sin(ph * 0.5) > 0.7 else 0.0)
        out = mix(lowpass(out, 0.3), gain(lowpass(noise(d, rng), 0.05), 0.5 if drive else 0.3))
        return loopify(normalize(out, 0.7), 0.3)
    return gen


def g_crank(fail):
    def gen(rng):
        d = 1.4
        out = silence(d)
        t = 0.0
        while t < d - 0.1:
            place(out, mix(decay(sine(70, 0.08, 0.8), 0.03), burst(0.06, rng, lp=0.3, tau=0.02)), t)
            t += 0.11 + 0.02 * rng.random()
        if not fail:
            catch = gain(g_engine_loop(False)(rng)[:n_of(0.8)], 0.8)
            place(out, envelope_shape(catch, 0.05, 0.4, 0.3), d - 0.6)
        return normalize(out, 0.85)
    return gen


def g_car_door(rng):
    return normalize(mix(g_door("close")(rng), partials([400, 950], 0.2, [0.04, 0.02], [0.3, 0.2])), 0.85)


def g_alarm(car):
    def gen(rng):
        d = 1.2
        n = n_of(d)
        out = [0.0] * n
        ph = 0.0
        for i in range(n):
            t = i / RATE
            if car:
                f = 900 + 500 * (1.0 if int(t * 4) % 2 == 0 else 0.0)
            else:
                f = 1100 + 400 * math.sin(TWO_PI * 3 * t)
            ph += TWO_PI * f / RATE
            out[i] = 0.6 * (1.0 if math.sin(ph) > 0 else -1.0) * 0.5 + 0.4 * math.sin(ph)
        return loopify(normalize(lowpass(out, 0.6), 0.75), 0.05)
    return gen


def g_car_impact(rng):
    x = mix(decay(sine(70, 0.5, 1.0), 0.1), burst(0.4, rng, lp=0.6, tau=0.1), partials([380, 920, 1700], 0.6, [0.15, 0.1, 0.06], [0.5, 0.3, 0.2]))
    return normalize(mix(x, grains(0.5, rng, 100, 0.004, 0.9)), 0.98)


def g_bicycle(rng):
    out = silence(0.8)
    for k in range(10):
        place(out, partials([2600 + 200 * rng.random()], 0.02, [0.004], [0.4]), k * 0.075)
    return normalize(out, 0.35)


def g_gate(opened):
    def gen(rng):
        d = 2.5 if opened else 1.6
        grind = grains(d, rng, 500, 0.004, 0.4, 0.5)
        groan = bandpass(sweep(70, 95, d, 1.0, "saw"), 220, 3)
        x = mix(grind, gain(groan, 0.7), gain(lowpass(noise(d, rng), 0.03), 0.8))
        if opened:
            x = mix(x, place(silence(d), mix(decay(sine(60, 0.5, 1.0), 0.12), burst(0.3, rng, lp=0.3, tau=0.06)), d - 0.5))
        return normalize(envelope_shape(x, 0.1, d * 0.5, 0.4), 0.95)
    return gen


def g_generator_loop(rng):
    d = 3.0
    n = n_of(d)
    out = [0.0] * n
    for i in range(n):
        t = i / RATE
        chug = 1.0 if (t * 15.0) % 1.0 < 0.35 else 0.3
        out[i] = chug * (0.5 * math.sin(TWO_PI * 60 * t) + 0.3 * math.sin(TWO_PI * 120 * t))
    out = mix(out, gain(grains(d, rng, 120, 0.003, 0.7), 0.4), gain(lowpass(noise(d, rng), 0.08), 0.4))
    return loopify(normalize(out, 0.8), 0.2)


# --- Ambience loops ---------------------------------------------------------

AMB = 8.0


def g_wind(kind):
    def gen(rng):
        x = noise(AMB, rng)
        if kind == "trees":
            x = bandpass(x, 1800, 0.6)
            w = slow_wobble(len(x), rng, 2.5, 0.85)
        elif kind == "gust":
            x = lowpass(x, 0.04)
            w = slow_wobble(len(x), rng, 0.9, 0.95)
        else:
            x = lowpass(lowpass(x, 0.03), 0.2)
            w = slow_wobble(len(x), rng, 0.6, 0.6)
        return loopify(normalize([s * g for s, g in zip(x, w)], 0.7), 1.0)
    return gen


def g_rain(kind):
    def gen(rng):
        if kind == "roof":
            bed = lowpass(noise(AMB, rng), 0.06)
            taps = grains(AMB, rng, 140, 0.006, 0.25, 0.2)
            return loopify(normalize(mix(bed, gain(taps, 0.8)), 0.6), 0.6)
        heavy = kind == "heavy"
        bed = highpass(lowpass(noise(AMB, rng), 0.5 if heavy else 0.35), 0.05)
        drops = grains(AMB, rng, 300 if heavy else 80, 0.003, 0.7, 0.2)
        return loopify(normalize(mix(gain(bed, 1.0 if heavy else 0.6), drops), 0.7), 0.6)
    return gen


def g_snow(blizzard):
    def gen(rng):
        if blizzard:
            x = lowpass(noise(AMB, rng), 0.04)
            x = [s * g for s, g in zip(x, slow_wobble(len(x), rng, 0.9, 0.9))]
            hiss = gain(highpass(noise(AMB, rng), 0.3), 0.15)
            return loopify(normalize(mix(x, hiss), 0.8), 1.0)
        x = lowpass(noise(AMB, rng), 0.015)
        return loopify(normalize(x, 0.3), 1.0)
    return gen


def g_fog_drip(rng):
    out = silence(AMB)
    t = 0.2
    while t < AMB - 0.3:
        f = 900 + 1200 * rng.random()
        place(out, decay(sweep(f, f * 1.4, 0.08, 0.5), 0.02), t)
        t += 0.3 + rng.random() * 1.1
    return loopify(normalize(mix(out, gain(lowpass(noise(AMB, rng), 0.01), 0.3)), 0.4), 0.3)


def chirp(rng):
    f = 2600 + 1800 * rng.random()
    notes = []
    for k in range(2 + int(rng.random() * 4)):
        notes.append(place(silence(0.1 * (k + 1)), envelope_shape(sweep(f, f * (1.2 + 0.3 * rng.random()), 0.06, 0.4), 0.005, 0.02, 0.02), 0.08 * k))
    return mix(*notes)


def g_birds(rng):
    out = silence(AMB)
    t = 0.1
    while t < AMB - 0.6:
        place(out, chirp(rng), t, 0.4 + 0.6 * rng.random())
        t += 0.25 + rng.random() * 0.9
    return loopify(normalize(out, 0.5), 0.3)


def g_crows(rng):
    out = silence(AMB)
    t = 0.5
    while t < AMB - 1.0:
        for k in range(1 + int(rng.random() * 3)):
            caw = voice(520, 420, 0.22, rng, formants=((1000, 3), (2000, 3)), rough=1.4, vib=20, vib_depth=0.05, breath=0.8)
            place(out, envelope_shape(caw, 0.01, 0.1, 0.08), t + 0.3 * k)
        t += 1.5 + rng.random() * 2.0
    return loopify(normalize(lowpass(out, 0.6), 0.45), 0.3)


def g_insects(rng):
    n = n_of(AMB)
    out = [0.0] * n
    for c in range(3):
        f = 3800 + 900 * c + 300 * rng.random()
        rate = 14 + 5 * rng.random()
        w = TWO_PI * f / RATE
        pulse = TWO_PI * rate / RATE
        phase = rng.random() * 6.0
        for i in range(n):
            p = math.sin(pulse * i + phase)
            if p > 0.6:
                out[i] += 0.2 * math.sin(w * i) * (p - 0.6) * 2.5
    out = [s * g for s, g in zip(out, slow_wobble(n, rng, 0.5, 0.5))]
    return loopify(normalize(out, 0.35), 0.5)


def g_hum(rng):
    n = n_of(4.0)
    out = [0.30 * math.sin(TWO_PI * 50 * i / RATE) + 0.2 * math.sin(TWO_PI * 100 * i / RATE) + 0.08 * math.sin(TWO_PI * 150 * i / RATE)
           for i in range(n)]
    return normalize(mix(out, gain(lowpass(noise(4.0, rng), 0.02), 0.3)), 0.4)


def g_room_tone(rng):
    x = lowpass(lowpass(noise(AMB, rng), 0.01), 0.1)
    return loopify(normalize(x, 0.3), 1.0)


def g_tinnitus(rng):
    n = n_of(3.0)
    out = [0.3 * math.sin(TWO_PI * 4200 * i / RATE) + 0.05 * math.sin(TWO_PI * 6300 * i / RATE) for i in range(n)]
    return normalize(out, 0.35)


def g_breath_loop(rng):
    out = silence(2.4)
    for k in range(4):
        b = envelope_shape(bandpass(noise(0.3, rng), 650 if k % 2 == 0 else 900, 1.0), 0.05, 0.1, 0.12)
        place(out, b, k * 0.6)
    return loopify(normalize(out, 0.5), 0.1)


def g_flies(rng):
    n = n_of(3.0)
    out = [0.0] * n
    for c in range(3):
        f = 180 + 50 * c + 20 * rng.random()
        w = slow_wobble(n, rng, 4, 0.9)
        ph = 0.0
        for i in range(n):
            ph += TWO_PI * f * (0.95 + 0.1 * w[i]) / RATE
            out[i] += 0.3 * w[i] * (((ph / TWO_PI) % 1.0) * 2.0 - 1.0)
    return loopify(normalize(bandpass(out, 400, 1.0), 0.3), 0.3)


# --- Music ------------------------------------------------------------------

def note_hz(semitones_from_a3):
    return 220.0 * 2.0 ** (semitones_from_a3 / 12.0)


def pad(chords, seconds_per_chord, rng, brightness=0.04):
    """Slow sine-and-noise chords, each swelling in and out."""
    total = seconds_per_chord * len(chords)
    out = silence(total)
    for k, chord in enumerate(chords):
        seg = silence(seconds_per_chord + 4.0)
        for semis in chord:
            f = note_hz(semis)
            detune = 1.0 + 0.002 * (rng.random() - 0.5)
            tone = mix(sine(f * detune, seconds_per_chord + 4.0, 0.22), sine(f * 2.0 * detune, seconds_per_chord + 4.0, 0.05))
            seg = mix(seg, tone)
        air = gain(bandpass(noise(seconds_per_chord + 4.0, rng), note_hz(chord[0]) * 2.0, 2.0), brightness * 4)
        seg = envelope_shape(mix(seg, air), 3.5, seconds_per_chord, 4.0)
        place(out, seg, k * seconds_per_chord)
    # the end swells back into the start
    return loopify(normalize(out[:n_of(total + 3.0)], 0.5), 3.0)


def g_pad_a(rng):   # A minor, F, D minor (sus), E sus4
    return pad([[-12, -5, 0, 3], [-16, -9, -4, 0], [-19, -7, -2, 3], [-17, -10, -5, 0]], 16.0, rng)


def g_pad_b(rng):   # suspended, open fifths
    return pad([[-14, -7, 0, 5], [-12, -5, 2, 7], [-17, -10, -3, 2], [-14, -7, -2, 3]], 17.0, rng, 0.03)


def g_pad_c(rng):   # lower, darker
    return pad([[-24, -17, -12, -9], [-26, -19, -14, -10], [-21, -16, -12, -9], [-24, -19, -12, -7]], 18.0, rng, 0.02)


def g_pulse(rng):
    d = 8.0
    out = silence(d)
    beat = 60.0 / 72.0
    t = 0.0
    while t < d - 0.4:
        place(out, decay(sine(45, 0.5, 1.0), 0.12), t)
        place(out, gain(decay(sine(90, 0.3, 0.4), 0.05), 0.5), t + beat * 0.5)
        t += beat
    return loopify(normalize(lowpass(out, 0.2), 0.6), 0.2)


def g_radio(rng):
    """A thin tune through a small speaker: square-ish lead, band-limited."""
    melody = [0, 3, 7, 5, 3, 0, -2, 0, 3, 5, 7, 10, 7, 5, 3, 2]
    beat = 0.4
    out = silence(beat * len(melody) + 0.5)
    for k, semi in enumerate(melody):
        f = note_hz(semi + 12)
        tone = sweep(f, f, beat * 0.9, 0.5, "square")
        place(out, envelope_shape(tone, 0.01, beat * 0.5, beat * 0.3), k * beat)
        bass = sine(note_hz(-12 + (0 if k < 8 else -4)), beat * 0.9, 0.4)
        place(out, envelope_shape(bass, 0.01, beat * 0.4, beat * 0.4), k * beat)
    out = bandpass(out, 1200, 0.5)
    out = mix(out, gain(highpass(noise(len(out) / RATE, rng), 0.2), 0.05))
    return loopify(normalize(out, 0.5), 0.3)


def g_sting(kind):
    def gen(rng):
        if kind == "dusk":
            x = mix(sine(note_hz(-21), 4.0, 0.5), sine(note_hz(-9), 4.0, 0.15), sine(note_hz(-14), 4.0, 0.12))
            return normalize(envelope_shape(x, 0.6, 1.5, 2.5), 0.55)
        if kind == "caller":
            x = mix(sine(note_hz(-26), 2.5, 0.6), sine(note_hz(-14), 2.5, 0.1))
            return normalize(decay(x, 0.9, 0.05), 0.6)
        if kind == "dawn":
            out = silence(4.0)
            place(out, envelope_shape(mix(sine(note_hz(-9), 2.5, 0.4), sine(note_hz(-2), 2.5, 0.15)), 0.4, 1.0, 1.2), 0.0)
            place(out, envelope_shape(mix(sine(note_hz(-5), 2.5, 0.4), sine(note_hz(3), 2.5, 0.15)), 0.4, 1.0, 1.2), 1.2)
            return normalize(out, 0.5)
        if kind == "day":
            x = partials([note_hz(-21), note_hz(-9), note_hz(-2)], 3.0, [1.0, 0.5, 0.3], [0.6, 0.25, 0.1])
            return normalize(x, 0.6)
        # death: softer than before, a falling low note
        x = mix(sweep(note_hz(-14), note_hz(-21), 2.4, 0.5), sweep(note_hz(-26), note_hz(-33), 2.4, 0.3))
        return normalize(envelope_shape(x, 0.2, 1.0, 1.4), 0.5)
    return gen


def g_ui(kind):
    def gen(rng):
        f = {"move": 1500, "select": 1100, "back": 800, "error": 260, "tab": 1300}[kind]
        d = 0.05 if kind != "error" else 0.12
        x = mix(decay(sine(f * (0.97 + 0.06 * rng.random()), d, 0.6), d * 0.25), gain(burst(0.01, rng, lp=0.7, tau=0.003), 0.4))
        if kind == "error":
            x = mix(x, place(silence(0.2), decay(sine(220, 0.1, 0.5), 0.03), 0.08))
        return normalize(fade(x, 0.001, 0.01), 0.35)
    return gen


# ---------------------------------------------------------------------------
# The table: id -> (recipe, variants, range, priority, volume, flags)
#   flags: "loop", "muffled", "far", "music", "ambience"
# ---------------------------------------------------------------------------

def build_table():
    t = []

    def one(key, gen, variants, rng_tiles, priority, volume, *flags):
        t.append(dict(id=key, gen=gen, variants=variants, range=rng_tiles, priority=priority, volume=volume, flags=set(flags)))

    # Feet
    for surface in SURFACES:
        for stance in STANCES:
            rng_tiles = {"walk": SOFT, "run": MEDIUM, "crouch": WHISPER}[stance]
            one(f"foot_{surface}_{stance}", make_foot(surface, stance), 4, rng_tiles, 2, 0.32)
    # Body
    one("body_heartbeat", g_heartbeat, 3, WHISPER, 7, 0.7)
    one("body_eat", g_eat, 3, WHISPER, 5, 0.55)
    one("body_drink", g_drink, 3, WHISPER, 5, 0.55)
    one("body_cough", g_cough, 4, SOFT, 6, 0.6)
    one("body_breath_out", g_breath_out, 3, WHISPER, 6, 0.55)
    one("body_grunt_small", g_grunt(False), 4, WHISPER, 7, 0.55)
    one("body_grunt_big", g_grunt(True), 4, SOFT, 7, 0.7)
    one("body_scream", g_scream, 3, LOUD, 8, 0.85)
    one("body_climb", g_curtain, 3, SOFT, 4, 0.5)
    # Hands
    one("door_open", g_door("open"), 4, SOFT, 5, 0.55)
    one("door_close", g_door("close"), 4, SOFT, 5, 0.6)
    one("door_locked", g_door("locked"), 3, SOFT, 5, 0.55)
    one("door_creak", g_door("creak"), 3, WHISPER, 5, 0.4)
    one("open_cupboard", g_furniture_open("cupboard"), 3, WHISPER, 4, 0.45)
    one("open_fridge", g_furniture_open("fridge"), 3, WHISPER, 4, 0.45)
    one("open_drawer", g_furniture_open("drawer"), 3, WHISPER, 4, 0.45)
    one("open_locker", g_furniture_open("locker"), 3, SOFT, 4, 0.5)
    one("hammer", g_hammer, 5, LOUD, 6, 0.85, "muffled")
    one("board_pry", g_pry, 3, MEDIUM, 6, 0.7)
    one("plank_split", g_split, 4, LOUD, 6, 0.8)
    one("drag_light", g_drag(False), 3, MEDIUM, 4, 0.5)
    one("drag_heavy", g_drag(True), 3, MEDIUM, 4, 0.6)
    one("furniture_break", g_furniture_break, 4, LOUD, 6, 0.85)
    one("window_break", g_window_break, 4, LOUD, 7, 0.85)
    one("glass_step", g_glass_step, 4, SOFT, 3, 0.45)
    one("curtain", g_curtain, 3, WHISPER, 3, 0.4)
    one("can_alarm", g_can_alarm, 4, LOUD, 7, 0.8)
    one("spike_board", g_spike, 3, SOFT, 6, 0.65)
    one("match", g_match, 3, WHISPER, 4, 0.5)
    one("fire_ignite", g_fire_ignite, 3, EXPLOSIVE, 7, 0.85)
    one("water_pour", g_water("pour"), 3, WHISPER, 4, 0.45)
    one("tap_run", g_water("tap_run"), 3, SOFT, 4, 0.45)
    one("tap_sputter", g_water("tap_sputter"), 3, SOFT, 4, 0.45)
    one("tap_dry", g_water("tap_dry"), 3, WHISPER, 4, 0.4)
    one("bottle_fill", g_water("fill"), 3, WHISPER, 4, 0.45)
    one("pack_open", g_pack(True), 3, WHISPER, 6, 0.35)
    one("pack_close", g_pack(False), 3, WHISPER, 6, 0.35)
    one("pickup_light", g_item(False, False), 4, WHISPER, 5, 0.4)
    one("pickup_heavy", g_item(True, False), 4, WHISPER, 5, 0.45)
    one("drop_light", g_item(False, True), 4, WHISPER, 5, 0.4)
    one("drop_heavy", g_item(True, True), 4, SOFT, 5, 0.5)
    one("craft_done", g_craft_done, 3, SOFT, 6, 0.5)
    one("bandage", g_bandage, 3, WHISPER, 5, 0.45)
    one("splint", g_splint, 3, WHISPER, 5, 0.45)
    one("stitches", g_stitches, 3, WHISPER, 5, 0.4)
    one("reload", g_reload, 3, WHISPER, 6, 0.55)
    one("dry_click", g_dry_click, 3, WHISPER, 6, 0.5)
    one("jam", g_jam, 3, WHISPER, 6, 0.55)
    # Weapons
    one("swing_light", g_swing(False), 4, MEDIUM, 5, 0.45)
    one("swing_heavy", g_swing(True), 4, MEDIUM, 5, 0.55)
    one("hit_blunt", g_hit("blunt"), 5, MEDIUM, 6, 0.8)
    one("hit_blade", g_hit("blade"), 5, MEDIUM, 6, 0.75)
    one("hit_heavy", g_hit("heavy"), 5, MEDIUM, 6, 0.85)
    one("hit_pan", g_hit("pan"), 3, LOUD, 7, 0.85)
    one("hit_wall", g_hit("wall"), 4, MEDIUM, 5, 0.6)
    one("stick_pull", g_stick_pull, 3, SOFT, 5, 0.55)
    one("shove", g_shove, 4, SOFT, 6, 0.6)
    one("stomp", g_stomp, 4, MEDIUM, 6, 0.8)
    one("weapon_break", g_weapon_break, 3, MEDIUM, 7, 0.8)
    one("pistol", g_gun(False), 4, LOUD, 9, 0.95, "muffled", "far")
    one("shotgun", g_gun(True), 4, EXPLOSIVE, 9, 1.0, "muffled", "far")
    one("casing", g_casing, 4, WHISPER, 3, 0.35)
    one("throw_whoosh", g_throw, 3, SOFT, 4, 0.4)
    one("rock_land", g_rock_land, 4, MEDIUM, 5, 0.65)
    one("bottle_smash", g_bottle_smash, 4, LOUD, 6, 0.8)
    one("clock_ring", g_clock_ring, 3, LOUD, 7, 0.75)
    one("flare_hiss", g_flare, 3, MEDIUM, 5, 0.55)
    # The dead
    one("shambler_idle", g_groan("idle"), 5, SOFT, 4, 0.55, "muffled")
    one("shambler_alert", g_groan("alert"), 4, MEDIUM, 5, 0.65, "muffled")
    one("shambler_chase", g_groan("chase"), 5, MEDIUM, 5, 0.7, "muffled")
    one("runner_shriek", g_runner_shriek, 4, LOUD, 8, 0.85, "muffled")
    one("runner_breath", g_runner_breath, 3, SOFT, 5, 0.5)
    one("brute_growl", g_brute_growl, 4, MEDIUM, 7, 0.85, "muffled")
    one("brute_step", g_brute_step, 4, MEDIUM, 4, 0.7)
    one("crawler_scrape", g_crawler("scrape"), 4, SOFT, 5, 0.6)
    one("crawler_drag", g_crawler("drag"), 3, SOFT, 4, 0.5)
    one("screamer_scream", g_screamer, 3, LOUD, 9, 1.0, "muffled")
    one("dead_lunge", g_lunge, 4, MEDIUM, 8, 0.8)
    one("dead_grab", g_grab, 3, SOFT, 8, 0.8)
    one("dead_bite", g_bite, 4, SOFT, 8, 0.85)
    one("dead_fall", g_body_fall, 4, SOFT, 5, 0.65)
    one("dead_door_bang", g_dead_door, 5, LOUD, 7, 0.9, "muffled")
    one("dead_board_crack", g_board_crack, 3, LOUD, 7, 0.9, "muffled")
    one("dead_window_bang", g_window_bang, 4, LOUD, 7, 0.85, "muffled")
    # People
    one("person_cough", g_person_cough, 3, SOFT, 4, 0.5)
    one("person_hey", g_hey, 3, MEDIUM, 6, 0.65, "muffled")
    one("person_door_slam", g_door_slam, 3, MEDIUM, 5, 0.75)
    # World
    one("thunder_near", g_thunder(True), 3, EXPLOSIVE, 8, 0.95)
    one("thunder_far", g_thunder(False), 3, EXPLOSIVE, 6, 0.6)
    one("dog_far", g_dog, 3, EXPLOSIVE, 2, 0.35)
    one("power_down", g_power_down, 3, SOFT, 6, 0.5)
    one("car_crank", g_crank(False), 3, LOUD, 7, 0.8)
    one("car_crank_fail", g_crank(True), 3, LOUD, 7, 0.75)
    one("car_door", g_car_door, 3, MEDIUM, 5, 0.65)
    one("car_alarm", g_alarm(True), 3, LOUD, 8, 0.8)
    one("shop_alarm", g_alarm(False), 3, LOUD, 8, 0.8)
    one("car_impact", g_car_impact, 4, LOUD, 8, 0.9)
    one("bicycle_tick", g_bicycle, 3, WHISPER, 3, 0.4)
    one("gate_grind", g_gate(False), 3, LOUD, 7, 0.85)
    one("gate_open", g_gate(True), 3, EXPLOSIVE, 9, 0.95)
    # Stings and the interface (heard, not placed)
    one("sting_dusk", g_sting("dusk"), 3, 0, 8, 0.5, "music")
    one("sting_caller", g_sting("caller"), 3, 0, 8, 0.55, "music")
    one("sting_dawn", g_sting("dawn"), 3, 0, 8, 0.5, "music")
    one("day_card", g_sting("day"), 3, 0, 9, 0.5)
    one("death_sting", g_sting("death"), 3, 0, 9, 0.6)
    one("ui_move", g_ui("move"), 3, 0, 9, 0.3)
    one("ui_select", g_ui("select"), 3, 0, 9, 0.35)
    one("ui_back", g_ui("back"), 3, 0, 9, 0.3)
    one("ui_error", g_ui("error"), 3, 0, 9, 0.35)
    one("ui_tab", g_ui("tab"), 3, 0, 9, 0.3)

    # Loops (layers): one file each, volume set by the mixer.
    def loop(key, gen, volume, *flags):
        t.append(dict(id=key, gen=gen, variants=1, range=0, priority=0, volume=volume, flags=set(flags) | {"loop"}))

    loop("loop_wind", g_wind("plain"), 0.6, "ambience")
    loop("loop_wind_gust", g_wind("gust"), 0.6, "ambience")
    loop("loop_wind_trees", g_wind("trees"), 0.45, "ambience")
    loop("loop_rain_light", g_rain("light"), 0.55, "ambience")
    loop("loop_rain_heavy", g_rain("heavy"), 0.7, "ambience")
    loop("loop_rain_roof", g_rain("roof"), 0.6, "ambience")
    loop("loop_snow", g_snow(False), 0.5, "ambience")
    loop("loop_blizzard", g_snow(True), 0.7, "ambience")
    loop("loop_fog_drip", g_fog_drip, 0.45, "ambience")
    loop("loop_birds", g_birds, 0.45, "ambience")
    loop("loop_insects", g_insects, 0.4, "ambience")
    loop("loop_crows", g_crows, 0.45, "ambience")
    loop("loop_hum", g_hum, 0.35, "ambience")
    loop("loop_room", g_room_tone, 0.35, "ambience")
    loop("loop_crowd", g_crowd_loop, 0.8, "ambience")
    loop("loop_fire_small", g_fire_loop(False), 0.55, "ambience")
    loop("loop_fire_large", g_fire_loop(True), 0.75, "ambience")
    loop("loop_tinnitus", g_tinnitus, 0.3, "effects")
    loop("loop_breath", g_breath_loop, 0.5, "effects")
    loop("loop_rummage", g_rummage_loop, 0.5, "effects")
    loop("loop_flies", g_flies, 0.35, "effects")
    loop("loop_engine_idle", g_engine_loop(False), 0.6, "effects")
    loop("loop_engine_drive", g_engine_loop(True), 0.7, "effects")
    loop("loop_generator", g_generator_loop, 0.8, "effects")
    loop("loop_car_alarm", g_alarm(True), 0.8, "effects")
    loop("loop_radio", g_radio, 0.45, "music")
    loop("loop_pad_a", g_pad_a, 0.4, "music")
    loop("loop_pad_b", g_pad_b, 0.4, "music")
    loop("loop_pad_c", g_pad_c, 0.4, "music")
    loop("loop_pulse", g_pulse, 0.45, "music")
    return t


def trim(samples, floor=0.004):
    """Cut the silent tail a one-shot was built with (it would hold a voice)."""
    last = len(samples) - 1
    while last > 0 and abs(samples[last]) < floor:
        last -= 1
    return samples[:min(len(samples), last + n_of(0.01))]


def write_wav(path, samples):
    data = array.array("h", (int(max(-1.0, min(1.0, s)) * 32767) for s in samples))
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data.tobytes())


def main():
    os.makedirs(SOUND_DIR, exist_ok=True)
    table = build_table()
    keep = set()
    entries = []
    total_bytes = 0
    for row in table:
        key = row["id"]
        seed = zlib.crc32(key.encode("utf-8"))
        files, seconds, muffled, far = [], [], [], []
        for v in range(row["variants"]):
            rng = random.Random(seed + 7919 * v)
            samples = row["gen"](rng)
            if row["variants"] > 1:
                # pitch +-6 %, spread evenly over the variants
                factor = 0.94 + 0.12 * (v / (row["variants"] - 1))
                if abs(factor - 1.0) > 0.005:
                    samples = resample(samples, factor)
            if "loop" not in row["flags"]:
                samples = fade(trim(samples), 0.001, 0.01)
            name = f"{key}_{v + 1}.wav" if row["variants"] > 1 else f"{key}.wav"
            write_wav(os.path.join(SOUND_DIR, name), samples)
            keep.add(name)
            files.append(name)
            seconds.append(round(len(samples) / RATE, 3))
            total_bytes += 44 + 2 * len(samples)
            if "muffled" in row["flags"]:
                m = muffle(samples)
                mname = f"{key}_muffled_{v + 1}.wav"
                write_wav(os.path.join(SOUND_DIR, mname), m)
                keep.add(mname)
                muffled.append(mname)
                total_bytes += 44 + 2 * len(m)
            if "far" in row["flags"]:
                d = distant(samples, random.Random(seed + 31 * v))
                dname = f"{key}_far_{v + 1}.wav"
                write_wav(os.path.join(SOUND_DIR, dname), d)
                keep.add(dname)
                far.append(dname)
                total_bytes += 44 + 2 * len(d)
        category = "music" if "music" in row["flags"] else ("ambience" if "ambience" in row["flags"] else "effects")
        if key.startswith("ui_") or key in ("day_card", "death_sting"):
            category = "interface"
        entry = dict(id=key, files=files, seconds=seconds, range=row["range"], priority=row["priority"],
                     volume=row["volume"], loop="loop" in row["flags"], category=category)
        if muffled:
            entry["muffled"] = muffled
        if far:
            entry["far"] = far
        entries.append(entry)
        print(f"  {key}: {len(files)} file(s)")

    # Old files from before phase 12 go.
    for name in os.listdir(SOUND_DIR):
        if name.endswith(".wav") and name not in keep:
            os.remove(os.path.join(SOUND_DIR, name))

    with open(TABLE_PATH, "w", encoding="utf-8", newline="\n") as f:
        f.write("{\n  \"rate\": %d,\n  \"sounds\": [\n" % RATE)
        f.write(",\n".join("    " + json.dumps(e, separators=(", ", ": ")) for e in entries))
        f.write("\n  ]\n}\n")
    print(f"\n{len(entries)} sounds, {len(keep)} files, {total_bytes / 1e6:.1f} MB in {SOUND_DIR}")


if __name__ == "__main__":
    main()
