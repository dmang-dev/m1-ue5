"""Editor stage: import the scorch texture and build a deferred-decal material.

Reference/Sprites/T_scorch_decal.png -> /Game/Sprites/VFX/T_scorch_decal, then a
deferred-decal material /Game/Materials/Decals/M_ScorchDecal (texture RGB -> base
color, A -> opacity). AM1Impact / weapon hits spawn this decal at impact points
so shots leave a persistent scorch (the emissive spark is the transient pop).

    UnrealEditor-Cmd.exe <project> -ExecutePythonScript=<this file> \\
        -RenderOffScreen -unattended -nosplash -nopause -stdout
"""
import os

import unreal

PNG = r"C:\m1-ue5\M1UE5\Reference\Sprites\T_scorch_decal.png"
TEX_DIR = "/Game/Sprites/VFX"
TEX_NAME = "T_scorch_decal"
MAT_DIR = "/Game/Materials/Decals"
MAT_NAME = "M_ScorchDecal"

tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary

# 1. Import the texture.
if not os.path.isfile(PNG):
    unreal.log_error(f"[decal] missing {PNG}")
else:
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", PNG)
    t.set_editor_property("destination_path", TEX_DIR)
    t.set_editor_property("destination_name", TEX_NAME)
    t.set_editor_property("automated", True)
    t.set_editor_property("save", True)
    t.set_editor_property("replace_existing", True)
    tools.import_asset_tasks([t])

tex = unreal.load_asset(f"{TEX_DIR}/{TEX_NAME}.{TEX_NAME}")

# 2. Build (or rebuild) the deferred-decal material.
mat_path = f"{MAT_DIR}/{MAT_NAME}"
if unreal.EditorAssetLibrary.does_asset_exist(mat_path):
    unreal.EditorAssetLibrary.delete_asset(mat_path)
mat = tools.create_asset(MAT_NAME, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
# Decal-domain materials use decal_blend_mode (NOT the regular blend_mode); the
# latter leaves the decal uncompilable -> default material in game.
mat.set_editor_property("decal_blend_mode", unreal.DecalBlendMode.DBM_TRANSLUCENT)
if tex:
    ts = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -350, 0)
    ts.set_editor_property("texture", tex)
    mel.connect_material_property(ts, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(ts, "A", unreal.MaterialProperty.MP_OPACITY)
mel.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(mat.get_path_name())
unreal.log(f"[decal] built {mat.get_path_name()} (tex={'ok' if tex else 'MISSING'})")

unreal.SystemLibrary.quit_editor()
