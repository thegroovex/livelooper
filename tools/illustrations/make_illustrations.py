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

def rpad(x, y, w, h, col, lit=True, r=6):
    o = ''
    if lit:
        o += f"<rect x='{x-4}' y='{y-4}' width='{w+8}' height='{h+8}' rx='{r*1.4}' fill='{col}' opacity='0.5' filter='url(#blur8)'/>"
    o += f"<rect x='{x}' y='{y+2}' width='{w}' height='{h}' rx='{r}' fill='#000' opacity='0.55'/>"
    o += f"<rect x='{x}' y='{y}' width='{w}' height='{h}' rx='{r}' fill='{col}'/>"
    o += f"<rect x='{x+w*0.06}' y='{y+h*0.06}' width='{w*0.88}' height='{h*0.3}' rx='{r*0.7}' fill='url(#spec)' opacity='{0.45 if lit else 0.2}'/>"
    return o

# Push 1 geometry (landscape, as the device sits in front of you) - the same numbers are used by
# src/plugin/ControllerView.cpp (kPush... tables); checked against a photo of a real Push 1.
PUSH_W, PUSH_H = 1000, 790
PUSH_ENC_X0, PUSH_ENC_DX, PUSH_ENC_Y = 181, 77.25, 40
PUSH_LCD = (147, 92, 618, 86)
PUSH_GRID_X, PUSH_COL_DX, PUSH_PAD_W = 151, 76.25, 68
PUSH_ROW1_Y, PUSH_ROW2_Y, PUSH_ROW_H = 196, 230, 24
PUSH_GRID_Y, PUSH_ROW_DY, PUSH_PAD_H = 270, 60, 52
PUSH_SCENE_X, PUSH_SCENE_W = 784, 54
# left column: (label, y, h) top to bottom
PUSH_LEFT = [('Undo', 268, 32), ('Delete', 306, 32), ('Double', 344, 32), ('Quantize', 382, 32),
             ('Fixed Length', 444, 32), ('Automation', 482, 32), ('Duplicate', 520, 32), ('New', 558, 32),
             ('Record', 618, 54), ('Play', 688, 54)]
PUSH_LEFT_X, PUSH_LEFT_W = 14, 62
PUSH_STRIP = (88, 270, 46, 472)
PUSH_TOPLEFT = [('Tap Tempo', 92, 44), ('Metronome', 144, 40)]
PUSH_SMALL_ENC = [(38, 222), (106, 222)]
# right block: two columns, (left label, right label, y, h)
PUSH_RIGHT = [('Volume', 'Pan & Send', 70, 30), ('Track', 'Clip', 106, 30), ('Device', 'Browse', 142, 30),
              ('In', 'Out', 196, 28), ('Mute', 'Solo', 230, 28), ('Scales', 'User', 264, 30),
              ('Repeat', 'Accent', 300, 30), ('Octave Down', 'Octave Up', 336, 36),
              ('Add Effect', 'Add Track', 448, 30), ('Note', 'Session', 484, 30), ('Select', 'Shift', 520, 30)]
PUSH_RIGHT_X, PUSH_RIGHT_W = (852, 912), 54
PUSH_DPAD = (920, 664, 42, 36)        # centre x, centre y, offset, button size
PUSH_SCENE_LABELS = ['1/4', '1/4t', '1/8', '1/8t', '1/16', '1/16t', '1/32', '1/32t']
PRINT, PRINT_LIT = '#c98a48', '#ffb35a'

def push1(mode='loop'):
    """returns (svg body, w, h, anchors) - the device at (0,0) .. (PUSH_W, PUSH_H), landscape"""
    W, H = PUSH_W, PUSH_H
    o = []
    a = {}
    o.append(f"<rect x='0' y='12' width='{W}' height='{H}' rx='26' fill='#000' opacity='0.6' filter='url(#blur20)'/>")
    o.append(f"<rect x='0' y='0' width='{W}' height='{H}' rx='26' fill='#151518'/>")
    o.append(f"<rect x='0' y='0' width='{W}' height='{H}' rx='26' fill='url(#body)' opacity='0.55'/>")
    o.append(f"<rect x='1' y='1' width='{W-2}' height='{H-2}' rx='25' fill='none' stroke='#fff' stroke-opacity='0.12' stroke-width='2'/>")
    o.append(f"<rect x='40' y='3' width='{W-80}' height='50' rx='20' fill='url(#spec)' opacity='0.12'/>")
    o.append(text(22, 50, 'Push 1', 15, 800, '#ffffff', 'start', 0.28))

    def enc(cx, cy, r, ang):
        e = f"<circle cx='{cx}' cy='{cy+3}' r='{r+2}' fill='#000' opacity='0.5' filter='url(#blur4)'/>"
        e += f"<circle cx='{cx}' cy='{cy}' r='{r}' fill='url(#knob)' stroke='#000' stroke-opacity='0.6'/>"
        e += f"<circle cx='{cx}' cy='{cy}' r='{r-5}' fill='none' stroke='#fff' stroke-opacity='0.10'/>"
        e += (f"<line x1='{cx}' y1='{cy}' x2='{cx + math.sin(ang)*(r-6):.1f}' y2='{cy - math.cos(ang)*(r-6):.1f}' "
              f"stroke='#f4f4f8' stroke-width='3' stroke-linecap='round'/>")
        return e

    def key(x, y, w, h, label, lit=False, litcol=PRINT_LIT, size=None, glyph=None):
        k = f"<rect x='{x}' y='{y+2}' width='{w}' height='{h}' rx='5' fill='#000' opacity='0.6'/>"
        k += f"<rect x='{x}' y='{y}' width='{w}' height='{h}' rx='5' fill='#232327' stroke='#000' stroke-opacity='0.7'/>"
        k += f"<rect x='{x+3}' y='{y+2}' width='{w-6}' height='5' rx='2.5' fill='#fff' opacity='0.07'/>"
        if glyph:
            k += glyph
        elif label:
            words = label.split(' ')
            fs = size or (8.4 if max(len(t) for t in words) > 7 else 9)
            lines = words if (len(label) > 8 and len(words) > 1 and h >= 28) else [label]
            if len(lines) > 2:
                lines = [' '.join(words[:-1]), words[-1]]
            for li, t in enumerate(lines):
                yy = y + h / 2 + 3.2 + (li - (len(lines) - 1) / 2) * (fs + 1)
                k += text(x + 6, yy, t, fs, 700, litcol if lit else PRINT, 'start')
        return k

    # encoders: 8 track encoders + master encoder (top), two small encoders top left
    for i in range(9):
        o.append(enc(PUSH_ENC_X0 + i * PUSH_ENC_DX, PUSH_ENC_Y, 21, -2.2 + 4.4 * ((math.sin(i * 1.3 + 0.4) + 1) / 2)))
    a['encoders'] = (PUSH_ENC_X0 + 3.5 * PUSH_ENC_DX, PUSH_ENC_Y); a['master_enc'] = (PUSH_ENC_X0 + 8 * PUSH_ENC_DX, PUSH_ENC_Y)
    for (cx, cy) in PUSH_SMALL_ENC:
        o.append(enc(cx, cy, 16, 0.6))
    a['tempo'] = PUSH_SMALL_ENC[0]
    for (n, y, h) in PUSH_TOPLEFT:
        lit = n == 'Metronome'
        o.append(key(PUSH_LEFT_X, y, PUSH_LEFT_W, h, n, lit))
        a['L_' + n.upper()] = (PUSH_LEFT_X, y + h / 2)

    # display: 4 lines, orange characters
    lx, ly, lw, lh = PUSH_LCD
    o.append(f"<rect x='{lx}' y='{ly}' width='{lw}' height='{lh}' rx='5' fill='#000'/>")
    o.append(f"<rect x='{lx+4}' y='{ly+4}' width='{lw-8}' height='{lh-8}' rx='3' fill='#1f1206'/>")
    mono = "font-family='DejaVu Sans Mono' font-size='14.5' font-weight='bold' fill='#ff9a2e'"
    if mode == 'step':
        lines = ['Kick     Snare    Clap     CHat     OHat     Rim      Shakr    Tom', ' 100      90       80       85       70       60       55       75',
                 'STEP     House    Jam               1/2', 'KIT House   FILL   DRUMS>BFX   120.0']
    else:
        lines = ['Lvl 1    Lvl 2    Lvl 3    Lvl 4    Lvl 5             Master   Tempo', ' 75%      80%     100%      90%     100%              100%    120.0',
                 '>PLAY    DUB      PLAY     REC      STOP              MASTER   BPM', 'livelooper  120.0 BPM   Scene 1   Link 2']
    for i, l in enumerate(lines):
        o.append(f"<text x='{lx+12}' y='{ly + 23 + i * 17}' {mono} xml:space='preserve' opacity='{1 if i < 3 else 0.85}'>{l}</text>")
    o.append(f"<rect x='{lx+4}' y='{ly+4}' width='{lw-8}' height='26' rx='3' fill='url(#spec)' opacity='0.08'/>")
    a['display'] = (lx + lw / 2, ly + lh / 2)

    # two rows of 8 buttons under the display (+ MASTER / STOP at their right end)
    for row in range(2):
        by = PUSH_ROW1_Y if row == 0 else PUSH_ROW2_Y
        for c in range(8):
            bx = PUSH_GRID_X + c * PUSH_COL_DX
            if mode == 'loop':
                on = c < 5
                col = '#ff9228' if row == 0 else '#6a7cff'
                if row == 0 and c == 1: col = '#ffc070'
            else:
                on = row == 0 and c == 6
                col = '#f2d232'
            o.append(f"<rect x='{bx}' y='{by}' width='{PUSH_PAD_W}' height='{PUSH_ROW_H}' rx='5' fill='{col if on else '#3a3a40'}' opacity='{0.9 if on else 1}'/>")
            if on:
                o.append(f"<rect x='{bx}' y='{by}' width='{PUSH_PAD_W}' height='{PUSH_ROW_H}' rx='5' fill='{col}' opacity='0.45' filter='url(#blur4)'/>")
            o.append(f"<rect x='{bx+3}' y='{by+2}' width='{PUSH_PAD_W-6}' height='6' rx='3' fill='#fff' opacity='0.12'/>")
        n = 'Master' if row == 0 else 'Stop'
        o.append(key(PUSH_SCENE_X, by - 2, PUSH_SCENE_W, PUSH_ROW_H + 4, n, mode == 'loop' and n == 'Master'))
        a['R_' + n.upper()] = (PUSH_SCENE_X + PUSH_SCENE_W, by + PUSH_ROW_H / 2)
    a['row1'] = (PUSH_GRID_X + 4 * PUSH_COL_DX, PUSH_ROW1_Y + PUSH_ROW_H / 2)
    a['row2'] = (PUSH_GRID_X + 4 * PUSH_COL_DX, PUSH_ROW2_Y + PUSH_ROW_H / 2)

    # 8 x 8 pads (row 0 = bottom)
    def padxy(c, r):
        return PUSH_GRID_X + c * PUSH_COL_DX, PUSH_GRID_Y + (7 - r) * PUSH_ROW_DY
    if mode == 'loop':
        states = ['play', 'dub', 'play', 'rec', 'stop', None, None, None]
        pos = [3, 1, 5, 2, 0]
        for c in range(8):
            for r in range(8):
                x, y = padxy(c, r)
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
                if c == 7 and r == 7:      # v1.6: free top-right pad = beat flash
                    col, lit = PADC['orange'], True
                elif c == 7 and r == 6:    # v1.6: the pad below = set recording
                    col, lit = PADC['red'], True
                o.append(rpad(x, y, PUSH_PAD_W, PUSH_PAD_H, col, lit))
        x, y = padxy(7, 7); a['pad_beat'] = (x + PUSH_PAD_W / 2, y + PUSH_PAD_H / 2)
        x, y = padxy(7, 6); a['pad_recset'] = (x + PUSH_PAD_W / 2, y + PUSH_PAD_H / 2)
    else:
        for r in range(8):
            patt = STEP_ROWS[r]
            for c in range(8):
                x, y = padxy(c, r)
                ch = patt[c]; lit = True
                if c == 2: col = PADC['green'] if ch != '.' else PADC['white']
                elif ch == 'X': col = PADC['red']
                elif ch == 'x': col = PADC['orange']
                elif ch == 'g': col = PADC['ghost']
                else: col, lit = (PADC['off2'] if c % 4 == 0 else PADC['off']), False
                o.append(rpad(x, y, PUSH_PAD_W, PUSH_PAD_H, col, lit))
    x, y = padxy(0, 0); a['pads_bottom'] = (x + PUSH_PAD_W / 2, y + PUSH_PAD_H / 2)
    x, y = padxy(0, 1); a['pads_stop'] = (x + PUSH_PAD_W / 2, y + PUSH_PAD_H / 2)
    x, y = padxy(2, 5); a['pads_pos'] = (x + PUSH_PAD_W / 2, y + PUSH_PAD_H / 2)
    x, y = padxy(2, 7); a['pads_play'] = (x + PUSH_PAD_W / 2, y + PUSH_PAD_H * 0.3)
    a['pads'] = (PUSH_GRID_X + 4 * PUSH_COL_DX, PUSH_GRID_Y + 4 * PUSH_ROW_DY)

    # touch strip between the left button column and the pads
    sx, sy, sw, sh = PUSH_STRIP
    o.append(f"<rect x='{sx}' y='{sy}' width='{sw}' height='{sh}' rx='6' fill='#0d0d10' stroke='#fff' stroke-opacity='0.08'/>")
    o.append(f"<rect x='{sx+4}' y='{sy+4}' width='{sw-8}' height='{sh-8}' rx='4' fill='url(#spec)' opacity='0.05'/>")
    for i in range(31):
        on = 13 <= i <= 17
        o.append(f"<circle cx='{sx-5}' cy='{sy + 10 + i * (sh - 20) / 30:.1f}' r='1.6' fill='{'#ffa030' if on else '#3a2a1a'}'/>")
    a['strip'] = (sx + sw / 2, sy + sh * 0.5)

    # left button column (bottom to top: Play, Record, New, Duplicate, Automation, Fixed Length, Quantize, Double, Delete, Undo)
    for (n, y, h) in PUSH_LEFT:
        if n == 'Record':
            g = f"<circle cx='{PUSH_LEFT_X + PUSH_LEFT_W/2}' cy='{y + h/2}' r='11' fill='none' stroke='#ff4060' stroke-width='3'/>"
            o.append(key(PUSH_LEFT_X, y, PUSH_LEFT_W, h, '', glyph=g))
        elif n == 'Play':
            cx, cy = PUSH_LEFT_X + PUSH_LEFT_W / 2, y + h / 2
            lit = mode == 'loop'
            g = (f"<path d='M{cx-8},{cy-11} L{cx+11},{cy} L{cx-8},{cy+11} Z' fill='none' stroke='{'#46ff6a' if lit else '#2f8a40'}' "
                 f"stroke-width='3' stroke-linejoin='round'/>")
            if lit:
                g = f"<circle cx='{cx}' cy='{cy}' r='16' fill='#38dc5a' opacity='0.18' filter='url(#blur4)'/>" + g
            o.append(key(PUSH_LEFT_X, y, PUSH_LEFT_W, h, '', glyph=g))
        else:
            o.append(key(PUSH_LEFT_X, y, PUSH_LEFT_W, h, n, False))
        a['L_' + n.upper()] = (PUSH_LEFT_X, y + h / 2)

    # scene buttons (1/4 .. 1/32t) right of the pads
    for r in range(8):
        y = PUSH_GRID_Y + r * PUSH_ROW_DY
        on = mode == 'loop' and r == 0
        o.append(key(PUSH_SCENE_X, y, PUSH_SCENE_W, PUSH_PAD_H, '', glyph=(
            (f"<rect x='{PUSH_SCENE_X+4}' y='{y+4}' width='{PUSH_SCENE_W-8}' height='{PUSH_PAD_H-8}' rx='4' fill='#38dc5a' opacity='0.85'/>" if on else '')
            + text(PUSH_SCENE_X + 6, y + PUSH_PAD_H - 7, PUSH_SCENE_LABELS[r], 8.5, 700, '#0b2a12' if on else PRINT, 'start'))))
    a['scenes'] = (PUSH_SCENE_X + PUSH_SCENE_W / 2, PUSH_GRID_Y + 1.5 * PUSH_ROW_DY)

    # right block: two columns
    for (n1, n2, y, h) in PUSH_RIGHT:
        for j, n in enumerate((n1, n2)):
            x = PUSH_RIGHT_X[j]
            lit = (mode == 'loop' and n in ('Session', 'User')) or (mode == 'step' and n in ('Scales', 'User'))
            glyph = None
            if n in ('In', 'Out'):
                cx, cy = x + PUSH_RIGHT_W / 2, y + h / 2
                d = -1 if n == 'In' else 1
                glyph = (f"<path d='M{cx - 9*d},{cy-4} L{cx + 7*d},{cy-4} M{cx + 7*d},{cy-4} L{cx + 3*d},{cy-8} M{cx + 7*d},{cy-4} L{cx + 3*d},{cy}' "
                         f"stroke='{PRINT}' stroke-width='1.8' fill='none' stroke-linecap='round'/>"
                         + text(cx, cy + 9, n, 7.5, 700, PRINT, 'middle'))
            o.append(key(x, y, PUSH_RIGHT_W, h, n, lit, glyph=glyph))
            a['R_' + n.upper()] = (x + (PUSH_RIGHT_W if j else 0), y + h / 2)
    # 4-way arrows (D-pad) in the bottom right corner
    cx, cy, off, bs = PUSH_DPAD
    for name, dx, dy, ang in (('UP', 0, -1, 0), ('DOWN', 0, 1, 180), ('LEFT', -1, 0, 270), ('RIGHT', 1, 0, 90)):
        x = cx + dx * off - bs / 2; y = cy + dy * off - bs / 2
        o.append(f"<rect x='{x}' y='{y+2}' width='{bs}' height='{bs}' rx='6' fill='#000' opacity='0.6'/>")
        o.append(f"<rect x='{x}' y='{y}' width='{bs}' height='{bs}' rx='6' fill='#232327' stroke='#000' stroke-opacity='0.7'/>")
        o.append(f"<path d='M-6,4 L0,-5 L6,4' transform='translate({cx + dx*off},{cy + dy*off}) rotate({ang})' stroke='{PRINT}' stroke-width='2' fill='none' "
                 f"stroke-linecap='round' stroke-linejoin='round'/>")
        a['R_' + name] = (cx + dx * off + dx * bs / 2, cy + dy * off + dy * bs / 2)
    a['R_▶'] = a['R_RIGHT']; a['R_◀'] = a['R_LEFT']
    a['R_OCTAVE ▲'] = a['R_OCTAVE UP']
    return ''.join(o), W, H, a

# (number, anchor, badge dx, dy) - keep in sync with the legends in src/plugin/HelpGuide.cpp and docs PUSH.md
PUSH_BADGES = [
    (1, 'pads_bottom', 0, 96), (2, 'pads_stop', 36, -14), (3, 'pads_pos', 36, -14), (4, 'row1', 0, -14),
    (5, 'row2', 0, 14), (6, 'strip', 0, 0), (7, 'scenes', 0, 0), (8, 'L_PLAY', -44, 0),
    (9, 'L_RECORD', -44, 0), (10, 'L_UNDO', -44, 0), (11, 'L_METRONOME', -44, 0), (12, 'R_SCALES', -36, 0),
    (13, 'R_SHIFT', 44, 0), (14, 'R_ACCENT', 44, 0), (15, 'R_SOLO', 44, 0), (16, 'R_OUT', 44, 0),
    (17, 'encoders', 0, -62), (18, 'display', 0, 0), (19, 'R_OCTAVE UP', 44, 0), (20, 'R_SESSION', 44, 0),
    (21, 'pad_beat', 0, -16), (22, 'pad_recset', 0, -16),
]
PUSH_BADGES_STEP = [
    (1, 'pads_bottom', 0, 96), (2, 'pads_play', 0, -16), (3, 'row1', 0, -14), (4, 'row2', 0, 14),
    (5, 'R_SCALES', -36, 0), (6, 'R_RIGHT', 30, 30), (7, 'R_IN', -36, -26), (8, 'R_OUT', 44, 0), (9, 'encoders', 0, -62),
    (10, 'R_SHIFT', 44, 0),
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
KO_BODY, KO_EDGE, KO_ORANGE, KO_INK = '#d2d2d0', '#a9a9a6', '#f2541b', '#262626'
KO_CREAM, KO_GRILLE, KO_DARK, KO_LIGHT = '#efebe1', '#9a9a98', '#2b2b2c', '#e9e8e4'
KO_PAD_LABELS = [['7', '8', '9'], ['4', '5', '6'], ['1', '2', '3'], ['.', '0', 'ENTER']]
KO_PAD_SMALL = [['LEVEL', 'PITCH', 'TIME'], ['LPF', 'HPF', '→FX'], ['ATK', 'REL', 'PAN'], ['TUNE', 'VEL', 'MOD']]
KO_FUNC_A = [['T7', 'T8', 'ALL'], ['T4', 'T5', 'T6'], ['T1', 'T2', 'T3'], ['STOP', 'UNDO', 'CAPT']]
KO_DE = {'T%d' % i: 'S%d' % i for i in range(1, 9)}
KO_DE.update({'ALL': 'ALLE', 'UNDO': 'ZURÜCK'})
# K.O. II geometry (portrait, as on a photo of the real device) - the same numbers are used by
# src/plugin/ControllerView.cpp (kKo... tables)
KO_W, KO_H = 880, 1180
KO_PAD_X = [265, 380, 495]; KO_PAD_W = 75
KO_PAD_Y = [665, 780, 895, 1010]; KO_PAD_H = 76
KO_GROUP_X, KO_GROUP_W = 152, 78
KO_RIGHT = [  # (top label, bottom label, x, y, w, h, top colour, bottom colour)
    ('SAMPLE', 'CHOP', 610, 650, 80, 80, KO_ORANGE, KO_LIGHT), ('TIMING', 'CORRECT', 730, 650, 85, 80, KO_DARK, KO_LIGHT),
    ('FX', 'OUTPUT', 615, 770, 80, 76, KO_DARK, KO_LIGHT), ('ERASE', 'SYSTEM', 735, 770, 85, 76, KO_LIGHT, '#dcdbd6')]
KO_TOP = [('SOUND', 'EDIT', 150, KO_DARK, KO_LIGHT), ('MAIN', 'COMMIT', 260, KO_DARK, KO_ORANGE), ('TEMPO', 'LOOP', 372, KO_DARK, '#9d9d9b')]

def ko2_hatch(x, y, w, h, r=8):
    return f"<rect x='{x}' y='{y}' width='{w}' height='{h}' rx='{r}' fill='url(#hatch)' opacity='0.55'/>"

def ko2(page='A', lang='en', mark_unused=True):
    """the K.O. II as on the photo of the real device - returns (svg body, w, h, anchors)"""
    W, H = KO_W, KO_H
    o, a = [], {}
    o.append("<defs><pattern id='hatch' width='8' height='8' patternUnits='userSpaceOnUse' patternTransform='rotate(45)'>"
             "<rect width='8' height='8' fill='none'/><line x1='0' y1='0' x2='0' y2='8' stroke='#5a5a5a' stroke-width='2'/></pattern></defs>")
    o.append(f"<rect x='0' y='60' width='{W}' height='{H-60}' rx='18' fill='#000' opacity='0.55' filter='url(#blur20)'/>")
    # jacks / tabs on the top edge
    tabs = [('OUTPUT', 30, 72, '#e4e1da', KO_INK), ('INPUT', 175, 72, KO_ORANGE, '#fff'), ('SYNC', 285, 85, '#1b1b1c', '#e8e8e8'),
            ('MIDI', 370, 88, '#1b1b1c', '#e8e8e8'), ('USB', 585, 75, '#8f8f8d', '#f2f2f2'), ('POWER', 665, 155, '#dedcd6', KO_INK)]
    for n, x, w, fill, ink in tabs:
        o.append(f"<rect x='{x}' y='4' width='{w}' height='44' rx='4' fill='{fill}' stroke='#000' stroke-opacity='0.25'/>")
        o.append(text(x + w / 2, 33, n, 13, 700, ink, 'middle'))
    a['jacks'] = (420, 26)
    # cream label panel (left) and speaker grille (right)
    o.append(f"<rect x='0' y='40' width='552' height='234' rx='6' fill='{KO_CREAM}'/>")
    o.append(text(34, 120, 'K.O. II', 34, 500, KO_INK, 'start', 0.85))
    o.append(f"<rect x='552' y='40' width='290' height='228' rx='6' fill='{KO_GRILLE}'/>")
    for r in range(11):
        for c in range(15):
            o.append(f"<circle cx='{570 + c * 18.2:.1f}' cy='{58 + r * 19.5:.1f}' r='5.4' fill='#2e2e2e' opacity='0.9'/>")
    # dark display strip with the segment display
    o.append(f"<rect x='0' y='268' width='{W}' height='224' rx='4' fill='#121214'/>")
    o.append(f"<rect x='0' y='268' width='{W}' height='40' fill='url(#spec)' opacity='0.08'/>")
    seg = "font-family='DejaVu Sans Mono' font-weight='bold'"
    o.append(f"<text x='300' y='372' {seg} font-size='64' fill='#ff6a2a' opacity='0.92' letter-spacing='4'>120</text>")
    for i, (ix, iy) in enumerate(((60, 330), (60, 380), (60, 430), (130, 450), (560, 330), (620, 400), (700, 420), (760, 360))):
        o.append(f"<rect x='{ix}' y='{iy}' width='28' height='16' rx='3' fill='#ffffff' opacity='0.06'/>")
    a['display'] = (480, 312)
    # control deck
    o.append(f"<rect x='0' y='478' width='{W}' height='{H-478}' rx='14' fill='{KO_BODY}'/>")
    o.append(f"<rect x='0' y='478' width='{W}' height='{H-478}' rx='14' fill='none' stroke='{KO_EDGE}' stroke-width='2'/>")
    o.append(f"<rect x='14' y='482' width='{W-28}' height='14' rx='7' fill='#fff' opacity='0.25'/>")

    def split_btn(x, y, w, h, top, bot, ctop, cbot):
        e = f"<rect x='{x}' y='{y+4}' width='{w}' height='{h}' rx='6' fill='#000' opacity='0.25'/>"
        e += f"<rect x='{x}' y='{y}' width='{w}' height='{h/2}' rx='5' fill='{ctop}'/>"
        e += f"<rect x='{x}' y='{y + h/2}' width='{w}' height='{h/2}' rx='5' fill='{cbot}'/>"
        e += f"<rect x='{x}' y='{y + h/2 - 6}' width='{w}' height='12' fill='{ctop}'/>"
        e += f"<rect x='{x}' y='{y + h/2}' width='{w}' height='6' fill='{cbot}'/>"
        def ink(c): return '#ffffff' if c in (KO_DARK, KO_ORANGE, '#9d9d9b') else KO_INK
        e += text(x + w / 2, y + h / 4 + 5, top, 13, 600, ink(ctop), 'middle')
        e += text(x + w / 2, y + h * 3 / 4 + 5, bot, 13, 600, ink(cbot), 'middle')
        return e

    # top row: VOLUME, SOUND / EDIT, MAIN / COMMIT, TEMPO / LOOP, the orange BPM / GAIN knob, the black METRONOME / SWING knob
    o.append(text(78, 552, 'VOLUME', 12, 600, KO_INK, 'middle', 0.8))
    o.append(f"<circle cx='65' cy='600' r='33' fill='#f4f2ec' stroke='{KO_EDGE}' stroke-width='2'/>")
    o.append(f"<circle cx='58' cy='593' r='19' fill='#8e8e8c'/><circle cx='54' cy='589' r='8' fill='#a9a9a7'/>")
    a['volume'] = (65, 600)
    for top, bot, x, ct, cb in KO_TOP:
        o.append(split_btn(x, 558, 76, 72, top, bot, ct, cb))
        if mark_unused: o.append(ko2_hatch(x, 558, 76, 72, 6))
    a['btn_sound'] = (188, 594)
    o.append(f"<path d='M412,548 L412,532 L630,532' stroke='{KO_INK}' stroke-width='1.5' fill='none' opacity='0.6'/>")
    o.append(text(655, 537, 'BPM', 12, 600, KO_INK, 'middle', 0.8))
    o.append(f"<path d='M680,532 L715,532' stroke='{KO_INK}' stroke-width='1.5' opacity='0.6'/>")
    o.append(text(765, 512, 'METRONOME', 12, 600, KO_INK, 'middle', 0.8))
    o.append(f"<circle cx='645' cy='574' r='34' fill='{KO_ORANGE}' stroke='#b33a10' stroke-width='2'/>")
    o.append(f"<circle cx='642' cy='566' r='17' fill='#ff7a45'/><circle cx='645' cy='635' r='8' fill='{KO_ORANGE}' opacity='0.7'/>")
    o.append(text(598, 590, 'GAIN', 10, 600, KO_ORANGE, 'middle', 0.9, "transform='rotate(-90 598 590)'"))
    o.append(f"<circle cx='765' cy='565' r='36' fill='#1d1d1e'/><circle cx='760' cy='556' r='15' fill='#4a4a4a'/>")
    o.append(f"<rect x='757' y='620' width='16' height='16' rx='3' fill='#2b2b2b'/>")
    o.append(text(765, 632, 'Y', 10, 700, '#ddd', 'middle'))
    o.append(text(822, 582, 'SWING', 10, 600, KO_INK, 'middle', 0.8, "transform='rotate(90 822 582)'"))
    if mark_unused:
        for cx, cy, r in ((65, 600, 33), (645, 574, 34), (765, 565, 36)):
            o.append(f"<circle cx='{cx}' cy='{cy}' r='{r}' fill='url(#hatch)' opacity='0.55'/>")
    a['knob'] = (765, 565)

    # left column: KEYS, FADER, the vertical fader, SHIFT
    for n, y in (('KEYS', 675), ('FADER', 750)):
        o.append(f"<rect x='45' y='{y+3}' width='70' height='35' rx='5' fill='#000' opacity='0.25'/>")
        o.append(f"<rect x='45' y='{y}' width='70' height='35' rx='5' fill='{KO_DARK}'/>")
        o.append(text(80, y + 23, n, 12, 600, '#fff', 'middle'))
        if mark_unused: o.append(ko2_hatch(45, y, 70, 35, 5))
    o.append(f"<rect x='76' y='840' width='8' height='165' rx='4' fill='#3a3a3a'/>")
    o.append(f"<path d='M52,925 L108,925' stroke='{KO_INK}' stroke-width='1.5' opacity='0.5'/>")
    o.append(f"<circle cx='68' cy='995' r='23' fill='#f4f2ec' stroke='{KO_EDGE}' stroke-width='2'/>")
    if mark_unused: o.append(ko2_hatch(44, 836, 72, 186, 10))
    a['fader'] = (80, 900)
    o.append(f"<rect x='50' y='1058' width='70' height='32' rx='5' fill='#dcdbd6' stroke='{KO_EDGE}'/>")
    o.append(text(85, 1079, 'SHIFT', 12, 600, '#8a8a88', 'middle'))

    # group buttons A - D (light grey) left of the number pads
    icons = ['✳', '↩', '⇡', '⇣']
    for i, g in enumerate('ABCD'):
        y = KO_PAD_Y[i] + (5 if i < 3 else 5)
        on = g == page
        o.append(f"<circle cx='172' cy='{y - 18}' r='5' fill='{KO_ORANGE if on else '#4a4a4a'}'/>")
        o.append(f"<rect x='{KO_GROUP_X}' y='{y+4}' width='{KO_GROUP_W}' height='72' rx='7' fill='#000' opacity='0.22'/>")
        o.append(f"<rect x='{KO_GROUP_X}' y='{y}' width='{KO_GROUP_W}' height='72' rx='7' fill='#e2e1dd' stroke='{KO_EDGE}'/>")
        o.append(text(KO_GROUP_X + 12, y + 28, g, 24, 500, '#9a9a98', 'start'))
        o.append(text(KO_GROUP_X + 14, y + 60, icons[i], 14, 600, '#9a9a98', 'start'))
        a['group_' + g] = (KO_GROUP_X, y + 36)

    # 3 x 4 number pads, small labels above each row
    cols = {'T1': '#38dc5a', 'T2': '#ff3040', 'T3': '#ff9228', 'T4': '#38dc5a', 'T5': '#2f7a3e'}
    for r in range(4):
        for c in range(3):
            x = KO_PAD_X[c]; y = KO_PAD_Y[r]
            o.append(f"<circle cx='{x + 18}' cy='{y - 18}' r='5' fill='#4a4a4a'/>")
            o.append(text(x + 30, y - 13, KO_PAD_SMALL[r][c], 12, 600, KO_INK, 'start', 0.8))
            fn = KO_FUNC_A[r][c]
            led = cols.get(fn)
            o.append(f"<rect x='{x}' y='{y+5}' width='{KO_PAD_W}' height='{KO_PAD_H}' rx='6' fill='#000' opacity='0.35'/>")
            o.append(f"<rect x='{x}' y='{y}' width='{KO_PAD_W}' height='{KO_PAD_H}' rx='6' fill='{KO_DARK}'/>")
            o.append(f"<rect x='{x+4}' y='{y+3}' width='{KO_PAD_W-8}' height='12' rx='4' fill='#fff' opacity='0.07'/>")
            if led:
                o.append(f"<rect x='{x+6}' y='{y+KO_PAD_H-12}' width='{KO_PAD_W-12}' height='6' rx='3' fill='{led}'/>")
                o.append(f"<rect x='{x+2}' y='{y+KO_PAD_H-16}' width='{KO_PAD_W-4}' height='14' rx='6' fill='{led}' opacity='0.35' filter='url(#blur4)'/>")
            lab = KO_PAD_LABELS[r][c]
            o.append(text(x + 10, y + 30, lab, 22 if len(lab) == 1 else 14, 500, '#e6e6e6', 'start'))
            if lang == 'de':
                fn = KO_DE.get(fn, fn)
            o.append(text(x + KO_PAD_W - 8, y + KO_PAD_H - 20, fn, 13, 800, '#ffffff', 'end', 0.95))
            a['pad_' + lab] = (x + KO_PAD_W / 2, y + KO_PAD_H / 2)
            a['padtop_' + lab] = (x + KO_PAD_W / 2, y + 2)
            a['padbot_' + lab] = (x + KO_PAD_W / 2, y + KO_PAD_H - 2)

    # right: SAMPLE / CHOP, TIMING / CORRECT, FX / OUTPUT, ERASE / SYSTEM, - / +, RECORD, PLAY
    for top, bot, x, y, w, h, ct, cb in KO_RIGHT:
        o.append(split_btn(x, y, w, h, top, bot, ct, cb))
        if mark_unused: o.append(ko2_hatch(x, y, w, h, 6))
    a['fx'] = (655, 790)
    for n, x in (('−', 620), ('+', 740)):
        o.append(f"<rect x='{x}' y='{895}' width='82' height='80' rx='6' fill='#000' opacity='0.25'/>")
        o.append(f"<rect x='{x}' y='890' width='82' height='80' rx='6' fill='#f3f1ea' stroke='{KO_EDGE}'/>")
        o.append(text(x + 41, 943, n, 34, 400, KO_INK, 'middle'))
        if mark_unused: o.append(ko2_hatch(x, 890, 82, 80, 6))
    o.append(f"<circle cx='640' cy='990' r='5' fill='#4a4a4a'/><circle cx='682' cy='990' r='5' fill='#4a4a4a'/>")
    for n, x, col, ink in (('RECORD', 625, KO_ORANGE, '#fff'), ('PLAY', 745, '#8a8a88', '#f0f0f0')):
        o.append(f"<rect x='{x}' y='1015' width='82' height='80' rx='6' fill='#000' opacity='0.3'/>")
        o.append(f"<rect x='{x}' y='1010' width='82' height='80' rx='6' fill='{col}'/>")
        o.append(text(x + 41, 1036, n, 13, 600, ink, 'middle'))
    if mark_unused: o.append(ko2_hatch(625, 1010, 82, 80, 6))
    a['play'] = (786, 1050)
    return ''.join(o), W, H, a

# legend 1 - 8: docs KO2.md and src/plugin/HelpGuide.cpp
KO_BADGES = [(1, 'padtop_2', 34, -26), (2, 'padtop_9', 34, -26), (3, 'padbot_0', 0, 44), (4, 'group_A', -58, -52),
             (5, 'group_C', -58, -52), (6, 'padbot_ENTER', 0, 44), (7, 'knob', 110, 0), (8, 'display', 0, -64)]

def ko2_numbered(lang='en'):
    body, W, H, a = ko2('A', lang)
    M = 120
    o = [body]
    for num, key, dx, dy in KO_BADGES:
        x, y = a[key]
        bx, by = x + dx, y + dy
        o.append(f"<line x1='{x}' y1='{y}' x2='{bx}' y2='{by}' stroke='#fff' stroke-width='2.5' stroke-opacity='0.9'/>")
        o.append(f"<circle cx='{x}' cy='{y}' r='5' fill='#fff' stroke='#000' stroke-opacity='0.4'/>")
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
    kb, kw, kh = ko2_numbered('de')        # v1.6: German pad labels (S = Spur)
    render('livelooper-ko2-layout-de', background(kw, kh) + kb, kw, kh, DOC_DIR, 1.0)
    render('help-ko2-de', background(kw, kh) + kb, kw, kh, APP_DIR, 0.75)
    render('help-loops', feat_loops(), FW, FH, APP_DIR, 0.75)
    render('help-fx', feat_fx(), FW, FH, APP_DIR, 0.75)
    render('help-beats', feat_beats(), FW, FH, APP_DIR, 0.75)
    render('help-rec', feat_rec(), FW, FH, APP_DIR, 0.75)
    render('help-link', feat_link(), FW, FH, APP_DIR, 0.75)
    b, w, h = welcome_hero(); render('welcome-hero', b, w, h, APP_DIR, 0.75)
    b, w, h = about_art(); render('about-art', b, w, h, APP_DIR, 0.7)

if __name__ == '__main__':
    main()
