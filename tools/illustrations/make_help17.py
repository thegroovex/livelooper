#!/usr/bin/env python3
"""v1.7 in-app help pictures (resources/help/help-{looplen,live,display,latency,controllers,backup}[-de].png).

Unlike the drawn v1.6 pages these show the real v1.7 interface: they are composed from the
documentation screenshots in docs/images (rendered by livelooper_screenshot) on the brand
background (dark glass, gradient glows #FF7A18 -> #FF2D6F -> #B14BFF), 900 x 525 like the other pages.
Run after regenerating docs/images:
    python3 tools/illustrations/make_help17.py
Needs Pillow.
"""
import os
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
DOC = os.path.join(ROOT, 'docs', 'images')
OUT = os.path.join(ROOT, 'resources', 'help')
W, H = 900, 525
GLOWS = ((0.15, 0.1, '#FF7A18', 0.32), (0.55, 1.0, '#FF2D6F', 0.22), (0.95, 0.2, '#B14BFF', 0.30))


def background():
    bg = Image.new('RGB', (W, H), '#0b0a10')
    for cx, cy, col, op in GLOWS:
        layer = Image.new('RGBA', (W, H), (0, 0, 0, 0))
        d = ImageDraw.Draw(layer)
        r = int(max(W, H) * 0.45)
        c = tuple(int(col[i:i + 2], 16) for i in (1, 3, 5))
        d.ellipse((W * cx - r, H * cy - r * 0.8, W * cx + r, H * cy + r * 0.8), fill=c + (int(255 * op),))
        bg.paste(layer.filter(ImageFilter.GaussianBlur(90)), (0, 0), layer.filter(ImageFilter.GaussianBlur(90)))
    return bg


def rounded(img, radius=14):
    mask = Image.new('L', img.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, img.size[0] - 1, img.size[1] - 1), radius, fill=255)
    return mask


def place(bg, img, box):
    """Fit img into box (x, y, w, h), centred, with a soft shadow and rounded corners."""
    x, y, w, h = box
    s = min(w / img.width, h / img.height)
    img = img.convert('RGB').resize((max(1, int(img.width * s)), max(1, int(img.height * s))), Image.LANCZOS)
    px, py = x + (w - img.width) // 2, y + (h - img.height) // 2
    sh = Image.new('RGBA', (img.width + 40, img.height + 40), (0, 0, 0, 0))
    ImageDraw.Draw(sh).rounded_rectangle((20, 24, img.width + 20, img.height + 24), 16, fill=(0, 0, 0, 150))
    sh = sh.filter(ImageFilter.GaussianBlur(10))
    bg.paste(sh, (px - 20, py - 20), sh)
    bg.paste(img, (px, py), rounded(img))
    ImageDraw.Draw(bg).rounded_rectangle((px, py, px + img.width - 1, py + img.height - 1), 14, outline=(70, 70, 82), width=1)


def doc(name, sfx):
    return Image.open(os.path.join(DOC, name + sfx + '.png'))


def make(sfx):
    out = {}
    # loop length: the pills of tracks 3-5 above the Push 1 FIXED LENGTH view (display + pads)
    bg = background()
    strip = doc('livelooper-loop-length', sfx)
    place(bg, strip.crop((725, 0, strip.width, strip.height)), (30, 18, 840, 175))
    push = doc('livelooper-controller-push-length', sfx)
    place(bg, push.crop((270, 150, 830, 745)), (220, 210, 460, 300))
    out['help-looplen'] = bg
    for key, src in (('help-live', 'livelooper-live'), ('help-perform', 'livelooper-perform'), ('help-display', 'livelooper-setup-display'),
                     ('help-latency', 'livelooper-setup-audio'), ('help-controllers', 'livelooper-setup-controllers-17')):
        bg = background()
        place(bg, doc(src, sfx), (30, 22, 840, 481))
        out[key] = bg
    # auto backup: SETUP > Sets with the notice after a crash on top
    bg = background()
    place(bg, doc('livelooper-setup-sets', sfx), (30, 22, 700, 481))
    place(bg, doc('livelooper-backup-notice', sfx), (440, 330, 440, 150))
    out['help-backup'] = bg
    for key, img in out.items():
        path = os.path.join(OUT, key + sfx + '.png')
        img.save(path, optimize=True)
        print('wrote', os.path.relpath(path, ROOT), os.path.getsize(path) // 1024, 'KB')


for sfx in ('', '-de'):
    make(sfx)
