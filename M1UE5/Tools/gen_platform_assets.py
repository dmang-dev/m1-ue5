"""Generate M1PlatformTable.h -- Marathon doors as movable panel actors.

Marathon doors are *platforms*: a narrow sector polygon between two rooms whose
floor rises to the ceiling to seal the passage (closed) and drops to the
neighbour floor to open it. We model each as a box panel (AM1Platform) that
fills the doorway when closed and sinks below the floor when open.

v1 scope -- DOORS ONLY, zero geometry risk:
  * Only narrow platform polygons (min AABB dimension < DOOR_MAX_WU) are doors.
  * Only *authored-open* polygons (floor < ceiling) are emitted, so the static
    sector mesh already has a walkable floor through the doorway -- the panel is
    pure addition, no converter/geometry changes needed.
  Ride-on elevators (wide platforms) and authored-closed doors are a documented
  follow-up (they need the static floor normalised / the floor itself to move).

Run with the venv interpreter (py-marathon-utils not needed -- reads the JSON):

    C:\\m1-ue5\\.venv\\Scripts\\python.exe Tools/gen_platform_assets.py

Coordinate frame matches scen_to_obj placements (the final imported UE frame):
    S = 100/1024 cm per Marathon unit;  X = mx*S,  Y = my*S (+y),  Z = mz*S.
"""
import glob
import os
import re

import json

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAPS = os.path.join(ROOT, "Reference", "Maps")
OUT = os.path.join(ROOT, "Source", "M1UE5", "M1PlatformTable.h")

S = 100.0 / 1024.0          # cm per Marathon internal unit
TICKS_PER_SEC = 30.0
DOOR_MAX_WU = 1.5           # narrow dimension below this (World Units) => a door
MIN_DOOR_TALL_WU = 0.3      # skip near-degenerate panels
PLATFORM_POLY_TYPES = {5, 7, 9}  # platform, platform-on-trigger, platform-off-trigger


def safe(stem):
    return re.sub(r"[^0-9A-Za-z]+", "_", stem).strip("_")


def tex_cs(descriptor):
    """Marathon shape_descriptor -> (collection, shape) for /Game/Materials/
    tex_<coll>_<shape>. (-1,-1) for an empty/landscape descriptor."""
    if not isinstance(descriptor, int) or descriptor == 0xFFFF:
        return (-1, -1)
    return ((descriptor >> 8) & 0x1F, descriptor & 0xFF)


def light_pct(lite, idx):
    """Surface brightness percent for a LITE channel. MUST match scen_to_obj.py's
    light_pct (0 -> 35%, 1.0 -> 100%, 10% steps) so a door/elevator loads the same
    tex_<coll>_<shape>_b<pct> material that its sector floor was baked with."""
    on = 1.0
    if idx is not None and 0 <= idx < len(lite):
        L = lite[idx]
        on = L.get("max_intensity") or L.get("intensity") or 0.0
    on = max(0.0, min(1.0, on))
    return int(round((0.35 + 0.65 * on) * 10) * 10)


def aabb(epnt, eids):
    xs = [epnt[e]["x"] for e in eids]
    ys = [epnt[e]["y"] for e in eids]
    return min(xs), max(xs), min(ys), max(ys)


ELEV_HEADROOM_CM = 100.0    # keep the rider clear of the ceiling (no crush)
ELEV_MIN_RISE_CM = 30.0     # below this it's not worth a lift
ELEV_DEFAULT_RISE_CM = 200.0  # rise when no higher neighbour floor is found
ELEV_SLAB_THICK_CM = 16.0


def neighbor_max_floor(data, pi):
    """Highest floor_height among polygons sharing a line with polygon pi (the
    natural top of a ride-on lift). None if it has no neighbours."""
    polys = data.get("POLY") or []
    lins = data.get("LINS") or []
    p = polys[pi]
    best = None
    for li in p.get("lines", []):
        if not (0 <= li < len(lins)):
            continue
        ln = lins[li]
        for key in ("cw_poly", "ccw_poly"):
            npi = ln.get(key, -1)
            if npi is not None and 0 <= npi < len(polys) and npi != pi:
                fh = polys[npi]["floor_height"]
                if best is None or fh > best:
                    best = fh
    return best


def elevators_for_level(data):
    """WIDE platform polygons modelled as ride-on lifts: a movable floor slab that
    rises from the polygon floor to the highest neighbour floor (clamped to leave
    headroom under the ceiling), carrying the player. Authored-closed polygons
    (floor>=ceiling) still need a geometry rebake and are skipped."""
    polys = data.get("POLY") or []
    epnt = data.get("EPNT") or []
    plats = data.get("plat") or []
    out = []
    for pl in plats:
        pi = pl["polygon_index"]
        if not (0 <= pi < len(polys)):
            continue
        p = polys[pi]
        n = p["vertex_count"]
        if n < 3:
            continue
        eids = p["endpoints"][:n]
        min_x, max_x, min_y, max_y = aabb(epnt, eids)
        if min((max_x - min_x), (max_y - min_y)) / 1024.0 < DOOR_MAX_WU:
            continue  # narrow -> handled as a door panel
        floor_i, ceil_i = p["floor_height"], p["ceiling_height"]
        if floor_i >= ceil_i:
            continue  # authored-closed -> floor rebake (follow-up)
        low_cm = floor_i * S
        nb = neighbor_max_floor(data, pi)
        cap_cm = ceil_i * S - ELEV_HEADROOM_CM
        high_cm = min(cap_cm, (nb * S if (nb is not None and nb > floor_i)
                               else low_cm + ELEV_DEFAULT_RISE_CM))
        if high_cm - low_cm < ELEV_MIN_RISE_CM:
            continue
        coll, shape = tex_cs(p.get("floor_shape"))
        bucket = light_pct(data.get("LITE") or [], p.get("floor_light"))
        out.append({
            "x": round((min_x + max_x) / 2.0 * S, 2),
            "y": round((min_y + max_y) / 2.0 * S, 2),
            "sx": round((max_x - min_x) * S, 2),
            "sy": round((max_y - min_y) * S, 2),
            "low": round(low_cm, 2),
            "high": round(high_cm, 2),
            "speed": round(max(abs(pl["speed"]), 1) * S * TICKS_PER_SEC, 1),
            "poly": pi,
            "tag": pl.get("tag", -1),
            "coll": coll, "shape": shape,        # floor texture -> tex_<coll>_<shape>
            "bucket": bucket,                    # per-surface brightness -> _b<pct>
        })
    return out


def doors_for_level(data):
    polys = data.get("POLY") or []
    epnt = data.get("EPNT") or []
    plats = data.get("plat") or []
    out = []
    for pl in plats:
        pi = pl["polygon_index"]
        if not (0 <= pi < len(polys)):
            continue
        p = polys[pi]
        n = p["vertex_count"]
        if n < 3:
            continue
        eids = p["endpoints"][:n]
        floor_i, ceil_i = p["floor_height"], p["ceiling_height"]
        if floor_i >= ceil_i:
            continue  # authored-closed -> needs floor normalisation (follow-up)
        min_x, max_x, min_y, max_y = aabb(epnt, eids)
        w_wu = (max_x - min_x) / 1024.0
        h_wu = (max_y - min_y) / 1024.0
        if min(w_wu, h_wu) >= DOOR_MAX_WU:
            continue  # wide -> ride-on platform/elevator (follow-up)
        if (ceil_i - floor_i) / 1024.0 < MIN_DOOR_TALL_WU:
            continue
        cx = (min_x + max_x) / 2.0
        cy = (min_y + max_y) / 2.0
        size_z = (ceil_i - floor_i) * S
        speed_cms = max(abs(pl["speed"]), 1) * S * TICKS_PER_SEC
        dwell = pl["delay"] / TICKS_PER_SEC
        if dwell <= 0.0:
            dwell = 3.0  # default open-dwell for instant-return doors
        coll, shape = tex_cs(p.get("floor_shape"))
        bucket = light_pct(data.get("LITE") or [], p.get("floor_light"))
        out.append({
            "x": round(cx * S, 2),
            "y": round(cy * S, 2),               # +y: final imported frame
            "z": round((floor_i + ceil_i) / 2.0 * S, 2),  # closed centre
            "sx": round((max_x - min_x) * S, 2),
            "sy": round((max_y - min_y) * S, 2),
            "sz": round(size_z, 2),
            "speed": round(speed_cms, 1),
            "dwell": round(dwell, 2),
            "poly": pi,                          # for platform-switch matching
            "tag": pl.get("tag", -1),            # for tag-switch matching
            "coll": coll, "shape": shape,        # floor texture -> tex_<coll>_<shape>
            "bucket": bucket,                    # per-surface brightness -> _b<pct>
        })
    return out


def main():
    files = sorted(f for f in glob.glob(os.path.join(MAPS, "*.json"))
                   if not f.endswith("manifest.json"))
    levels = []
    elev_levels = []
    total = 0
    total_elev = 0
    for f in files:
        d = json.load(open(f, encoding="utf-8"))
        data = d.get("data", {})
        stem = safe(os.path.splitext(os.path.basename(f))[0])
        level_name = "L_" + stem
        doors = doors_for_level(data)
        total += len(doors)
        levels.append((level_name, doors))
        elevs = elevators_for_level(data)
        total_elev += len(elevs)
        elev_levels.append((level_name, elevs))

    lines = [
        "// GENERATED by Tools/gen_platform_assets.py -- do not hand-edit.",
        "// Regenerate with:  python Tools/gen_platform_assets.py",
        "",
        "#pragma once",
        "",
        "#include \"CoreMinimal.h\"",
        "#include \"Containers/ArrayView.h\"",
        "",
        "/** One Marathon door: a box panel that fills the doorway when closed and",
        " *  sinks by SizeZ when open. Position/size in UE cm (final imported frame). */",
        "struct FM1Door",
        "{",
        "\tfloat X, Y, Z;        // closed-state centre",
        "\tfloat SizeX, SizeY, SizeZ;",
        "\tfloat Speed;          // cm/s travel",
        "\tfloat Dwell;          // seconds held open before auto-close",
        "\tint32 PolyIndex;      // source polygon (platform-switch target)",
        "\tint32 Tag;            // platform tag (tag-switch target; -1 = none)",
        "\tint32 TexColl, TexShape;  // floor texture -> /Game/Materials/tex_<coll>_<shape>",
        "\tint32 TexBucket;      // per-surface brightness -> ..._b<TexBucket>",
        "};",
        "",
        "struct FM1LevelDoors",
        "{",
        "\tconst TCHAR* LevelName;",
        "\tconst FM1Door* Doors;",
        "\tint32 Count;",
        "};",
        "",
    ]
    for level_name, doors in levels:
        arr = "GM1Doors_" + level_name
        lines.append("static const FM1Door %s[] =" % arr)
        lines.append("{")
        for d in doors:
            lines.append("\t{ %.2ff, %.2ff, %.2ff, %.2ff, %.2ff, %.2ff, %.1ff, %.2ff, %d, %d, %d, %d, %d },"
                         % (d["x"], d["y"], d["z"], d["sx"], d["sy"], d["sz"], d["speed"], d["dwell"],
                            d["poly"], d["tag"], d["coll"], d["shape"], d["bucket"]))
        if not doors:
            # C++ forbids zero-size arrays; emit a sentinel kept out by Count=0.
            lines.append("\t{ 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.0f, 0.00f, -1, -1, -1, -1, 100 },  // no doors")
        lines.append("};")
        lines.append("")

    lines.append("static const FM1LevelDoors GM1LevelDoorTable[] =")
    lines.append("{")
    for level_name, doors in levels:
        lines.append("\t{ TEXT(\"%s\"), GM1Doors_%s, %d },"
                     % (level_name, level_name, len(doors)))
    lines.append("};")
    lines.append("")
    lines.append("static const int32 GM1LevelDoorTableCount = UE_ARRAY_COUNT(GM1LevelDoorTable);")
    lines.append("")
    lines.append("static TArrayView<const FM1Door> M1DoorsForLevel(const FString& MapName)")
    lines.append("{")
    lines.append("\tfor (const FM1LevelDoors& Level : GM1LevelDoorTable)")
    lines.append("\t{")
    lines.append("\t\tif (MapName.Contains(Level.LevelName, ESearchCase::IgnoreCase))")
    lines.append("\t\t{")
    lines.append("\t\t\treturn TArrayView<const FM1Door>(Level.Doors, Level.Count);")
    lines.append("\t\t}")
    lines.append("\t}")
    lines.append("\treturn TArrayView<const FM1Door>();")
    lines.append("}")
    lines.append("")

    # --- Ride-on elevators (wide platforms) -------------------------------------
    lines += [
        "/** One Marathon ride-on elevator: a movable floor slab (footprint SizeX x",
        " *  SizeY) whose TOP rises from LowZ to HighZ, carrying the player. cm. */",
        "struct FM1Elevator",
        "{",
        "\tfloat X, Y;            // footprint centre (UE cm, final imported frame)",
        "\tfloat SizeX, SizeY;",
        "\tfloat LowZ, HighZ;     // slab-top travel range",
        "\tfloat Speed;           // cm/s",
        "\tint32 PolyIndex, Tag;  // switch targets (platform/tag switch)",
        "\tint32 TexColl, TexShape;  // floor texture -> /Game/Materials/tex_<coll>_<shape>",
        "\tint32 TexBucket;      // per-surface brightness -> ..._b<TexBucket>",
        "};",
        "",
        "struct FM1LevelElevators",
        "{",
        "\tconst TCHAR* LevelName;",
        "\tconst FM1Elevator* Elevators;",
        "\tint32 Count;",
        "};",
        "",
    ]
    for level_name, elevs in elev_levels:
        lines.append("static const FM1Elevator GM1Elevators_%s[] =" % level_name)
        lines.append("{")
        for e in elevs:
            lines.append("\t{ %.2ff, %.2ff, %.2ff, %.2ff, %.2ff, %.2ff, %.1ff, %d, %d, %d, %d, %d },"
                         % (e["x"], e["y"], e["sx"], e["sy"], e["low"], e["high"],
                            e["speed"], e["poly"], e["tag"], e["coll"], e["shape"], e["bucket"]))
        if not elevs:
            lines.append("\t{ 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.0f, -1, -1, -1, -1, 100 },  // none")
        lines.append("};")
        lines.append("")
    lines.append("static const FM1LevelElevators GM1LevelElevatorTable[] =")
    lines.append("{")
    for level_name, elevs in elev_levels:
        lines.append("\t{ TEXT(\"%s\"), GM1Elevators_%s, %d },"
                     % (level_name, level_name, len(elevs)))
    lines.append("};")
    lines.append("")
    lines.append("static TArrayView<const FM1Elevator> M1ElevatorsForLevel(const FString& MapName)")
    lines.append("{")
    lines.append("\tfor (const FM1LevelElevators& Level : GM1LevelElevatorTable)")
    lines.append("\t{")
    lines.append("\t\tif (MapName.Contains(Level.LevelName, ESearchCase::IgnoreCase))")
    lines.append("\t\t{")
    lines.append("\t\t\treturn TArrayView<const FM1Elevator>(Level.Elevators, Level.Count);")
    lines.append("\t\t}")
    lines.append("\t}")
    lines.append("\treturn TArrayView<const FM1Elevator>();")
    lines.append("}")
    lines.append("")

    with open(OUT, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines))

    print("wrote %s" % OUT)
    print("%d doors, %d elevators across %d levels" % (total, total_elev, len(levels)))
    for level_name, doors in levels:
        if doors:
            print("  %-34s %d" % (level_name, len(doors)))


if __name__ == "__main__":
    main()
