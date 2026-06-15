"""Generate an ORIGINAL, clean-room wall/floor texture set (installer Phase 4, 4b).

The runtime loader reads each level's MTL, which references the wall textures as
`../Shapes/Coll_NN/bitmaps/SSS.png`. To ship a SELF-CONTAINED build with zero Bungie
pixel art, those PNGs must be replaced with original art. This tool generates a
**procedural, deterministic, non-derived** texture for exactly the (collection, shape)
IDs the converted levels use -- so a `M1Data` bundle = geometry (functional layout) +
THESE original textures (+ original sounds, TODO) needs no Bungie image data.

It is NOT a faithful re-creation of Marathon's art -- it is placeholder original art
(distinct per collection, textured per shape) that lets the geometry read as surfaces.
A real art pass replaces these with hand-authored originals; the pipeline is identical.

Input : a converted cache's Geometry/*.mtl (the texture reference list).
Output: <out>/Shapes/Coll_NN/bitmaps/SSS.png  (mirrors the layout the MTLs expect).

Run:  python Tools/gen_original_textures.py --cache "%LOCALAPPDATA%\\M1UE5\\Data" --out "%LOCALAPPDATA%\\M1UE5\\Data\\Original"
"""
import argparse
import glob
import math
import os
import re
from pathlib import Path

from PIL import Image

SIZE = 128
MAP_KD = re.compile(r"^map_Kd\s+.*Coll_(\d+)/bitmaps/(\d+)\.png", re.IGNORECASE)


def collect_refs(geo_dir: Path):
    """Unique (coll, shape) pairs referenced by every MTL in the cache."""
    refs = set()
    for mtl in glob.glob(str(geo_dir / "*.mtl")):
        with open(mtl, encoding="utf-8", errors="ignore") as fh:
            for line in fh:
                m = MAP_KD.match(line.strip())
                if m:
                    refs.add((int(m.group(1)), int(m.group(2))))
    return sorted(refs)


def hsv_rgb(h, s, v):
    i = int(h * 6.0)
    f = h * 6.0 - i
    p, q, t = v * (1 - s), v * (1 - s * f), v * (1 - s * (1 - f))
    r, g, b = [(v, t, p), (q, v, p), (p, v, t),
               (p, q, v), (t, p, v), (v, p, q)][i % 6]
    return (int(r * 255), int(g * 255), int(b * 255))


def make_texture(coll: int, shape: int) -> Image.Image:
    """Deterministic original texture: a per-collection hue + a per-shape procedural
    block/brush pattern. Pure math (no source image), so it's clean-room + regenerable."""
    base_h = (coll * 0.61803398875) % 1.0          # golden-angle hue per collection
    seed = (coll * 73856093) ^ (shape * 19349663)  # deterministic per (coll, shape)
    img = Image.new("RGB", (SIZE, SIZE))
    px = img.load()
    blocks = 4 + (shape % 5)                        # coarse panel grid varies by shape
    bw = SIZE / blocks
    for y in range(SIZE):
        for x in range(SIZE):
            bx, by = int(x / bw), int(y / bw)
            # Per-block value step + a cheap hash dither so it reads as a surface.
            hsh = (seed ^ (bx * 374761393) ^ (by * 668265263)) & 0xFFFF
            v = 0.34 + 0.30 * ((hsh % 97) / 97.0)
            # Mortar lines between blocks (darker grout).
            if (x % int(bw) < 2) or (y % int(bw) < 2):
                v *= 0.55
            s = 0.30 + 0.25 * (((seed >> 3) % 53) / 53.0)
            # subtle per-pixel grain
            v += 0.04 * (((x * 17 + y * 31 + seed) & 7) / 7.0 - 0.5)
            px[x, y] = hsv_rgb((base_h + 0.02 * (shape % 7)) % 1.0,
                               min(0.85, s), max(0.05, min(1.0, v)))
    return img


def main(argv=None) -> int:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--cache", type=Path, required=True, help="converted cache (has Geometry/*.mtl)")
    p.add_argument("--out", type=Path, required=True, help="output root (writes <out>/Shapes/...)")
    args = p.parse_args(argv)

    geo = args.cache / "Geometry"
    refs = collect_refs(geo)
    if not refs:
        print(f"no texture refs found in {geo}/*.mtl -- run the Asset Setup wizard first")
        return 2
    n = 0
    for coll, shape in refs:
        dst = args.out / "Shapes" / ("Coll_%02d" % coll) / "bitmaps"
        dst.mkdir(parents=True, exist_ok=True)
        make_texture(coll, shape).save(dst / ("%03d.png" % shape))
        n += 1
    print(f"wrote {n} original textures to {args.out / 'Shapes'} "
          f"({len({c for c, _ in refs})} collections)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
