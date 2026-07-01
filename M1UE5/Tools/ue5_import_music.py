"""Editor stage: import the Marathon music WAVs as looping SoundWaves.

Reference/Music/A_music_NN.wav -> /Game/Audio/Music/A_music_NN. Each wave is
marked looping so a track keeps playing for the whole level (tracks are
1.5-6 min; levels can outlast them). Run gen_music_assets.py first.

    UnrealEditor-Cmd.exe <project> -ExecutePythonScript=<this file> \\
        -RenderOffScreen -unattended -nosplash -nopause -stdout
"""
import glob
import os

import unreal

_PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SRC = os.path.join(_PROJECT, "Reference", "Music")
DEST = "/Game/Audio/Music"

wavs = sorted(glob.glob(os.path.join(SRC, "*.wav")))
tasks = []
for w in wavs:
    name = os.path.splitext(os.path.basename(w))[0]  # A_music_NN
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", w)
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("save", True)
    t.set_editor_property("replace_existing", True)
    tasks.append(t)

if tasks:
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

imported = 0
for t in tasks:
    for p in list(t.get_editor_property("imported_object_paths")):
        a = unreal.load_asset(p)
        if isinstance(a, unreal.SoundWave):
            a.set_editor_property("looping", True)
            unreal.EditorAssetLibrary.save_asset(a.get_path_name())
            imported += 1
            unreal.log(f"[music] imported (looping) {a.get_path_name()}")

unreal.log(f"[music] done: {imported}/{len(tasks)} tracks")
unreal.SystemLibrary.quit_editor()
