"""Headless UE5 batch-import of the Marathon sprite art.

    UnrealEditor-Cmd.exe <project> -ExecutePythonScript=<this file> \\
        -RenderOffScreen -unattended -nosplash -nopause -stdout

Imports two generated lists with pixel-art settings (TF_NEAREST +
TC_EDITOR_ICON uncompressed + no mips):

    Reference/Sprites/import_list.json         -> /Game/Sprites/Monsters
        (gen_monster_assets.py: T_mon<type>_<state> enemy billboards)
    Reference/Sprites/weapon_import_list.json  -> /Game/Sprites/Weapons
        (gen_weapon_sprites.py: T_wpn<i>_<state><frame> viewmodel frames)

then builds the single master sprite material /Game/Sprites/M_M1Sprite:

    UNLIT, BLEND_Masked, two-sided
    TextureSampleParameter2D "Sprite"  RGB -> Emissive, A -> Opacity Mask

AM1Enemy / AM1HUD load the textures by path at runtime. Results ->
Reference/Sprites/import_result.json.
"""
import json
import os

import unreal

SPRITES = r"C:\m1-ue5\M1UE5\Reference\Sprites"
OUT = os.path.join(SPRITES, "import_result.json")
MAT_DEST = "/Game/Sprites"
MAT_NAME = "M_M1Sprite"
DEST = "/Game/Sprites/Monsters"  # default-texture lookup for the material

LISTS = [
    (os.path.join(SPRITES, "import_list.json"), "/Game/Sprites/Monsters"),
    (os.path.join(SPRITES, "weapon_import_list.json"), "/Game/Sprites/Weapons"),
]

result = {"textures": 0, "missing": [], "material": None}

# ---------------------------------------------------------------- textures
for list_path, dest in LISTS:
    if not os.path.isfile(list_path):
        unreal.log_warning(f"[sprites] no list {list_path}; skipping")
        continue
    with open(list_path, encoding="utf-8") as fh:
        entries = json.load(fh)

    if unreal.EditorAssetLibrary.does_directory_exist(dest):
        unreal.EditorAssetLibrary.delete_directory(dest)

    tasks = []
    for e in entries:
        if not os.path.isfile(e["png_path"]):
            result["missing"].append(e["png_path"])
            continue
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", e["png_path"])
        t.set_editor_property("destination_path", dest)
        t.set_editor_property("destination_name", e["asset_name"])
        t.set_editor_property("automated", True)
        t.set_editor_property("save", True)
        t.set_editor_property("replace_existing", True)
        tasks.append(t)

    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    for t in tasks:
        for p in list(t.get_editor_property("imported_object_paths")):
            tex = unreal.load_asset(p)
            if isinstance(tex, unreal.Texture2D):
                # M1 pixel art: nearest filtering, uncompressed (no BC blocks),
                # no mip blur -- same recipe as the crisp wall textures.
                tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
                tex.set_editor_property("compression_settings",
                                        unreal.TextureCompressionSettings.TC_EDITOR_ICON)
                tex.set_editor_property("mip_gen_settings",
                                        unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
                unreal.EditorAssetLibrary.save_asset(tex.get_path_name())
                result["textures"] += 1

unreal.log(f"[sprites] imported {result['textures']} textures "
           f"({len(result['missing'])} missing PNGs)")

# ---------------------------------------------------------------- material
mat_path = f"{MAT_DEST}/{MAT_NAME}"
if unreal.EditorAssetLibrary.does_asset_exist(mat_path):
    unreal.EditorAssetLibrary.delete_asset(mat_path)

mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    MAT_NAME, MAT_DEST, unreal.Material, unreal.MaterialFactoryNew())
mel = unreal.MaterialEditingLibrary

mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
mat.set_editor_property("two_sided", True)

node = mel.create_material_expression(
    mat, unreal.MaterialExpressionTextureSampleParameter2D, -380, 0)
node.set_editor_property("parameter_name", "Sprite")
# Give the parameter a sane default so the material compiles standalone. The
# old per-type T_mon2_stand asset is gone (shared textures now); use the first
# successfully imported monster texture instead.
default_tex = None
for p in sorted(unreal.EditorAssetLibrary.list_assets(DEST, recursive=False)):
    asset = unreal.load_asset(p)
    if isinstance(asset, unreal.Texture2D):
        default_tex = asset
        break
if default_tex:
    node.set_editor_property("texture", default_tex)
mel.connect_material_property(node, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.connect_material_property(node, "A", unreal.MaterialProperty.MP_OPACITY_MASK)

mel.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset(mat.get_path_name())
result["material"] = mat.get_path_name()
unreal.log(f"[sprites] material {result['material']}")

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(result, fh, indent=2)
unreal.log("[sprites] done")
unreal.SystemLibrary.quit_editor()
