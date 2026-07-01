"""Config-driven packaged build (RunUAT BuildCookRun).

Resolves the Unreal install, the .uproject, and the cook map list from
m1_paths -- no hard-coded paths. Produces Build/Windows/M1UE5.exe.

    python Tools/package.py           # build + cook + stage + pak + archive
    python Tools/package.py --dry     # just print the resolved command

If RunUAT can't be found, set env M1_UE or Tools/m1_paths.local.json.
"""
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import m1_paths as P


def main(argv):
    dry = "--dry" in argv
    maps = P.campaign_maps()
    if not maps:
        print(f"ERROR: no campaign maps under {P.CONTENT / 'Levels' / 'M1'}", file=sys.stderr)
        return 2

    args = [
        "BuildCookRun",
        f"-project={P.UPROJECT}",
        "-noP4", "-platform=Win64", "-clientconfig=Development",
        "-cook", f"-map={'+'.join(maps)}",
        "-stage", "-pak",
        "-archive", f"-archivedirectory={P.BUILD}",
        "-build", "-utf8output", "-unattended",
    ]

    print(f"UE:    {P.UE_ROOT}")
    print(f"maps:  {len(maps)} ({maps[0]} ... {maps[-1]})")
    print(f"out:   {P.BUILD / 'Windows' / 'M1UE5.exe'}\n")
    print(f'"{P.RUNUAT}" ' + " ".join(args) + "\n")
    if dry:
        return 0
    if not P.RUNUAT.exists():
        print(f"ERROR: RunUAT not found: {P.RUNUAT}\n"
              "Set env M1_UE or Tools/m1_paths.local.json (ue_root).", file=sys.stderr)
        return 2
    # cmd /c runs the .bat and handles quoting of the space-containing UE path.
    return subprocess.call(["cmd", "/c", str(P.RUNUAT), *args])


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
