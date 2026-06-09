"""Headless: create the empty L_MainMenu level whose GameMode (AMenuGameMode)
shows the main menu. Run AFTER the C++ module is compiled (needs unreal.MenuGameMode).

    UnrealEditor-Cmd <proj> -ExecutePythonScript=<this> -RenderOffScreen \\
        -unattended -nosplash -nopause -stdout
"""
import unreal

LEVEL = "/Game/Levels/M1/L_MainMenu"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

if unreal.EditorAssetLibrary.does_asset_exist(LEVEL):
    les.load_level(LEVEL)
    for a in list(eas.get_all_level_actors()):
        eas.destroy_actor(a)
else:
    les.new_level(LEVEL)

# Point the level's GameMode override at AMenuGameMode (floating spectator pawn +
# the M1 player controller, which shows the main menu on BeginPlay).
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
ws = world.get_world_settings()
ws.set_editor_property("default_game_mode", unreal.MenuGameMode)

les.save_current_level()
unreal.log("[make_menu] L_MainMenu saved with MenuGameMode override")
try:
    unreal.SystemLibrary.quit_editor()
except Exception:  # noqa: BLE001
    pass
