#!/usr/bin/env python3
"""Convert an extracted M1 level (Reference/Maps/*.json) into UE5-importable
geometry: a Wavefront .obj of the sector floors, ceilings, and walls, plus a
sidecar placements.json (player start, lights, object spawns).

Marathon levels are Doom-style sector geometry: a 2D floor plan (EPNT points +
LINS lines) where each POLY (sector) has its own floor_height and
ceiling_height. We rebuild 3D by:
  * floor   = the polygon at z = floor_height
  * ceiling = the polygon at z = ceiling_height
  * walls   = vertical quads along each line, covering either the full height
              (where the line borders the void) or just the floor/ceiling
              "step" between two adjacent sectors of different heights.

Coordinates: Marathon stores x/y/heights as int16 in 1/1024 World-Unit. We map
  UE_cm = value * (cm_per_wu / 1024)
with Marathon's screen-style +y-down flipped to keep the level un-mirrored, and
heights on UE's +Z. Default 100 cm per World Unit (see CLAUDE.md); tune with
--scale if the playable scale feels off.

Usage:
    python Tools/scen_to_obj.py Reference/Maps/00_Arrival.json
    python Tools/scen_to_obj.py Reference/Maps/00_Arrival.json --out Reference/Geometry --scale 100
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

# Marathon OBJS class ids
_OBJ_MONSTER, _OBJ_SCENERY, _OBJ_ITEM, _OBJ_PLAYER, _OBJ_GOAL, _OBJ_SOUND = range(6)


class ObjBuilder:
    """Accumulates vertices + triangle faces and writes a Wavefront OBJ."""

    def __init__(self) -> None:
        self.verts: list[tuple[float, float, float]] = []
        self.faces: list[tuple[int, int, int]] = []
        self.groups: list[tuple[str, int]] = []  # (name, first-face-index)

    def vert(self, x: float, y: float, z: float) -> int:
        self.verts.append((x, y, z))
        return len(self.verts)  # OBJ indices are 1-based

    def tri(self, a: int, b: int, c: int) -> None:
        self.faces.append((a, b, c))

    def group(self, name: str) -> None:
        self.groups.append((name, len(self.faces)))

    def quad(self, a: int, b: int, c: int, d: int) -> None:
        # a-b-c-d as a planar quad → two triangles
        self.tri(a, b, c)
        self.tri(a, c, d)

    def write(self, path: Path) -> None:
        lines = [f"# py-marathon-utils sector geometry - {len(self.verts)} verts, "
                 f"{len(self.faces)} tris"]
        for x, y, z in self.verts:
            lines.append(f"v {x:.2f} {y:.2f} {z:.2f}")
        gi = 0
        for fi, (a, b, c) in enumerate(self.faces):
            while gi < len(self.groups) and self.groups[gi][1] == fi:
                lines.append(f"g {self.groups[gi][0]}")
                gi += 1
            lines.append(f"f {a} {b} {c}")
        path.write_text("\n".join(lines) + "\n", encoding="ascii")


def _signed_area(ring: list[tuple[float, float]]) -> float:
    """2D shoelace; >0 = counter-clockwise in the given coordinate frame."""
    s = 0.0
    n = len(ring)
    for i in range(n):
        x1, y1 = ring[i]
        x2, y2 = ring[(i + 1) % n]
        s += x1 * y2 - x2 * y1
    return s * 0.5


def convert(level: dict, scale_cm_per_wu: float = 100.0, flip_y: bool = True) -> tuple[ObjBuilder, dict]:
    data = level["data"]
    epnt = data["EPNT"]
    polys = data["POLY"]
    lins = data["LINS"]
    S = scale_cm_per_wu / 1024.0
    fy = -1.0 if flip_y else 1.0

    def px(e: int) -> float:
        return epnt[e]["x"] * S

    def py(e: int) -> float:
        return epnt[e]["y"] * S * fy

    obj = ObjBuilder()

    # ---- Floors + ceilings -------------------------------------------------
    obj.group("floors_ceilings")
    for poly in polys:
        n = poly["vertex_count"]
        if n < 3:
            continue
        eids = poly["endpoints"][:n]
        fz = poly["floor_height"] * S
        cz = poly["ceiling_height"] * S
        ring2d = [(px(e), py(e)) for e in eids]
        ccw = _signed_area(ring2d) > 0

        floor_ids = [obj.vert(px(e), py(e), fz) for e in eids]
        ceil_ids = [obj.vert(px(e), py(e), cz) for e in eids]

        # Fan-triangulate (Marathon polygons are convex). Floor normal up (+Z):
        # emit CCW when viewed from above. Reverse the ring if it's CW.
        order = list(range(n)) if ccw else list(range(n - 1, -1, -1))
        for i in range(1, n - 1):
            obj.tri(floor_ids[order[0]], floor_ids[order[i]], floor_ids[order[i + 1]])
        # Ceiling normal down (−Z): opposite winding.
        rorder = order[::-1]
        for i in range(1, n - 1):
            obj.tri(ceil_ids[rorder[0]], ceil_ids[rorder[i]], ceil_ids[rorder[i + 1]])

    # ---- Walls -------------------------------------------------------------
    obj.group("walls")
    for line in lins:
        e1, e2 = line["endpoint1"], line["endpoint2"]
        cwp, ccwp = line["cw_poly"], line["ccw_poly"]
        a = (px(e1), py(e1))
        b = (px(e2), py(e2))

        def wall(z_bot: float, z_top: float) -> None:
            if z_top - z_bot <= 0.01:
                return
            v1 = obj.vert(a[0], a[1], z_bot)
            v2 = obj.vert(b[0], b[1], z_bot)
            v3 = obj.vert(b[0], b[1], z_top)
            v4 = obj.vert(a[0], a[1], z_top)
            obj.quad(v1, v2, v3, v4)
            # Two-sided: emit the reverse winding so walls are visible (and
            # block the view) from inside the sector too.
            obj.quad(v1, v4, v3, v2)

        cw = polys[cwp] if 0 <= cwp < len(polys) else None
        ccw = polys[ccwp] if 0 <= ccwp < len(polys) else None

        if cw and ccw:
            # Shared line between two sectors: fill the floor + ceiling steps.
            wall(min(cw["floor_height"], ccw["floor_height"]) * S,
                 max(cw["floor_height"], ccw["floor_height"]) * S)
            wall(min(cw["ceiling_height"], ccw["ceiling_height"]) * S,
                 max(cw["ceiling_height"], ccw["ceiling_height"]) * S)
        else:
            # Solid wall bordering the void: full height of the real sector.
            solid = cw or ccw
            if solid:
                wall(solid["floor_height"] * S, solid["ceiling_height"] * S)

    # ---- Placements sidecar ------------------------------------------------
    placements = _placements(data, polys, S, fy)
    return obj, placements


def _placements(data: dict, polys: list, S: float, fy: float) -> dict:
    out: dict = {"player_start": None, "lights": [], "objects": []}
    minf = data.get("Minf") or {}
    out["level_name"] = minf.get("level_name")

    for o in data.get("OBJS", []) or []:
        # OBJS x/y are absolute world coords; z is height above the polygon floor.
        poly = polys[o["polygon_index"]] if 0 <= o["polygon_index"] < len(polys) else None
        base = poly["floor_height"] if poly else 0
        pos = {
            "x": round(o["x"] * S, 2),
            "y": round(o["y"] * S * fy, 2),
            "z": round((base + o["z"]) * S, 2),
        }
        # Marathon facing: 512 units == 360°. UE yaw, flipped to match flip_y.
        yaw = (o["facing"] / 512.0) * 360.0
        if fy < 0:
            yaw = -yaw
        rec = {"type": o["type"], "object_index": o["object_index"],
               "facing_deg": round(yaw, 1), **pos}
        if o["type"] == _OBJ_PLAYER and out["player_start"] is None:
            out["player_start"] = rec
        out["objects"].append(rec)
    return out


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("level_json", type=Path, nargs="?",
                   help="a Reference/Maps/*.json file")
    p.add_argument("--all", type=Path, metavar="MAPS_DIR", default=None,
                   help="convert every *.json in this directory (e.g. Reference/Maps)")
    p.add_argument("--out", type=Path, default=None,
                   help="output dir (default: Reference/Geometry next to Maps)")
    p.add_argument("--scale", type=float, default=100.0, help="cm per World Unit")
    p.add_argument("--no-flip-y", action="store_true",
                   help="don't flip Marathon's +y-down (use if the level imports mirrored)")
    args = p.parse_args(argv)

    if args.all:
        files = sorted(p for p in args.all.glob("*.json") if not p.name.endswith("manifest.json"))
        out_dir = args.out or (args.all.resolve().parents[0] / "Geometry")
        for f in files:
            _convert_one(f, out_dir, args.scale, not args.no_flip_y)
        print(f"\nConverted {len(files)} levels -> {out_dir}")
        return 0

    if not args.level_json:
        p.error("provide a level_json file or --all <maps-dir>")
    out_dir = args.out or (args.level_json.resolve().parents[1] / "Geometry")
    _convert_one(args.level_json, out_dir, args.scale, not args.no_flip_y)
    return 0


def _convert_one(level_json: Path, out_dir: Path, scale: float, flip_y: bool) -> None:
    level = json.loads(Path(level_json).read_text(encoding="utf-8"))
    obj, placements = convert(level, scale_cm_per_wu=scale, flip_y=flip_y)

    out_dir.mkdir(parents=True, exist_ok=True)
    stem = level_json.stem
    obj_path = out_dir / f"{stem}.obj"
    json_path = out_dir / f"{stem}_placements.json"
    obj.write(obj_path)
    json_path.write_text(json.dumps(placements, indent=2), encoding="utf-8")

    bb = _bbox(obj.verts)
    print(f"{stem}: {len(obj.verts)} verts, {len(obj.faces)} tris")
    print(f"  bounds (cm): X[{bb[0]:.0f},{bb[1]:.0f}] Y[{bb[2]:.0f},{bb[3]:.0f}] Z[{bb[4]:.0f},{bb[5]:.0f}]")
    ps = placements.get("player_start")
    print(f"  player_start: {ps}")


def _bbox(verts):
    xs = [v[0] for v in verts]; ys = [v[1] for v in verts]; zs = [v[2] for v in verts]
    return (min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)) if verts else (0,)*6


if __name__ == "__main__":
    raise SystemExit(main())
