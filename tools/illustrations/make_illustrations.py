#!/usr/bin/env python3
"""livelooper illustrations (vector, rendered with rsvg-convert).

Draws the in-app help / welcome / about pictures and the Push 1 pictures of the docs in the
livelooper brand style (dark glass, gradient #FF7A18 -> #FF2D6F -> #B14BFF, Manrope).

    python3 tools/illustrations/make_illustrations.py

Writes SVG sources to tools/illustrations/svg/, PNGs for the app to resources/help/ (embedded
as BinaryData) and the Push 1 pictures for the docs to docs/images/.
Needs rsvg-convert and the Manrope fonts (resources/fonts) installed for fontconfig.
The numbered badges of the Push pictures match the legends in src/plugin/HelpGuide.cpp
and docs/en/PUSH.md / docs/de/PUSH.md.
"""
import math, os, subprocess

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SVG_DIR = os.path.join(ROOT, 'tools', 'illustrations', 'svg')
APP_DIR = os.path.join(ROOT, 'resources', 'help')
DOC_DIR = os.path.join(ROOT, 'docs', 'images')
G0, G1, G2 = '#FF7A18', '#FF2D6F', '#B14BFF'
FONT = "font-family='Manrope' "

# ------------------------------------------------------------------ helpers
def defs_common():
    return f"""
<linearGradient id='brand' x1='0' y1='0' x2='1' y2='0'>
  <stop offset='0' stop-color='{G0}'/><stop offset='0.5' stop-color='{G1}'/><stop offset='1' stop-color='{G2}'/></linearGradient>
<linearGradient id='brandD' x1='0' y1='0' x2='1' y2='1'>
  <stop offset='0' stop-color='{G0}'/><stop offset='0.5' stop-color='{G1}'/><stop offset='1' stop-color='{G2}'/></linearGradient>
<linearGradient id='glass' x1='0' y1='0' x2='0' y2='1'>
  <stop offset='0' stop-color='#ffffff' stop-opacity='0.13'/><stop offset='0.5' stop-color='#ffffff' stop-opacity='0.05'/>
  <stop offset='1' stop-color='#ffffff' stop-opacity='0.03'/></linearGradient>
<linearGradient id='spec' x1='0' y1='0' x2='0' y2='1'>
  <stop offset='0' stop-color='#ffffff' stop-opacity='0.35'/><stop offset='1' stop-color='#ffffff' stop-opacity='0'/></linearGradient>
<linearGradient id='body' x1='0' y1='0' x2='0' y2='1'>
  <stop offset='0' stop-color='#34343b'/><stop offset='1' stop-color='#18181d'/></linearGradient>
<linearGradient id='key' x1='0' y1='0' x2='0' y2='1'>
  <stop offset='0' stop-color='#4a4a52'/><stop offset='1' stop-color='#2a2a30'/></linearGradient>
<linearGradient id='lcd' x1='0' y1='0' x2='0' y2='1'>
  <stop offset='0' stop-color='#03080c'/><stop offset='1' stop-color='#0a1a24'/></linearGradient>
<radialGradient id='knob' cx='0.4' cy='0.3' r='0.8'>
  <stop offset='0' stop-color='#6a6a74'/><stop offset='0.6' stop-color='#33333a'/><stop offset='1' stop-color='#1c1c21'/></radialGradient>
<filter id='blur20' x='-50%' y='-50%' width='200%' height='200%'><feGaussianBlur stdDeviation='20'/></filter>
<filter id='blur8' x='-50%' y='-50%' width='200%' height='200%'><feGaussianBlur stdDeviation='8'/></filter>
<filter id='blur4' x='-50%' y='-50%' width='200%' height='200%'><feGaussianBlur stdDeviation='4'/></filter>
"""

def background(w, h, glow=True):
    s = f"<rect width='{w}' height='{h}' fill='#0b0a10'/>"
    if glow:
        for i, (cx, cy, rx, col, op) in enumerate(((0.15, 0.1, 0.55, G0, 0.32), (0.55, 1.0, 0.5, G1, 0.22), (0.95, 0.2, 0.5, G2, 0.30))):
            s += (f"<defs><radialGradient id='bg{i}' cx='0.5' cy='0.5' r='0.5'><stop offset='0' stop-color='{col}' stop-opacity='{op}'/>"
                  f"<stop offset='1' stop-color='{col}' stop-opacity='0'/></radialGradient></defs>"
                  f"<ellipse cx='{w*cx}' cy='{h*cy}' rx='{max(w, h)*rx}' ry='{max(w, h)*rx*0.8}' fill='url(#bg{i})'/>")
    return s

def glass_panel(x, y, w, h, r=28, shadow=True):
    s = ''
    if shadow:
        s += f"<rect x='{x}' y='{y+14}' width='{w}' height='{h}' rx='{r}' fill='#000' opacity='0.5' filter='url(#blur20)'/>"
    s += f"<rect x='{x}' y='{y}' width='{w}' height='{h}' rx='{r}' fill='#1a1922' opacity='0.82'/>"
    s += f"<rect x='{x}' y='{y}' width='{w}' height='{h}' rx='{r}' fill='url(#glass)'/>"
    s += f"<rect x='{x+0.75}' y='{y+0.75}' width='{w-1.5}' height='{h-1.5}' rx='{r}' fill='none' stroke='#ffffff' stroke-opacity='0.16' stroke-width='1.5'/>"
    s += f"<rect x='{x+r*0.6}' y='{y+1.5}' width='{w-r*1.2}' height='{min(h*0.3, 40)}' rx='{r*0.5}' fill='url(#spec)' opacity='0.35'/>"
    return s

def text(x, y, t, size, weight=600, fill='#fff', anchor='start', opacity=1.0, extra=''):
    t = t.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
    return (f"<text x='{x}' y='{y}' {FONT}font-size='{size}' font-weight='{weight}' fill='{fill}' "
            f"text-anchor='{anchor}' opacity='{opacity}' {extra}>{t}</text>")

def svg(w, h, body):
    return (f"<svg xmlns='http://www.w3.org/2000/svg' width='{w}' height='{h}' viewBox='0 0 {w} {h}'>"
            f"<defs>{defs_common()}</defs>{body}</svg>")

def pad(x, y, s, col, lit=True, r=None):
    r = r if r is not None else s * 0.14
    o = ''
    if lit:
        o += f"<rect x='{x-s*0.08}' y='{y-s*0.08}' width='{s*1.16}' height='{s*1.16}' rx='{r*1.3}' fill='{col}' opacity='0.55' filter='url(#blur8)'/>"
    o += f"<rect x='{x}' y='{y}' width='{s}' height='{s}' rx='{r}' fill='{col}'/>"
    o += f"<rect x='{x+s*0.08}' y='{y+s*0.05}' width='{s*0.84}' height='{s*0.32}' rx='{r*0.7}' fill='url(#spec)' opacity='{0.55 if lit else 0.25}'/>"
    return o

def wave_path(x, y, w, h, seed=1.0, n=90):
    pts_top, pts_bot = [], []
    for i in range(n + 1):
        u = i / n
        a = (0.35 + 0.65 * abs(math.sin(u * 7.3 * seed + seed)) * (0.6 + 0.4 * math.sin(u * 31 + seed * 3)))
        a *= 0.25 + 0.75 * (1 - abs(math.sin(u * math.pi * 4 + seed)) ** 6)
        px = x + u * w
        pts_top.append(f"{px:.1f},{y + h/2 - a*h/2:.1f}")
        pts_bot.append(f"{px:.1f},{y + h/2 + a*h/2:.1f}")
    return "M" + " L".join(pts_top) + " L" + " L".join(reversed(pts_bot)) + " Z"

def render(name, content, w, h, outdir, scale=1.0):
    os.makedirs(SVG_DIR, exist_ok=True)
    os.makedirs(outdir, exist_ok=True)
    sp = os.path.join(SVG_DIR, name + '.svg')
    with open(sp, 'w') as f:
        f.write(svg(w, h, content))
    out = os.path.join(outdir, name + '.png')
    subprocess.check_call(['rsvg-convert', '-z', str(scale), '-o', out, sp])
    return out

# ------------------------------------------------------------------ Push 1
PADC = {'red': '#ff3040', 'redDim': '#6a1a20', 'orange': '#ff9228', 'green': '#38dc5a', 'greenDim': '#1d5a2a',
        'yellow': '#f2d232', 'yellowDim': '#5a5020', 'cyan': '#5ae6ff', 'off': '#2c2c33', 'off2': '#36363e',
        'white': '#e8fff0', 'ghost': '#e6c83c'}

STEP_ROWS = ['X...x...X.x.x...', '....X.......X..x', '........g...X...', 'x.x.x.x.x.x.x.x.',
             '..............x.', '.......g.....g..', 'g.g.g.g.g.g.g.g.', '..........x...x.']

def push1(mode='loop'):
    """returns (svg body, w, h, anchors) - the device at (0,0) .. (1000,880)"""
    W, H = 1000, 880
    o = []
    a = {}
    o.append(f"<rect x='0' y='12' width='{W}' height='{H}' rx='38' fill='#000' opacity='0.6' filter='url(#blur20)'/>")
    o.append(f"<rect x='0' y='0' width='{W}' height='{H}' rx='38' fill='url(#body)'/>")
    o.append(f"<rect x='1' y='1' width='{W-2}' height='{H-2}' rx='37' fill='none' stroke='#fff' stroke-opacity='0.12' stroke-width='2'/>")
    o.append(f"<rect x='40' y='3' width='{W-80}' height='60' rx='20' fill='url(#spec)' opacity='0.18'/>")
    def enc(cx, cy, r, ang):
        e = f"<circle cx='{cx}' cy='{cy+3}' r='{r+2}' fill='#000' opacity='0.5' filter='url(#blur4)'/>"
        e += f"<circle cx='{cx}' cy='{cy}' r='{r}' fill='url(#knob)' stroke='#000' stroke-opacity='0.6'/>"
        e += f"<circle cx='{cx}' cy='{cy}' r='{r-5}' fill='none' stroke='#fff' stroke-opacity='0.10'/>"
        e += (f"<line x1='{cx}' y1='{cy}' x2='{cx + math.sin(ang)*(r-6):.1f}' y2='{cy - math.cos(ang)*(r-6):.1f}' "
              f"stroke='#f4f4f8' stroke-width='3' stroke-linecap='round'/>")
        return e
    o.append(enc(110, 58, 18, -0.8)); o.append(enc(165, 58, 18, 0.6))
    for i in range(8):
        o.append(enc(258 + i * 72, 58, 20, -2.2 + 4.4 * ((math.sin(i * 1.3 + 0.4) + 1) / 2)))
    o.append(enc(880, 58, 20, 1.2))
    a['encoders'] = (510, 58); a['tempo'] = (137, 58); a['master_enc'] = (880, 58)
    o.append(f"<rect x='220' y='96' width='572' height='82' rx='8' fill='#000'/>")
    o.append(f"<rect x='224' y='100' width='564' height='74' rx='6' fill='url(#lcd)'/>")
    mono = "font-family='DejaVu Sans Mono' font-size='14' fill='#7fe3ff'"
    if mode == 'step':
        lines = ['Kick  Snare Clap  CHat  OHat  Rim   Shakr Tom', ' 100   90    80    85    70    60    55    75',
                 'STEP  House Jam            1/2', 'KIT House   FILL   DRUMS>BFX  120.0']
    else:
        lines = ['Level  Level  Level  Level  Level          Master Tempo', ' 75     80     100    90     100           100    120.0',
                 '>P     D      P      R      S              M ||||||', '120.0 BPM  Scene 1  Link 2']
    for i, l in enumerate(lines):
        o.append(f"<text x='236' y='{117 + i * 16}' {mono} xml:space='preserve' opacity='{1 if i < 3 else 0.8}'>{l}</text>")
    o.append(f"<rect x='224' y='100' width='564' height='30' rx='6' fill='url(#spec)' opacity='0.12'/>")
    a['display'] = (506, 137)
    for row in range(2):
        for c in range(8):
            bx = 222 + c * 71.25 + 4; by = 192 + row * 30
            if mode == 'loop':
                on = (row == 0 and c == 1) or (row == 1 and c == 2)
                col = '#ff9228' if row == 0 else '#5ae6ff'
            else:
                on = row == 0 and c == 6
                col = '#f2d232'
            o.append(f"<rect x='{bx}' y='{by}' width='63' height='20' rx='5' fill='{col if on else '#3a3a42'}'/>")
            if on:
                o.append(f"<rect x='{bx}' y='{by}' width='63' height='20' rx='5' fill='{col}' opacity='0.6' filter='url(#blur4)'/>")
            o.append(f"<rect x='{bx+3}' y='{by+2}' width='57' height='6' rx='3' fill='#fff' opacity='0.12'/>")
    a['row1'] = (506, 202); a['row2'] = (506, 232)
    gx, gy, side = 222, 266, 570
    gap = 9; ps = (side - 7 * gap) / 8
    if mode == 'loop':
        states = ['play', 'dub', 'play', 'rec', 'stop', None, None, None]
        pos = [3, 1, 5, 2, 0]
        for c in range(8):
            for r in range(8):
                x = gx + c * (ps + gap); y = gy + (7 - r) * (ps + gap)
                st = states[c]; col = PADC['off']; lit = False
                if st:
                    if r == 0:
                        col = {'play': PADC['green'], 'dub': PADC['orange'], 'rec': PADC['red'], 'stop': PADC['greenDim']}[st]; lit = st != 'stop'
                    elif r == 1:
                        col = PADC['yellow'] if st != 'stop' else PADC['yellowDim']; lit = st != 'stop'
                    else:
                        if st in ('play', 'dub') and r - 2 == pos[c]: col, lit = PADC['cyan'], True
                        elif st == 'rec' and r - 2 <= pos[c]: col, lit = '#c02a36', True
                        elif st == 'stop' and r - 2 == 0: col = '#3d6a74'
                        else: col = PADC['off2']
                if mode == 'loop' and c == 7 and r == 7:      # v1.6: free top-right pad = beat flash
                    col, lit = PADC['orange'], True
                elif mode == 'loop' and c == 7 and r == 6:    # v1.6: the pad below = set recording
                    col, lit = PADC['red'], True
                o.append(pad(x, y, ps, col, lit))
        a['pad_beat'] = (gx + 7 * (ps + gap) + ps / 2, gy + ps / 2)
        a['pad_recset'] = (gx + 7 * (ps + gap) + ps / 2, gy + (ps + gap) + ps / 2)
    else:
        for r in range(8):
            patt = STEP_ROWS[r]
            for c in range(8):
                x = gx + c * (ps + gap); y = gy + (7 - r) * (ps + gap)
                ch = patt[c]; lit = True
                if c == 2: col = PADC['green'] if ch != '.' else PADC['white']
                elif ch == 'X': col = PADC['red']
                elif ch == 'x': col = PADC['orange']
                elif ch == 'g': col = PADC['ghost']
                else: col, lit = (PADC['off2'] if c % 4 == 0 else PADC['off']), False
                o.append(pad(x, y, ps, col, lit))
    a['pads_bottom'] = (gx + ps * 0.5, gy + 7 * (ps + gap) + ps / 2)
    a['pads_stop'] = (gx + ps * 0.5, gy + 6 * (ps + gap) + ps / 2)
    a['pads_pos'] = (gx + 2.5 * (ps + gap), gy + 2 * (ps + gap) + ps / 2)
    a['pads'] = (gx + side / 2, gy + side / 2)
    a['pads_play'] = (gx + 2 * (ps + gap) + ps / 2, gy + ps * 0.3)
    o.append(f"<rect x='40' y='{gy}' width='26' height='{side}' rx='13' fill='#101014' stroke='#fff' stroke-opacity='0.10'/>")
    for i in range(24):
        o.append(f"<circle cx='53' cy='{gy + 18 + i * (side - 36) / 23:.1f}' r='2.2' fill='{'#5ae6ff' if i > 15 else '#2a3a44'}'/>")
    a['strip'] = (53, gy + side * 0.5)
    left = ['TAP TEMPO', 'METRONOME', 'UNDO', 'DELETE', 'DOUBLE', 'QUANTIZE', 'FIXED LENGTH', 'AUTOMATION',
            'DUPLICATE', 'NEW', 'RECORD', 'PLAY']
    step = side / len(left)
    for i, n in enumerate(left):
        y = gy + i * step + 6
        lit = (mode == 'loop' and n in ('PLAY', 'METRONOME')) or (mode == 'step' and n == 'METRONOME')
        col = '#38dc5a' if n == 'PLAY' else '#f2d232'
        o.append(f"<rect x='84' y='{y}' width='118' height='{step-12:.1f}' rx='7' fill='url(#key)' stroke='#000' stroke-opacity='0.5'/>")
        o.append(f"<rect x='88' y='{y+2}' width='110' height='8' rx='4' fill='#fff' opacity='0.10'/>")
        o.append(text(143, y + (step - 12) / 2 + 4, n, 10.5, 800, col if lit else '#c9c9d2', 'middle'))
        a['L_' + n] = (84, y + (step - 12) / 2)
    for r in range(8):
        y = gy + r * (ps + gap) + ps * 0.3
        on = mode == 'loop' and r == 0
        o.append(f"<rect x='804' y='{y:.1f}' width='34' height='{ps*0.4:.1f}' rx='6' fill='{'#38dc5a' if on else '#3a3a42'}'/>")
    a['scenes'] = (838, gy + 1.5 * (ps + gap))
    right = [('MASTER', 'STOP'), ('VOLUME', 'PAN & SEND'), ('TRACK', 'CLIP'), ('DEVICE', 'BROWSE'), ('◀', '▶'),
             ('SELECT', 'SHIFT'), ('NOTE', 'SESSION'), ('ADD EFFECT', 'ADD TRACK'), ('OCTAVE ▼', 'OCTAVE ▲'),
             ('REPEAT', 'ACCENT'), ('SCALES', 'USER'), ('MUTE', 'SOLO'), ('IN', 'OUT')]
    rstep = side / len(right)
    for i, (n1, n2) in enumerate(right):
        y = gy + i * rstep + 5
        for j, n in enumerate((n1, n2)):
            x = 856 + j * 64
            lit = (mode == 'loop' and n in ('SESSION', 'USER', 'MASTER')) or (mode == 'step' and n in ('SCALES', 'USER'))
            o.append(f"<rect x='{x}' y='{y}' width='58' height='{rstep-10:.1f}' rx='6' fill='url(#key)' stroke='#000' stroke-opacity='0.5'/>")
            o.append(f"<rect x='{x+3}' y='{y+2}' width='52' height='6' rx='3' fill='#fff' opacity='0.10'/>")
            o.append(text(x + 29, y + (rstep - 10) / 2 + 3.5, n, 8.6 if len(n) > 8 else 9.5, 800, '#ffb35a' if lit else '#c9c9d2', 'middle'))
            a['R_' + n] = (x + (58 if j else 0), y + (rstep - 10) / 2)
    o.append(text(W - 34, H - 18, 'Push 1', 13, 800, '#ffffff', 'end', 0.35))
    return ''.join(o), W, H, a

# (number, anchor, badge dx, dy) - keep in sync with the legends in src/plugin/HelpGuide.cpp and docs PUSH.md
PUSH_BADGES = [
    (1, 'pads_bottom', -30, 0), (2, 'pads_stop', -30, 0), (3, 'pads_pos', 0, -28), (4, 'row1', 300, -6),
    (5, 'row2', 300, 6), (6, 'strip', -30, 0), (7, 'scenes', 0, -40), (8, 'L_PLAY', -40, 0),
    (9, 'L_RECORD', -40, 0), (10, 'L_UNDO', -40, 0), (11, 'L_METRONOME', -40, 0), (12, 'R_SCALES', -22, 40),
    (13, 'R_SHIFT', 22, 0), (14, 'R_REPEAT', -24, -28), (15, 'R_SOLO', 22, 0), (16, 'R_OUT', 22, 0),
    (17, 'encoders', 0, -40), (18, 'display', 300, 0), (19, 'R_OCTAVE ▲', 22, 0), (20, 'R_SESSION', 22, 0),
    (21, 'pad_beat', 0, -44), (22, 'pad_recset', 46, 0),
]
PUSH_BADGES_STEP = [
    (1, 'pads_bottom', -30, 0), (2, 'pads_play', 0, -40), (3, 'row1', 300, -6), (4, 'row2', 300, 6),
    (5, 'R_SCALES', -22, 40), (6, 'R_▶', 22, 0), (7, 'R_IN', -22, 30), (8, 'R_OUT', 22, 0), (9, 'encoders', 0, -40),
    (10, 'R_SHIFT', 22, 0),
]

def push_numbered(mode='loop'):
    body, W, H, a = push1(mode)
    M = 60
    badges = PUSH_BADGES_STEP if mode == 'step' else PUSH_BADGES
    o = [body]
    for num, key, dx, dy in badges:
        x, y = a[key]
        bx, by = x + dx, y + dy
        o.append(f"<line x1='{x}' y1='{y}' x2='{bx}' y2='{by}' stroke='#fff' stroke-width='2' stroke-opacity='0.85'/>")
        o.append(f"<circle cx='{x}' cy='{y}' r='4' fill='#fff'/>")
        o.append(f"<circle cx='{bx}' cy='{by+2}' r='17' fill='#000' opacity='0.5' filter='url(#blur4)'/>")
        o.append(f"<circle cx='{bx}' cy='{by}' r='16' fill='url(#brandD)' stroke='#fff' stroke-width='2'/>")
        o.append(text(bx, by + 5.5, str(num), 15, 800, '#fff', 'middle'))
    return f"<g transform='translate({M},{M})'>{''.join(o)}</g>", W + 2 * M, H + 2 * M

# ------------------------------------------------------------------ feature pictures (1200 x 700)
FW, FH = 1200, 700

def big_pad(cx, cy, r, col, label='', ring=True, phase=0.62):
    o = f"<circle cx='{cx}' cy='{cy+18}' r='{r+20}' fill='#000' opacity='0.6' filter='url(#blur20)'/>"
    o += f"<circle cx='{cx}' cy='{cy}' r='{r+24}' fill='#141419' stroke='#fff' stroke-opacity='0.08' stroke-width='2'/>"
    if ring:
        n = 28
        for i in range(n):
            ang = 2 * math.pi * i / n - math.pi / 2
            on = i / n <= phase
            x1, y1 = cx + math.cos(ang) * (r + 8), cy + math.sin(ang) * (r + 8)
            x2, y2 = cx + math.cos(ang) * (r + 18), cy + math.sin(ang) * (r + 18)
            o += (f"<line x1='{x1:.1f}' y1='{y1:.1f}' x2='{x2:.1f}' y2='{y2:.1f}' stroke='{col if on else '#2c2c34'}' "
                  f"stroke-width='7' stroke-linecap='round'/>")
        o += f"<circle cx='{cx}' cy='{cy}' r='{r+13}' fill='none' stroke='{col}' stroke-width='12' opacity='0.35' filter='url(#blur8)'/>"
    gid = f"pg{int(cx)}_{int(cy)}"
    o += f"<defs><radialGradient id='{gid}' cx='0.45' cy='0.35' r='0.75'><stop offset='0' stop-color='{col}' stop-opacity='0.95'/>"
    o += f"<stop offset='0.7' stop-color='{col}' stop-opacity='0.55'/><stop offset='1' stop-color='#1a1a20'/></radialGradient></defs>"
    o += f"<circle cx='{cx}' cy='{cy}' r='{r}' fill='url(#{gid})'/>"
    o += f"<ellipse cx='{cx}' cy='{cy - r*0.45}' rx='{r*0.62}' ry='{r*0.36}' fill='url(#spec)' opacity='0.55'/>"
    if label:
        o += text(cx, cy + r * 0.12, label, r * 0.26, 800, '#fff', 'middle')
    return o

def feat_loops():
    o = [background(FW, FH)]
    o.append(glass_panel(80, 70, 1040, 560, 40))
    cols = [('#38dc5a', 'PLAY', 0.7), ('#ff9228', 'DUB', 0.35), ('#ff3040', 'REC', 0.9)]
    for i, (c, l, ph) in enumerate(cols):
        cx = 260 + i * 340
        o.append(big_pad(cx, 300, 92, c, l, True, ph))
        o.append(f"<rect x='{cx-130}' y='470' width='260' height='92' rx='18' fill='#07141c' stroke='#fff' stroke-opacity='0.1'/>")
        o.append(f"<path d='{wave_path(cx-116, 484, 232, 64, 1.0 + i * 0.7)}' fill='{c}' opacity='0.85'/>")
        o.append(f"<line x1='{cx-116 + 232*ph:.1f}' y1='480' x2='{cx-116 + 232*ph:.1f}' y2='552' stroke='#fff' stroke-width='3'/>")
    return ''.join(o)

def knob(cx, cy, r, val, col):
    s = math.radians(-135); e = math.radians(135); v = s + (e - s) * val
    def pt(ang, rr): return cx + math.sin(ang) * rr, cy - math.cos(ang) * rr
    x0, y0 = pt(s, r + 12); x1, y1 = pt(e, r + 12); xv, yv = pt(v, r + 12)
    o = f"<path d='M{x0:.1f},{y0:.1f} A{r+12},{r+12} 0 1 1 {x1:.1f},{y1:.1f}' fill='none' stroke='#000' stroke-opacity='0.55' stroke-width='8' stroke-linecap='round'/>"
    large = 1 if (v - s) > math.pi else 0
    o += f"<path d='M{x0:.1f},{y0:.1f} A{r+12},{r+12} 0 {large} 1 {xv:.1f},{yv:.1f}' fill='none' stroke='{col}' stroke-width='8' stroke-linecap='round'/>"
    o += f"<circle cx='{cx}' cy='{cy+8}' r='{r}' fill='#000' opacity='0.55' filter='url(#blur8)'/>"
    o += f"<circle cx='{cx}' cy='{cy}' r='{r}' fill='url(#knob)' stroke='#000' stroke-opacity='0.7' stroke-width='1.5'/>"
    o += f"<circle cx='{cx}' cy='{cy}' r='{r-6}' fill='none' stroke='#fff' stroke-opacity='0.09' stroke-width='2'/>"
    xi, yi = pt(v, r - 10)
    o += f"<line x1='{cx}' y1='{cy}' x2='{xi:.1f}' y2='{yi:.1f}' stroke='#fff' stroke-width='6' stroke-linecap='round'/>"
    return o

def feat_fx():
    o = [background(FW, FH)]
    o.append(glass_panel(80, 70, 1040, 560, 40))
    for i, n in enumerate(['A', 'B', 'C', 'D']):
        x = 150 + i * 130; on = i in (0, 2)
        col = '#5ae6ff' if on else '#3a3a42'
        o.append(f"<rect x='{x}' y='170' width='108' height='108' rx='24' fill='{'#1e3a46' if on else '#2a2a31'}' stroke='{col}' stroke-width='3'/>")
        if on:
            o.append(f"<rect x='{x}' y='170' width='108' height='108' rx='24' fill='none' stroke='{col}' stroke-width='10' opacity='0.4' filter='url(#blur8)'/>")
        o.append(f"<rect x='{x+8}' y='174' width='92' height='30' rx='14' fill='url(#spec)' opacity='0.4'/>")
        o.append(text(x + 54, 236, n, 40, 800, '#fff' if on else '#9a9aa4', 'middle'))
    o.append(knob(750, 230, 62, 0.62, '#5ae6ff')); o.append(knob(930, 230, 62, 0.35, '#ff9228'))
    pts1 = ' '.join(f"{150 + i*4:.0f},{470 + math.sin(i*0.12)*50:.1f}" for i in range(225))
    pts2 = ' '.join(f"{150 + i*4:.0f},{470 + math.sin(i*0.12)*50*(0.4+0.6*abs(math.sin(i*0.03))) + math.sin(i*0.9)*10:.1f}" for i in range(225))
    o.append(f"<polyline points='{pts1}' fill='none' stroke='#ffffff' stroke-opacity='0.25' stroke-width='4'/>")
    o.append(f"<polyline points='{pts2}' fill='none' stroke='url(#brand)' stroke-width='6' stroke-linecap='round'/>")
    return ''.join(o)

def feat_beats():
    o = [background(FW, FH)]
    o.append(glass_panel(70, 60, 1060, 580, 40))
    rows, cols = 8, 16
    x0, y0, cw, ch = 130, 110, 60, 58
    for r in range(rows):
        patt = STEP_ROWS[r]
        o.append(f"<rect x='{x0-28}' y='{y0 + r*ch + 14}' width='6' height='26' rx='3' fill='url(#brandD)'/>")
        for c in range(cols):
            x = x0 + c * cw; y = y0 + r * ch
            u = c / (cols - 1)
            col = ['#ff7a18', '#ff5a3c', '#ff2d6f', '#e13a9a', '#b14bff'][min(4, int(u * 5))]
            if patt[c] != '.':
                o.append(pad(x + 4, y + 4, 48, col, True, 9))
            else:
                o.append(f"<rect x='{x+4}' y='{y+4}' width='48' height='48' rx='9' fill='{'#2e2e36' if c % 4 == 0 else '#25252c'}'/>")
    o.append(f"<rect x='{x0 + 6*cw}' y='{y0-8}' width='{cw}' height='{rows*ch+12}' rx='14' fill='#ffffff' opacity='0.08'/>")
    o.append(f"<rect x='{x0 + 6*cw}' y='{y0-8}' width='{cw}' height='{rows*ch+12}' rx='14' fill='none' stroke='#fff' stroke-opacity='0.5' stroke-width='2'/>")
    return ''.join(o)

def feat_rec():
    o = [background(FW, FH)]
    o.append(glass_panel(80, 70, 1040, 560, 40))
    cx, cy, r = 340, 350, 130
    o.append(f"<circle cx='{cx}' cy='{cy+20}' r='{r+30}' fill='#000' opacity='0.6' filter='url(#blur20)'/>")
    o.append(f"<circle cx='{cx}' cy='{cy}' r='{r+26}' fill='#17171c' stroke='#fff' stroke-opacity='0.12' stroke-width='3'/>")
    o.append(f"<circle cx='{cx}' cy='{cy}' r='{r+10}' fill='none' stroke='#ff3040' stroke-width='22' opacity='0.45' filter='url(#blur20)'/>")
    o.append("<defs><radialGradient id='recg' cx='0.42' cy='0.35' r='0.75'><stop offset='0' stop-color='#ff7070'/>"
             "<stop offset='0.65' stop-color='#ff2a3a'/><stop offset='1' stop-color='#8a0d18'/></radialGradient></defs>")
    o.append(f"<circle cx='{cx}' cy='{cy}' r='{r}' fill='url(#recg)'/>")
    o.append(f"<ellipse cx='{cx}' cy='{cy - r*0.48}' rx='{r*0.6}' ry='{r*0.32}' fill='url(#spec)' opacity='0.6'/>")
    o.append(text(580, 300, '00:42', 110, 800, '#ffffff'))
    o.append(text(586, 350, 'WAV  ·  48 kHz', 26, 600, '#ffffff', 'start', 0.55))
    o.append(f"<rect x='580' y='400' width='480' height='120' rx='22' fill='#07141c' stroke='#fff' stroke-opacity='0.1'/>")
    o.append(f"<path d='{wave_path(600, 418, 440, 84, 2.3)}' fill='url(#brand)'/>")
    return ''.join(o)

def device_icon(cx, cy, kind, s):
    if kind == 'laptop':
        return (f"<rect x='{cx-s}' y='{cy-s*0.65}' width='{2*s}' height='{s*1.25}' rx='{s*0.1}' fill='#22222a' stroke='#fff' stroke-opacity='0.5' stroke-width='3'/>"
                f"<rect x='{cx-s*1.25}' y='{cy+s*0.62}' width='{2.5*s}' height='{s*0.16}' rx='{s*0.08}' fill='#8a8a96'/>")
    if kind == 'phone':
        return f"<rect x='{cx-s*0.42}' y='{cy-s*0.8}' width='{s*0.84}' height='{s*1.6}' rx='{s*0.16}' fill='#22222a' stroke='#fff' stroke-opacity='0.5' stroke-width='3'/>"
    return f"<rect x='{cx-s*0.75}' y='{cy-s*0.95}' width='{s*1.5}' height='{s*1.9}' rx='{s*0.14}' fill='#22222a' stroke='#fff' stroke-opacity='0.5' stroke-width='3'/>"

def feat_link():
    o = [background(FW, FH)]
    o.append(glass_panel(80, 70, 1040, 560, 40))
    cx, cy = 600, 330
    for i, rr in enumerate([115, 170, 225]):
        o.append(f"<circle cx='{cx}' cy='{cy}' r='{rr}' fill='none' stroke='url(#brandD)' stroke-width='{5-i}' opacity='{0.9-i*0.25}'/>")
    o.append(f"<circle cx='{cx}' cy='{cy}' r='86' fill='url(#brandD)'/>")
    o.append(f"<circle cx='{cx}' cy='{cy}' r='86' fill='url(#spec)' opacity='0.35'/>")
    o.append(text(cx, cy + 6, '120.0', 40, 800, '#fff', 'middle'))
    o.append(text(cx, cy + 40, 'BPM', 20, 800, '#fff', 'middle', 0.8))
    for ang, kind in ((200, 'laptop'), (330, 'phone'), (90, 'tablet')):
        x = cx + math.cos(math.radians(ang)) * 300; y = cy + math.sin(math.radians(ang)) * 200
        o.append(f"<line x1='{cx}' y1='{cy}' x2='{x:.0f}' y2='{y:.0f}' stroke='#fff' stroke-opacity='0.25' stroke-width='3' stroke-dasharray='8 10'/>")
        o.append(f"<circle cx='{x:.0f}' cy='{y:.0f}' r='64' fill='#101016' stroke='#fff' stroke-opacity='0.2' stroke-width='2'/>")
        o.append(device_icon(x, y, kind, 32))
    return ''.join(o)

def welcome_hero():
    w, h = 1200, 360
    o = [background(w, h)]
    cols = ['#38dc5a', '#ff9228', '#ff3040', '#5ae6ff', '#b14bff']
    for i, c in enumerate(cols):
        o.append(big_pad(170 + i * 215, 165, 62, c, '', True, [0.8, 0.4, 0.95, 0.6, 0.25][i]))
    o.append(f"<path d='{wave_path(60, 288, 1080, 50, 1.7, 160)}' fill='url(#brand)' opacity='0.8'/>")
    return ''.join(o), w, h

def about_art():
    w, h = 1000, 300
    o = [background(w, h)]
    o.append(f"<path d='{wave_path(40, 90, 920, 120, 2.9, 160)}' fill='url(#brand)' opacity='0.85'/>")
    return ''.join(o), w, h

# ------------------------------------------------------------------ K.O. II (generic drawing, v1.6)
KO_BODY, KO_EDGE, KO_ORANGE, KO_INK = '#d9d6cf', '#b7b2a8', '#ff6a13', '#2b2b2b'
KO_PAD_LABELS = [['7', '8', '9'], ['4', '5', '6'], ['1', '2', '3'], ['.', '0', 'ENTER']]
KO_FUNC_A = [['T7', 'T8', 'ALL'], ['T4', 'T5', 'T6'], ['T1', 'T2', 'T3'], ['STOP', 'UNDO', 'CAPT']]

def ko2(page='A'):
    """generic drawing of a 12-pad sampler / controller with labels - returns (svg body, w, h, anchors)"""
    W, H = 620, 900
    o, a = [], {}
    o.append(f"<rect x='0' y='14' width='{W}' height='{H}' rx='34' fill='#000' opacity='0.55' filter='url(#blur20)'/>")
    o.append(f"<rect x='0' y='0' width='{W}' height='{H}' rx='34' fill='{KO_BODY}' stroke='{KO_EDGE}' stroke-width='3'/>")
    # speaker dots
    for r in range(5):
        for c in range(9):
            o.append(f"<circle cx='{52 + c * 16}' cy='{58 + r * 16}' r='3.2' fill='{KO_INK}' opacity='0.55'/>")
    # display
    o.append(f"<rect x='230' y='40' width='340' height='110' rx='12' fill='#121416'/>")
    o.append(text(250, 84, '3.2   120.0 BPM', 22, 800, '#ff8a3a'))
    o.append(text(250, 120, 'T2 REC    CLK > KO', 17, 600, '#ff8a3a', opacity=0.85))
    a['display'] = (400, 95)
    # knobs X / Y
    for i, n in enumerate(('X', 'Y')):
        cx = 300 + i * 120
        o.append(f"<circle cx='{cx}' cy='212' r='34' fill='#a9a49a' stroke='{KO_EDGE}' stroke-width='3'/>")
        o.append(f"<line x1='{cx}' y1='212' x2='{cx + 18}' y2='190' stroke='{KO_INK}' stroke-width='4' stroke-linecap='round'/>")
        o.append(text(cx, 268, n, 15, 800, KO_INK, 'middle', 0.6))
    a['knob'] = (300, 212)
    # small function buttons
    for i, n in enumerate(('SOUND', 'MAIN', 'TEMPO', 'KEYS', 'FX', 'ERASE')):
        x = 54 + i * 88
        o.append(f"<rect x='{x}' y='298' width='70' height='34' rx='8' fill='#c7c2b8' stroke='{KO_EDGE}' stroke-width='2'/>")
        o.append(text(x + 35, 320, n, 11, 800, KO_INK, 'middle', 0.65))
    a['fx'] = (54 + 4 * 88 + 35, 315)
    # group buttons A - D and the fader on the left
    for i, g in enumerate('ABCD'):
        y = 380 + i * 96
        on = g == page
        o.append(f"<rect x='48' y='{y}' width='70' height='70' rx='14' fill='{KO_ORANGE if on else '#c7c2b8'}' stroke='{KO_EDGE}' stroke-width='2'/>")
        o.append(text(83, y + 44, g, 26, 800, '#fff' if on else KO_INK, 'middle'))
        a['group_' + g] = (118, y + 35)
    o.append(f"<rect x='148' y='384' width='26' height='370' rx='13' fill='#b9b4aa'/>")
    o.append(f"<rect x='136' y='520' width='50' height='26' rx='6' fill='#8f8a80'/>")
    a['fader'] = (161, 600)
    # 3 x 4 pads
    px, py, ps, gap = 214, 380, 116, 14
    cols = {'T1': '#38dc5a', 'T2': '#ff3040', 'T3': '#ff9228', 'T4': '#38dc5a', 'T5': '#1d5a2a'}
    for r in range(4):
        for c in range(3):
            x = px + c * (ps + gap); y = py + r * (ps + gap)
            fn = KO_FUNC_A[r][c]
            col = cols.get(fn, '#3b4250' if fn in ('ALL', 'STOP', 'UNDO', 'CAPT') else '#3a3a3a')
            o.append(f"<rect x='{x}' y='{y + 5}' width='{ps}' height='{ps}' rx='14' fill='#8d887e'/>")
            o.append(f"<rect x='{x}' y='{y}' width='{ps}' height='{ps}' rx='14' fill='{col}'/>")
            o.append(text(x + 12, y + 24, KO_PAD_LABELS[r][c], 15, 800, '#fff', 'start', 0.7))
            o.append(text(x + ps / 2, y + ps / 2 + 16, fn, 17, 800, '#fff', 'middle'))
            a['pad_' + KO_PAD_LABELS[r][c]] = (x + ps / 2, y + ps / 2)
    return ''.join(o), W, H, a

KO_BADGES = [(1, 'pad_2', 0, -70), (2, 'pad_9', 70, -50), (3, 'pad_0', 0, 80), (4, 'group_A', -110, 0),
             (5, 'group_C', -110, 0), (6, 'pad_ENTER', 70, 70), (7, 'knob', -110, 0), (8, 'display', 0, -70)]

def ko2_numbered():
    body, W, H, a = ko2('A')
    M = 120
    o = [body]
    for num, key, dx, dy in KO_BADGES:
        x, y = a[key]
        bx, by = x + dx, y + dy
        o.append(f"<line x1='{x}' y1='{y}' x2='{bx}' y2='{by}' stroke='#fff' stroke-width='2.5' stroke-opacity='0.9'/>")
        o.append(f"<circle cx='{x}' cy='{y}' r='5' fill='#fff'/>")
        o.append(f"<circle cx='{bx}' cy='{by}' r='20' fill='url(#brandD)' stroke='#fff' stroke-width='2.5'/>")
        o.append(text(bx, by + 7, str(num), 19, 800, '#fff', 'middle'))
    return f"<g transform='translate({M},{M // 2})'>{''.join(o)}</g>", W + 2 * M, H + M

# ------------------------------------------------------------------ main
def main():
    body, W, H, a = push1('loop')
    M = 70
    render('livelooper-push1', background(W + 2 * M, H + 2 * M) + f"<g transform='translate({M},{M})'>{body}</g>",
           W + 2 * M, H + 2 * M, DOC_DIR, 1.2)
    for mode in ('loop', 'step'):
        nb, nw, nh = push_numbered(mode)
        content = background(nw, nh) + nb
        render('livelooper-push1-layout' if mode == 'loop' else 'livelooper-push1-step', content, nw, nh, DOC_DIR, 1.2)
        render('help-push1' if mode == 'loop' else 'help-push1-step', content, nw, nh, APP_DIR, 0.9)
    kb, kw, kh = ko2_numbered()
    render('livelooper-ko2-layout', background(kw, kh) + kb, kw, kh, DOC_DIR, 1.0)
    render('help-ko2', background(kw, kh) + kb, kw, kh, APP_DIR, 0.75)
    render('help-loops', feat_loops(), FW, FH, APP_DIR, 0.75)
    render('help-fx', feat_fx(), FW, FH, APP_DIR, 0.75)
    render('help-beats', feat_beats(), FW, FH, APP_DIR, 0.75)
    render('help-rec', feat_rec(), FW, FH, APP_DIR, 0.75)
    render('help-link', feat_link(), FW, FH, APP_DIR, 0.75)
    b, w, h = welcome_hero(); render('welcome-hero', b, w, h, APP_DIR, 0.75)
    b, w, h = about_art(); render('about-art', b, w, h, APP_DIR, 0.7)

if __name__ == '__main__':
    main()
