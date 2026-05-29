"""Import a converted Marathon level into UE5 — RUN THIS INSIDE THE EDITOR.

Unlike the other Tools/ scripts, this uses the `unreal` module, which only
exists inside Unreal's embedded Python. Run it from the editor:

    Window → Developer Tools → Output Log → (Cmd dropdown: Python)
    or:  Tools → Execute Python Script…  → pick this file
    or in the Python console:
        exec(open(r"<repo>/M1UE5/Tools/ue5_import_level.py").read())

It will:
  1. Import Reference/Geometry/<LEVEL>.obj as a Static Mesh into
     /Game/Meshes/Sectors, with collision built from the mesh (complex-as-simple
     — fine for walking a greybox).
  2. Drop the mesh into the current level at the origin.
  3. Read <LEVEL>_placements.json and spawn a PlayerStart at the Marathon player
     start (raised half a capsule off the floor) with the right yaw.

Tested patterns target UE 5.x. OBJ import options vary a little by version /
whether Interchange is enabled; if import fails, see docs/scen_to_ue5_import.md
for the manual steps.

Edit LEVEL below (or set the MARATHON_LEVEL env var) to import a different map.
"""
import json
import os

import unreal  # only available inside the UE5 editor

# --- config -----------------------------------------------------------------
LEVEL = os.environ.get("MARATHON_LEVEL", "00_Arrival")
# Reference/Geometry sits two levels up from Tools/ (…/M1UE5/Reference/Geometry).
_THIS = os.path.dirname(os.path.abspath(__file__))
GEO_DIR = os.path.normpath(os.path.join(_THIS, "..", "Reference", "Geometry"))
DEST_PATH = "/Game/Meshes/Sectors"
CAPSULE_HALF_HEIGHT_CM = 88.0  # default UE character half-height


def _import_obj(obj_path: str, dest: str) -> str:
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", obj_path)
    task.set_editor_property("destination_path", dest)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", True)

    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    smid = options.static_mesh_import_data
    smid.set_editor_property("combine_meshes", True)
    smid.set_editor_property("generate_lightmap_u_vs", True)
    smid.set_editor_property("auto_generate_collision", True)
    task.set_editor_property("options", options)

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    paths = task.get_editor_property("imported_object_paths")
    return paths[0] if paths else ""


def main() -> None:
    obj_path = os.path.join(GEO_DIR, LEVEL + ".obj")
    placements_path = os.path.join(GEO_DIR, LEVEL + "_placements.json")
    if not os.path.isfile(obj_path):
        unreal.log_error(f"OBJ not found: {obj_path} (run Tools/scen_to_obj.py first)")
        return

    asset_path = _import_obj(obj_path, DEST_PATH)
    if not asset_path:
        unreal.log_error("Import failed — see docs/scen_to_ue5_import.md for manual steps")
        return
    unreal.log(f"Imported static mesh: {asset_path}")

    mesh = unreal.load_asset(asset_path)
    # Greybox collision: treat the imported triangles as collision.
    try:
        mesh.get_editor_property("body_setup").set_editor_property(
            "collision_trace_flag",
            unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE,
        )
    except Exception as e:  # noqa: BLE001 — non-fatal; auto collision already built
        unreal.log_warning(f"Could not set complex-as-simple collision: {e}")

    eas = unreal.EditorActorSubsystem()
    actor = eas.spawn_actor_from_object(mesh, unreal.Vector(0, 0, 0))
    actor.set_actor_label(f"Sector_{LEVEL}")

    # Player start from placements.json
    if os.path.isfile(placements_path):
        with open(placements_path, encoding="utf-8") as fh:
            placements = json.load(fh)
        ps = placements.get("player_start")
        if ps:
            loc = unreal.Vector(ps["x"], ps["y"], ps["z"] + CAPSULE_HALF_HEIGHT_CM)
            rot = unreal.Rotator(0.0, 0.0, ps.get("facing_deg", 0.0))
            start = eas.spawn_actor_from_class(unreal.PlayerStart, loc, rot)
            start.set_actor_label(f"PlayerStart_{LEVEL}")
            unreal.log(f"PlayerStart at {loc}")
        else:
            unreal.log_warning("No player_start in placements.json")

    unreal.log(f"Done importing {LEVEL}. Press Play to walk it.")


if __name__ == "__main__":
    main()
else:
    main()
