"""Headless UE5 batch-import of the extracted Marathon sounds, named canonically.

    UnrealEditor-Cmd.exe <project> -ExecutePythonScript=<this file> \\
        -RenderOffScreen -unattended -nosplash -nopause -stdout

Imports every WAV under Reference/Sounds/<family>/ as a SoundWave into
/Game/Audio/<Family>/. Asset names come from the canonical map in
Tools/m1_sound_names.json (resource id -> Marathon _snd_ name), so e.g.
weapon/02020.wav -> A_teleport_in and monster/01030.wav -> A_magnum_firing.
Unmapped/low-confidence ids keep an A_<id> name; permutation duplicates get a
_<id> suffix. The family FOLDER is py-marathon-utils' cosmetic id//1000 bucket
(not a real category) -- the asset NAME now carries the meaning.

Results -> Reference/Sounds/import_result.json. Uses the `unreal` module.
"""
import glob
import json
import os

import unreal

SND = r"C:\m1-ue5\M1UE5\Reference\Sounds"
NAME_MAP_PATH = r"C:\m1-ue5\M1UE5\Tools\m1_sound_names.json"
OUT = os.path.join(SND, "import_result.json")

with open(NAME_MAP_PATH, encoding="utf-8") as fh:
    NAME_MAP = json.load(fh)["map"]

result = {}
families = sorted(d for d in os.listdir(SND)
                  if os.path.isdir(os.path.join(SND, d)))
for fam in families:
    dest = f"/Game/Audio/{fam.capitalize()}"
    if unreal.EditorAssetLibrary.does_directory_exist(dest):
        unreal.EditorAssetLibrary.delete_directory(dest)
    wavs = sorted(glob.glob(os.path.join(SND, fam, "*.wav")))
    used = set()
    tasks = []
    for w in wavs:
        stem = os.path.splitext(os.path.basename(w))[0]
        canonical = NAME_MAP.get(stem)
        if canonical:
            asset = f"A_{canonical}"
            if asset in used:  # permutation / cross-slot duplicate
                asset = f"A_{canonical}_{stem}"
        else:
            asset = f"A_{stem}"
        used.add(asset)
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", w)
        t.set_editor_property("destination_path", dest)
        t.set_editor_property("destination_name", asset)
        t.set_editor_property("automated", True)
        t.set_editor_property("save", True)
        t.set_editor_property("replace_existing", True)
        tasks.append(t)
    if tasks:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    imported = 0
    named = 0
    for t in tasks:
        for p in list(t.get_editor_property("imported_object_paths")):
            a = unreal.load_asset(p)
            if isinstance(a, unreal.SoundWave):
                imported += 1
                if not a.get_name().startswith("A_0") and not a.get_name().startswith("A_2"):
                    named += 1
    result[fam] = {"imported": imported, "named": named}
    unreal.log(f"[sounds] {fam}: imported {imported} ({named} canonical names)")

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(result, fh, indent=2)
unreal.log("[sounds] done")
unreal.SystemLibrary.quit_editor()
