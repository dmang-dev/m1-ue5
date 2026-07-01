"""Headless UE5 batch-import of the Marathon terminal (lore) page art.

    UnrealEditor-Cmd.exe <project> -ExecutePythonScript=<this file> \\
        -RenderOffScreen -unattended -nosplash -nopause -stdout

Reads Reference/Terminals/term_import_list.json (gen_terminal_assets.py) and
imports every authentic green-on-black page PNG into /Game/UI/Terminals with
crisp pixel-art settings (the renders are already crisp text -- nearest filter,
uncompressed editor-icon, no mips). AM1HUD loads each page by string path at
runtime (/Game/UI/Terminals/T_term<id>_p<page>).

Also ensures /Game/UI is in DefaultGame.ini DirectoriesToAlwaysCook (the pages
load by string path, invisible to the cooker's reference walk). Results ->
Reference/Terminals/import_result.json.
"""
import json
import os

import unreal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TERM_DIR = os.path.join(ROOT, "Reference", "Terminals")
LIST_PATH = os.path.join(TERM_DIR, "term_import_list.json")
OUT = os.path.join(TERM_DIR, "import_result.json")
DEST = "/Game/UI/Terminals"

result = {"textures": 0, "missing": [], "always_cook": None}

# ---------------------------------------------------------------- import
if not os.path.isfile(LIST_PATH):
    unreal.log_error("[terminals] no list %s; run gen_terminal_assets.py first" % LIST_PATH)
    with open(OUT, "w", encoding="utf-8") as fh:
        json.dump(result, fh, indent=2)
    unreal.SystemLibrary.quit_editor()
else:
    with open(LIST_PATH, encoding="utf-8") as fh:
        entries = json.load(fh)

    # Wipe + re-import so the folder always matches the current list.
    if unreal.EditorAssetLibrary.does_directory_exist(DEST):
        unreal.EditorAssetLibrary.delete_directory(DEST)

    tasks = []
    for e in entries:
        if not os.path.isfile(e["png_path"]):
            result["missing"].append(e["png_path"])
            continue
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", e["png_path"])
        t.set_editor_property("destination_path", DEST)
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
                # Crisp terminal text: nearest filter, uncompressed (no BC blocks
                # that would smear the green text), no mip blur.
                tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
                tex.set_editor_property("compression_settings",
                                        unreal.TextureCompressionSettings.TC_EDITOR_ICON)
                tex.set_editor_property("mip_gen_settings",
                                        unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
                unreal.EditorAssetLibrary.save_asset(tex.get_path_name())
                result["textures"] += 1

    unreal.log("[terminals] imported %d textures (%d missing PNGs)"
               % (result["textures"], len(result["missing"])))

    # ------------------------------------------------------ always-cook /Game/UI
    # The pages load by string path, so the cooker won't discover them as
    # dependencies -- force-cook the whole /Game/UI tree. Add it once.
    ini = os.path.join(ROOT, "Config", "DefaultGame.ini")
    cook_line = '+DirectoriesToAlwaysCook=(Path="/Game/UI")'
    try:
        with open(ini, encoding="utf-8") as fh:
            text = fh.read()
    except OSError:
        text = ""
    if '(Path="/Game/UI")' in text:
        result["always_cook"] = "already present"
    else:
        if text and not text.endswith("\n"):
            text += "\n"
        text += cook_line + "\n"
        with open(ini, "w", encoding="utf-8") as fh:
            fh.write(text)
        result["always_cook"] = "added"
    unreal.log("[terminals] /Game/UI always-cook: %s" % result["always_cook"])

    with open(OUT, "w", encoding="utf-8") as fh:
        json.dump(result, fh, indent=2)
    unreal.log("[terminals] done")
    unreal.SystemLibrary.quit_editor()
