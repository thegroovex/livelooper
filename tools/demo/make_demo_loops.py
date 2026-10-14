#!/usr/bin/env python3
"""Synthesises the three demo loops of livelooper's demo set (v1.6).

Everything is generated from scratch with numpy (no samples, no third-party audio), so the
loops are self-made and released under CC0 1.0 (public domain dedication).

    python3 tools/demo/make_demo_loops.py      # writes resources/demo/*.flac (needs ffmpeg)

Key A minor, 100 BPM, 4/4, two bars per loop (Am7 | Fmaj7), 44.1 kHz stereo.
"""
import os, subprocess, tempfile, wave
import numpy as np

SR = 44100
BPM = 100.0
BEAT = 60.0 / BPM
BARS = 2
N = int(round(BARS * 4 * BEAT * SR))
OUT = os.path.join(os.path.dirname(__file__), '..', '..', 'resources', 'demo')
rng = np.random.default_rng(1607)

def hz(midi): return 440.0 * 2 ** ((midi - 69) / 12.0)

def onepole_lp(x, cutoff):
    """time-varying one-pole low-pass (cutoff array in Hz)"""
    y = np.zeros_like(x); s = 0.0
    a = 1.0 - np.exp(-2 * np.pi * np.asarray(cutoff) / SR)
    a = np.broadcast_to(a, x.shape)
    for i in range(len(x)):
        s += a[i] * (x[i] - s); y[i] = s
    return y

def place(buf, sig, start):
    """adds sig at start, wrapping around the loop end (seamless loop)"""
    n = len(sig); s = start % N
    first = min(n, N - s)
    buf[s:s + first] += sig[:first]
    if first < n: buf[:n - first] += sig[first:]

def env_adsr(n, a, d, s, r_start, r):
    t = np.arange(n) / SR
    e = np.where(t < a, t / max(a, 1e-6), s + (1 - s) * np.exp(-(t - a) / max(d, 1e-6)))
    rel = t > r_start
    e[rel] *= np.exp(-(t[rel] - r_start) / r)
    return e

# ---------------------------------------------------------------- bass (saw + sub, filter envelope)
def bass():
    out = np.zeros(N)
    # 8th-note line, two bars: Am7 then Fmaj7 (MIDI notes, None = rest)
    line = [45, None, 45, 57, 45, None, 43, 45,   41, None, 41, 53, 41, 43, None, 40]
    eighth = BEAT / 2
    for k, note in enumerate(line):
        if note is None: continue
        dur = eighth * 0.92
        n = int(dur * SR) + int(0.05 * SR)
        t = np.arange(n) / SR
        f = hz(note)
        saw = 2 * ((t * f) % 1.0) - 1
        saw2 = 2 * ((t * f * 1.004) % 1.0) - 1
        sub = np.sin(2 * np.pi * f / 2 * t)
        x = 0.5 * saw + 0.3 * saw2 + 0.6 * sub
        cut = 180 + 1400 * np.exp(-t / 0.07)
        x = onepole_lp(onepole_lp(x, cut), cut)
        x *= env_adsr(n, 0.004, 0.25, 0.7, dur, 0.03)
        place(out, x * (1.0 if k % 2 == 0 else 0.8), int(k * eighth * SR))
    return np.stack([out, out])

# ---------------------------------------------------------------- keys (two-operator FM electric piano, offbeat stabs)
def keys():
    L = np.zeros(N); R = np.zeros(N)
    chords = [[57, 60, 64, 67], [53, 57, 60, 64]]       # Am7, Fmaj7
    hits = [1.5, 2.5, 3.5]                              # beats within the bar (offbeats)
    for bar, chord in enumerate(chords):
        for b in hits + ([0.0] if bar == 0 else [0.0]):
            start = (bar * 4 + b) * BEAT
            dur = BEAT * (0.9 if b == 0.0 else 0.35)
            n = int((dur + 0.6) * SR)
            t = np.arange(n) / SR
            for v, note in enumerate(chord):
                f = hz(note)
                mod = np.sin(2 * np.pi * f * 14 * t) * 1.2 * np.exp(-t / 0.08)
                car = np.sin(2 * np.pi * f * t + mod + 0.8 * np.sin(2 * np.pi * f * t) * np.exp(-t / 0.3))
                e = env_adsr(n, 0.002, 0.5, 0.25, dur, 0.12)
                pan = -0.35 + 0.23 * v
                g = 0.22 * (0.8 if b > 0 else 1.0)
                place(L, car * e * g * (1 - pan), int(start * SR))
                place(R, car * e * g * (1 + pan), int(start * SR))
    # tremolo for a little movement
    tt = np.arange(N) / SR
    trem = 1.0 + 0.15 * np.sin(2 * np.pi * tt / (BEAT / 2))
    return np.stack([L * trem, R * (2.0 - trem)])

# ---------------------------------------------------------------- arp (Karplus-Strong pluck, 16ths, ping-pong pan)
def pluck(f, dur):
    n = int(dur * SR)
    period = int(SR / f)
    buf = rng.uniform(-1, 1, period)
    out = np.zeros(n)
    for i in range(n):
        out[i] = buf[i % period]
        buf[i % period] = 0.996 * 0.5 * (buf[i % period] + buf[(i + 1) % period])
    return out * np.exp(-np.arange(n) / SR / 0.6)

def arp():
    L = np.zeros(N); R = np.zeros(N)
    pat = [[69, 72, 76, 79, 76, 72, 81, 76], [65, 69, 72, 76, 72, 69, 77, 72]]
    six = BEAT / 4
    for bar in range(2):
        for k in range(16):
            note = pat[bar][k % 8]
            if k in (7, 15):
                continue        # little gaps make it breathe
            x = pluck(hz(note), 0.5) * (0.5 if k % 4 else 0.7)
            pan = -0.5 if k % 2 == 0 else 0.5
            start = int(((bar * 4) * BEAT + k * six) * SR)
            place(L, x * (1 - pan) * 0.5, start)
            place(R, x * (1 + pan) * 0.5, start)
            # dotted-8th echo, quieter, opposite side
            place(L, x * (1 + pan) * 0.18, start + int(BEAT * 0.75 * SR))
            place(R, x * (1 - pan) * 0.18, start + int(BEAT * 0.75 * SR))
    return np.stack([L, R])

def write(name, data, peak_db):
    data = data / np.max(np.abs(data)) * 10 ** (peak_db / 20)
    pcm = (np.clip(data.T, -1, 1) * 32767).astype('<i2')
    with tempfile.TemporaryDirectory() as d:
        wav = os.path.join(d, 'x.wav')
        with wave.open(wav, 'wb') as w:
            w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR); w.writeframes(pcm.tobytes())
        os.makedirs(OUT, exist_ok=True)
        dst = os.path.join(OUT, name)
        subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-i', wav, '-compression_level', '12', dst], check=True)
    print(name, os.path.getsize(dst), 'bytes')

if __name__ == '__main__':
    write('demo_bass.flac', bass(), -4.0)
    write('demo_keys.flac', keys(), -6.0)
    write('demo_arp.flac', arp(), -7.0)
