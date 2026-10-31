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

The game keeps its own colors and takes only light and dark from the textures.
Usage: python3 -I tools/assets/make_detail.py [board|clay|fonts]   (needs Pillow and the network; downloads
into tools/out/assets; 'board', 'clay' or 'fonts' builds that alone)
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


def main():
    os.makedirs(CACHE, exist_ok=True)
    one = {'board': board, 'clay': clay, 'fonts': fonts}
    if len(sys.argv) == 2 and sys.argv[1] in one:
        one[sys.argv[1]]()
        print('wrote', 'assets/fonts' if sys.argv[1] == 'fonts' else 'assets/%s.png' % sys.argv[1])
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
    print('wrote assets/detail.png, detail2.png, detail3.png, flame.png, board.png and clay.png (fonts: run with fonts)')


if __name__ == '__main__':
    sys.exit(main())
