"""Headless UE5 build/validate — drives the editor with no GUI:

    UnrealEditor-Cmd.exe <project> -run=pythonscript -script=<this file> ...

Every step is wrapped so a single (slow) editor launch reports exactly which
capabilities work. Results -> Reference/Geometry/ue5_build_result.json, which
the calling agent reads back to decide next steps. Uses the `unreal` module,
so it only runs inside UnrealEditor-Cmd, not plain Python.
"""
import json
import os
import re
import traceback

import unreal

REPO = r"C:\m1-ue5\M1UE5"
GEO = os.path.join(REPO, "Reference", "Geometry")
OBJ = os.path.join(GEO, "00_Arrival.obj")
MTL = os.path.join(GEO, "00_Arrival.mtl")
PLACEMENTS = os.path.join(GEO, "00_Arrival_placements.json")
DEST_MESH = "/Game/Meshes/Sectors"
LEVEL_PKG = "/Game/Levels/Chapter01_Tau_Ceti/L_TC1_Arrival_Auto"
RESULT = os.path.join(GEO, "ue5_build_result.json")

result = {"steps": {}, "errors": {}}


def step(name, fn):
    try:
        result["steps"][name] = fn()
        unreal.log(f"[headless] OK: {name}")
    except Exception as e:  # noqa: BLE001
        result["errors"][name] = f"{e!r}\n{traceback.format_exc()}"
        unreal.log_error(f"[headless] FAILED {name}: {e}")


def _world():
    try:
        return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    except Exception:  # noqa: BLE001
        return None


def _version():
    return unreal.SystemLibrary.get_engine_version()


def _import():
    # Wipe the destination first so re-runs are deterministic (otherwise
    # Interchange leaves orphaned/duplicate TEX_* and tex_* assets behind).
    if unreal.EditorAssetLibrary.does_directory_exist(DEST_MESH):
        unreal.EditorAssetLibrary.delete_directory(DEST_MESH)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", OBJ)
    task.set_editor_property("destination_path", DEST_MESH)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return list(task.get_editor_property("imported_object_paths"))


def _parse_mtl():
    """Map material-name -> absolute texture path (or None for no-texture)."""
    mats = {}
    cur = None
    base = os.path.dirname(MTL)
    if not os.path.isfile(MTL):
        return mats
    with open(MTL, encoding="utf-8") as fh:
        for raw in fh:
            line = raw.strip()
            if line.startswith("newmtl "):
                cur = line.split(None, 1)[1]
                mats[cur] = None
            elif line.startswith("map_Kd ") and cur:
                rel = line.split(None, 1)[1]
                mats[cur] = os.path.normpath(os.path.join(base, rel))
    return mats


def _import_texture(abs_png, asset_name):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", abs_png)
    task.set_editor_property("destination_path", DEST_MESH)
    task.set_editor_property("destination_name", asset_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    paths = list(task.get_editor_property("imported_object_paths"))
    tex = unreal.load_asset(paths[0]) if paths else None
    if isinstance(tex, unreal.Texture2D):
        # Crisp retro pixels (Marathon textures are 128px CLUT bitmaps).
        tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
        return tex
    return None


def _make_material(name, tex):
    """Build an UNLIT material (texture on emissive) so it reads fully at any
    light angle — matching Marathon's flat-shaded look."""
    if unreal.EditorAssetLibrary.does_asset_exist(f"{DEST_MESH}/{name}"):
        unreal.EditorAssetLibrary.delete_asset(f"{DEST_MESH}/{name}")
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, DEST_MESH, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mel = unreal.MaterialEditingLibrary
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


def _apply_materials():
    path, mesh = _find_mesh()
    if mesh is None:
        return "skipped (no static mesh)"
    mtl = _parse_mtl()
    built = {}
    for name, abs_png in mtl.items():
        tex = None
        if abs_png and os.path.isfile(abs_png):
            m = re.match(r"tex_(\d+)_(\d+)", name)
            key = f"T_{m.group(1)}_{int(m.group(2)):03d}" if m else f"T_{name}"
            tex = _import_texture(abs_png, key)
        built[name] = _make_material(name, tex)
    static_mats = list(mesh.get_editor_property("static_materials"))
    slot_names = [str(sm.get_editor_property("material_slot_name")) for sm in static_mats]
    assigned = []
    for sm, slot in zip(static_mats, slot_names):
        if slot in built:
            sm.set_editor_property("material_interface", built[slot])
            assigned.append(slot)
    mesh.set_editor_property("static_materials", static_mats)
    unreal.EditorAssetLibrary.save_asset(mesh.get_path_name())
    return {"built": sorted(built), "slots": slot_names, "assigned": assigned,
            "missing": sorted(set(slot_names) - set(built))}


def _find_mesh():
    # A textured OBJ import returns many assets (the StaticMesh + one material
    # and one texture per `usemtl` group). Pick the StaticMesh, not paths[0]
    # (which is now a material like `.../default.default`).
    paths = result["steps"].get("import") or []
    for p in paths:
        asset = unreal.load_asset(p)
        if isinstance(asset, unreal.StaticMesh):
            return p, asset
    return None, None


def _report():
    path, mesh = _find_mesh()
    if mesh is None:
        return None
    info = {"asset": path, "class": type(mesh).__name__}
    try:
        info["num_triangles"] = mesh.get_num_triangles(0)
        info["num_vertices"] = mesh.get_num_vertices(0)
    except Exception as e:  # noqa: BLE001
        info["stats_error"] = repr(e)
    try:
        box = mesh.get_bounding_box()
        info["bounds_min"] = [box.min.x, box.min.y, box.min.z]
        info["bounds_max"] = [box.max.x, box.max.y, box.max.z]
    except Exception as e:  # noqa: BLE001
        info["bounds_error"] = repr(e)
    return info


def _build_level():
    path, mesh = _find_mesh()
    if mesh is None:
        return "skipped (no static mesh)"
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    # Idempotent: `new_level` refuses if the asset exists. On a re-run, load the
    # existing level and wipe its actors for a clean slate (otherwise each run
    # piles duplicate meshes/lights/PlayerStarts on top of the last).
    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL_PKG):
        les.load_level(LEVEL_PKG)
        for a in list(eas.get_all_level_actors()):
            eas.destroy_actor(a)
    else:
        les.new_level(LEVEL_PKG)
    sector = eas.spawn_actor_from_object(mesh, unreal.Vector(0, 0, 0))
    sector.set_actor_label("Sector_Arrival")
    spawned = ["Sector_Arrival"]
    # Basic lighting so the level is visible in PIE and in captures.
    eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1500),
                               unreal.Rotator(-50.0, 0.0, 45.0))
    eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 1500))
    spawned += ["DirectionalLight", "SkyLight"]

    # PlayerStart placement.
    # NOTE: this auto-generated map is a *geometry-preview* map. We put the
    # PlayerStart at a high, straight-down vantage so that when rendered via
    # `-game ?game=GameModeBase` (free-fly DefaultPawn — no gravity, no pitch
    # clamp), the pawn IS the top-down overview camera. The true Marathon spawn
    # is preserved in <level>_placements.json for the eventual playable level.
    box = mesh.get_bounding_box()
    cx = (box.min.x + box.max.x) / 2.0
    cy = (box.min.y + box.max.y) / 2.0
    span = max(box.max.x - box.min.x, box.max.y - box.min.y)
    overview = unreal.Vector(cx, cy, box.max.z + span * 1.2)
    eas.spawn_actor_from_class(unreal.PlayerStart, overview, unreal.Rotator(-90.0, 0.0, 0.0))
    spawned.append("PlayerStart(overview)")

    les.save_current_level()
    return {"level": LEVEL_PKG, "spawned": spawned}


step("version", _version)
result["obj_exists"] = os.path.isfile(OBJ)
step("import", _import)
step("apply_materials", _apply_materials)
step("report", _report)
step("build_level", _build_level)

os.makedirs(GEO, exist_ok=True)
with open(RESULT, "w", encoding="utf-8") as fh:
    json.dump(result, fh, indent=2)
unreal.log(f"[headless] wrote {RESULT}")

# This editor pass only builds + saves the level (import, actors, lights,
# top-down camera). Rendering a frame is done separately via `-game` +
# HighResShot (see Tools/ue5_render_level.py), because a one-shot
# -ExecutePythonScript editor pass never runs a frame loop.
try:
    unreal.SystemLibrary.quit_editor()
except Exception:  # noqa: BLE001
    pass

