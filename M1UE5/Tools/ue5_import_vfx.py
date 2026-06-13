"""Editor stage: import the procedural VFX sprites (muzzle flash) to /Game/Sprites/VFX.

Source PNGs are generated (no M1 extraction) into Reference/Sprites; the HUD
composites T_muzzle_flash additively over the weapon viewmodel during the fire
window. Bilinear filter (smooth glow, unlike the nearest-filtered pixel art).

    UnrealEditor-Cmd.exe <project> -ExecutePythonScript=<this file> \\
        -RenderOffScreen -unattended -nosplash -nopause -stdout
"""
import os

import unreal

ITEMS = [
    (r"C:\m1-ue5\M1UE5\Reference\Sprites\T_muzzle_flash.png", "/Game/Sprites/VFX", "T_muzzle_flash"),
]

for png, dest, name in ITEMS:
    if not os.path.isfile(png):
        unreal.log_error(f"[vfx] missing {png}")
        continue
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", png)
    t.set_editor_property("destination_path", dest)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("save", True)
    t.set_editor_property("replace_existing", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    for p in list(t.get_editor_property("imported_object_paths")):
        tex = unreal.load_asset(p)
        if isinstance(tex, unreal.Texture2D):
            tex.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
            tex.set_editor_property("mip_gen_settings",
                                    unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            unreal.EditorAssetLibrary.save_asset(tex.get_path_name())
            unreal.log(f"[vfx] imported {tex.get_path_name()}")

unreal.SystemLibrary.quit_editor()
