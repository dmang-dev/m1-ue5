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

# Terminal look-test knobs:
#   M1_MATMODE = emissive(default) / lit / hybrid  -> material treatment
#     emissive (shipped look): UNLIT, but scen_to_obj bakes each surface's LITE
#     brightness into the material name (tex_C_S_b<pct>) and _make_material scales
#     the emissive by it -> per-surface Marathon lighting (room-by-room mood) with
#     NO dynamic lights, so no hotspots, no black gaps, never brighter than texture.
#     lit/hybrid: DEFAULT_LIT, rely on dynamic lights (legacy; prone to washout).
#   M1_CRISP   = textures imported uncompressed + no mipmaps (crisp pixel art)
#   M1_FIXEXP  = drop a PostProcessVolume that locks auto-exposure (eye adaptation)
# CRISP gets its own texture+material folders so it doesn't share assets.
MATMODE = os.environ.get("M1_MATMODE", "emissive")
CRISP = bool(os.environ.get("M1_CRISP"))
FIXEXP = bool(os.environ.get("M1_FIXEXP"))
_csfx = "_crisp" if CRISP else ""
TEX_LIB = "/Game/Textures" + _csfx        # shared Texture2D assets (T_<coll>_<shape>)
MAT_LIB = "/Game/Materials" + ("" if MATMODE == "emissive" else "_" + MATMODE) + _csfx
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
    """Union of material-name -> absolute texture path across every level .mtl.
    Honors M1_ONLY so a single-level test build only imports that slice's textures."""
    mats = {}
    _only = os.environ.get("M1_ONLY")
    pattern = f"{_only}.mtl" if _only else "*.mtl"
    for mtl in sorted(glob.glob(os.path.join(GEO, pattern))):
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
        if CRISP:
            # keep the M1 pixel art crisp: uncompressed (UI group = no BC blocks,
            # sRGB color) + no mip blur.
            try:
                tex.set_editor_property("compression_settings",
                                        unreal.TextureCompressionSettings.TC_EDITOR_ICON)
                tex.set_editor_property("mip_gen_settings",
                                        unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
                unreal.EditorAssetLibrary.save_asset(tex.get_path_name())
            except Exception:  # noqa: BLE001
                pass
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
    # Treatment per M1_MATMODE. coll 8 = computer/terminal panels (glowing CRTs).
    coll = -1
    m = re.match(r"tex_(\d+)_", name)
    if m:
        coll = int(m.group(1))
    is_screen = (coll == 8)
    use_lit = (MATMODE == "lit") or (MATMODE == "hybrid" and not is_screen)
    # Per-surface Marathon brightness: scen_to_obj bakes the surface's LITE level
    # into the name as _b<pct>. In emissive mode we scale the emissive by it, so each
    # surface reads at its room's authored brightness (no dynamic lights). Screens
    # (coll 8) ignore it -- CRTs always glow full.
    bm = re.search(r"_b(\d+)$", name)
    bucket = (int(bm.group(1)) / 100.0) if bm else 1.0
    if is_screen:
        bucket = 1.0
    mat.set_editor_property("shading_model",
                            unreal.MaterialShadingModel.MSM_DEFAULT_LIT if use_lit
                            else unreal.MaterialShadingModel.MSM_UNLIT)
    if tex is not None:
        node = mel.create_material_expression(
            mat, unreal.MaterialExpressionTextureSample, -380, 0)
        node.set_editor_property("texture", tex)
        if use_lit:
            mel.connect_material_property(node, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
            if MATMODE == "lit":
                # faint emissive so bright pixels (screens) still glow under light
                mul = mel.create_material_expression(
                    mat, unreal.MaterialExpressionMultiply, -140, 150)
                mul.set_editor_property("const_b", 0.5)
                mel.connect_material_expressions(node, "RGB", mul, "A")
                mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        elif bucket < 0.999:
            # emissive scaled by the surface's LITE brightness
            mul = mel.create_material_expression(
                mat, unreal.MaterialExpressionMultiply, -140, 0)
            mul.set_editor_property("const_b", bucket)
            mel.connect_material_expressions(node, "RGB", mul, "A")
            mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        else:
            mel.connect_material_property(node, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        node = mel.create_material_expression(
            mat, unreal.MaterialExpressionConstant3Vector, -380, 0)
        node.set_editor_property("constant", unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
        mel.connect_material_property(
            node, "", unreal.MaterialProperty.MP_BASE_COLOR if use_lit
            else unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(mat.get_path_name())
    return mat


def _build_shared_library():
    """Import every referenced bitmap once and wrap each in a shared material.
    Per-surface brightness means one texture (tex_C_S) backs many material variants
    (tex_C_S_b30, _b70, ...); import each PNG only once and reuse it across variants."""
    mats = _scan_materials()
    lib = {}
    tex_cache = {}  # absolute png -> imported Texture2D
    for name, abs_png in sorted(mats.items()):
        tex = None
        if abs_png and os.path.isfile(abs_png):
            if abs_png in tex_cache:
                tex = tex_cache[abs_png]
            else:
                m = re.match(r"tex_(\d+)_(\d+)", name)
                tname = f"T_{m.group(1)}_{int(m.group(2)):03d}" if m else "T_" + _safe(name)
                tex = _import_texture(abs_png, tname, TEX_LIB)
                tex_cache[abs_png] = tex
        lib[name] = _make_material(name, tex, MAT_LIB)
    result["shared"] = {"materials": len(lib), "textures": len(tex_cache)}
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

    if os.environ.get("M1_NOCOLLAPSE"):
        # Keep every triangle: the mesh build's degenerate-removal drops ~19% of
        # Arrival's faces (sliver tris from the floor/ceiling fan triangulation).
        try:
            sms = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
            bs = sms.get_lod_build_settings(mesh, 0)
            bs.set_editor_property("remove_degenerates", False)
            bs.set_editor_property("use_full_precision_uvs", True)
            sms.set_lod_build_settings(mesh, 0, bs)  # rebuilds the LOD
        except Exception:  # noqa: BLE001
            pass

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
    dl = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1500),
                                    unreal.Rotator(roll=0.0, pitch=-50.0, yaw=45.0))
    sl = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 1500))
    if MATMODE in ("lit", "hybrid"):
        # brighter key + ambient so the lit (darker) textures read cleanly
        try:
            dl.directional_light_component.set_intensity(6.0)
            sl.sky_light_component.set_intensity(3.0)
        except Exception:  # noqa: BLE001
            pass
    if FIXEXP:
        # Lock auto-exposure (eye adaptation) so the terminal renders at a constant
        # brightness regardless of what else is in view (min==max == no adaptation).
        try:
            ppv = eas.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 200))
            ppv.set_editor_property("unbound", True)
            s = ppv.get_editor_property("settings")
            s.set_editor_property("override_auto_exposure_min_brightness", True)
            s.set_editor_property("auto_exposure_min_brightness", 1.0)
            s.set_editor_property("override_auto_exposure_max_brightness", True)
            s.set_editor_property("auto_exposure_max_brightness", 1.0)
            ppv.set_editor_property("settings", s)
        except Exception:  # noqa: BLE001
            pass

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

# Route OBJ import through the LEGACY UStaticMeshFactory path, not Interchange.
# UE5's Interchange OBJ translator silently culls ~17% of Arrival's triangles
# at the translate stage ("nearly zero normals" weld/degenerate removal that no
# import or build flag disables), which punches holes around the terminal
# niches and in other levels' terminals. The legacy importer keeps every real
# face (2738 -> 2678; only the ~60 genuinely zero-area tris are dropped, which
# is correct). Verified: Interchange=2208 tris, legacy=2678 tris on ARR_Fix.
unreal.SystemLibrary.execute_console_command(
    None, "Interchange.FeatureFlags.Import.OBJ 0")

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
elif not os.environ.get("M1_INCLUDE_TEST"):
    # Default: ship only the numbered campaign levels (NN_*). The L_TEST_* look
    # rooms and L_ARR_* full-Arrival look variants stay out of the build/cook;
    # their OBJs remain in Reference/Geometry for ad-hoc rebuilds via M1_ONLY=<stem>.
    objs = [o for o in objs
            if re.match(r"\d\d_", os.path.splitext(os.path.basename(o))[0])]
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
