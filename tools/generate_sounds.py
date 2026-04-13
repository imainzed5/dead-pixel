"""Generate placeholder WAV sound effects using waveform synthesis."""

import math
import os
import struct
import random

SAMPLE_RATE = 44100
OUTPUT_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets", "sounds")


def write_wav(path: str, samples: list[float], sample_rate: int = SAMPLE_RATE) -> None:
    """Write 16-bit mono WAV file from float samples [-1.0, 1.0]."""
    num_samples = len(samples)
    data_size = num_samples * 2
    file_size = 36 + data_size

    with open(path, "wb") as f:
        f.write(b"RIFF")
        f.write(struct.pack("<I", file_size))
        f.write(b"WAVE")
        f.write(b"fmt ")
        f.write(struct.pack("<I", 16))       # chunk size
        f.write(struct.pack("<H", 1))        # PCM
        f.write(struct.pack("<H", 1))        # mono
        f.write(struct.pack("<I", sample_rate))
        f.write(struct.pack("<I", sample_rate * 2))  # byte rate
        f.write(struct.pack("<H", 2))        # block align
        f.write(struct.pack("<H", 16))       # bits per sample
        f.write(b"data")
        f.write(struct.pack("<I", data_size))

        for s in samples:
            clamped = max(-1.0, min(1.0, s))
            f.write(struct.pack("<h", int(clamped * 32767)))


def envelope(samples: list[float], attack: float = 0.01, decay: float = 0.05) -> list[float]:
    """Apply attack/decay envelope."""
    n = len(samples)
    attack_samples = int(attack * SAMPLE_RATE)
    decay_samples = int(decay * SAMPLE_RATE)
    result = []
    for i, s in enumerate(samples):
        if i < attack_samples and attack_samples > 0:
            s *= i / attack_samples
        elif i > n - decay_samples and decay_samples > 0:
            remaining = n - i
            s *= remaining / decay_samples
        result.append(s)
    return result


def noise(duration: float, volume: float = 0.5) -> list[float]:
    """White noise."""
    rng = random.Random(42)
    n = int(duration * SAMPLE_RATE)
    return [rng.uniform(-volume, volume) for _ in range(n)]


def sine(freq: float, duration: float, volume: float = 0.5) -> list[float]:
    n = int(duration * SAMPLE_RATE)
    return [math.sin(2 * math.pi * freq * i / SAMPLE_RATE) * volume for i in range(n)]


def square(freq: float, duration: float, volume: float = 0.5) -> list[float]:
    n = int(duration * SAMPLE_RATE)
    period = SAMPLE_RATE / freq
    return [volume if (i % int(period)) < int(period / 2) else -volume for i in range(n)]


def mix_samples(*sample_lists: list[float]) -> list[float]:
    max_len = max(len(s) for s in sample_lists)
    result = [0.0] * max_len
    for samples in sample_lists:
        for i, s in enumerate(samples):
            result[i] += s
    # Normalize
    peak = max(abs(s) for s in result) if result else 1.0
    if peak > 1.0:
        result = [s / peak for s in result]
    return result


def lowpass(samples: list[float], cutoff: float = 0.1) -> list[float]:
    """Simple one-pole lowpass filter."""
    result = [0.0] * len(samples)
    alpha = cutoff
    result[0] = samples[0] * alpha
    for i in range(1, len(samples)):
        result[i] = result[i - 1] + alpha * (samples[i] - result[i - 1])
    return result


def gen_footstep_soft() -> list[float]:
    s = noise(0.06, 0.2)
    s = lowpass(s, 0.15)
    return envelope(s, 0.002, 0.03)


def gen_footstep_normal() -> list[float]:
    s = noise(0.08, 0.35)
    s = lowpass(s, 0.2)
    s2 = sine(120, 0.04, 0.15)
    return envelope(mix_samples(s, s2), 0.003, 0.04)


def gen_footstep_loud() -> list[float]:
    s = noise(0.1, 0.5)
    s = lowpass(s, 0.25)
    s2 = sine(80, 0.06, 0.25)
    return envelope(mix_samples(s, s2), 0.003, 0.05)


def gen_weapon_swing() -> list[float]:
    """Whoosh sound."""
    n = int(0.15 * SAMPLE_RATE)
    result = []
    for i in range(n):
        t = i / SAMPLE_RATE
        freq = 200 + 800 * (t / 0.15)
        s = math.sin(2 * math.pi * freq * t) * 0.3
        ns = random.Random(i).uniform(-0.15, 0.15)
        result.append(s + ns)
    return envelope(result, 0.01, 0.06)


def gen_weapon_hit() -> list[float]:
    s = noise(0.1, 0.6)
    s = lowpass(s, 0.3)
    s2 = sine(60, 0.08, 0.4)
    return envelope(mix_samples(s, s2), 0.002, 0.05)


def gen_zombie_growl() -> list[float]:
    n = int(0.4 * SAMPLE_RATE)
    result = []
    for i in range(n):
        t = i / SAMPLE_RATE
        freq = 55 + 15 * math.sin(2 * math.pi * 3 * t)
        s = math.sin(2 * math.pi * freq * t) * 0.35
        s += math.sin(2 * math.pi * freq * 1.5 * t) * 0.15
        result.append(s)
    ns = noise(0.4, 0.1)
    ns = lowpass(ns, 0.05)
    return envelope(mix_samples(result, ns), 0.05, 0.1)


def gen_zombie_attack() -> list[float]:
    s = noise(0.12, 0.5)
    s = lowpass(s, 0.3)
    s2 = square(80, 0.08, 0.3)
    return envelope(mix_samples(s, s2), 0.005, 0.05)


def gen_food_eat() -> list[float]:
    """Crunch sound."""
    rng = random.Random(99)
    n = int(0.15 * SAMPLE_RATE)
    result = []
    for i in range(n):
        burst = rng.uniform(-0.5, 0.5) if rng.random() > 0.6 else 0.0
        result.append(burst)
    result = lowpass(result, 0.4)
    return envelope(result, 0.003, 0.06)


def gen_player_hurt() -> list[float]:
    s = noise(0.1, 0.4)
    s = lowpass(s, 0.2)
    s2 = sine(150, 0.08, 0.3)
    return envelope(mix_samples(s, s2), 0.003, 0.05)


def gen_death() -> list[float]:
    """Low descending tone."""
    n = int(0.8 * SAMPLE_RATE)
    result = []
    for i in range(n):
        t = i / SAMPLE_RATE
        freq = 200 - 150 * (t / 0.8)
        s = math.sin(2 * math.pi * freq * t) * 0.4
        s += math.sin(2 * math.pi * freq * 0.5 * t) * 0.2
        result.append(s)
    return envelope(result, 0.05, 0.3)


def gen_ambient_wind() -> list[float]:
    """Filtered noise loop, 5 seconds."""
    s = noise(5.0, 0.15)
    s = lowpass(s, 0.02)
    # Gentle fade at loop boundary
    fade = int(0.5 * SAMPLE_RATE)
    for i in range(fade):
        t = i / fade
        s[i] *= t
        s[-(i + 1)] *= t
    return s


def main() -> None:
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    sounds = {
        "footstep_soft.wav": gen_footstep_soft,
        "footstep_normal.wav": gen_footstep_normal,
        "footstep_loud.wav": gen_footstep_loud,
        "weapon_swing.wav": gen_weapon_swing,
        "weapon_hit.wav": gen_weapon_hit,
        "zombie_growl.wav": gen_zombie_growl,
        "zombie_attack.wav": gen_zombie_attack,
        "food_eat.wav": gen_food_eat,
        "player_hurt.wav": gen_player_hurt,
        "death.wav": gen_death,
        "ambient_wind.wav": gen_ambient_wind,
    }

    for filename, generator in sounds.items():
        path = os.path.join(OUTPUT_DIR, filename)
        samples = generator()
        write_wav(path, samples)
        print(f"  {filename}: {len(samples)} samples ({len(samples)/SAMPLE_RATE:.2f}s)")

    print(f"\nGenerated {len(sounds)} sounds in {OUTPUT_DIR}")


if __name__ == "__main__":
    main()
