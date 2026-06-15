"""Create the blank persistent level for the runtime loader (installer Phase 1).

    UnrealEditor-Cmd.exe <project> -ExecutePythonScript=ue5_make_runtime_map.py \\
        -RenderOffScreen -unattended -nosplash -nopause -stdout

L_Runtime is an empty map: the runtime loader (AM1GameMode::BuildRuntimeGeometry ->
AM1RuntimeLevel) builds the current level's geometry into it at runtime, so there is
no baked .uasset/.umap geometry. It uses the GLOBAL default GameMode (M1GameMode,
DefaultEngine.ini) -- no per-map override. A PlayerStart sits at the origin so the
pawn has somewhere to spawn before AM1Character teleports it onto the real
placements player_start on the first Tick.

Saved to /Game/Levels/M1/L_Runtime. Add it to the cook -map list.
"""
import unreal

MAP_PATH = "/Game/Levels/M1/L_Runtime"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# Fresh empty level at the target path (overwrites any existing).
les.new_level(MAP_PATH)

# A PlayerStart at the origin (the runtime placement teleports the pawn to the real
# start on Tick 1; this just gives ChoosePlayerStart something to return).
ps = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0.0, 0.0, 100.0),
                                unreal.Rotator(0.0, 0.0, 0.0))
if ps:
    ps.set_actor_label("PlayerStart_Runtime")

les.save_current_level()
unreal.log(f"[runtime-map] created {MAP_PATH}")
unreal.SystemLibrary.quit_editor()
