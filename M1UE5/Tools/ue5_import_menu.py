"""Headless: import the authentic M1 main-menu screen bitmap as a UE texture.

The menu art is Shapes collection 10 bitmap 001 (the title/menu interface),
already rendered opaque (clut-correct) by render_sprites.py to
Reference/Sprites/png/c10_k0_001.png. Import it to /Game/UI/Menu/T_menu_main
with crisp pixel-art settings; AM1PlayerController draws it as the main menu
with clickable hotspots over the baked-in buttons.

    UnrealEditor-Cmd <proj> -ExecutePythonScript=<this> -RenderOffScreen \\
        -unattended -nosplash -nopause -stdout
"""
import os

import unreal

PNG = r"C:\m1-ue5\M1UE5\Reference\Sprites\png\c10_k0_001.png"
DEST = "/Game/UI/Menu"
NAME = "T_menu_main"

if not os.path.isfile(PNG):
    unreal.log_error(f"[menu] source PNG missing: {PNG}")
else:
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", PNG)
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("destination_name", NAME)
    t.set_editor_property("automated", True)
    t.set_editor_property("save", True)
    t.set_editor_property("replace_existing", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    for p in list(t.get_editor_property("imported_object_paths")):
        tex = unreal.load_asset(p)
        if isinstance(tex, unreal.Texture2D):
            tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
            tex.set_editor_property("compression_settings",
                                    unreal.TextureCompressionSettings.TC_EDITOR_ICON)
            tex.set_editor_property("mip_gen_settings",
                                    unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            unreal.EditorAssetLibrary.save_asset(tex.get_path_name())
            unreal.log(f"[menu] imported {tex.get_path_name()}")

unreal.SystemLibrary.quit_editor()
