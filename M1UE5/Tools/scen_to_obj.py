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
import re
from pathlib import Path

# Marathon OBJS class ids
_OBJ_MONSTER, _OBJ_SCENERY, _OBJ_ITEM, _OBJ_PLAYER, _OBJ_GOAL, _OBJ_SOUND = range(6)


class ObjBuilder:
    """Accumulates vertices + triangle faces and writes a Wavefront OBJ."""

    def __init__(self) -> None:
        self.verts: list[tuple[float, float, float]] = []
        self.uvs: list[tuple[float, float]] = []          # parallel to verts (1:1)
        self.faces: list[tuple[int, int, int, str]] = []  # (a, b, c, material)
        # material name -> texture path relative to the .obj (or None = default)
        self.materials: dict[str, str | None] = {}

    def vert(self, x: float, y: float, z: float, u: float = 0.0, v: float = 0.0) -> int:
        self.verts.append((x, y, z))
        self.uvs.append((u, v))
        return len(self.verts)  # OBJ indices are 1-based (shared for v and vt)

    def material(self, name: str, texture: str | None) -> None:
        self.materials.setdefault(name, texture)

    def tri(self, a: int, b: int, c: int, mat: str = "default") -> None:
        self.faces.append((a, b, c, mat))

    def quad(self, a: int, b: int, c: int, d: int, mat: str = "default") -> None:
        self.tri(a, b, c, mat)
        self.tri(a, c, d, mat)

    def write(self, path: Path) -> None:
        mtl_name = path.stem + ".mtl"
        lines = [f"# py-marathon-utils sector geometry - {len(self.verts)} verts, "
                 f"{len(self.faces)} tris",
                 f"mtllib {mtl_name}"]
        for x, y, z in self.verts:
            lines.append(f"v {x:.2f} {y:.2f} {z:.2f}")
        for u, v in self.uvs:
            lines.append(f"vt {u:.4f} {v:.4f}")
        # Faces grouped by material. Normals are intentionally omitted: the build's
        # importer derives them. (We route OBJ import through UE's LEGACY importer,
        # not Interchange -- Interchange's OBJ translator silently culls ~17% of our
        # triangles, punching holes around terminals. See ue5_headless_build.py.)
        by_mat: dict[str, list[tuple[int, int, int]]] = {}
        for a, b, c, mat in self.faces:
            by_mat.setdefault(mat, []).append((a, b, c))
        for mat, tris in by_mat.items():
            lines.append(f"usemtl {mat}")
            for a, b, c in tris:
                lines.append(f"f {a}/{a} {b}/{b} {c}/{c}")
        path.write_text("\n".join(lines) + "\n", encoding="ascii")
        self._write_mtl(path.with_suffix(".mtl"))

    def _write_mtl(self, path: Path) -> None:
        lines = ["# py-marathon-utils materials"]
        for name, tex in self.materials.items():
            lines.append(f"newmtl {name}")
            lines.append("Ka 1.0 1.0 1.0")
            lines.append("Kd 1.0 1.0 1.0")
            if tex:
                lines.append(f"map_Kd {tex}")
            else:
                lines.append("Kd 0.6 0.6 0.6")
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


def convert(level: dict, scale_cm_per_wu: float = 100.0, flip_y: bool = True,
            poly_subset: set | None = None, flat: bool = False) -> tuple[ObjBuilder, dict]:
    data = level["data"]
    epnt = data["EPNT"]
    polys = data["POLY"]
    # Micro-slice mode: emit only the listed polygons (walls facing excluded
    # polys still seal as exterior). Used to isolate the terminal for debugging.
    if poly_subset is not None:
        polys = [pp for i, pp in enumerate(data["POLY"]) if i in poly_subset]
    lins = data["LINS"]
    S = scale_cm_per_wu / 1024.0
    fy = -1.0 if flip_y else 1.0

    def px(e: int) -> float:
        return epnt[e]["x"] * S

    def py(e: int) -> float:
        return epnt[e]["y"] * S * fy

    sids = data.get("SIDS") or []
    obj = ObjBuilder()

    # Marathon tiles a 128px texture once per World-Unit (1024 internal units),
    # so UV = marathon_units / 1024.  Texture PNGs live in ../Shapes/Coll_NN/.
    # Per-surface Marathon lighting: each surface carries the brightness of its
    # LITE channel (POLY.floor_light / ceiling_light), baked into the material name
    # as tex_C_S_b<pct>. The importer scales that material's emissive by pct/100, so
    # every wall/floor reads at its authored brightness -- room-by-room mood with NO
    # dynamic lights (so no hotspots, no black gaps, and never brighter than the
    # texture). Floored at 20% so the dimmest rooms stay navigable, not pure black.
    lite = data.get("LITE") or []

    def light_pct(idx) -> int:
        on = 1.0
        if idx is not None and 0 <= idx < len(lite):
            L = lite[idx]
            on = L.get("max_intensity") or L.get("intensity") or 0.0
        on = max(0.0, min(1.0, on))
        # Lift the dark end off pure-black (filmic tonemapping crushes low emissive)
        # while keeping bright rooms at full texture color: 0 -> 35%, 1.0 -> 100%.
        disp = 0.35 + 0.65 * on
        return int(round(disp * 10) * 10)   # 10% steps -> bounded material count

    _wall_light = None  # set per-polygon in the wall loop; read by emit_wall

    def mat_for(descriptor: int, light_idx=None) -> str:
        """Register + return a material name for a packed shape_descriptor, with the
        surface's LITE brightness baked in as a _b<pct> suffix."""
        if flat:
            obj.material("flat", None)   # one material, no textures (geometry-only test level)
            return "flat"
        if descriptor == 0xFFFF:
            obj.material("default", None)
            return "default"
        coll = (descriptor >> 8) & 0x1F
        shape = descriptor & 0xFF
        if light_idx is None:
            name = f"tex_{coll}_{shape}"
        else:
            name = f"tex_{coll}_{shape}_b{light_pct(light_idx)}"
        obj.material(name, f"../Shapes/Coll_{coll:02d}/bitmaps/{shape:03d}.png")
        return name

    def uv_e(e: int) -> tuple[float, float]:
        return (epnt[e]["x"] / 1024.0, epnt[e]["y"] / 1024.0)

    # ---- Closed-platform rebake --------------------------------------------
    # Some platform polygons are authored CLOSED (floor >= ceiling -> zero height),
    # so the loop below would emit a degenerate floor == a hole you fall through.
    # Re-open them: floor at the LOWEST neighbour floor, ceiling at the HIGHEST
    # neighbour ceiling, giving a walkable passage. (poly_subset/micro-slice mode
    # keeps original behaviour.)
    closed_open: dict[int, tuple[int, int]] = {}
    if poly_subset is None:
        all_polys = data["POLY"]

        def _nbr(pi: int, attr: str, agg):
            vals = []
            for li in all_polys[pi].get("lines", []):
                if 0 <= li < len(lins):
                    ln = lins[li]
                    for k in ("cw_poly", "ccw_poly"):
                        npi = ln.get(k, -1)
                        if npi is not None and 0 <= npi < len(all_polys) and npi != pi:
                            vals.append(all_polys[npi][attr])
            return agg(vals) if vals else None

        for pl in (data.get("plat") or []):
            pi = pl["polygon_index"]
            if 0 <= pi < len(all_polys) and all_polys[pi]["floor_height"] >= all_polys[pi]["ceiling_height"]:
                of = _nbr(pi, "floor_height", min)
                oc = _nbr(pi, "ceiling_height", max)
                if of is not None and oc is not None and oc > of:
                    closed_open[pi] = (of, oc)

    # Ride-on elevator polygons get their floor as a MOVABLE mesh (AM1Elevator at
    # runtime), so we must NOT bake a static floor for them -- the slab IS the
    # floor. Use the SAME detection as the elevator table so every dropped floor
    # has a matching slab (drop ⟺ slab, no holes). Ceiling + walls stay baked.
    elev_polys: set = set()
    if poly_subset is None:
        try:
            import gen_platform_assets as _gp
            elev_polys = {e["poly"] for e in _gp.elevators_for_level(data)}
        except Exception:
            elev_polys = set()

    # ---- Floors + ceilings -------------------------------------------------
    for idx, poly in enumerate(polys):
        n = poly["vertex_count"]
        if n < 3:
            continue
        eids = poly["endpoints"][:n]
        fh, ch = poly["floor_height"], poly["ceiling_height"]
        if idx in closed_open:
            fh, ch = closed_open[idx]   # walkable passage instead of a sealed hole
        fz = fh * S
        cz = ch * S
        ring2d = [(px(e), py(e)) for e in eids]
        ccw = _signed_area(ring2d) > 0
        cmat = mat_for(poly["ceiling_shape"], poly.get("ceiling_light"))
        order = list(range(n)) if ccw else list(range(n - 1, -1, -1))

        # Floor: skipped for elevator polys (the movable slab provides it).
        if idx not in elev_polys:
            fmat = mat_for(poly["floor_shape"], poly.get("floor_light"))
            floor_ids = [obj.vert(px(e), py(e), fz, *uv_e(e)) for e in eids]
            for i in range(1, n - 1):  # fan-triangulate; floor normal up (+Z)
                obj.tri(floor_ids[order[0]], floor_ids[order[i]], floor_ids[order[i + 1]], fmat)

        ceil_ids = [obj.vert(px(e), py(e), cz, *uv_e(e)) for e in eids]
        rorder = order[::-1]
        for i in range(1, n - 1):
            obj.tri(ceil_ids[rorder[0]], ceil_ids[rorder[i]], ceil_ids[rorder[i + 1]], cmat)

    # ---- Walls (per-polygon side, faithful to Aleph One's renderer) --------
    # Each side is textured facing INTO its polygon, so the two sides of a line
    # are coplanar but face opposite ways -> single-sided geometry back-face-culls
    # the far one (no z-fighting). Side types (map.h): full=0 high=1 low=2
    # composite=3 split=4. Texture UVs (RenderRasterize_Shader render_node_side):
    #   U = (x0 + dist_from_endpoint_i) / WORLD_ONE   along the wall
    #   V (Marathon, +down from the surface top h1) = (h1 + y0 - z)/WORLD_ONE
    # UE's OBJ importer flips V, so we emit V_obj = (z - h1 - y0)/WORLD_ONE which
    # is right-side-up after the flip and carries the per-side offset. x0/y0 are
    # taken mod WORLD_ONE (the offset's fractional tile, as the engine does).
    def emit_wall(e0: int, e1: int, h0_i: float, h1_i: float,
                  texdef: dict, length_i: float) -> None:
        shape = texdef.get("shape", 0xFFFF)
        if shape == 0xFFFF or h1_i - h0_i <= 0.5:
            return
        mat = mat_for(shape, _wall_light)   # wall lit by its polygon's floor_light
        x0 = texdef.get("x", 0) % 1024
        y0 = texdef.get("y", 0) % 1024
        u0 = x0 / 1024.0
        u1 = (x0 + length_i) / 1024.0
        vb = (h0_i - h1_i - y0) / 1024.0      # V at bottom (z=h0)
        vt = (-y0) / 1024.0                   # V at top    (z=h1)
        ax, ay, zb = px(e0), py(e0), h0_i * S
        bx, by, zt = px(e1), py(e1), h1_i * S
        q0 = obj.vert(ax, ay, zb, u0, vb)
        q1 = obj.vert(bx, by, zb, u1, vb)
        q2 = obj.vert(bx, by, zt, u1, vt)
        q3 = obj.vert(ax, ay, zt, u0, vt)
        obj.quad(q0, q1, q2, q3, mat)         # faces into the polygon

    obj.material("default", None)
    for poly in polys:
        n = poly["vertex_count"]
        if n < 3:
            continue
        _wall_light = poly.get("floor_light")   # emit_wall reads this via closure
        eids = poly["endpoints"][:n]
        sides_i = (poly.get("sides") or [])[:n]
        lines_i = (poly.get("lines") or [])[:n]
        floor_i = poly["floor_height"]
        ceil_i = poly["ceiling_height"]
        for i in range(n):
            si = sides_i[i] if i < len(sides_i) else -1
            if not (0 <= si < len(sids)):
                continue
            side = sids[si]
            li = lines_i[i] if i < len(lines_i) else -1
            line = lins[li] if 0 <= li < len(lins) else {}
            e0, e1 = eids[i], eids[(i + 1) % n]
            length_i = line.get("length") or int((
                (epnt[e1]["x"] - epnt[e0]["x"]) ** 2
                + (epnt[e1]["y"] - epnt[e0]["y"]) ** 2) ** 0.5)
            hi_floor = line.get("highest_floor", floor_i)
            lo_ceil = line.get("lowest_ceiling", ceil_i)
            stype = side.get("type", 0)
            prim = side.get("primary", {})
            sec = side.get("secondary", {})
            trans = side.get("transparent", {})
            if stype == 1:          # high: primary panel down from the ceiling
                emit_wall(e0, e1, min(lo_ceil, ceil_i), ceil_i, prim, length_i)
            elif stype == 2:        # low: primary panel up from the floor
                emit_wall(e0, e1, floor_i, max(hi_floor, floor_i), prim, length_i)
            elif stype == 4:        # split: secondary low + primary high
                emit_wall(e0, e1, floor_i, max(hi_floor, floor_i), sec, length_i)
                emit_wall(e0, e1, min(lo_ceil, ceil_i), ceil_i, prim, length_i)
            else:                   # full (0) / composite (3): primary floor->ceiling
                emit_wall(e0, e1, floor_i, ceil_i, prim, length_i)
                if stype == 3:
                    emit_wall(e0, e1, floor_i, ceil_i, sec, length_i)
            # transparent texture mounted in the opening (e.g. a window/panel)
            if trans.get("shape", 0xFFFF) != 0xFFFF:
                emit_wall(e0, e1, max(hi_floor, floor_i), lo_ceil, trans, length_i)

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
        # UE's OBJ importer negates Y *on top of* scen_to_obj's flip_y, so the
        # imported sector mesh ends up at y = +epnt_y*S. Placements must live in
        # that final imported frame (mesh_fy = -fy). Using `fy` here mirrors every
        # spawn out of the geometry, so the player starts beside the level and
        # falls forever.
        mesh_fy = -fy
        pos = {
            "x": round(o["x"] * S, 2),
            "y": round(o["y"] * S * mesh_fy, 2),
            "z": round((base + o["z"]) * S, 2),
        }
        # Marathon facing: 512 units == 360°. The net Marathon->UE transform is a
        # pure scale (no reflection), so the heading maps straight through.
        yaw = (o["facing"] / 512.0) * 360.0
        if mesh_fy < 0:
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
    p.add_argument("--polys", default=None,
                   help="comma list of polygon indices to emit (micro-slice test level)")
    p.add_argument("--flat", action="store_true",
                   help="emit a single texture-less 'flat' material (geometry-only)")
    p.add_argument("--out-name", default=None,
                   help="override the output stem (e.g. TEST_Terminal -> L_TEST_Terminal)")
    args = p.parse_args(argv)
    subset = {int(x) for x in args.polys.split(",") if x.strip()} if args.polys else None

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
    _convert_one(args.level_json, out_dir, args.scale, not args.no_flip_y,
                 poly_subset=subset, flat=args.flat, out_name=args.out_name)
    return 0


def _convert_one(level_json: Path, out_dir: Path, scale: float, flip_y: bool,
                 poly_subset: set | None = None, flat: bool = False,
                 out_name: str | None = None) -> None:
    level = json.loads(Path(level_json).read_text(encoding="utf-8"))
    obj, placements = convert(level, scale_cm_per_wu=scale, flip_y=flip_y,
                              poly_subset=poly_subset, flat=flat)

    out_dir.mkdir(parents=True, exist_ok=True)
    # Space-free stem: an OBJ `mtllib` line is whitespace-delimited, so UE's
    # legacy OBJ importer reads only the first token of a spaced filename, fails
    # to find the .mtl, and falls back to a single 'defaultMat' slot instead of
    # the real per-texture slots. Underscores keep obj/mtl/placements consistent
    # and match the build's _safe() level-name normalization.
    stem = out_name or re.sub(r"\s+", "_", level_json.stem)
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
