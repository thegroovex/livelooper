#!/usr/bin/env python3
"""
Self-synthesised drum sounds for the livelooper style kits (v1.6): House, Techno, Boom Bap,
Trap, Breaks, Disco and the kick layers of Rock / Afro Latin. Every sound is generated here from
oscillators and noise (fixed random seeds, so the result is reproducible) - no recordings are
used, so the output is free of third-party rights and is released under CC0 1.0 like the rest
of the livelooper sample set.

Each voice gets several velocity layers (softer = darker, shorter) and round-robin takes
(small random changes of pitch, decay, noise colour and transient per take), so a run of equal
hits never sounds machine-gunned.

Used by build_drum_samples.py; run that script to rebuild resources/drums.
"""
import numpy as np
from scipy.signal import butter, sosfilt

RATE = 44100


def _t(seconds):
    return np.arange(int(seconds * RATE)) / RATE


def _filt(x, kind, freq, order=2):
    if isinstance(freq, (list, tuple)):
        f = [min(max(20.0, v), RATE * 0.45) for v in freq]
    else:
        f = min(max(20.0, freq), RATE * 0.45)
    sos = butter(order, f, btype=kind, fs=RATE, output="sos")
    return sosfilt(sos, x)


def _env(t, decay, attack=0.0005):
    e = np.exp(-t / max(1e-4, decay))
    if attack > 0:
        a = np.clip(t / attack, 0.0, 1.0)
        e = e * a
    return e


def _sat(x, drive):
    return np.tanh(x * drive) / np.tanh(drive)


def _crush(x, bits, rate):
    hold = RATE / rate
    idx = (np.floor(np.arange(len(x)) / hold) * hold).astype(int)
    x = x[np.clip(idx, 0, len(x) - 1)]
    q = 2 ** (bits - 1)
    return np.round(x * q) / q


class Rand:
    def __init__(self, seed):
        self.r = np.random.default_rng(seed)

    def j(self, value, amount):
        """value with +- amount (relative) jitter"""
        return value * (1.0 + self.r.uniform(-amount, amount))

    def noise(self, n):
        return self.r.uniform(-1.0, 1.0, n)


# ------------------------------------------------------------------ voices
def kick(p, vel, rnd):
    t = _t(p.get("len", 0.8))
    f0, f1 = rnd.j(p["start"], 0.04), rnd.j(p["end"], 0.02)
    pt = rnd.j(p["pitch_time"], 0.08) * (0.8 + 0.2 * vel)
    freq = f1 + (f0 - f1) * np.exp(-t / pt)
    phase = 2 * np.pi * np.cumsum(freq) / RATE
    decay = rnd.j(p["decay"], 0.08) * (0.7 + 0.3 * vel)
    body = np.sin(phase) * _env(t, decay, 0.0008)
    if p.get("sub", 0) > 0:
        body += p["sub"] * np.sin(phase * 0.5) * _env(t, decay * 1.3, 0.004)
    click = _filt(rnd.noise(len(t)), "bandpass", [rnd.j(p.get("click_freq", 3500), 0.15), 9000]) * _env(t, 0.003)
    x = body + click * p["click"] * (0.4 + 0.6 * vel)
    x = _sat(x * (0.6 + 0.4 * vel), p["drive"])
    if p.get("lp"):
        x = _filt(x, "lowpass", rnd.j(p["lp"], 0.1) * (0.6 + 0.4 * vel))
    if p.get("crush"):
        x = _crush(x, *p["crush"])
    return x


def snare(p, vel, rnd):
    t = _t(p.get("len", 0.5))
    bend = 1.0 + 0.35 * np.exp(-t / 0.01)
    f1, f2 = rnd.j(p["tone1"], 0.03), rnd.j(p["tone2"], 0.03)
    ph1 = 2 * np.pi * np.cumsum(f1 * bend) / RATE
    ph2 = 2 * np.pi * np.cumsum(f2 * bend) / RATE
    tone = (0.65 * np.sin(ph1) + 0.35 * np.sin(ph2)) * _env(t, rnd.j(p["tone_decay"], 0.1))
    n = rnd.noise(len(t))
    band = _filt(n, "bandpass", [rnd.j(p["band_lo"], 0.1), rnd.j(p["band_hi"], 0.1)])
    hi = _filt(n, "highpass", rnd.j(p["high"], 0.1))
    ndec = rnd.j(p["noise_decay"], 0.1) * (0.55 + 0.45 * vel)
    rattle = (0.8 * band + 0.5 * hi * (0.4 + 0.6 * vel)) * _env(t, ndec, 0.0008)
    snap = n * _env(t, 0.0025) * 0.6 * vel
    x = tone * p["tone_mix"] + rattle * (1 - p["tone_mix"]) * 1.5 + snap
    x = _sat(x, p.get("drive", 1.4))
    if p.get("lp"):
        x = _filt(x, "lowpass", p["lp"] * (0.65 + 0.35 * vel))
    if p.get("crush"):
        x = _crush(x, *p["crush"])
    return x


def clap(p, vel, rnd):
    t = _t(p.get("len", 0.5))
    n = rnd.noise(len(t))
    x = np.zeros(len(t))
    pos = 0.0
    for i in range(4):
        start = int(pos * RATE)
        seg = np.zeros(len(t))
        seg[start:] = _env(t[: len(t) - start], 0.006 if i < 3 else rnd.j(p["tail"], 0.12), 0.0004)
        x += seg * (1.0 if i < 3 else 0.9)
        pos += rnd.j(0.0095, 0.25)
    x = _filt(n * x, "bandpass", [rnd.j(p["lo"], 0.08), rnd.j(p["hi"], 0.08)])
    x = _sat(x * (0.6 + 0.4 * vel) * 2.5, 1.3)
    if p.get("crush"):
        x = _crush(x, *p["crush"])
    return x


HAT_FREQS = [205.3, 304.4, 369.6, 522.7, 540.0, 800.0]


def hat(p, vel, rnd, open_hat):
    t = _t(p["open_len"] if open_hat else p["closed_len"])
    metal = np.zeros(len(t))
    scale = rnd.j(p.get("metal_scale", 1.7), 0.02)
    for f in HAT_FREQS:
        ph = np.cumsum(np.full(len(t), f * scale / RATE)) + rnd.r.uniform(0, 1)
        metal += np.where((ph % 1.0) < 0.5, 1.0, -1.0)
    metal /= len(HAT_FREQS)
    src = metal * p["metal"] + rnd.noise(len(t)) * (1 - p["metal"])
    x = _filt(src, "bandpass", [rnd.j(p["band_lo"], 0.08), min(19000, rnd.j(p["band_hi"], 0.05))])
    x = _filt(x, "highpass", rnd.j(p["hp"], 0.08) * (0.85 + 0.15 * vel))
    decay = rnd.j(p["open_decay"] if open_hat else p["closed_decay"], 0.12) * (0.75 + 0.35 * vel)
    x = x * _env(t, decay, 0.0003)
    if p.get("crush"):
        x = _crush(x, *p["crush"])
    return x


def rim(p, vel, rnd):
    t = _t(0.25)
    a = np.sin(2 * np.pi * rnd.j(p.get("f1", 1720), 0.03) * t) * 0.5
    b = np.sin(2 * np.pi * rnd.j(p.get("f2", 820), 0.03) * t) * 0.35
    crack = _filt(rnd.noise(len(t)), "bandpass", [2000, 5000])
    e = _env(t, rnd.j(p.get("decay", 0.03), 0.1))
    x = (a + b) * e + crack * e * e * 1.4 * vel
    if p.get("crush"):
        x = _crush(x, *p["crush"])
    return x


def shaker(p, vel, rnd):
    t = _t(0.25)
    att = 1 - np.exp(-t / rnd.j(0.007, 0.2))
    x = _filt(rnd.noise(len(t)), "bandpass", [rnd.j(p.get("lo", 5000), 0.1), rnd.j(p.get("hi", 11000), 0.05)])
    x = x * att * _env(t, rnd.j(p.get("decay", 0.06), 0.15)) * (0.6 + 0.4 * vel)
    if p.get("crush"):
        x = _crush(x, *p["crush"])
    return x


def tom(p, vel, rnd):
    t = _t(0.7)
    f0, f1 = rnd.j(p.get("start", 180), 0.04), rnd.j(p.get("end", 95), 0.03)
    freq = f1 + (f0 - f1) * np.exp(-t / 0.05)
    ph = 2 * np.pi * np.cumsum(freq) / RATE
    x = np.sin(ph) * _env(t, rnd.j(p.get("decay", 0.25), 0.1))
    x += _filt(rnd.noise(len(t)), "bandpass", [800, 4000]) * _env(t, 0.006) * 0.3 * vel
    x = _sat(x * 1.4, 1.5)
    if p.get("crush"):
        x = _crush(x, *p["crush"])
    return x


# ------------------------------------------------------------------ kits
KITS = {
    "house": {
        "kick": dict(start=155, end=50, pitch_time=0.03, decay=0.42, click=0.35, drive=1.8, lp=9000),
        "snare": dict(tone1=205, tone2=360, tone_decay=0.06, band_lo=1800, band_hi=7500, high=4000, noise_decay=0.13, tone_mix=0.35),
        "clap": dict(lo=900, hi=3200, tail=0.12),
        "hat": dict(closed_len=0.2, open_len=0.8, closed_decay=0.035, open_decay=0.30, metal=0.35, band_lo=7000, band_hi=16000, hp=8000),
        "rim": dict(),
        "shaker": dict(lo=5500, hi=12000, decay=0.05),
        "tom": dict(start=170, end=90, decay=0.25),
    },
    "techno": {
        "kick": dict(start=210, end=46, pitch_time=0.022, decay=0.55, click=0.7, drive=3.2, sub=0.15, click_freq=3000),
        "snare": dict(tone1=185, tone2=320, tone_decay=0.04, band_lo=1500, band_hi=6000, high=3000, noise_decay=0.10, tone_mix=0.25, drive=2.0),
        "clap": dict(lo=1000, hi=2600, tail=0.18),
        "hat": dict(closed_len=0.15, open_len=0.6, closed_decay=0.025, open_decay=0.22, metal=0.55, band_lo=6000, band_hi=15000, hp=7000),
        "rim": dict(f1=1900, f2=600, decay=0.025),
        "shaker": dict(lo=6000, hi=13000, decay=0.04),
        "tom": dict(start=150, end=70, decay=0.3),
    },
    "boombap": {
        "kick": dict(start=115, end=52, pitch_time=0.045, decay=0.33, click=0.5, drive=2.4, lp=4500, crush=(12, 26000)),
        "snare": dict(tone1=215, tone2=330, tone_decay=0.1, band_lo=1200, band_hi=5500, high=2500, noise_decay=0.2, tone_mix=0.5, drive=2.0, lp=7000, crush=(12, 26000)),
        "clap": dict(lo=800, hi=2400, tail=0.14, crush=(12, 26000)),
        "hat": dict(closed_len=0.2, open_len=0.7, closed_decay=0.04, open_decay=0.25, metal=0.25, band_lo=5000, band_hi=11000, hp=5500, crush=(12, 26000)),
        "rim": dict(f1=1600, f2=780, crush=(12, 26000)),
        "shaker": dict(lo=4000, hi=9000, decay=0.06, crush=(12, 26000)),
        "tom": dict(start=160, end=85, decay=0.22, crush=(12, 26000)),
    },
    "trap": {
        "kick": dict(start=125, end=44, pitch_time=0.07, decay=1.25, click=0.3, drive=2.4, len=1.4, sub=0.0),
        "snare": dict(tone1=235, tone2=470, tone_decay=0.05, band_lo=2500, band_hi=9000, high=5000, noise_decay=0.11, tone_mix=0.4, drive=1.8),
        "clap": dict(lo=1100, hi=3800, tail=0.16),
        "hat": dict(closed_len=0.12, open_len=0.6, closed_decay=0.018, open_decay=0.25, metal=0.5, band_lo=7500, band_hi=17000, hp=9000),
        "rim": dict(f1=1800, f2=900),
        "shaker": dict(lo=6500, hi=13000, decay=0.04),
        "tom": dict(start=140, end=60, decay=0.45),
    },
    "breaks": {
        "kick": dict(start=165, end=58, pitch_time=0.03, decay=0.28, click=0.9, drive=2.8, lp=8000),
        "snare": dict(tone1=225, tone2=370, tone_decay=0.08, band_lo=1500, band_hi=7000, high=3500, noise_decay=0.18, tone_mix=0.4, drive=2.6),
        "clap": dict(lo=900, hi=3000, tail=0.13),
        "hat": dict(closed_len=0.18, open_len=0.7, closed_decay=0.03, open_decay=0.28, metal=0.3, band_lo=6000, band_hi=14000, hp=6500),
        "rim": dict(f1=1700, f2=760),
        "shaker": dict(lo=5000, hi=11000, decay=0.06),
        "tom": dict(start=175, end=95, decay=0.22),
    },
    "disco": {
        "kick": dict(start=140, end=54, pitch_time=0.035, decay=0.36, click=0.45, drive=1.7, lp=6000),
        "snare": dict(tone1=200, tone2=345, tone_decay=0.08, band_lo=1600, band_hi=7000, high=3500, noise_decay=0.16, tone_mix=0.42),
        "clap": dict(lo=950, hi=3000, tail=0.15),
        "hat": dict(closed_len=0.2, open_len=0.9, closed_decay=0.04, open_decay=0.38, metal=0.3, band_lo=6500, band_hi=15000, hp=7000),
        "rim": dict(),
        "shaker": dict(lo=5000, hi=11500, decay=0.07),
        "tom": dict(start=200, end=110, decay=0.3),
    },
    # kick layers for the sample-based kits
    "rock": {"kick": dict(start=95, end=52, pitch_time=0.035, decay=0.22, click=0.0, drive=1.3, lp=500)},
    "afrolatin": {"kick": dict(start=105, end=48, pitch_time=0.04, decay=0.48, click=0.25, drive=1.5, lp=3500)},
}

# layers per voice: (velocity, number of round-robin takes)
LAYERS = {
    "kick": [(0.4, 2), (0.7, 2), (1.0, 3)],
    "snare": [(0.35, 2), (0.7, 2), (1.0, 3)],
    "clap": [(0.8, 3)],
    "hatc": [(0.35, 2), (0.7, 2), (1.0, 2)],
    "hato": [(0.8, 2)],
    "rim": [(0.6, 1), (1.0, 1)],
    "shaker": [(0.8, 3)],
    "tom": [(0.8, 2)],
}


def render(kit, voice, vel, take):
    """One take of a voice. Returns mono float64 at 44.1 kHz."""
    p = KITS[kit]
    # stable seed (hash() of str is salted per process)
    seed = sum(ord(c) * (i + 1) for i, c in enumerate(f"{kit}/{voice}/{vel:.2f}/{take}"))
    rnd = Rand(seed)
    if voice == "kick":
        return kick(p["kick"], vel, rnd)
    if voice == "snare":
        return snare(p["snare"], vel, rnd)
    if voice == "clap":
        return clap(p["clap"], vel, rnd)
    if voice == "hatc":
        return hat(p["hat"], vel, rnd, False)
    if voice == "hato":
        return hat(p["hat"], vel, rnd, True)
    if voice == "rim":
        return rim(p["rim"], vel, rnd)
    if voice == "shaker":
        return shaker(p["shaker"], vel, rnd)
    if voice == "tom":
        return tom(p["tom"], vel, rnd)
    raise ValueError(voice)


def kit_entries(kit, voices=None):
    """[(voice, velocity, take, audio)] for the voices of a synthesised kit."""
    out = []
    for voice, layers in LAYERS.items():
        if voices is not None and voice not in voices:
            continue
        for vel, takes in layers:
            for take in range(takes):
                out.append((voice, vel, take, render(kit, voice, vel, take)))
    return out
