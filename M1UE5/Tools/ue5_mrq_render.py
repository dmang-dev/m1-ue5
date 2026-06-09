"""Render the auto-built level top-down via Movie Render Queue (MRQ).

MRQ is the production-correct headless renderer: it renders through an exact
camera transform (via a Level Sequence + Camera Cut) and does proper lighting
warm-up, so the result is correctly framed AND lit — unlike a one-shot
SceneCapture/-game HighResShot.

Run it so the editor keeps ticking (MRQ renders async over PIE frames):

    UnrealEditor-Cmd.exe <proj> -ExecCmds="py C:\\m1-ue5\\M1UE5\\Tools\\ue5_mrq_render.py" \\
        -RenderOffScreen -nosplash -nopause -stdout

It builds a 1-frame top-down sequence, renders a PNG to Reference/Geometry/mrq/,
writes mrq_result.json, and quits when done.
"""
import json
import math
import os
import time
import traceback

import unreal

# Which level to render. Env M1_LEVEL is the UE-safe level name (e.g.
# "00_Arrival", "05_The_Rose") under /Game/Levels/M1/L_<name>. Defaults to the
# first level. The batch build labels every level's mesh actor "Sector".
LEVEL = os.environ.get("M1_LEVEL", "00_Arrival")
# View mode: "topdown" (default — angled bird's-eye overview) or "fp" (first-
# person eye-level verification render from the real PlayerStart, with a
# straight-down collision probe / drop-test).
VIEW = os.environ.get("M1_VIEW", "topdown").lower()
LEVEL_PKG = f"/Game/Levels/M1/L_{LEVEL}"
SEQ_PKG = f"/Game/Cinematics/Seq_{LEVEL}"
OUT_DIR = r"C:\m1-ue5\M1UE5\Reference\Geometry\mrq"
RESULT = (r"C:\m1-ue5\M1UE5\Reference\Geometry\fp_verify_result.json" if VIEW == "fp"
          else r"C:\m1-ue5\M1UE5\Reference\Geometry\mrq_result.json")
SECTOR_LABEL = "Sector"
EYE_OFFSET = 14.0    # FP eye above the PlayerStart spawn (floor+36) -> ~floor+50, matches the pawn
OUT_NAME = f"{LEVEL}_{'fp' if VIEW == 'fp' else 'topdown'}"

result = {"steps": {}, "errors": {}}


def _write():
    os.makedirs(os.path.dirname(RESULT), exist_ok=True)
    with open(RESULT, "w", encoding="utf-8") as fh:
        json.dump(result, fh, indent=2)


def step(name, fn):
    unreal.log(f"[mrq] START: {name}")
    _write()  # checkpoint before each step so a hang is diagnosable
    try:
        result["steps"][name] = fn()
        unreal.log(f"[mrq] OK: {name}")
    except Exception as e:  # noqa: BLE001
        result["errors"][name] = f"{e!r}\n{traceback.format_exc()}"
        unreal.log_error(f"[mrq] FAILED {name}: {e}")
        result["steps"][name] = None
    _write()
    return result["steps"][name]


def _load_level():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    les.load_level(LEVEL_PKG)
    return LEVEL_PKG


_cam_holder = {}


def _place_camera():
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    # Remove any camera left over from a previous render so they don't pile up.
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == "MRQ_TopDown":
            eas.destroy_actor(a)
    sector = next((a for a in eas.get_all_level_actors()
                   if a.get_actor_label() == SECTOR_LABEL), None)
    if sector is None:
        raise RuntimeError(f"{SECTOR_LABEL} actor not found in level")
    origin, extent = sector.get_actor_bounds(False)
    half_span = max(extent.x, extent.y)
    # Angled bird's-eye (not straight-down): a ~50-degree overhead view shows
    # floors AND wall faces. (Straight down hides the vertical walls and blows
    # out to white.) Aim at the level center.
    center = unreal.Vector(origin.x, origin.y, origin.z)
    cam_loc = unreal.Vector(origin.x, origin.y - half_span * 1.3,
                            origin.z + half_span * 1.5)
    cam_rot = unreal.MathLibrary.find_look_at_rotation(cam_loc, center)
    cam = eas.spawn_actor_from_class(unreal.CameraActor, cam_loc, cam_rot)
    cam.set_actor_label("MRQ_TopDown")
    cam.camera_component.set_editor_property("field_of_view", 75.0)
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    _cam_holder["cam"] = cam  # actor kept out of `result` (not JSON-serializable)
    return {"label": "MRQ_TopDown", "loc": [cam_loc.x, cam_loc.y, cam_loc.z],
            "rot": [cam_rot.pitch, cam_rot.yaw, cam_rot.roll]}


def _collision_check(eas):
    """Headless 'drop test' (config assertion). Editor-world line traces don't
    function in headless -ExecCmds (no physics query scene), so instead of
    raycasting we assert the Sector's collision is set up so a player capsule WILL
    stand on it: component QueryAndPhysics + the mesh's complex-as-simple flag.
    A true runtime physics settle-test belongs with the PIE/pawn step."""
    sector = next((a for a in eas.get_all_level_actors()
                   if a.get_actor_label() == SECTOR_LABEL), None)
    out = {"method": "collision-config-assert", "walkable": False}
    if sector is None:
        out["error"] = f"{SECTOR_LABEL} actor not found"
        return out
    comp = sector.get_component_by_class(unreal.StaticMeshComponent)
    if comp is None:
        out["error"] = "Sector has no StaticMeshComponent"
        return out
    ce = comp.get_collision_enabled()
    mesh = comp.get_editor_property("static_mesh")
    bs = mesh.get_editor_property("body_setup") if mesh else None
    tf = bs.get_editor_property("collision_trace_flag") if bs else None
    out["collision_enabled"] = str(ce)
    out["profile"] = str(comp.get_collision_profile_name())
    out["mesh_trace_flag"] = str(tf) if tf is not None else None
    query_ok = ce in (unreal.CollisionEnabled.QUERY_AND_PHYSICS,
                      unreal.CollisionEnabled.QUERY_ONLY)
    complex_ok = tf == unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
    out["walkable"] = bool(query_ok and complex_ok)
    return out


def _place_camera_fp():
    """First-person eye-level camera at the real PlayerStart, looking along the
    Marathon facing. Asserts walkable collision before rendering."""
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == "MRQ_FP":
            eas.destroy_actor(a)
    ps = next((a for a in eas.get_all_level_actors()
               if a.get_actor_label() == "PlayerStart_M1"), None)
    if ps is None:
        raise RuntimeError(
            "PlayerStart_M1 not found — rebuild this level with the walkable "
            "ue5_headless_build.py (real spawn + collision)")
    ps_loc = ps.get_actor_location()
    ps_rot = ps.get_actor_rotation()
    check = _collision_check(eas)
    result["collision_check"] = check
    # Optional dolly: M1_FWD cm forward along the gaze (to close in on a wall for
    # texture inspection); M1_EYEZ overrides the eye height (absolute world cm).
    fwd = float(os.environ.get("M1_FWD", "0") or 0)
    yaw_rad = math.radians(ps_rot.yaw)
    ex = ps_loc.x + math.cos(yaw_rad) * fwd
    ey = ps_loc.y + math.sin(yaw_rad) * fwd
    eyez = os.environ.get("M1_EYEZ")
    ez = float(eyez) if eyez else ps_loc.z + EYE_OFFSET
    eye = unreal.Vector(ex, ey, ez)
    cam_rot = unreal.Rotator(roll=0.0, pitch=0.0, yaw=ps_rot.yaw)  # level gaze along facing
    cam = eas.spawn_actor_from_class(unreal.CameraActor, eye, cam_rot)
    cam.set_actor_label("MRQ_FP")
    cam.camera_component.set_editor_property("field_of_view", 90.0)
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    _cam_holder["cam"] = cam
    return {"label": "MRQ_FP", "eye": [eye.x, eye.y, eye.z], "yaw": ps_rot.yaw,
            "collision_check": check}


def _place_camera_free():
    """Free camera from absolute world coords: M1_CAMPOS / M1_LOOKAT = 'x,y,z' (UE cm).
    Used to frame a specific wall head-on for faithfulness comparisons."""
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in eas.get_all_level_actors():
        if a.get_actor_label() in ("MRQ_FP", "MRQ_TopDown"):
            eas.destroy_actor(a)
    cp = [float(v) for v in os.environ["M1_CAMPOS"].split(",")]
    lk = [float(v) for v in os.environ["M1_LOOKAT"].split(",")]
    pos = unreal.Vector(cp[0], cp[1], cp[2])
    tgt = unreal.Vector(lk[0], lk[1], lk[2])
    rot = unreal.MathLibrary.find_look_at_rotation(pos, tgt)
    cam = eas.spawn_actor_from_class(unreal.CameraActor, pos, rot)
    cam.set_actor_label("MRQ_FP")
    cam.camera_component.set_editor_property("field_of_view",
                                             float(os.environ.get("M1_FOV", "90")))
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    _cam_holder["cam"] = cam
    return {"label": "MRQ_FP", "pos": cp, "lookat": lk}


def _binding_id(seq, binding):
    """Build a MovieSceneObjectBindingID for a camera-cut, across UE5 API spellings."""
    # SequencerBindingProxy exposes its GUID as `binding_id` in UE5.
    guid = binding.get_editor_property("binding_id")
    try:
        return unreal.MovieSceneObjectBindingID(guid=guid)
    except Exception:  # noqa: BLE001
        bid = unreal.MovieSceneObjectBindingID()
        bid.set_editor_property("guid", guid)
        return bid


def _make_sequence():
    cam = _cam_holder["cam"]
    # Re-creating over an existing asset can stall; delete the prior one first.
    if unreal.EditorAssetLibrary.does_asset_exist(SEQ_PKG):
        unreal.EditorAssetLibrary.delete_asset(SEQ_PKG)
    seq = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        f"Seq_{LEVEL}", "/Game/Cinematics", unreal.LevelSequence,
        unreal.LevelSequenceFactoryNew())
    seq.set_display_rate(unreal.FrameRate(24, 1))
    seq.set_playback_start(0)
    seq.set_playback_end(1)
    binding = seq.add_possessable(cam)
    cut_track = seq.add_track(unreal.MovieSceneCameraCutTrack)
    cut_section = cut_track.add_section()
    cut_section.set_range(0, 1)
    cut_section.set_camera_binding_id(_binding_id(seq, binding))
    unreal.EditorAssetLibrary.save_asset(SEQ_PKG)
    return SEQ_PKG


def _quit():
    h = _cam_holder.get("tick")
    if h is not None:
        try:
            unreal.unregister_slate_post_tick_callback(h)
        except Exception:  # noqa: BLE001
            pass
        _cam_holder["tick"] = None
    try:
        unreal.SystemLibrary.quit_editor()
    except Exception:  # noqa: BLE001
        pass


def _arm_watchdog():
    """Force the editor to quit once the async render reports done, or after a
    hard timeout — a belt-and-suspenders backup to the finished delegate."""
    _cam_holder["start"] = time.time()

    def _tick(_delta):
        if result.get("mrq_success") is not None:
            unreal.log("[mrq] watchdog: render done, quitting")
            _quit()
            return
        if time.time() - _cam_holder["start"] > 240.0:
            png = os.path.join(OUT_DIR, OUT_NAME + ".png")
            result["mrq_timeout"] = True
            result["mrq_png_exists"] = os.path.isfile(png)
            _write()
            unreal.log_error("[mrq] watchdog: timeout, force quitting")
            _quit()

    _cam_holder["tick"] = unreal.register_slate_post_tick_callback(_tick)


def _render():
    sub = unreal.get_editor_subsystem(unreal.MoviePipelineQueueSubsystem)
    queue = sub.get_queue()
    for j in list(queue.get_jobs()):
        queue.delete_job(j)
    job = queue.allocate_new_job(unreal.MoviePipelineExecutorJob)
    job.set_editor_property("map", unreal.SoftObjectPath(LEVEL_PKG))
    job.set_editor_property("sequence", unreal.SoftObjectPath(SEQ_PKG))
    config = job.get_configuration()
    config.find_or_add_setting_by_class(unreal.MoviePipelineDeferredPassBase)
    config.find_or_add_setting_by_class(unreal.MoviePipelineImageSequenceOutput_PNG)
    out = config.find_or_add_setting_by_class(unreal.MoviePipelineOutputSetting)
    out.set_editor_property("output_directory", unreal.DirectoryPath(OUT_DIR))
    out.set_editor_property("output_resolution", unreal.IntPoint(1280, 720))
    out.set_editor_property("file_name_format", OUT_NAME)

    executor = unreal.MoviePipelinePIEExecutor()

    def _on_finished(_executor, success):
        result["mrq_success"] = bool(success)
        result["steps"]["render"] = "finished"
        _write()
        unreal.log(f"[mrq] render finished success={success}")

    executor.on_executor_finished_delegate.add_callable(_on_finished)
    sub.render_queue_with_executor_instance(executor)
    # CRITICAL: keep a live reference to the executor. If it's only a local, the
    # Python wrapper (and its bound finished-delegate) can be GC'd before the
    # async render completes, so the callback never fires and the editor hangs
    # open forever (observed: 20 min idle after the PNG was already written).
    _cam_holder["executor"] = executor
    _arm_watchdog()
    return "render armed (async)"


step("load_level", _load_level)
unreal.log(f"[mrq] view mode: {VIEW}")
_cam_fn = {"fp": _place_camera_fp, "free": _place_camera_free}.get(VIEW, _place_camera)
step("place_camera", _cam_fn)
if _cam_holder.get("cam") is not None:
    seq = step("make_sequence", _make_sequence)
    if seq is not None:
        step("render", _render)
_write()  # checkpoint before async render completes
unreal.log("[mrq] setup complete; awaiting async render")
