"""Render CLUT-correct, alpha-forced sprite PNGs straight from Shapes.shps.

Replaces fix_sprite_alpha.py. Two M1 quirks make the plain extraction output
unusable for sprites:

  1. Marathon 1's Shapes format predates the per-bitmap transparent flag
     (0x4000), so py-marathon-utils' flag-gated alpha rule never fires and
     sprites extract fully opaque (index 0 -- the engine's implicit sprite
     transparency, exactly what Aleph One color-keys -- bakes in as chroma
     blue). Transparency is forced here.
  2. The extraction renders CLUT 0 only, but monster ranks select color
     tables via the high bits of their physics 'collection' field (Fighter
     Major = clut 1, etc.). Each (collection, clut, bitmap) is rendered with
     its real palette.

Inputs:  Reference/Sprites/render_list_monsters.json  (gen_monster_assets.py)
         Reference/Sprites/render_list_weapons.json   (gen_weapon_sprites.py)
Output:  Reference/Sprites/png/c<CC>_k<K>_<NNN>.png   (referenced by the
         import lists consumed by ue5_import_sprites.py)

Run with the project venv (needs marathon_utils + Pillow):
    .venv/Scripts/python.exe Tools/render_sprites.py
Order: gen_* scripts -> THIS -> ue5_import_sprites.py.
"""
import json
import sys
from pathlib import Path

from marathon_utils import shapes

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT.parent / "M1SOURCE" / "Marathon-20250829" / "Shapes.shps"
RENDER_LISTS = [
    ROOT / "Reference" / "Sprites" / "render_list_monsters.json",
    ROOT / "Reference" / "Sprites" / "render_list_weapons.json",
]


def main() -> int:
    wanted = {}  # (coll, clut, bitmap) -> out path
    for lst in RENDER_LISTS:
        if not lst.is_file():
            print(f"missing {lst}; run the gen_* scripts first", file=sys.stderr)
            return 1
        for e in json.loads(lst.read_text(encoding="utf-8")):
            wanted[(e["coll"], e["clut"], e["bitmap"])] = Path(e["out"])
    colls = {c for c, _, _ in wanted}
    print(f"renders: {len(wanted)} across collections {sorted(colls)}")

    blob = SRC.read_bytes()
    fmt, resources = shapes._detect_format(blob)
    if fmt != 1:
        print(f"expected M1 format (1), got {fmt}", file=sys.stderr)
        return 1

    done = 0
    for res in resources:
        coll_idx = res["id"] - 128
        if coll_idx not in colls:
            continue
        parsed = shapes.parse_collection(res["data"])
        cluts = parsed["cluts"] or [[(0, 0, 0)] * 256]
        for (c, k, b), out in wanted.items():
            if c != coll_idx:
                continue
            if b >= len(parsed["bitmaps"]):
                print(f"  WARN Coll_{c:02d} has no bitmap {b}", file=sys.stderr)
                continue
            bm = parsed["bitmaps"][b]
            bm.transparent = True  # M1: index 0 is implicitly transparent
            out.parent.mkdir(parents=True, exist_ok=True)
            bm.to_image(cluts[min(k, len(cluts) - 1)]).save(out)
            done += 1
    print(f"rendered {done}/{len(wanted)} clut-correct alpha sprites")
    return 0 if done == len(wanted) else 1


if __name__ == "__main__":
    raise SystemExit(main())
