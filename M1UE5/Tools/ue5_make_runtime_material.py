"""Create the runtime-loader master material (installer Phase 0).

    UnrealEditor-Cmd.exe <project> -ExecutePythonScript=ue5_make_runtime_material.py \\
        -RenderOffScreen -unattended -nosplash -nopause -stdout

The runtime OBJ loader (AM1RuntimeLevel) can't author material graphs at runtime,
so it instances ONE committed master material per surface and sets its params. This
mirrors the cooked per-surface look (Tools/ue5_headless_build.py _make_material):

    /Game/Runtime/M_M1Runtime
      UNLIT, OPAQUE, two-sided (safety vs winding)
      TextureSampleParameter2D "BaseTex"  RGB --(x Emissive)--> Emissive Color
      ScalarParameter        "Emissive"  (per-surface LITE brightness bucket; 1.0 = full)

Ships via DirectoriesToAlwaysCook=/Game/Runtime. Results -> Reference/runtime_material_result.json.
"""
import json
import os

import unreal

MAT_DEST = "/Game/Runtime"
MAT_NAME = "M_M1Runtime"
OUT = r"C:\m1-ue5\M1UE5\Reference\runtime_material_result.json"

mat_path = f"{MAT_DEST}/{MAT_NAME}"
if unreal.EditorAssetLibrary.does_asset_exist(mat_path):
    unreal.EditorAssetLibrary.delete_asset(mat_path)

mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    MAT_NAME, MAT_DEST, unreal.Material, unreal.MaterialFactoryNew())
mel = unreal.MaterialEditingLibrary
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
mat.set_editor_property("two_sided", True)

tex = mel.create_material_expression(
    mat, unreal.MaterialExpressionTextureSampleParameter2D, -520, 0)
tex.set_editor_property("parameter_name", "BaseTex")

emis = mel.create_material_expression(
    mat, unreal.MaterialExpressionScalarParameter, -520, 260)
emis.set_editor_property("parameter_name", "Emissive")
emis.set_editor_property("default_value", 1.0)

mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -240, 60)
mel.connect_material_expressions(tex, "RGB", mul, "A")
mel.connect_material_expressions(emis, "", mul, "B")
mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

mel.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(mat.get_path_name())

os.makedirs(os.path.dirname(OUT), exist_ok=True)
with open(OUT, "w", encoding="utf-8") as fh:
    json.dump({"material": mat.get_path_name()}, fh, indent=2)
unreal.log(f"[runtime-mat] created {mat.get_path_name()}")
unreal.SystemLibrary.quit_editor()
