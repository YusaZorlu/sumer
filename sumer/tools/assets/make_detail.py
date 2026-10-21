#!/usr/bin/env python3
"""Builds the game's three pictures from CC0 sources (public domain, free for any use):

- assets/detail.png: four grey detail maps in one 512x512 RGBA texture from Poly Haven (polyhaven.com):
  R the ground, G mud-brick walls, B wood, A rock.
- assets/detail2.png: the same for people and roads, from ambientCG (ambientcg.com): R woven cloth,
  G fleece and fur, B leather, A the roads' packed earth.
- assets/flame.png: two frames of a flame side by side, from Kenney's Particle Pack (kenney.nl).

The game keeps its own colors and takes only light and dark from the textures.
Usage: python3 -I tools/assets/make_detail.py   (needs Pillow and the network; downloads into tools/out/assets)
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
KENNEY = 'https://kenney.nl/media/pages/assets/particle-pack/f8fe0f8cb8-1677578741/kenney_particle-pack.zip'
FLAMES = ['flame_05', 'flame_06']
SIZE = 512


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


def main():
    os.makedirs(CACHE, exist_ok=True)
    chans = [detail(fetch(a)) for a in SOURCES]
    Image.merge('RGBA', chans).save(os.path.join(ROOT, 'assets', 'detail.png'), optimize=True)
    chans = [detail(fetch_cg(a, k)) for a, k in PEOPLE]
    Image.merge('RGBA', chans).save(os.path.join(ROOT, 'assets', 'detail2.png'), optimize=True)
    flames()
    print('wrote assets/detail.png, detail2.png and flame.png')


if __name__ == '__main__':
    sys.exit(main())
