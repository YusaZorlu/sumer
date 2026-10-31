#!/usr/bin/env python3
"""Builds the game's three pictures from CC0 sources (public domain, free for any use):

- assets/detail.png: four grey detail maps in one 512x512 RGBA texture from Poly Haven (polyhaven.com):
  R the ground, G mud-brick walls, B wood, A rock.
- assets/detail2.png: the same for people and roads, from ambientCG (ambientcg.com): R woven cloth,
  G fleece and fur, B leather, A the roads' packed earth.
- assets/detail3.png: trees and fields: R palm bark, G leaves, B tilled soil (Poly Haven), A straw for the
  crops (ambientCG).
- assets/flame.png: two frames of a flame side by side, from Kenney's Particle Pack (kenney.nl).
- assets/board.png: the wood of the alehouse's game boards (their frame on the panel), in its own colors,
  from Poly Haven's wood_table_001.
- assets/clay.png: the grain of the windows' clay tablets, from Poly Haven's clay_plaster, in light grey.
- assets/fonts: OSerif (titles) and Jupiteroid (the rest), two fonts by GGBotNet from their CC0 pack on
  OpenGameArt, cut down to the letters the game loads (lang.c). Needs fontTools (pip install fonttools).
- assets/sky_day.jpg, sky_dusk.jpg, sky_grey.jpg: the upper half of three Poly Haven sky panoramas (2k HDR,
  2048x512): a clear day with clouds, a sunset and an overcast sky, with the sun painted out (the game draws
  its own), the sunset's sun turned to the middle, each brought to the same mean brightness. Needs numpy.

The game keeps its own colors and takes only light and dark from the textures.
Usage: python3 -I tools/assets/make_detail.py [board|clay|fonts|skies]   (needs Pillow and the network;
downloads into tools/out/assets; a name builds that alone)
"""
import json
import os
import sys
import urllib.request
import zipfile

from PIL import Image, ImageFilter, ImageStat

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CACHE = os.path.join(ROOT, 'tools', 'out', 'assets')
SOURCES = ['dry_ground_rocks', 'white_sandstone_bricks_03', 'wood_planks_dirt', 'rock_boulder_dry']
# ambientCG material and the map to take: the weave of cloth is in its color, fleece and leather show in
# their height maps.
PEOPLE = [('Fabric019', 'Color'), ('Carpet016', 'Displacement'), ('Leather037', 'Displacement'), ('Pathway004', 'Color')]
TREES = [('ph', 'palm_tree_bark'), ('ph', 'forest_leaves_02'), ('ph', 'dry_mud_field_001'), ('cg', 'ThatchedRoof001A')]
KENNEY = 'https://kenney.nl/media/pages/assets/particle-pack/f8fe0f8cb8-1677578741/kenney_particle-pack.zip'
FLAMES = ['flame_05', 'flame_06']
SIZE = 512
GGBOT = 'https://opengameart.org/sites/default/files/ggbotnet_fonts_cc0_2025_dec.zip'
FONTS = ['OSerif/TrueType (.ttf)/OSerif-Regular.ttf', 'Jupiteroid/TrueType (.ttf)/Jupiteroid-Regular.ttf']
# The letters lang.c loads: printable ASCII, Latin-1, the Turkish ones beyond it and a few marks.
UNICODES = 'U+0020-007E,U+00A0-00FF,U+011E,U+011F,U+0130,U+0131,U+015E,U+015F,U+2013,U+2014,U+2018,U+2019,U+201C,U+201D,U+2022,U+2026'


def get(url):
    # Poly Haven turns away requests without a user agent of their own.
    with urllib.request.urlopen(urllib.request.Request(url, headers={'User-Agent': 'sumer-asset-builder'})) as r:
        return r.read()


def fetch(asset):
    path = os.path.join(CACHE, asset + '.jpg')
    if not os.path.exists(path):
        url = json.loads(get('https://api.polyhaven.com/files/' + asset))['Diffuse']['1k']['jpg']['url']
        with open(path, 'wb') as f:
            f.write(get(url))
    return path


def fetch_cg(asset, kind):
    path = os.path.join(CACHE, asset + '.zip')
    if not os.path.exists(path):
        with open(path, 'wb') as f:
            f.write(get('https://ambientcg.com/get?file=%s_1K-JPG.zip' % asset))
    out = os.path.join(CACHE, '%s_%s.jpg' % (asset, kind))
    with zipfile.ZipFile(path) as z, open(out, 'wb') as f:
        f.write(z.read('%s_1K-JPG_%s.jpg' % (asset, kind)))
    return out


def flames():
    path = os.path.join(CACHE, 'kenney_particle-pack.zip')
    if not os.path.exists(path):
        with open(path, 'wb') as f:
            f.write(get(KENNEY))
    sheet = Image.new('L', (256, 256))
    with zipfile.ZipFile(path) as z:
        for i, name in enumerate(FLAMES):
            with z.open('PNG (Black background)/%s.png' % name) as f:
                # The flame stands in the middle of its square: crop to it, a frame twice as tall as wide.
                im = Image.open(f).convert('L').crop((128, 0, 384, 512)).resize((128, 256), Image.LANCZOS)
            sheet.paste(im, (128 * i, 0))
    sheet.save(os.path.join(ROOT, 'assets', 'flame.png'), optimize=True)


def detail(path):
    # Light and dark only, with the broad shading taken out (so the tiling does not show from afar) and the
    # contrast brought to the same strength for every map: mid grey 128, about +-40 for one deviation.
    g = Image.open(path).convert('L').resize((SIZE, SIZE), Image.LANCZOS)
    broad = g.filter(ImageFilter.GaussianBlur(SIZE / 8))
    px, bl = g.load(), broad.load()
    diff = [px[x, y] - bl[x, y] for y in range(SIZE) for x in range(SIZE)]
    sd = (sum(d * d for d in diff) / len(diff)) ** 0.5 or 1.0
    out = Image.new('L', (SIZE, SIZE))
    out.putdata([max(0, min(255, int(128 + d / sd * 40))) for d in diff])
    return out


def board():
    # The one picture kept in color: a 256x256 square of the table's wood, a little darker.
    im = Image.open(fetch('wood_table_001')).convert('RGB').resize((256, 256), Image.LANCZOS)
    im = im.point(lambda v: int(v * 0.85))
    im.save(os.path.join(ROOT, 'assets', 'board.png'), optimize=True)


def clay():
    # Grey, stretched to full contrast (the darkest and lightest 1% clipped), then kept light, 224 to 255:
    # the game tints it with the clay's color, leaving only a soft grain under the writing.
    g = Image.open(fetch('clay_plaster')).convert('L').resize((256, 256), Image.LANCZOS)
    hist, n, acc = g.histogram(), 256 * 256, 0
    lo = hi = None
    for v in range(256):
        acc += hist[v]
        if lo is None and acc >= n * 0.01:
            lo = v
        if hi is None and acc >= n * 0.99:
            hi = v
    span = max(1, hi - lo)
    g.point(lambda v: 224 + int(31 * min(1.0, max(0.0, (v - lo) / span)))).save(os.path.join(ROOT, 'assets', 'clay.png'), optimize=True)


def fonts():
    from fontTools import subset
    path = os.path.join(CACHE, 'ggbotnet_fonts_cc0.zip')
    if not os.path.exists(path):
        with open(path, 'wb') as f:
            f.write(get(GGBOT))
    os.makedirs(os.path.join(ROOT, 'assets', 'fonts'), exist_ok=True)
    with zipfile.ZipFile(path) as z:
        for name in FONTS:
            member = next(n for n in z.namelist() if n.endswith(name))
            src = os.path.join(CACHE, os.path.basename(name))
            with open(src, 'wb') as f:
                f.write(z.read(member))
            out = os.path.join(ROOT, 'assets', 'fonts', os.path.basename(name))
            subset.main([src, '--unicodes=' + UNICODES, '--layout-features=kern', '--output-file=' + out])
        licence = next(n for n in z.namelist() if n.endswith('/LICENSE'))
        with open(os.path.join(ROOT, 'assets', 'fonts', 'LICENSE-CC0.txt'), 'wb') as f:
            f.write(z.read(licence))


SKIES = [('sky_day', 'kloofendal_48d_partly_cloudy_puresky'), ('sky_dusk', 'kloppenheim_06_puresky'),
         ('sky_grey', 'overcast_soil_puresky')]
SKY_MEAN = 0.3   # each sky's mean brightness, linear (gfx.c scales it to the hour's light)


def read_hdr(path):
    # Radiance RGBE, run-length coded scanlines: linear RGB as floats.
    import numpy as np
    data = open(path, 'rb').read()
    pos = data.index(b'\n\n') + 2
    end = data.index(b'\n', pos)
    res = data[pos:end].split()
    pos = end + 1
    h, w = int(res[1]), int(res[3])
    px = np.zeros((h, w, 4), np.uint8)
    for y in range(h):
        if not (data[pos] == 2 and data[pos + 1] == 2):
            raise ValueError('only run-length coded HDR files are read')
        pos += 4
        for c in range(4):
            x = 0
            while x < w:
                n = data[pos]
                pos += 1
                if n > 128:
                    n -= 128
                    px[y, x:x + n, c] = data[pos]
                    pos += 1
                else:
                    px[y, x:x + n, c] = np.frombuffer(data, np.uint8, n, pos)
                    pos += n
                x += n
    e = px[..., 3].astype(np.int32)
    f = np.where(e > 0, np.ldexp(1.0, e - 136), 0.0)
    return px[..., :3].astype(np.float64) * f[..., None]


def sky(out, asset):
    import numpy as np
    path = os.path.join(CACHE, asset + '_2k.hdr')
    if not os.path.exists(path):
        url = json.loads(get('https://api.polyhaven.com/files/' + asset))['hdri']['2k']['hdr']['url']
        with open(path, 'wb') as f:
            f.write(get(url))
    img = read_hdr(path)
    h, w = img.shape[0] // 2, img.shape[1]
    up = img[:h].copy()                      # the sky: row 0 the zenith, the last row the horizon
    lum = up @ np.array([0.2126, 0.7152, 0.0722])
    elev = (0.5 - (np.arange(h) + 0.5) / (2 * h)) * np.pi          # each row's height above the horizon
    weight = np.cos(elev)[:, None] * np.ones((1, w))                # solid angle, as the rows shrink to the zenith
    sy, sx = np.unravel_index(np.argmax(lum), lum.shape)
    sunny = lum[sy, sx] > 40 * np.median(lum)
    if sunny:
        # The sun painted out (the game draws its own): each point of a disc round it takes the color of the
        # ring just outside, in its own direction from the sun, then the patch is smoothed. The sunset keeps
        # its glow and loses only the bright core.
        radius = np.radians(7.0 if out == 'sky_day' else 3.0)
        az = (np.arange(w) + 0.5) / w * 2 * np.pi
        ce, ca = elev[sy], az[sx]
        cosd = np.sin(elev)[:, None] * np.sin(ce) + np.cos(elev)[:, None] * np.cos(ce) * np.cos(az[None, :] - ca)
        mask = cosd > np.cos(radius)
        ring = (cosd <= np.cos(radius)) & (cosd > np.cos(radius + np.radians(1.5)))

        def angle(ys, xs):
            return np.arctan2(ys - sy, ((xs - sx + w // 2) % w) - w // 2)
        nb = 96
        ry, rx = np.nonzero(ring)
        rb = ((angle(ry, rx) + np.pi) / (2 * np.pi) * nb).astype(int) % nb
        sums, counts = np.zeros((nb, 3)), np.zeros(nb)
        np.add.at(sums, rb, up[ry, rx])
        np.add.at(counts, rb, 1)
        have = counts > 0
        cols = np.zeros((nb, 3))
        cols[have] = sums[have] / counts[have][:, None]
        idx = np.arange(nb)
        for c in range(3):   # directions with no ring (under the horizon) from their neighbours round the circle
            cols[:, c] = np.interp(idx, np.concatenate([idx[have] - nb, idx[have], idx[have] + nb]), np.tile(cols[have, c], 3))
        my, mx = np.nonzero(mask)
        t = (angle(my, mx) + np.pi) / (2 * np.pi) * nb - 0.5
        i0 = np.floor(t).astype(int) % nb
        f = (t - np.floor(t))[:, None]
        up[my, mx] = cols[i0] * (1 - f) + cols[(i0 + 1) % nb] * f
        rows = np.where(mask.any(axis=1))[0]
        y0, y1 = max(rows.min() - 2, 0), min(rows.max() + 3, h)
        sub, m = up[y0:y1], mask[y0:y1]
        for _ in range(120):
            avg = (np.roll(sub, 1, 1) + np.roll(sub, -1, 1) + np.vstack([sub[:1], sub[:-1]]) + np.vstack([sub[1:], sub[-1:]])) / 4
            sub[m] = avg[m]
        up[y0:y1] = sub
        if out == 'sky_dusk':                # its sun turned to the middle, where gfx.c expects it
            up = np.roll(up, w // 2 - sx, axis=1)
    lum = up @ np.array([0.2126, 0.7152, 0.0722])
    up *= SKY_MEAN / (lum * weight).sum() * weight.sum()
    hor = up[h - h // 16:].reshape(-1, 3).mean(axis=0)
    rgb = np.clip(up, 0.0, 1.0) ** (1 / 2.2) * 255.0
    Image.fromarray(np.round(rgb).astype(np.uint8), 'RGB').save(os.path.join(ROOT, 'assets', out + '.jpg'), quality=86, optimize=True)
    print('%s: sun %s, horizon color %.3f %.3f %.3f (linear, mean %.2f)' % (out, 'painted out' if sunny else 'none', hor[0], hor[1], hor[2], SKY_MEAN))


def skies():
    for out, asset in SKIES:
        sky(out, asset)


def main():
    os.makedirs(CACHE, exist_ok=True)
    one = {'board': board, 'clay': clay, 'fonts': fonts, 'skies': skies}
    if len(sys.argv) == 2 and sys.argv[1] in one:
        one[sys.argv[1]]()
        print('wrote', {'fonts': 'assets/fonts', 'skies': 'assets/sky_*.jpg'}.get(sys.argv[1], 'assets/%s.png' % sys.argv[1]))
        return 0
    chans = [detail(fetch(a)) for a in SOURCES]
    Image.merge('RGBA', chans).save(os.path.join(ROOT, 'assets', 'detail.png'), optimize=True)
    chans = [detail(fetch_cg(a, k)) for a, k in PEOPLE]
    Image.merge('RGBA', chans).save(os.path.join(ROOT, 'assets', 'detail2.png'), optimize=True)
    chans = [detail(fetch(a) if site == 'ph' else fetch_cg(a, 'Color')) for site, a in TREES]
    Image.merge('RGBA', chans).save(os.path.join(ROOT, 'assets', 'detail3.png'), optimize=True)
    flames()
    board()
    clay()
    skies()
    print('wrote assets/detail.png, detail2.png, detail3.png, flame.png, board.png, clay.png and sky_*.jpg (fonts: run with fonts)')


if __name__ == '__main__':
    sys.exit(main())
