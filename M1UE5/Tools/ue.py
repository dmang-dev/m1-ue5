"""Config-driven headless UE editor-python runner.

Resolves UnrealEditor-Cmd + the .uproject from m1_paths and runs an editor
python script (the ue5_import_*.py / ue5_*.py tools) with the standard headless
flags -- no hard-coded engine path.

    python Tools/ue.py ue5_import_menu.py
    python Tools/ue.py ue5_mrq_render.py            # env vars (M1_LEVEL etc.) pass through
    python Tools/ue.py --dry ue5_import_chapters.py

Set env M1_UE or Tools/m1_paths.local.json if the engine isn't auto-found.
"""
import os
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import m1_paths as P


def main(argv):
    dry = "--dry" in argv
    argv = [a for a in argv if a != "--dry"]
    if not argv:
        print("usage: python Tools/ue.py [--dry] <editor_script.py> [extra UE args]", file=sys.stderr)
        return 2

    script = Path(argv[0])
    if not script.is_absolute():
        script = P.TOOLS / script.name
    extra = argv[1:]

    cmd = [str(P.UE_CMD), str(P.UPROJECT),
           f"-ExecutePythonScript={script}",
           "-RenderOffScreen", "-unattended", "-nosplash", "-nopause", "-stdout", *extra]

    print(f"UE:     {P.UE_CMD}")
    print(f"script: {script}\n")
    print(" ".join(f'"{c}"' if " " in c else c for c in cmd) + "\n")
    if dry:
        return 0
    if not P.UE_CMD.exists():
        print(f"ERROR: UnrealEditor-Cmd not found: {P.UE_CMD}\n"
              "Set env M1_UE or Tools/m1_paths.local.json (ue_root).", file=sys.stderr)
        return 2
    if not script.exists():
        print(f"ERROR: script not found: {script}", file=sys.stderr)
        return 2
    return subprocess.call(cmd)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
