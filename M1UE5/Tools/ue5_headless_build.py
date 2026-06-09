"""Headless UE5 batch build — imports ALL Marathon levels with textures:

    UnrealEditor-Cmd.exe <project> -ExecutePythonScript=<this file> \\
        -RenderOffScreen -unattended -nosplash -nopause -stdout

Architecture:
  * A SHARED texture/material library (/Game/Textures, /Game/Materials). Each
    Marathon shape (collection+shape) is imported once as a Texture2D and wrapped
    in one UNLIT material (texture on emissive, nearest filter — matches the
    game's flat-shaded look and reads correctly at any light angle). Levels that
    reuse a shape reference the same material instead of duplicating it.
  * Per level: the OBJ is imported to /Game/Meshes/M1/<safe>/ as a StaticMesh,
    its material slots are reassigned to the shared library by slot name, and a
    /Game/Levels/M1/L_<safe> map is built (mesh actor + lights + overview
    PlayerStart) and saved.

Results -> Reference/Geometry/ue5_build_result.json. Set env M1_LIMIT=N to build
only the first N levels (for a cheap validation run before the full 37).
Uses the `unreal` module, so it only runs inside UnrealEditor-Cmd.
"""
import glob
import json
import os
import re
import traceback

import unreal

REPO = r"C:\m1-ue5\M1UE5"
GEO = os.path.join(REPO, "Reference", "Geometry")
RESULT = os.path.join(GEO, "ue5_build_result.json")

TEX_LIB = "/Game/Textures"      # shared Texture2D assets  (T_<coll>_<shape>)
MAT_LIB = "/Game/Materials"     # shared Material assets   (slot name)
MESH_DIR = "/Game/Meshes/M1"    # per-level: <MESH_DIR>/<safe>/<safe>
LEVEL_DIR = "/Game/Levels/M1"   # per-level: <LEVEL_DIR>/L_<safe>

_LIMIT = os.environ.get("M1_LIMIT")
LIMIT = int(_LIMIT) if _LIMIT else None

# Player spawn placement (real Marathon start from <stem>_placements.json).
# Must match AM1Character's capsule half-height (32) so the spawn sits just above
# the floor and does NOT straddle Marathon's low (~1m) ceilings.
PLAYER_CAPSULE_HALF = 32.0  # UE PlayerStart spawn height above floor (cm)
SPAWN_GAP = 4.0             # rest the capsule just above the floor

result = {"version": None, "shared": {}, "levels": {}, "errors": {}}


def _write():
    os.makedirs(GEO, exist_ok=True)
    with open(RESULT, "w", encoding="utf-8") as fh:
        json.dump(result, fh, indent=2)


def _safe(stem):
    """UE-safe asset name from a level filename stem."""
    return re.sub(r"[^0-9A-Za-z]+", "_", stem).strip("_")


# ---------------------------------------------------------------- shared library

def _scan_materials():
    """Union of material-name -> absolute texture path across every level .mtl."""
    mats = {}
    for mtl in sorted(glob.glob(os.path.join(GEO, "*.mtl"))):
        base = os.path.dirname(mtl)
        cur = None
        with open(mtl, encoding="utf-8") as fh:
            for raw in fh:
                line = raw.strip()
                if line.startswith("newmtl "):
                    cur = line.split(None, 1)[1]
                    mats.setdefault(cur, None)
                elif line.startswith("map_Kd ") and cur:
                    rel = line.split(None, 1)[1]
                    mats[cur] = os.path.normpath(os.path.join(base, rel))
    return mats


def _import_texture(abs_png, asset_name, dest):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", abs_png)
    task.set_editor_property("destination_path", dest)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    paths = list(task.get_editor_property("imported_object_paths"))
    tex = unreal.load_asset(paths[0]) if paths else None
    if isinstance(tex, unreal.Texture2D):
        tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
        return tex
    return None


def _make_material(name, tex, dest):
    asset_path = f"{dest}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.EditorAssetLibrary.delete_asset(asset_path)
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, dest, unreal.Material, unreal.MaterialFactoryNew())
    mel = unreal.MaterialEditingLibrary
    if name == "flat":
        # Geometry-inspection material: LIT flat-gray so the level's existing
        # DirectionalLight + SkyLight shade each face by its angle -> every wall,
        # reveal, floor and ceiling reads as a distinct tone (no texture noise).
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
        base = mel.create_material_expression(
            mat, unreal.MaterialExpressionConstant3Vector, -380, 0)
        base.set_editor_property("constant", unreal.LinearColor(0.62, 0.63, 0.66, 1.0))
        mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
        mel.recompile_material(mat)
        unreal.EditorAssetLibrary.save_asset(mat.get_path_name())
        return mat
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    if tex is not None:
        node = mel.create_material_expression(
            mat, unreal.MaterialExpressionTextureSample, -380, 0)
        node.set_editor_property("texture", tex)
        mel.connect_material_property(node, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        node = mel.create_material_expression(
            mat, unreal.MaterialExpressionConstant3Vector, -380, 0)
        node.set_editor_property("constant", unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
        mel.connect_material_property(node, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(mat.get_path_name())
    return mat


def _build_shared_library():
    """Import every referenced bitmap once and wrap each in a shared material."""
    mats = _scan_materials()
    lib = {}
    tex_count = 0
    for name, abs_png in sorted(mats.items()):
        tex = None
        if abs_png and os.path.isfile(abs_png):
            m = re.match(r"tex_(\d+)_(\d+)", name)
            tname = f"T_{m.group(1)}_{int(m.group(2)):03d}" if m else "T_" + _safe(name)
            tex = _import_texture(abs_png, tname, TEX_LIB)
            if tex is not None:
                tex_count += 1
        lib[name] = _make_material(name, tex, MAT_LIB)
    result["shared"] = {"materials": len(lib), "textures": tex_count}
    return lib


# ---------------------------------------------------------------- per-level

def _find_mesh(paths):
    for p in paths:
        asset = unreal.load_asset(p)
        if isinstance(asset, unreal.StaticMesh):
            return p, asset
    return None, None


def _set_complex_collision(mesh):
    """Use the sector's render triangles as collision so the player can walk on
    floors and bump walls (complex-as-simple — no convex hulls needed for static
    level geometry). Returns True if the trace flag was set."""
    bs = mesh.get_editor_property("body_setup")
    if bs is None:
        return False
    bs.set_editor_property(
        "collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    unreal.EditorAssetLibrary.save_asset(mesh.get_path_name())
    return True


def _load_placements(stem):
    """Load <stem>_placements.json (sidecar from scen_to_obj.py). Its coords are
    already in the UE-cm frame of the mesh (same scale + y-flip)."""
    path = os.path.join(GEO, stem + "_placements.json")
    if not os.path.isfile(path):
        return None
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


def _spawn_player_start(eas, mesh, stem, box):
    """Place a PlayerStart at the real Marathon spawn (the OBJS player object).
    Falls back to a high top-down overview vantage for levels with no player
    object. The actor is labelled so the FP render can find it."""
    ps = (_load_placements(stem) or {}).get("player_start")
    if ps:
        loc = unreal.Vector(ps["x"], ps["y"],
                            ps["z"] + PLAYER_CAPSULE_HALF + SPAWN_GAP)
        rot = unreal.Rotator(roll=0.0, pitch=0.0, yaw=ps.get("facing_deg", 0.0))
        actor = eas.spawn_actor_from_class(unreal.PlayerStart, loc, rot)
        actor.set_actor_label("PlayerStart_M1")
        return {"mode": "real",
                "loc": [round(loc.x, 2), round(loc.y, 2), round(loc.z, 2)],
                "facing_deg": ps.get("facing_deg", 0.0), "floor_z": ps["z"]}
    cx = (box.min.x + box.max.x) / 2.0
    cy = (box.min.y + box.max.y) / 2.0
    span = max(box.max.x - box.min.x, box.max.y - box.min.y)
    overview = unreal.Vector(cx, cy, box.max.z + span * 1.2)
    actor = eas.spawn_actor_from_class(
        unreal.PlayerStart, overview, unreal.Rotator(roll=0.0, pitch=-90.0, yaw=0.0))
    actor.set_actor_label("PlayerStart_Overview")
    return {"mode": "overview",
            "loc": [round(overview.x, 2), round(overview.y, 2), round(overview.z, 2)]}


def _build_one(obj_path, lib):
    stem = os.path.splitext(os.path.basename(obj_path))[0]
    safe = _safe(stem)
    mesh_folder = f"{MESH_DIR}/{safe}"
    level_pkg = f"{LEVEL_DIR}/L_{safe}"

    # Import the OBJ into a per-level folder (Interchange's own per-level
    # materials/textures stay contained there; we ignore them and reassign to
    # the shared library). Wipe first for determinism.
    if unreal.EditorAssetLibrary.does_directory_exist(mesh_folder):
        unreal.EditorAssetLibrary.delete_directory(mesh_folder)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", obj_path)
    task.set_editor_property("destination_path", mesh_folder)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    paths = list(task.get_editor_property("imported_object_paths"))
    mesh_path, mesh = _find_mesh(paths)
    if mesh is None:
        raise RuntimeError(f"no static mesh imported for {stem}")

    # Reassign slots to shared materials by slot name.
    static_mats = list(mesh.get_editor_property("static_materials"))
    assigned = 0
    missing = []
    for sm in static_mats:
        slot = str(sm.get_editor_property("material_slot_name"))
        if slot in lib:
            sm.set_editor_property("material_interface", lib[slot])
            assigned += 1
        else:
            missing.append(slot)
    mesh.set_editor_property("static_materials", static_mats)
    # Walk on it: render triangles double as collision (floors + walls).
    collision_ok = _set_complex_collision(mesh)
    unreal.EditorAssetLibrary.save_asset(mesh.get_path_name())

    # Build the level map (idempotent: load+wipe if it already exists).
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if unreal.EditorAssetLibrary.does_asset_exist(level_pkg):
        les.load_level(level_pkg)
        for a in list(eas.get_all_level_actors()):
            eas.destroy_actor(a)
    else:
        les.new_level(level_pkg)

    sector = eas.spawn_actor_from_object(mesh, unreal.Vector(0, 0, 0))
    sector.set_actor_label("Sector")
    eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1500),
                               unreal.Rotator(roll=0.0, pitch=-50.0, yaw=45.0))
    eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 1500))

    box = mesh.get_bounding_box()
    ps_info = _spawn_player_start(eas, mesh, stem, box)
    les.save_current_level()

    return {
        "mesh": mesh_path,
        "level": level_pkg,
        "triangles": mesh.get_num_triangles(0),
        "slots": len(static_mats),
        "assigned": assigned,
        "missing": missing,
        "collision_complex_as_simple": collision_ok,
        "player_start": ps_info,
        "bounds_min": [box.min.x, box.min.y, box.min.z],
        "bounds_max": [box.max.x, box.max.y, box.max.z],
    }


# ---------------------------------------------------------------- driver

result["version"] = unreal.SystemLibrary.get_engine_version()
_write()

try:
    lib = _build_shared_library()
    unreal.log(f"[headless] shared library: {result['shared']}")
except Exception as e:  # noqa: BLE001
    result["errors"]["shared_library"] = f"{e!r}\n{traceback.format_exc()}"
    unreal.log_error(f"[headless] shared library FAILED: {e}")
    lib = {}

_write()

objs = sorted(glob.glob(os.path.join(GEO, "*.obj")))
_only = os.environ.get("M1_ONLY")          # build only the OBJ whose stem matches
if _only:
    objs = [o for o in objs if os.path.splitext(os.path.basename(o))[0] == _only]
if LIMIT:
    objs = objs[:LIMIT]
unreal.log(f"[headless] building {len(objs)} levels")

for obj in objs:
    stem = os.path.splitext(os.path.basename(obj))[0]
    unreal.log(f"[headless] START level: {stem}")
    try:
        result["levels"][stem] = _build_one(obj, lib)
        unreal.log(f"[headless] OK level: {stem}")
    except Exception as e:  # noqa: BLE001
        result["errors"][stem] = f"{e!r}\n{traceback.format_exc()}"
        unreal.log_error(f"[headless] FAILED level {stem}: {e}")
    _write()  # checkpoint after every level so a crash is diagnosable

result["built"] = len(result["levels"])
_write()
unreal.log(f"[headless] done: {result['built']}/{len(objs)} levels, "
           f"{len(result['errors'])} errors")

try:
    unreal.SystemLibrary.quit_editor()
except Exception:  # noqa: BLE001
    pass
