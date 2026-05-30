"""Headless UE5 build/validate — drives the editor with no GUI:

    UnrealEditor-Cmd.exe <project> -run=pythonscript -script=<this file> ...

Every step is wrapped so a single (slow) editor launch reports exactly which
capabilities work. Results -> Reference/Geometry/ue5_build_result.json, which
the calling agent reads back to decide next steps. Uses the `unreal` module,
so it only runs inside UnrealEditor-Cmd, not plain Python.
"""
import json
import os
import traceback

import unreal

REPO = r"C:\m1-ue5\M1UE5"
GEO = os.path.join(REPO, "Reference", "Geometry")
OBJ = os.path.join(GEO, "00_Arrival.obj")
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
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", OBJ)
    task.set_editor_property("destination_path", DEST_MESH)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return list(task.get_editor_property("imported_object_paths"))


def _report():
    paths = result["steps"].get("import") or []
    if not paths:
        return None
    mesh = unreal.load_asset(paths[0])
    info = {"asset": paths[0], "class": type(mesh).__name__}
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
    paths = result["steps"].get("import") or []
    if not paths:
        return "skipped (no import)"
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    les.new_level(LEVEL_PKG)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    mesh = unreal.load_asset(paths[0])
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

