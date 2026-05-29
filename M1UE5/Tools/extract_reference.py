#!/usr/bin/env python3
"""Refresh M1UE5/Reference/ from M1SOURCE using the published py-marathon-utils.

This replaces the old bespoke `Tools/extract/` package — the extraction code
now lives in the maintained PyPI package `py-marathon-utils`.

    pip install -r Tools/requirements.txt
    python Tools/extract_reference.py            # M1SOURCE -> Reference (defaults)
    python Tools/extract_reference.py --shapes   # also dump sprite/texture PNGs

Output under M1UE5/Reference/:
    Maps/<NN>_<name>.json   per-level geometry, lights, objects, terminals
    Sounds/<family>/*.wav   sound effects
    Physics/                physics model (raw + manifest)
    Shapes/  (optional)     per-collection sprite/texture PNGs
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

from marathon_utils import maps, physics, sounds

# Repo layout: <repo>/M1UE5/Tools/extract_reference.py
_REPO = Path(__file__).resolve().parents[2]
DEFAULT_SOURCE = _REPO / "M1SOURCE" / "Marathon-20250829"
DEFAULT_DEST = Path(__file__).resolve().parents[1] / "Reference"


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--source", type=Path, default=DEFAULT_SOURCE,
                   help="path to M1SOURCE/Marathon-20250829")
    p.add_argument("--dest", type=Path, default=DEFAULT_DEST,
                   help="output directory (M1UE5/Reference)")
    p.add_argument("--shapes", action="store_true",
                   help="also extract sprite/texture PNGs (needs Pillow)")
    args = p.parse_args(argv)

    src: Path = args.source
    dst: Path = args.dest
    if not src.exists():
        print(f"ERROR: source not found: {src}", file=sys.stderr)
        return 1
    dst.mkdir(parents=True, exist_ok=True)
    print(f"Source: {src}\nDest:   {dst}\n")

    m = maps.extract(src / "Map.scen", dst / "Maps")
    print(f"[maps]    {m['level_count']} levels")

    s = sounds.extract(src / "Sounds.sndz", dst / "Sounds")
    print(f"[sounds]  {s['count']} WAVs, {len(s['errors'])} errors")

    physics.extract(src / "Physics.phys", dst / "Physics")
    print("[physics] preserved")

    if args.shapes:
        from marathon_utils import shapes
        sh = shapes.extract(src / "Shapes.shps", dst / "Shapes")
        print(f"[shapes]  {len(sh['collections'])} collection banks -> PNG")

    print(f"\nDone. Reference refreshed at {dst}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
