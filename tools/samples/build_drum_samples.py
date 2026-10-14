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

v1.6: the style kits House, Techno, Boom Bap, Trap, Breaks, Rock, Disco and Afro Latin use real
recordings only (no synthesis): acoustic CC0 multi-sample kits from the sfzinstruments project
(Virtuosity Drums, Big Rusty Drums, Swirly Drums, Frankensnare, Body Percussion), the CC0 freesound
samples shipped with Sonic Pi and the CC0 TR-808 set by Michael Fischer. Multi-microphone sources are
summed to mono. Files missing under SRC/sfz are downloaded from GitHub on the first run. The Studio
kick / snare get extra takes derived from their VCSL recordings (pitch, decay, transient variants).

usage: build_drum_samples.py <SRC> <repo root>
"""
import os
import sys

import numpy as np
import soundfile as sf
from scipy.signal import resample_poly

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

RATE = 44100

VCSL_URL = "https://github.com/sgossner/VCSL"
FISCHER_URL = "https://github.com/tidalcycles/sounds-tr808-fischer"
OPEN_DRUMS_URL = "https://github.com/fluid-music/open-drums/tree/main/tr-707"
SONICPI_URL = "https://github.com/sonic-pi-net/sonic-pi/tree/dev/etc/samples"
SAMPLED_URL = "https://github.com/oramics/sampled/tree/master/DM/LM-2"

LIC_CC0 = "CC0 1.0 Universal"
LIC_PD = "Public domain"
SFZ_URL = "https://github.com/sfzinstruments/"
SFZ_REPOS = {   # repo -> (branch, readable name)
    "virtuosity_drums": ("master", "Virtuosity Drums (Versilian Studios / Karoryfer Samples)"),
    "karoryfer.big-rusty-drums": ("main", "Big Rusty Drums (Karoryfer Samples)"),
    "karoryfer.swirly-drums": ("main", "Swirly Drums (Karoryfer Samples)"),
    "karoryfer.frankensnare": ("main", "Frankensnare (Karoryfer Samples)"),
    "body_percussion": ("main", "Body Percussion (sfzinstruments)"),
}

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


# ---------------------------------------------------------------------------------------------
# v1.6 style kits - real recordings only

def fetch(src, repo, rel):
    """Local path of an sfzinstruments sample; downloaded from GitHub when missing."""
    path = os.path.join(src, "sfz", repo, rel)
    if not os.path.exists(path):
        import urllib.parse
        import urllib.request
        os.makedirs(os.path.dirname(path), exist_ok=True)
        url = (f"https://raw.githubusercontent.com/sfzinstruments/{repo}/{SFZ_REPOS[repo][0]}/"
               + urllib.parse.quote(rel))
        with urllib.request.urlopen(url, timeout=60) as r, open(path + ".part", "wb") as f:
            f.write(r.read())
        os.replace(path + ".part", path)
    return path


def mixdown(paths_gains):
    """Sum of several microphones of the same hit (mono, sample-aligned as recorded)."""
    parts = [load(p) * g for (p, g) in paths_gains]
    n = max(len(a) for a in parts)
    out = np.zeros(n)
    for a in parts:
        out[:len(a)] += a
    return out


def crush(x, rate=30000.0, bits=12, drive=1.2):
    """Sampler character for Boom Bap: sample-and-hold, reduced bit depth, gentle saturation."""
    hold = RATE / rate
    idx = (np.floor(np.arange(len(x)) / hold) * hold).astype(int)
    x = x[np.clip(idx, 0, len(x) - 1)]
    q = 2 ** (bits - 1)
    x = np.round(np.clip(x, -1.0, 1.0) * q) / q   # absolute scale: velocity layers keep their levels
    return np.tanh(x * drive) / np.tanh(drive)


def style_kits(src):
    sp_dir = os.path.join(src, "sonicpi")
    e = os.path.join(src, "808")

    def sfz(repo, mics, rel_fmt, vel, gains=None, post=None, label=None):
        """One entry from an sfzinstruments kit; rel_fmt contains {mic} for multi-mic sources."""
        gains = gains or [1.0] * len(mics)
        rels = [rel_fmt.format(mic=m) for m in mics]
        a = mixdown([(fetch(src, repo, r), g) for r, g in zip(rels, gains)])
        if post is not None:
            a = post(a)
        mic_txt = " + ".join(f"{r}" for r in rels)
        return (vel, a, f"{SFZ_REPOS[repo][1]}: {label or mic_txt}", SFZ_URL + repo, LIC_CC0)

    def sp(f, fs, vel, post=None, note=""):
        a = load(os.path.join(sp_dir, f))
        if post is not None:
            a = post(a)
        return (vel, a, f"Sonic Pi {f} (freesound {fs}){note}", SONICPI_URL, LIC_CC0)

    def tr8(d, f, vel, post=None, note=""):
        a = load(os.path.join(e, d, f))
        if post is not None:
            a = post(a)
        return (vel, a, f"TR-808 (M. Fischer) {d}/{f}{note}", FISCHER_URL, LIC_CC0)

    def take(pitch, decay, punch):
        return lambda a: variant(a, pitch, decay, punch)

    VD, BRD, SW, FS, BP = ("virtuosity_drums", "karoryfer.big-rusty-drums", "karoryfer.swirly-drums",
                           "karoryfer.frankensnare", "body_percussion")
    ss = lambda f: f"Samples/body/{f}"   # noqa: E731

    kits = {}

    # ---- House: tight electronic kick (freesound CC0 via Sonic Pi), real hand claps, crisp hats
    kits["house"] = {
        "kick":  [sp("bd_fat.flac", "124386 by cubix", 0.4, take(1.0, 4.0, 0.0), ", softer take"),
                  sp("bd_fat.flac", "124386 by cubix", 0.4, take(0.99, 3.0, 0.1), ", softer take 2"),
                  sp("bd_fat.flac", "124386 by cubix", 0.7),
                  sp("bd_gas.flac", "89460 by menegass", 0.7),
                  sp("bd_fat.flac", "124386 by cubix", 1.0, take(1.0, 0.0, 0.6), ", accent take"),
                  sp("bd_gas.flac", "89460 by menegass", 1.0, take(1.0, 0.0, 0.6), ", accent take"),
                  sp("bd_fat.flac", "124386 by cubix", 1.0, take(1.015, 0.5, 0.4), ", accent take 2")],
        "snare": [sp("sn_zome.flac", "55232 by Dolfeus", 0.5), sp("sn_dolf.flac", "57534 by Dolfeus", 0.8),
                  sp("sn_zome.flac", "55232 by Dolfeus", 1.0, take(1.0, 0.0, 0.5), ", accent take")],
        "clap":  [sfz(FS, [""], "Samples/xtra_claps/clap_stack_rr%d.flac" % i, 0.8) for i in (1, 2, 3, 4)],
        "hatc":  [sp("hat_tap.flac", "674296 by TheEndOfACycle", 0.5), sp("hat_tap.flac", "674296 by TheEndOfACycle", 0.8, take(1.02, 0.0, 0.3), ", take 2"),
                  sp("hat_snap.flac", "674294 by TheEndOfACycle", 1.0)],
        "hato":  [sp("drum_cymbal_open.flac", "100055 by menegass", 0.8)],
        "rim":   [sp("elec_wood.flac", "13135 by looppool", 0.8)],
        "shaker":[sfz(VD, ["mid"], "Samples/perc/{mic}/shaker/LShaker_Shake1D_rr4_Mid.wav", 0.8),
                  sfz(VD, ["mid"], "Samples/perc/{mic}/shaker/LShaker_Shake1U_rr4_Mid.wav", 0.8)],
        "tom":   [sp("drum_tom_mid_soft.flac", "100067 by menegass", 0.6), sp("drum_tom_mid_hard.flac", "100066 by menegass", 1.0)],
    }

    # ---- Techno: harder, brighter kicks, metallic hats, electronic snares
    kits["techno"] = {
        "kick":  [sp("bd_sone.flac", "223090 by soneproject", 0.4, take(1.0, 3.0, 0.0), ", softer take"),
                  sp("bd_zome.flac", "55231 by Dolfeus", 0.4, take(1.0, 3.0, 0.0), ", softer take"),
                  sp("bd_sone.flac", "223090 by soneproject", 0.7),
                  sp("bd_zome.flac", "55231 by Dolfeus", 0.7),
                  sp("bd_sone.flac", "223090 by soneproject", 1.0, take(1.0, 0.0, 0.7), ", accent take"),
                  sp("bd_zome.flac", "55231 by Dolfeus", 1.0, take(1.0, 0.0, 0.7), ", accent take"),
                  sp("bd_sone.flac", "223090 by soneproject", 1.0, take(0.985, 0.5, 0.5), ", accent take 2")],
        "snare": [sp("elec_snare.flac", "13146 by looppool", 0.6), sp("sn_dub.flac", "173142 by Adriak909", 1.0)],
        "clap":  [sfz(FS, [""], "Samples/xtra_claps/clap_person3_rr%d.flac" % i, 0.8) for i in (1, 2, 3)],
        "hatc":  [sp("hat_mess.flac", "363130 by cabled_mess", 0.5), sp("hat_raw.flac", "339278 by cabled_mess", 0.8),
                  sp("hat_metal.flac", "262516 by Stereo surgeon", 1.0)],
        "hato":  [sp("hat_gump.flac", "96144 by zgump", 0.8), sp("hat_noiz.flac", "177909 by medetix", 0.8)],
        "rim":   [sp("elec_pop.flac", "13134 by looppool", 0.6), sp("elec_tick.flac", "13113 by looppool", 1.0)],
        "shaker":[sp("hat_sci.flac", "250530 by waveplaySFX", 0.8), sp("hat_cab.flac", "363203 by cabled_mess", 0.8)],
        "tom":   [sp("elec_fuzz_tom.flac", "13140 by looppool", 0.6, take(1.0, 2.0, 0.0), ", softer take"),
                  sp("elec_fuzz_tom.flac", "13140 by looppool", 1.0)],
    }

    # ---- Boom Bap: jazz kit through the "lofi" microphone of Virtuosity Drums, sampler crunch, finger snaps
    def vd_lofi(drum, art, vls, vel, rr=None):
        out = []
        for v in vls:
            f = (f"Samples/lofi/{drum}/lofi_{drum}_{art}_vl{v}" + (f"_rr{rr}" if rr else "") + ".flac")
            out.append(sfz(VD, [""], f, vel, post=crush))
        return out
    kits["boombap"] = {
        "kick":  [sfz(VD, ["lofi", "kickmic"], "Samples/{mic}/kick/{mic}_kick_snoff_vl%d_rr%d.flac" % (v, r), vel, [1.0, 0.5], post=crush)
                  for (v, r, vel) in [(2, 1, 0.4), (2, 2, 0.4), (3, 1, 0.7), (3, 2, 0.7), (4, 1, 1.0), (4, 2, 1.0), (4, 3, 1.0)]],
        "snare": [sfz(VD, ["lofi", "snaremic"], "Samples/{mic}/snare/{mic}_snare_rimshot_vl%d.flac" % v, vel, [1.0, 0.5], post=crush)
                  for (v, vel) in [(4, 0.35), (5, 0.35), (8, 0.7), (9, 0.7), (11, 1.0), (12, 1.0)]],
        "clap":  [sfz(BP, [""], ss(f"snap_{lr}_rr{r}.wav"), 0.8) for (lr, r) in [("l", 1), ("r", 2), ("l", 3), ("r", 4)]],
        "hatc":  [sfz(VD, [""], f"Samples/lofi/hh/lofi_hh_closed_vl{v}_rr{r}.flac", vel, post=crush)
                  for (v, r, vel) in [(2, 1, 0.4), (2, 2, 0.4), (3, 1, 0.7), (3, 2, 0.7), (4, 1, 1.0), (4, 2, 1.0)]],
        "hato":  [sfz(VD, [""], f"Samples/lofi/hh/lofi_hh_half_vl{v}_rr{r}.flac", 0.8, post=crush) for (v, r) in [(3, 1), (4, 1)]],
        "rim":   vd_lofi("snare", "crossstick", [8, 14], 0.8),
        "shaker":[sfz(VD, ["close"], "Samples/perc/{mic}/shaker/LShaker_Shake1D_rr2_Close.wav", 0.8, post=crush),
                  sfz(VD, ["close"], "Samples/perc/{mic}/shaker/LShaker_Shake1U_rr2_Close.wav", 0.8, post=crush)],
        "tom":   vd_lofi("htom", "center", [10, 14], 0.8),
    }

    # ---- Trap: long 808 booms, crisp modern hats, layered clap
    def layered_clap(i):
        clap = load(os.path.join(e, "cp8", "CP.WAV"))
        real = load(fetch(src, BP, ss(f"handclap_vl2_rr{i}.wav")))
        n = max(len(clap), len(real))
        m = np.zeros(n)
        m[:len(clap)] += clap / (np.max(np.abs(clap)) + 1e-12)
        m[:len(real)] += 0.7 * real / (np.max(np.abs(real)) + 1e-12)
        return (0.8, m, f"TR-808 (M. Fischer) cp8/CP.WAV layered with Body Percussion (sfzinstruments): Samples/body/handclap_vl2_rr{i}.wav",
                FISCHER_URL + " + " + SFZ_URL + BP, LIC_CC0)
    kits["trap"] = {
        "kick":  [tr8("bd8", "BD0075.WAV", 0.4), tr8("bd8", "BD1075.WAV", 0.4),
                  tr8("bd8", "BD2575.WAV", 0.7), tr8("bd8", "BD1075.WAV", 0.7, take(0.98, 0.0, 0.3), ", take 2"),
                  sp("bd_808.flac", "208447 by EKVelika", 1.0), tr8("bd8", "BD2575.WAV", 1.0, take(1.0, 0.0, 0.6), ", accent take"),
                  tr8("bd8", "BD0075.WAV", 1.0, take(1.0, 0.0, 0.8), ", accent take")],
        "snare": [sp("sn_generic.flac", "415582 by hullum", 0.6), sp("elec_hi_snare.flac", "13125 by looppool", 1.0)],
        "clap":  [layered_clap(i) for i in (1, 2, 3)],
        "hatc":  [sp("hat_tap.flac", "674296 by TheEndOfACycle", 0.5), sp("hat_zap.flac", "674295 by TheEndOfACycle", 0.8),
                  sp("hat_snap.flac", "674294 by TheEndOfACycle", 1.0)],
        "hato":  [tr8("oh8", "OH75.WAV", 0.8), sp("hat_cats.flac", "674293 by TheEndOfACycle", 0.8)],
        "rim":   [tr8("rs8", "RS.WAV", 0.8)],
        "shaker":[tr8("ma8", "MA.WAV", 0.8)],
        "tom":   [tr8("mt8", "MT75.WAV", 0.8), tr8("lt8", "LT75.WAV", 0.8)],
    }

    # ---- Breaks: the Virtuosity house kit with close + room microphones, real hand claps
    def vd(drum, art, v, vel, mics, gains, rr=None):
        f = "Samples/{mic}/" + drum + "/{mic}_" + drum + "_" + art + f"_vl{v}" + (f"_rr{rr}" if rr else "") + ".flac"
        return sfz(VD, mics, f, vel, gains)
    kits["breaks"] = {
        "kick":  [vd("kick", "snon", v, vel, ["kickmic", "room", "oh"], [1.0, 0.8, 0.4], rr)
                  for (v, rr, vel) in [(2, 1, 0.4), (2, 2, 0.4), (3, 1, 0.7), (3, 2, 0.7), (4, 1, 1.0), (4, 2, 1.0), (4, 3, 1.0)]],
        "snare": [vd("snare", "center", v, vel, ["snaremic", "room", "oh"], [1.0, 0.8, 0.4])
                  for (v, vel) in [(14, 0.35), (16, 0.35), (24, 0.7), (26, 0.7), (32, 1.0), (34, 1.0), (36, 1.0)]],
        "clap":  [sfz(BP, [""], ss(f"handclap_vl2_rr{r}.wav"), 0.8) for r in (4, 5, 6)],
        "hatc":  [vd("hh", "closed", v, vel, ["oh", "room"], [1.0, 0.5], rr)
                  for (v, rr, vel) in [(2, 1, 0.4), (2, 2, 0.4), (3, 1, 0.7), (3, 2, 0.7), (4, 1, 1.0), (4, 2, 1.0)]],
        "hato":  [vd("hh", "open", v, 0.8, ["oh", "room"], [1.0, 0.5], rr) for (v, rr) in [(3, 1), (4, 1), (4, 2)]],
        "rim":   [vd("snare", "crossstick", v, vel, ["snaremic", "room"], [1.0, 0.6]) for (v, vel) in [(8, 0.6), (14, 1.0)]],
        "shaker":[sfz(VD, ["room"], "Samples/perc/{mic}/tambourine/Tamb1_Shake_rr%d_Far.wav" % r, 0.8) for r in (1, 2)],
        "tom":   [vd("ltom", "center", v, vel, ["oh", "room"], [1.0, 0.7]) for (v, vel) in [(8, 0.6), (10, 0.6), (14, 1.0), (16, 1.0)]],
    }

    # ---- Rock: Big Rusty Drums (24" kick, 14" snare rimshots, 14" hats, 18" floor tom), close + overhead mics
    def brd(rel_fmt, vel, mics, gains):
        return sfz(BRD, mics, rel_fmt, vel, gains)
    kits["rock"] = {
        "kick":  [brd("Samples/kick_24_nodamp/kick/{mic}/k_nodamp_vl%d_rr%d.flac" % (v, r), vel, ["kick", "oh"], [1.0, 0.5])
                  for (v, r, vel) in [(2, 1, 0.4), (2, 2, 0.4), (4, 1, 0.7), (4, 2, 0.7), (6, 1, 1.0), (6, 2, 1.0), (6, 3, 1.0)]],
        "snare": [brd("Samples/snare_14/rimshot/{mic}/sn_rims_vl%d_rr%d.flac" % (v, r), vel, ["top", "btm", "oh"], [1.0, 0.3, 0.6])
                  for (v, r, vel) in [(2, 1, 0.35), (2, 2, 0.35), (4, 1, 0.7), (4, 2, 0.7), (6, 1, 1.0), (6, 2, 1.0), (6, 3, 1.0)]],
        "clap":  [sfz(FS, [""], "Samples/xtra_claps/clap_person2_rr%d.flac" % i, 0.8) for i in (1, 2, 3)],
        "hatc":  [brd("Samples/hihat_14/tc/{mic}/ht_tc_vl%d_rr%d.flac" % (v, r), vel, ["cl", "oh"], [1.0, 0.5])
                  for (v, r, vel) in [(3, 1, 0.4), (3, 2, 0.4), (6, 1, 0.7), (6, 2, 0.7), (8, 1, 1.0), (8, 2, 1.0)]],
        "hato":  [brd("Samples/hihat_14/open/{mic}/ht_open_vl%d_rr%d.flac" % (v, r), 0.8, ["cl", "oh"], [1.0, 0.5])
                  for (v, r) in [(4, 1), (6, 1), (6, 2)]],
        "rim":   [brd("Samples/snare_14/sidestick/{mic}/sn_ss_vl%d_rr1.flac" % v, vel, ["top", "oh"], [1.0, 0.4]) for (v, vel) in [(2, 0.6), (4, 1.0)]],
        "shaker":[sfz(FS, [""], "Samples/xtra_tamb/tamb_vl%d_rr1.flac" % v, vel) for (v, vel) in [(3, 0.6), (5, 1.0)]],
        "tom":   [brd("Samples/tom_18/center/{mic}/t18_vl%d_rr%d.flac" % (v, r), vel, ["cl", "oh"], [1.0, 0.5])
                  for (v, r, vel) in [(4, 1, 0.6), (4, 2, 0.6), (7, 1, 1.0), (7, 2, 1.0)]],
    }

    # ---- Disco: damped, dry 70s sound - close-miked damped kick, muted snare, tight hats, tambourine
    kits["disco"] = {
        "kick":  [brd("Samples/kick_24/kick/{mic}/k_vl%d_rr%d.flac" % (v, r), vel, ["kick"], [1.0])
                  for (v, r, vel) in [(5, 1, 0.4), (5, 2, 0.4), (9, 1, 0.7), (9, 2, 0.7), (13, 1, 1.0), (13, 2, 1.0), (14, 1, 1.0)]],
        "snare": [sfz(FS, ["top", "oh"], "Samples/14p_muted/{mic}/sm_vl%d_rr%d.flac" % (v, r), vel, [1.0, 0.4])
                  for (v, r, vel) in [(3, 1, 0.35), (3, 2, 0.35), (5, 1, 0.7), (5, 2, 0.7), (7, 1, 1.0), (7, 2, 1.0)]],
        "clap":  [sfz(FS, [""], "Samples/xtra_claps/clap_person1_rr%d.flac" % i, 0.8) for i in (1, 2, 3)],
        "hatc":  [sfz(SW, [""], "Samples/hat_tight/hh_tight_vl%d_rr%d.wav" % (v, r), vel)
                  for (v, r, vel) in [(5, 1, 0.4), (5, 2, 0.4), (9, 1, 0.7), (9, 2, 0.7), (13, 1, 1.0), (13, 2, 1.0)]],
        "hato":  [sfz(SW, [""], "Samples/hat_open/hh_open_vl%d_rr%d.wav" % (v, r), 0.8) for (v, r) in [(8, 1), (12, 1), (12, 2)]],
        "rim":   [vd("snare", "crossstick", v, vel, ["snaremic"], [1.0]) for (v, vel) in [(6, 0.6), (12, 1.0)]],
        "shaker":[sfz(VD, ["close"], "Samples/perc/{mic}/tambourine/Tamb1_Shake_rr%d_Close.wav" % r, 0.8) for r in (3, 4, 5)],
        "tom":   [sfz(SW, [""], "Samples/tom_mhi/tom_mhi_vl%d_rr%d.wav" % (v, r), vel) for (v, r, vel) in [(6, 1, 0.6), (6, 2, 0.6), (10, 1, 1.0), (10, 2, 1.0)]],
    }

    # ---- Afro Latin: real hand percussion. voices: kick (cajon bass), bongo, claves, shaker,
    #      tambourine, cowbell, cabasa, conga
    def perc(inst, f, vel):
        return sfz(VD, ["mid"], f"Samples/perc/{{mic}}/{inst}/{f}_Mid.wav", vel)
    kits["afrolatin"] = {
        "kick":  [sfz(SW, ["front", "rear"], "Samples/cajon_kick/cajon_kick_vl%d_rr%d_{mic}.wav" % (v, r), vel, [0.6, 1.0])
                  for (v, r, vel) in [(5, 1, 0.4), (5, 2, 0.4), (9, 1, 0.7), (9, 2, 0.7), (13, 1, 1.0), (13, 2, 1.0), (13, 3, 1.0)]],
        "snare": [perc("bongoh", f"BongoH_Hit1_v{v}_rr{r}", vel) for (v, r, vel) in [(1, 1, 0.35), (1, 2, 0.35), (2, 1, 0.7), (2, 2, 0.7), (3, 1, 1.0), (3, 2, 1.0)]],
        "clap":  [perc("claves", f"Claves1_Hit_v{v}_rr{r}", vel) for (v, r, vel) in [(1, 1, 0.4), (2, 1, 0.7), (2, 2, 0.7), (3, 1, 1.0), (3, 2, 1.0)]],
        "hatc":  [perc("shaker", f"LShaker_Shake1{d}_rr{r}", 0.8) for (d, r) in [("D", 1), ("U", 1), ("D", 2), ("U", 2), ("D", 3), ("U", 3)]],
        "hato":  [perc("tambourine", f"Tamb1_Shake_rr{r}", 0.8) for r in (6, 7, 8)],
        "rim":   [perc("cowbell", f"Cowbell1_Normal_v{v}_rr{r}", vel) for (v, r, vel) in [(2, 1, 0.6), (3, 1, 1.0), (3, 2, 1.0)]],
        "shaker":[perc("cabasa", f"Cabasa1_Rub_v{v}_rr1", vel) for (v, vel) in [(1, 0.6), (2, 1.0)]],
        "tom":   [perc("conga", f"Conga_22_HitN_{rng}_rr{r}", vel)
                  for (rng, r, vel) in [("1_50", 1, 0.4), ("1_50", 2, 0.4), ("51_100", 1, 0.7), ("51_100", 2, 0.7), ("101_127", 1, 1.0), ("101_127", 2, 1.0)]],
    }
    return kits



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

    def studio_takes(files, voice):
        # v1.6: one extra take per recording (pitch / decay / transient variant)
        out = []
        for i, (vel, f) in enumerate(files):
            a = load(os.path.join(v, f))
            pitch = [0.97, 1.03, 0.985, 1.02][i % 4]
            out.append((vel, variant(a, pitch, [1.5, 3.0, 0.5, 2.2][i % 4], [0.5, 0.2, 0.8, 0.3][i % 4]),
                        f"VCSL {f}, derived take (pitch x{pitch})", VCSL_URL, LIC_CC0))
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
        # ---- v1.6 style kits: real recordings only (CC0), see style_kits()
        **style_kits(src),
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


def align_onset(x):
    """Starts the sample shortly before its main transient (shakers and tambourines swell in),
    so hits sit on the grid."""
    peak = np.max(np.abs(x)) + 1e-12
    onset = int(np.argmax(np.abs(x) > peak * 0.3))
    start = max(0, onset - int(0.004 * RATE))
    if start <= int(0.006 * RATE):
        return x
    y = x[start:].copy()
    fi = min(len(y), int(0.002 * RATE))
    y[:fi] *= np.linspace(0.0, 1.0, fi)
    return y


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
            style = k not in ("studio", "808", "909style", "lofi")
            audio = [trim(load(p) if isinstance(p, str) else np.asarray(p, dtype=np.float64), maxlen, k == "lofi")
                     for (_, p, _, _, _) in entries]
            if style:
                audio = [align_onset(a) for a in audio]
            peak = max(np.max(np.abs(a)) for a in audio) + 1e-12
            for i, ((vel, path, name, url, lic), a) in enumerate(zip(entries, audio)):
                if style:
                    # v1.6 style kits: velocity layers keep their order but half their level spread in dB
                    # (the engine scales by velocity on top)
                    own = np.max(np.abs(a)) + 1e-12
                    a = a / own * np.sqrt(own / peak) * 10 ** (-1.0 / 20)
                else:
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
        f.write("Every drum sample in livelooper is a real recording from a source released under **CC0 1.0** or "
                "into the **public domain**. The files here were converted to mono 44.1 kHz FLAC (24 bit; the v1.6 "
                "style kits 16 bit), trimmed, faded and normalised by `tools/samples/build_drum_samples.py`; sources "
                "recorded with several microphones were mixed down to mono, some takes were derived from a recording "
                "(pitch / decay / transient variant, marked as such), the Lo-Fi kit is reduced to 12 bit / 26 kHz and "
                "the Boom Bap kit to 12 bit / 30 kHz. Built-in kit names describe a sound style; they do not imply "
                "endorsement by any manufacturer.\n\n"
                "v1.6 style kits: House and Techno use freesound.org CC0 samples (as shipped with Sonic Pi) plus real "
                "hand claps; Trap uses the CC0 TR-808 set by Michael Fischer, freesound CC0 hats and real claps; Boom "
                "Bap, Breaks and Disco use the CC0 acoustic kit Virtuosity Drums; Rock uses Big Rusty Drums; Disco and "
                "Afro Latin also use Swirly Drums and Frankensnare; Afro Latin is real hand percussion (cajon, bongo, "
                "claves, shaker, tambourine, cowbell, cabasa, conga). No sound is synthesised.\n\n")
        f.write("| File | Source sample | Source | License |\n|---|---|---|---|\n")
        for (fname, name, url, lic) in sources:
            f.write(f"| {fname} | {name} | {url} | {lic} |\n")
        f.write("\nLicense texts / statements of the sources:\n\n")
        f.write(f"- VCSL: CC0 1.0 Universal - {VCSL_URL}/blob/master/LICENSE\n")
        f.write(f"- TR-808 sample set by Michael Fischer: CC0 1.0 Universal - {FISCHER_URL}/blob/main/LICENSE\n")
        f.write(f"- TR-707 set: \"All samples are public domain, so use em however ya want\" - {OPEN_DRUMS_URL}/TR707WAV/README.txt\n")
        f.write(f"- Sonic Pi samples: \"placed in the public domain via the Creative Commons 0 License\" - {SONICPI_URL}/README.md\n")
        f.write(f"- LinnDrum LM-2: license \"Public Domain\" (from machines.hyperreal.org) - {SAMPLED_URL}/README.md\n")
        for repo, (branch, name) in SFZ_REPOS.items():
            lic = "license" if repo in ("karoryfer.swirly-drums", "karoryfer.frankensnare") else "LICENSE"
            f.write(f"- {name}: CC0 1.0 Universal - {SFZ_URL}{repo}/blob/{branch}/{lic}\n")
    print(f"{len(table)} samples, {total / 1024 / 1024:.2f} MB")


if __name__ == "__main__":
    main()
