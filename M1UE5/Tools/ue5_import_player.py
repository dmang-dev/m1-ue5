"""Headless UE5 import of the Marathon player figure (Collection 6) + tint material.

    UnrealEditor-Cmd.exe <project> -ExecutePythonScript=ue5_import_player.py \\
        -RenderOffScreen -unattended -nosplash -nopause -stdout

The networked player body is the "stationary" high-shape of Coll_06 (the human
figure). It has 8 effective views built from 5 unique bitmaps + horizontal mirror
(views 5..7 mirror 3,2,1) -- the same scheme AM1Enemy already renders. We import
the 5 unique view bitmaps as:

    /Game/Sprites/Player/T_player_v0..v4   (v0 = front, v2 = side, v4 = back)

and build a player sprite material with a per-player tint:

    /Game/Sprites/M_M1Player
      UNLIT, BLEND_Masked, two-sided
      TextureSampleParameter2D "Sprite"  RGB --(x Tint)--> Emissive, A --> Opacity Mask
      VectorParameter        "Tint" (default white)

AM1Character loads the textures by path and drives view + tint at runtime.
Results -> Reference/Sprites/player_import_result.json.
"""
import json
import os

import unreal

# Derive the project root from this script's location (no hardcoded drive path).
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
COLL = os.path.join(ROOT, "Reference", "Shapes", "Coll_06")
SHAPES_JSON = os.path.join(COLL, "shapes.json")
BITMAPS = os.path.join(COLL, "bitmaps")
SPRITES = os.path.join(ROOT, "Reference", "Sprites")
OUT = os.path.join(SPRITES, "player_import_result.json")
DEST = "/Game/Sprites/Player"
MAT_DEST = "/Game/Sprites"
MAT_NAME = "M_M1Player"
STANDING_SHAPE = 4  # high_shapes index of "stationary"

result = {"textures": 0, "missing": [], "material": None}

# --- resolve the 5 unique view bitmaps of the standing figure ---------------
with open(SHAPES_JSON, encoding="utf-8") as fh:
    shapes = json.load(fh)
lo = shapes["low_shapes"]
stand = shapes["high_shapes"][STANDING_SHAPE]
# First NumViews entries (8 effective, 5 unique) -> view-major d=0..4 are the
# unique ones; d>=5 mirror earlier views, so we only import d=0..4.
view_low = stand["low_shape_indices"][:5]
bitmap_ids = [lo[li]["bitmap_index"] for li in view_low]
unreal.log(f"[player] standing views (d=0..4) -> bitmaps {bitmap_ids}")

if unreal.EditorAssetLibrary.does_directory_exist(DEST):
    unreal.EditorAssetLibrary.delete_directory(DEST)

tasks = []
for v, bi in enumerate(bitmap_ids):
    png = os.path.join(BITMAPS, f"{bi:03d}.png")
    if not os.path.isfile(png):
        result["missing"].append(png)
        continue
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", png)
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("destination_name", f"T_player_v{v}")
    t.set_editor_property("automated", True)
    t.set_editor_property("save", True)
    t.set_editor_property("replace_existing", True)
    tasks.append(t)

if tasks:
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

first_tex = None
for t in tasks:
    for p in list(t.get_editor_property("imported_object_paths")):
        tex = unreal.load_asset(p)
        if isinstance(tex, unreal.Texture2D):
            tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
            tex.set_editor_property("compression_settings",
                                    unreal.TextureCompressionSettings.TC_EDITOR_ICON)
            tex.set_editor_property("mip_gen_settings",
                                    unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            unreal.EditorAssetLibrary.save_asset(tex.get_path_name())
            result["textures"] += 1
            if first_tex is None:
                first_tex = tex

unreal.log(f"[player] imported {result['textures']} textures "
           f"({len(result['missing'])} missing)")

# --- tintable player sprite material ----------------------------------------
mat_path = f"{MAT_DEST}/{MAT_NAME}"
if unreal.EditorAssetLibrary.does_asset_exist(mat_path):
    unreal.EditorAssetLibrary.delete_asset(mat_path)

mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    MAT_NAME, MAT_DEST, unreal.Material, unreal.MaterialFactoryNew())
mel = unreal.MaterialEditingLibrary
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
mat.set_editor_property("two_sided", True)

sprite = mel.create_material_expression(
    mat, unreal.MaterialExpressionTextureSampleParameter2D, -520, 0)
sprite.set_editor_property("parameter_name", "Sprite")
if first_tex:
    sprite.set_editor_property("texture", first_tex)

tint = mel.create_material_expression(
    mat, unreal.MaterialExpressionVectorParameter, -520, 260)
tint.set_editor_property("parameter_name", "Tint")
tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))

mul = mel.create_material_expression(
    mat, unreal.MaterialExpressionMultiply, -240, 60)
mel.connect_material_expressions(sprite, "RGB", mul, "A")
mel.connect_material_expressions(tint, "RGB", mul, "B")

mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(sprite, "A", unreal.MaterialProperty.MP_OPACITY_MASK)

mel.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(mat.get_path_name())
result["material"] = mat.get_path_name()
unreal.log(f"[player] material {result['material']}")

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(result, fh, indent=2)
unreal.log("[player] done")
unreal.SystemLibrary.quit_editor()
