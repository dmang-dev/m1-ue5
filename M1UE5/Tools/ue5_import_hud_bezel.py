"""Headless: import the M1 in-game HUD bezel (PICT 128, hole punched transparent).

The source is Marathon.appl PICT 128 -- the authentic metal status-bar frame
that wraps the 3D view. gen step (Pillow) makes the central 3D-viewport
rectangle transparent so the constrained game view shows through it. Imported
to /Game/UI/HUD/T_hud_bezel; AM1HUD draws it full-screen (4:3, pillarboxed)
around the viewport-constrained 3D view.

    UnrealEditor-Cmd <proj> -ExecutePythonScript=<this> -RenderOffScreen \\
        -unattended -nosplash -nopause -stdout
"""
import os

import unreal

_PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PNG = os.path.join(_PROJECT, "Reference", "Images", "T_hud_bezel.png")
DEST = "/Game/UI/HUD"
NAME = "T_hud_bezel"

if not os.path.isfile(PNG):
    unreal.log_error(f"[bezel] source PNG missing: {PNG} (run gen step first)")
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
            unreal.log(f"[bezel] imported {tex.get_path_name()}")

unreal.SystemLibrary.quit_editor()
