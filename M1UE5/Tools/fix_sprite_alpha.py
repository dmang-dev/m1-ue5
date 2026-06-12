"""Force index-0 transparency on the extracted SPRITE bitmaps (M1 quirk).

Marathon 1's Shapes format predates the per-bitmap transparent flag (0x4000)
that Marathon 2/Infinity set, so py-marathon-utils' flag-gated rule
(`a = 0 if (self.transparent and ci == 0)`) never fires on M1 data and every
sprite PNG comes out fully opaque -- the engine itself treats palette index 0
as implicitly transparent when drawing M1 shapes as sprites. Until that's
fixed upstream, re-render exactly the bitmaps referenced by the sprite import
lists with transparency forced on, overwriting the extracted PNGs in place
(paths stay valid for the manifests + ue5_import_sprites.py).

Wall-texture bitmaps are deliberately NOT touched: index 0 can be a real
color there, and the level pipeline imports from the same folders.

Run with the project venv (needs marathon_utils + Pillow):
    .venv/Scripts/python.exe Tools/fix_sprite_alpha.py
Run AFTER extract_reference.py --shapes, BEFORE ue5_import_sprites.py.
"""
import json
import os
import re
import sys
from pathlib import Path

from marathon_utils import shapes

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT.parent / "M1SOURCE" / "Marathon-20250829" / "Shapes.shps"
SHAPES_DIR = ROOT / "Reference" / "Shapes"
IMPORT_LISTS = [
    ROOT / "Reference" / "Sprites" / "import_list.json",
    ROOT / "Reference" / "Sprites" / "weapon_import_list.json",
]


def main() -> int:
    # (collection, bitmap_index) pairs actually used as sprites.
    wanted: set[tuple[int, int]] = set()
    for lst in IMPORT_LISTS:
        if not lst.is_file():
            print(f"missing {lst}; run the gen_* scripts first", file=sys.stderr)
            return 1
        for e in json.loads(lst.read_text(encoding="utf-8")):
            m = re.search(r"Coll_(\d+)[\\/]+bitmaps[\\/]+(\d+)\.png",
                          e["png_path"])
            if m:
                wanted.add((int(m.group(1)), int(m.group(2))))
    colls = {c for c, _ in wanted}
    print(f"sprite bitmaps to fix: {len(wanted)} across collections {sorted(colls)}")

    blob = SRC.read_bytes()
    fmt, resources = shapes._detect_format(blob)
    if fmt != 1:
        print(f"expected M1 format (1), got {fmt}", file=sys.stderr)
        return 1

    fixed = 0
    for res in resources:
        coll_idx = res["id"] - 128
        if coll_idx not in colls:
            continue
        parsed = shapes.parse_collection(res["data"])
        cluts = parsed["cluts"]
        palette = cluts[0] if cluts else [(0, 0, 0)] * 256
        out_dir = SHAPES_DIR / f"Coll_{coll_idx:02d}" / "bitmaps"
        for bi, bm in enumerate(parsed["bitmaps"]):
            if (coll_idx, bi) not in wanted:
                continue
            bm.transparent = True  # M1: index 0 is implicitly transparent
            bm.to_image(palette).save(out_dir / f"{bi:03d}.png")
            fixed += 1
    print(f"re-rendered {fixed} sprite PNGs with index-0 alpha")
    return 0 if fixed else 1


if __name__ == "__main__":
    raise SystemExit(main())
