"""Headless: create the playable GameMode + pawn so the project can be packaged
into a double-clickable build.

    UnrealEditor-Cmd <proj> -ExecutePythonScript=<this> -RenderOffScreen \\
        -unattended -nosplash -nopause -stdout

Creates (idempotent — reuses existing assets, never clobbers user edits):
  * /Game/Blueprints/Player/BP_PlayerPawn        — a DefaultPawn subclass. The
    engine DefaultPawn ships built-in WASD + mouse-look + E/Q up-down FLYING
    controls (no Enhanced-Input assets or event-graph wiring needed), so the
    first playable build needs zero manual setup. Swap to a walking ACharacter
    later for ground movement against the sector collision.
  * /Game/Blueprints/GameFramework/BP_GameMode   — a GameModeBase subclass whose
    DefaultPawnClass points at BP_PlayerPawn.

The packaged game spawns this pawn at the level's PlayerStart_M1. Set the project
default GameMode + startup map in Config/DefaultEngine.ini (done separately).
Writes Reference/Geometry/setup_player_result.json.
"""
import json
import os

import unreal

OUT = r"C:\m1-ue5\M1UE5\Reference\Geometry\setup_player_result.json"
tools = unreal.AssetToolsHelpers.get_asset_tools()
result = {}


def _get_or_make_bp(name, path, parent):
    pkg = f"{path}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(pkg):
        return unreal.load_asset(pkg), False
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    bp = tools.create_asset(name, path, None, factory)
    unreal.EditorAssetLibrary.save_asset(bp.get_path_name())
    return bp, True


pawn_bp, pawn_new = _get_or_make_bp("BP_PlayerPawn", "/Game/Blueprints/Player",
                                    unreal.DefaultPawn)
gm_bp, gm_new = _get_or_make_bp("BP_GameMode", "/Game/Blueprints/GameFramework",
                                unreal.GameModeBase)
result["pawn"] = {"asset": pawn_bp.get_path_name(), "created": pawn_new}
result["gamemode"] = {"asset": gm_bp.get_path_name(), "created": gm_new}

# Point the GameMode's DefaultPawnClass at BP_PlayerPawn (best effort; if it
# fails the GameMode still inherits ADefaultPawn, so the build stays playable).
result["default_pawn_set"] = False
try:
    gm_cls = gm_bp.generated_class()
    pawn_cls = pawn_bp.generated_class()
    cdo = gm_cls.get_default_object()
    cdo.set_editor_property("default_pawn_class", pawn_cls)
    unreal.BlueprintEditorLibrary.compile_blueprint(gm_bp)
    unreal.EditorAssetLibrary.save_asset(gm_bp.get_path_name())
    after = gm_cls.get_default_object().get_editor_property("default_pawn_class")
    result["default_pawn_set"] = bool(after) and "BP_PlayerPawn" in str(after)
    result["default_pawn_now"] = str(after)
except Exception as e:  # noqa: BLE001
    result["default_pawn_error"] = repr(e)

# The generated-class path packaging/config needs for GlobalDefaultGameMode.
result["gamemode_class_path"] = gm_bp.generated_class().get_path_name()

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(result, fh, indent=2)
unreal.log("[setup_player] " + json.dumps(result))
try:
    unreal.SystemLibrary.quit_editor()
except Exception:  # noqa: BLE001
    pass
