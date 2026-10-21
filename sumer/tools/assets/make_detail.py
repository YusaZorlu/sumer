#!/usr/bin/env python3
"""Builds assets/detail.png: four grey detail maps in one 512x512 RGBA texture, from CC0 textures on
Poly Haven (https://polyhaven.com, all CC0). The game keeps its own colors and takes only light and
dark from these: R the ground, G mud-brick walls, B wood, A rock.

Usage: python3 -I tools/assets/make_detail.py   (needs Pillow and the network; downloads into tools/out/assets)
"""
import json
import os
import sys
import urllib.request

from PIL import Image, ImageFilter, ImageStat

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CACHE = os.path.join(ROOT, 'tools', 'out', 'assets')
SOURCES = ['dry_ground_rocks', 'white_sandstone_bricks_03', 'wood_planks_dirt', 'rock_boulder_dry']
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
    print('wrote assets/detail.png from', ', '.join(SOURCES))


if __name__ == '__main__':
    sys.exit(main())
