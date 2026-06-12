"""Headless: import M1 chapter screens as UE textures.

Reads Reference/Images_Shapes/chapter_import_list.json (produced by
Tools/gen_chapter_assets.py) and imports each PNG to
/Game/UI/Chapters/T_chapter_<NN> with crisp pixel-art settings.
AM1PlayerController shows the matching texture as a full-screen splash when the
player enters that level.

    UnrealEditor-Cmd <proj> -ExecutePythonScript=<this> -RenderOffScreen \\
        -unattended -nosplash -nopause -stdout
"""
import json
import os

import unreal

LIST = r"C:\m1-ue5\M1UE5\Reference\Images_Shapes\chapter_import_list.json"
DEST = "/Game/UI/Chapters"

if not os.path.isfile(LIST):
    unreal.log_error(f"[chapters] missing import list: {LIST} (run gen_chapter_assets.py)")
else:
    items = json.load(open(LIST, "r", encoding="utf-8"))
    tasks = []
    for it in items:
        if not os.path.isfile(it["png"]):
            unreal.log_error(f"[chapters] missing PNG: {it['png']}")
            continue
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", it["png"])
        t.set_editor_property("destination_path", DEST)
        t.set_editor_property("destination_name", it["asset"])
        t.set_editor_property("automated", True)
        t.set_editor_property("save", True)
        t.set_editor_property("replace_existing", True)
        tasks.append(t)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
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
                unreal.log(f"[chapters] imported {tex.get_path_name()}")

unreal.SystemLibrary.quit_editor()
