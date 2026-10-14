#!/usr/bin/env python3
"""
Builds the livelooper drum kits (resources/drums/*.flac + src/engine/DrumSamples.inc) from
CC0 / public-domain source samples. Every source and its license is listed in
resources/drums/SOURCES.md, which this script writes as well.

Processing: mono, 44.1 kHz, leading silence removed, trimmed to a maximum length with a
fade-out, normalised per voice (velocity layers keep their relative levels), 24-bit FLAC.

Sources (download them first, see SOURCES.md for the URLs):
  SRC/vcsl        Versilian Community Sample Library (CC0 1.0)
  SRC/808         tidalcycles/sounds-tr808-fischer (CC0 1.0)
  SRC/open-drums  fluid-music/open-drums tr-707 (public domain)
  SRC/sonicpi     Sonic Pi samples from freesound.org (CC0 1.0)
  SRC/sampled     oramics/sampled DM/LM-2 (public domain)
  SRC/vcsl16      more VCSL files for the v1.6 Rock and Afro Latin kits (CC0 1.0)

v1.6: the style kits House, Techno, Boom Bap, Trap, Breaks and Disco are synthesised by
synth_kits.py (no recordings), Rock and Afro Latin combine VCSL recordings with synthesised
kick layers, and the Studio kick / snare get extra takes derived from their VCSL recordings
(pitch, decay and transient variants) for more variation.

usage: build_drum_samples.py <SRC> <repo root>
"""
import os
import sys

import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import synth_kits  # noqa: E402

RATE = 44100

VCSL_URL = "https://github.com/sgossner/VCSL"
FISCHER_URL = "https://github.com/tidalcycles/sounds-tr808-fischer"
OPEN_DRUMS_URL = "https://github.com/fluid-music/open-drums/tree/main/tr-707"
SONICPI_URL = "https://github.com/sonic-pi-net/sonic-pi/tree/dev/etc/samples"
SAMPLED_URL = "https://github.com/oramics/sampled/tree/master/DM/LM-2"

LIC_CC0 = "CC0 1.0 Universal"
LIC_PD = "Public domain"
SELF_URL = "tools/samples/synth_kits.py (this repository)"
LIC_SELF = "CC0 1.0 Universal (self-synthesised for livelooper)"

# max length in seconds per voice
MAXLEN = {"kick": 0.75, "snare": 0.6, "clap": 0.5, "hatc": 0.28, "hato": 0.9, "rim": 0.3, "shaker": 0.3, "tom": 0.8}

def variant(x, pitch, decay, punch):
    """A new take derived from a recording: resampled (pitch factor), shorter / longer decay
    envelope, transient emphasis."""
    from scipy.signal import resample
    n = max(16, int(len(x) / pitch))
    y = resample(x, n)
    t = np.arange(len(y)) / RATE
    y = y * np.exp(-t * decay)
    att = int(0.006 * RATE)
    y[:att] *= 1.0 + punch * np.linspace(1.0, 0.0, att)
    return y


def synth_voice(kit, voice):
    """[(layer, audio, name, url, license)] for a synthesised voice."""
    return [(vel, a, f"self-synthesised {kit} {voice} (layer {vel:.2f}, take {take + 1})", SELF_URL, LIC_SELF)
            for (v, vel, take, a) in synth_kits.kit_entries(kit, [voice])]


# kit -> voice -> list of (layer velocity 0..1, source path or audio, source name, url, license)
def kits(src):
    v = os.path.join(src, "vcsl")
    e = os.path.join(src, "808")
    o = os.path.join(src, "open-drums", "tr-707", "TR707WAV")
    s = os.path.join(src, "sonicpi")
    l = os.path.join(src, "sampled", "DM", "LM-2", "samples")

    def vc(f): return (os.path.join(v, f), "VCSL " + f, VCSL_URL, LIC_CC0)
    def tr8(d, f): return (os.path.join(e, d, f), "TR-808 (M. Fischer) " + d + "/" + f, FISCHER_URL, LIC_CC0)
    def tr7(f): return (os.path.join(o, f), "TR-707 " + f, OPEN_DRUMS_URL, LIC_PD)
    def sp(f, fs): return (os.path.join(s, f), "Sonic Pi " + f + " (freesound " + fs + ")", SONICPI_URL, LIC_CC0)
    def lm(f): return (os.path.join(l, f), "LinnDrum LM-2 " + f, SAMPLED_URL, LIC_PD)
    w = os.path.join(src, "vcsl16")
    def v16(f): return (os.path.join(w, f + ".wav"), "VCSL " + f + ".wav", VCSL_URL, LIC_CC0)

    def studio_takes(files, voice):
        # v1.6: one extra take per recording (pitch / decay / transient variant)
        out = []
        for i, (vel, f) in enumerate(files):
            a = load(os.path.join(v, f))
            pitch = [0.97, 1.03, 0.985, 1.02][i % 4]
            out.append((vel, variant(a, pitch, [1.5, 3.0, 0.5, 2.2][i % 4], [0.5, 0.2, 0.8, 0.3][i % 4]),
                        f"VCSL {f}, derived take (pitch x{pitch})", VCSL_URL, LIC_CC0))
        return out

    def rock_kick():
        # VCSL kick with more attack and a short synthesised low layer underneath
        out = []
        layers = [(0.4, "BDrumNew_hit_v3_rr1_Sum.wav"), (0.4, "BDrumNew_hit_v3_rr2_Sum.wav"),
                  (0.7, "BDrumNew_hit_v5_rr1_Sum.wav"), (0.7, "BDrumNew_hit_v5_rr2_Sum.wav"),
                  (1.0, "BDrumNew_hit_v7_rr1_Sum.wav"), (1.0, "BDrumNew_hit_v7_rr2_Sum.wav")]
        for i, (vel, f) in enumerate(layers):
            a = variant(load(os.path.join(v, f)), 0.94, 4.0, 1.2)
            a = a / (np.max(np.abs(a)) + 1e-12)
            sub = synth_kits.render("rock", "kick", vel, i)
            n = max(len(a), len(sub))
            mix = np.zeros(n)
            mix[:len(a)] += a
            mix[:len(sub)] += sub * 0.55 * vel
            out.append((vel, np.tanh(mix * 1.4), f"VCSL {f} + self-synthesised low layer", VCSL_URL, LIC_CC0))
        return out

    return {
        "studio": {
            "kick":  [(0.3, *vc("BDrumNew_hit_v2_rr1_Sum.wav")), (0.3, *vc("BDrumNew_hit_v2_rr2_Sum.wav")),
                      (0.55, *vc("BDrumNew_hit_v3_rr1_Sum.wav")), (0.55, *vc("BDrumNew_hit_v3_rr2_Sum.wav")),
                      (0.8, *vc("BDrumNew_hit_v5_rr1_Sum.wav")), (0.8, *vc("BDrumNew_hit_v5_rr2_Sum.wav")),
                      (1.0, *vc("BDrumNew_hit_v7_rr1_Sum.wav")), (1.0, *vc("BDrumNew_hit_v7_rr2_Sum.wav"))]
                     + studio_takes([(0.3, "BDrumNew_hit_v2_rr1_Sum.wav"), (0.55, "BDrumNew_hit_v3_rr2_Sum.wav"),
                                     (0.8, "BDrumNew_hit_v5_rr1_Sum.wav"), (0.8, "BDrumNew_hit_v5_rr2_Sum.wav"),
                                     (1.0, "BDrumNew_hit_v7_rr1_Sum.wav"), (1.0, "BDrumNew_hit_v7_rr2_Sum.wav"),
                                     (1.0, "BDrumNew_hit_v7_rr2_Sum.wav"), (1.0, "BDrumNew_hit_v7_rr1_Sum.wav")], "kick"),
            "snare": [(0.3, *vc("Snare2_HitSN_v3_rr1_Mid.wav")), (0.3, *vc("Snare2_HitSN_v3_rr2_Mid.wav")),
                      (0.55, *vc("Snare2_HitSN_v5_rr1_Mid.wav")), (0.55, *vc("Snare2_HitSN_v5_rr2_Mid.wav")),
                      (0.8, *vc("Snare2_HitSN_v7_rr1_Mid.wav")), (0.8, *vc("Snare2_HitSN_v7_rr2_Mid.wav")),
                      (1.0, *vc("Snare2_HitSN_v9_rr1_Mid.wav")), (1.0, *vc("Snare2_HitSN_v9_rr2_Mid.wav"))]
                     + studio_takes([(0.8, "Snare2_HitSN_v7_rr1_Mid.wav"), (1.0, "Snare2_HitSN_v9_rr2_Mid.wav"),
                                     (1.0, "Snare2_HitSN_v9_rr1_Mid.wav"), (0.55, "Snare2_HitSN_v5_rr2_Mid.wav")], "snare"),
            "clap":  [(0.8, *vc("Clap_rr1.wav")), (0.8, *vc("Clap_rr2.wav")), (0.8, *vc("Clap_rr3.wav"))],
            "hatc":  [(0.3, *vc("HiHat_HitC_v1_rr1_Mid.wav")), (0.3, *vc("HiHat_HitC_v1_rr2_Mid.wav")),
                      (0.55, *vc("HiHat_HitC_v2_rr1_Mid.wav")), (0.55, *vc("HiHat_HitC_v2_rr2_Mid.wav")),
                      (0.8, *vc("HiHat_HitC_v3_rr1_Mid.wav")), (0.8, *vc("HiHat_HitC_v3_rr2_Mid.wav")),
                      (1.0, *vc("HiHat_HitC_v4_rr1_Mid.wav")), (1.0, *vc("HiHat_HitC_v4_rr2_Mid.wav"))],
            "hato":  [(0.8, *vc("HiHat_HitO_rr1_Mid.wav")), (0.8, *vc("HiHat_HitO_rr2_Mid.wav"))],
            "rim":   [(0.6, *vc("snare2_rimshot_mf_rr1.wav")), (1.0, *vc("snare2_rimshot_f_rr1.wav"))],
            "shaker":[(0.8, *vc("Mid_ShakerHighFaster_Down_rr1.wav")), (0.8, *vc("Mid_ShakerHighFaster_Down_rr2.wav")),
                      (0.8, *vc("Mid_ShakerHighFaster_Up_rr1.wav"))],
            "tom":   [(0.8, *vc("TomH_HitS_v3_rr1_Mid.wav")), (0.8, *vc("TomH_HitS_v3_rr2_Mid.wav"))],
        },
        "808": {
            "kick":  [(0.3, *tr8("bd8", "BD5010.WAV")), (0.3, *tr8("bd8", "BD5025.WAV")),
                      (0.7, *tr8("bd8", "BD5050.WAV")), (0.7, *tr8("bd8", "BD2550.WAV")),
                      (1.0, *tr8("bd8", "BD5075.WAV")), (1.0, *tr8("bd8", "BD7550.WAV"))],
            "snare": [(0.3, *tr8("sd8", "SD0025.WAV")), (0.3, *tr8("sd8", "SD2525.WAV")),
                      (0.7, *tr8("sd8", "SD5050.WAV")), (0.7, *tr8("sd8", "SD2550.WAV")),
                      (1.0, *tr8("sd8", "SD5075.WAV")), (1.0, *tr8("sd8", "SD7575.WAV"))],
            "clap":  [(0.8, *tr8("cp8", "CP.WAV"))],
            "hatc":  [(0.8, *tr8("ch8", "CH.WAV"))],
            "hato":  [(0.8, *tr8("oh8", "OH25.WAV")), (0.8, *tr8("oh8", "OH50.WAV"))],
            "rim":   [(0.8, *tr8("rs8", "RS.WAV"))],
            "shaker":[(0.8, *tr8("ma8", "MA.WAV"))],
            "tom":   [(0.8, *tr8("lt8", "LT25.WAV")), (0.8, *tr8("lt8", "LT50.WAV"))],
        },
        "909style": {
            "kick":  [(0.6, *sp("bd_tek.flac", "171104 by DWSD")), (1.0, *sp("bd_haus.flac", "137722 by Rodrigo The Mad")),
                      (1.0, *sp("bd_klub.flac", "83262 by zgump"))],
            "snare": [(0.5, *tr7("Snare1.wav")), (1.0, *tr7("Snare2.wav"))],
            "clap":  [(0.8, *tr7("HandClap.wav"))],
            "hatc":  [(0.8, *tr7("HhC.wav"))],
            "hato":  [(0.8, *tr7("HhO.wav"))],
            "rim":   [(0.8, *tr7("RimShot.wav"))],
            "shaker":[(0.8, *tr7("Tamb.wav"))],
            "tom":   [(0.8, *tr7("MedTom.wav")), (0.8, *tr7("LowTom.wav"))],
        },
        "lofi": {
            "kick":  [(0.8, *lm("kick.wav")), (0.8, *lm("kick-alt.wav"))],
            "snare": [(0.3, *lm("snare-l.wav")), (0.7, *lm("snare-m.wav")), (1.0, *lm("snare-h.wav"))],
            "clap":  [(0.8, *lm("clap.wav"))],
            "hatc":  [(0.5, *lm("hihat-closed-short.wav")), (0.8, *lm("hihat-closed.wav")), (1.0, *lm("hihat-closed-long.wav"))],
            "hato":  [(0.8, *lm("hihat-open.wav"))],
            "rim":   [(0.5, *lm("stick-l.wav")), (0.8, *lm("stick-m.wav")), (1.0, *lm("stick-h.wav"))],
            "shaker":[(0.8, *lm("cabasa.wav"))],
            "tom":   [(0.8, *lm("tom-m.wav")), (0.8, *lm("tom-l.wav"))],
        },
        # ---- v1.6 style kits
        **{k: {vn: synth_voice(k, vn) for vn in ["kick", "snare", "clap", "hatc", "hato", "rim", "shaker", "tom"]}
           for k in ["house", "techno", "boombap", "trap", "breaks"]},
        "rock": {
            "kick":  rock_kick(),
            "snare": [(0.3, *v16("Snare3M_HitSN_v2_rr1_Mid")), (0.3, *v16("Snare3M_HitSN_v2_rr2_Mid")),
                      (0.55, *v16("Snare3M_HitSN_v3_rr1_Mid")), (0.55, *v16("Snare3M_HitSN_v3_rr2_Mid")),
                      (0.8, *v16("Snare3M_HitSN_v4_rr1_Mid")), (0.8, *v16("Snare3M_HitSN_v4_rr2_Mid")),
                      (1.0, *v16("Snare3M_HitSN_v5_rr1_Mid")), (1.0, *v16("Snare3M_HitSN_v5_rr2_Mid"))],
            "clap":  [(0.8, *v16("Clap_rr4")), (0.8, *v16("Clap_rr5")), (0.8, *v16("Clap_rr6"))],
            "hatc":  [(0.4, *v16("HiHat_HitC_v2_rr1_Mid")), (0.4, *v16("HiHat_HitC_v2_rr2_Mid")),
                      (0.7, *v16("HiHat_HitC_v3_rr1_Mid")), (0.7, *v16("HiHat_HitC_v3_rr2_Mid")),
                      (1.0, *v16("HiHat_HitLoose_rr1_Mid")), (1.0, *v16("HiHat_HitLoose_rr2_Mid")),
                      (1.0, *v16("HiHat_HitC_v4_rr1_Mid")), (1.0, *v16("HiHat_HitC_v4_rr2_Mid"))],
            "hato":  [(0.8, *v16("HiHat_HitO_rr1_Mid")), (0.8, *v16("HiHat_HitO_rr2_Mid")), (0.8, *v16("HiHat_HitOC_rr5_Mid"))],
            "rim":   [(0.7, *v16("Snare3M_Xstick_v2_rr1_Mid")), (0.7, *v16("Snare3M_Xstick_v2_rr2_Mid"))],
            "shaker":[(0.6, *v16("Tamb1_Hit_v1_rr1_Mid")), (1.0, *v16("Tamb1_Hit_v2_rr1_Mid"))],
            "tom":   [(0.6, *v16("TomL_HitS_v3_rr1_Mid")), (0.6, *v16("TomL_HitS_v3_rr2_Mid")),
                      (1.0, *v16("TomL_HitS_v4_rr1_Mid")), (1.0, *v16("TomL_HitS_v4_rr2_Mid"))],
        },
        "disco": {vn: synth_voice("disco", vn) for vn in ["kick", "snare", "clap", "hatc", "hato", "rim", "shaker", "tom"]},
        "afrolatin": {
            # voices: kick, bongo, claves, shaker, tambourine, cowbell, cabasa, conga
            "kick":  synth_voice("afrolatin", "kick"),
            "snare": [(0.35, *v16("BongoH_Hit1_v1_rr1_Mid")), (0.35, *v16("BongoH_Hit1_v1_rr2_Mid")),
                      (0.7, *v16("BongoH_Hit1_v2_rr1_Mid")), (0.7, *v16("BongoH_Hit1_v2_rr2_Mid")),
                      (1.0, *v16("BongoH_Hit1_v3_rr1_Mid")), (1.0, *v16("BongoH_Hit1_v3_rr2_Mid"))],
            "clap":  [(0.4, *v16("Claves1_Hit_v1_rr1_Mid")), (0.7, *v16("Claves1_Hit_v2_rr1_Mid")), (1.0, *v16("Claves1_Hit_v3_rr1_Mid")),
                      (0.7, *v16("Claves2_Hit_v2_rr1_Mid")), (1.0, *v16("Claves2_Hit_v3_rr1_Mid"))],
            "hatc":  [(0.5, *v16("Mid_ShakerLowFaster_Down_rr1")), (0.5, *v16("Mid_ShakerLowFaster_Up_rr1")),
                      (0.8, *v16("Mid_ShakerLowFaster_Down_rr2")), (0.8, *v16("Mid_ShakerLowFaster_Up_rr2")),
                      (1.0, *v16("Mid_ShakerDouble_Down_rr1")), (1.0, *v16("Mid_ShakerDouble_Up_rr1"))],
            "hato":  [(0.6, *v16("Tamb1_Hit_v1_rr1_Mid")), (1.0, *v16("Tamb1_Hit_v2_rr1_Mid")), (0.8, *v16("Tamb1_Shake_rr1_Mid"))],
            "rim":   [(0.5, *v16("Cowbell1_Hit_v2_rr1_Mid")), (0.8, *v16("Cowbell1_Hit_v3_rr1_Mid")), (1.0, *v16("Cowbell1_Hit_v4_rr1_Mid"))],
            "shaker":[(0.8, *v16("Cabasa1_Hit_rr1_Mid")), (0.8, *v16("Cabasa1_Hit_rr2_Mid"))],
            "tom":   [(0.4, *v16("Conga_HitN_v1_rr1_Sum")), (0.4, *v16("Conga_HitN_v1_rr2_Sum")),
                      (0.7, *v16("Conga_HitN_v2_rr1_Sum")), (0.7, *v16("Conga_HitN_v2_rr2_Sum")),
                      (1.0, *v16("Conga_HitN_v3_rr1_Sum")), (1.0, *v16("Conga_HitN_v3_rr2_Sum"))],
        },
    }


def load(path):
    data, rate = sf.read(path, always_2d=True, dtype="float64")
    mono = data.mean(axis=1)
    if rate != RATE:
        from math import gcd
        g = gcd(RATE, rate)
        mono = resample_poly(mono, RATE // g, rate // g)
    return mono


def trim(x, maxlen, lofi):
    peak = np.max(np.abs(x)) + 1e-12
    above = np.where(np.abs(x) > peak * 10 ** (-45 / 20))[0]
    start = max(0, above[0] - int(0.0008 * RATE)) if len(above) else 0
    x = x[start:]
    # end: where it stays below -60 dB, at most maxlen
    above = np.where(np.abs(x) > peak * 10 ** (-60 / 20))[0]
    end = min(len(x), (above[-1] + 1) if len(above) else len(x), int(maxlen * RATE))
    x = x[:end].copy()
    fade = min(len(x) // 4, int(0.025 * RATE))
    if fade > 0:
        x[-fade:] *= np.linspace(1.0, 0.0, fade) ** 2
    # tiny fade-in against clicks
    fi = min(16, len(x))
    x[:fi] *= np.linspace(0.0, 1.0, fi)
    if lofi:
        # vintage character: 12-bit and 26 kHz sample-and-hold, gentle saturation
        hold = RATE / 26040.0
        idx = (np.floor(np.arange(len(x)) / hold) * hold).astype(int)
        x = x[np.clip(idx, 0, len(x) - 1)]
        q = 2 ** 11
        x = np.round(x * q) / q
        x = np.tanh(x * 1.3) / np.tanh(1.3)
    return x


def main():
    src, root = sys.argv[1], sys.argv[2]
    out_dir = os.path.join(root, "resources", "drums")
    os.makedirs(out_dir, exist_ok=True)
    for f in os.listdir(out_dir):
        if f.endswith(".flac"):
            os.remove(os.path.join(out_dir, f))

    voices = ["kick", "snare", "clap", "hatc", "hato", "rim", "shaker", "tom"]
    kit_order = ["studio", "808", "909style", "lofi",
                 "house", "techno", "boombap", "trap", "breaks", "rock", "disco", "afrolatin"]   # index = kit parameter
    table = []
    sources = []
    total = 0
    all_kits = kits(src)
    for k in kit_order:
        kit = all_kits[k]
        for vname in voices:
            entries = kit[vname]
            maxlen = 1.3 if (k == "trap" and vname == "kick") else MAXLEN[vname]
            audio = [trim(load(p) if isinstance(p, str) else np.asarray(p, dtype=np.float64), maxlen, k == "lofi")
                     for (_, p, _, _, _) in entries]
            peak = max(np.max(np.abs(a)) for a in audio) + 1e-12
            for i, ((vel, path, name, url, lic), a) in enumerate(zip(entries, audio)):
                a = a / peak * 10 ** (-1.0 / 20)
                fname = f"drum_{k}_{vname}_{i + 1}.flac"
                # v1.6 kits: 16 bit (smaller plug-in, no audible difference for drum one-shots)
                sf.write(os.path.join(out_dir, fname), a.astype(np.float32), RATE,
                         subtype="PCM_24" if k in ("studio", "808", "909style", "lofi") else "PCM_16")
                total += os.path.getsize(os.path.join(out_dir, fname))
                table.append((k, vname, vel, fname))
                sources.append((fname, name, url, lic))

    # C++ table (included by RhythmGenerator / DrumKit)
    inc = os.path.join(root, "src", "engine", "DrumSamples.inc")
    with open(inc, "w") as f:
        f.write("// Generated by tools/samples/build_drum_samples.py - do not edit.\n")
        f.write("// { kit, voice, velocity layer (0..1), resource file name }\n")
        for (k, vname, vel, fname) in table:
            f.write(f'{{ {kit_order.index(k)}, {voices.index(vname)}, {vel:.2f}f, "{fname}" }},\n')

    with open(os.path.join(out_dir, "SOURCES.md"), "w") as f:
        f.write("# Drum samples - sources and licenses\n\n")
        f.write("Every drum sample in livelooper comes from a source released under **CC0 1.0** or into the "
                "**public domain**, or was synthesised for livelooper by `tools/samples/synth_kits.py` (oscillators "
                "and noise, no recordings; released under CC0 1.0). The files here were converted to mono 44.1 kHz "
                "FLAC (24 bit; the v1.6 style kits 16 bit), trimmed, faded and normalised by "
                "`tools/samples/build_drum_samples.py` (the Lo-Fi kit is additionally reduced to 12 bit / 26 kHz). "
                "Built-in kit names describe a sound style; they do not imply endorsement by any manufacturer.\n\n"
                "v1.6 kits: House, Techno, Boom Bap, Trap, Breaks and Disco are fully self-synthesised; Rock and "
                "Afro Latin use VCSL recordings (CC0) plus self-synthesised kick layers; the Studio kick and snare "
                "got extra takes derived from their VCSL recordings.\n\n")
        f.write("| File | Source sample | Source | License |\n|---|---|---|---|\n")
        for (fname, name, url, lic) in sources:
            f.write(f"| {fname} | {name} | {url} | {lic} |\n")
        f.write("\nLicense texts / statements of the sources:\n\n")
        f.write(f"- VCSL: CC0 1.0 Universal - {VCSL_URL}/blob/master/LICENSE\n")
        f.write(f"- TR-808 sample set by Michael Fischer: CC0 1.0 Universal - {FISCHER_URL}/blob/main/LICENSE\n")
        f.write(f"- TR-707 set: \"All samples are public domain, so use em however ya want\" - {OPEN_DRUMS_URL}/TR707WAV/README.txt\n")
        f.write(f"- Sonic Pi samples: \"placed in the public domain via the Creative Commons 0 License\" - {SONICPI_URL}/README.md\n")
        f.write(f"- LinnDrum LM-2: license \"Public Domain\" (from machines.hyperreal.org) - {SAMPLED_URL}/README.md\n")
        f.write("- Self-synthesised sounds (tools/samples/synth_kits.py): CC0 1.0 Universal, dedicated by the livelooper authors\n")
    print(f"{len(table)} samples, {total / 1024 / 1024:.2f} MB")


if __name__ == "__main__":
    main()
