"""Extract M1 chapter screens (interlevel splash art) and emit an import list.

Marathon 1 shows a full-screen "chapter screen" when the player enters certain
story-beat levels (ARRIVAL, COUNTERATTACK, ...). These are 640x480 PICT
resources living in the Shapes file's resource fork, with the level name
already rendered into the art. The PICT id encodes the level: id = 10000 +
level_index. Ids >= 10100 are non-level specials (prologue art / epilogue
text) handled separately.

Run with the venv interpreter (has py-marathon-utils + Pillow):

    C:\\m1-ue5\\.venv\\Scripts\\python.exe Tools/gen_chapter_assets.py

Outputs:
    Reference/Images_Shapes/PICT_<id>.png        (all decoded chapter PICTs)
    Reference/Images_Shapes/chapter_import_list.json
        [{ "level": 0, "asset": "T_chapter_00", "png": "<abs path>" }, ...]

The editor-side Tools/ue5_import_chapters.py consumes the JSON.
"""
import json
from pathlib import Path

from marathon_utils import images

import m1_paths

SHAPES = m1_paths.SHAPES
OUT = m1_paths.REFERENCE / "Images_Shapes"
MAX_LEVEL = 36  # M1 has levels 0..36; ids above 10036 are non-level specials

manifest = images.extract(SHAPES, OUT)
items = []
for im in manifest["images"]:
    rid = im["id"]
    if rid < 10000 or rid > 10000 + MAX_LEVEL:
        continue  # skip prologue/epilogue specials (10100, 10101)
    level = rid - 10000
    items.append({
        "level": level,
        "asset": f"T_chapter_{level:02d}",
        "png": str((OUT / im["file"]).resolve()),
    })

items.sort(key=lambda d: d["level"])
(OUT / "chapter_import_list.json").write_text(json.dumps(items, indent=2), encoding="utf-8")
print(f"decoded {manifest['count']} PICTs, {len(items)} chapter screens:")
for it in items:
    print(f"  L_{it['level']:02d} -> {it['asset']}")
