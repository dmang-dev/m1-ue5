"""Single source of truth for machine-specific paths.

Goal: **zero path edits on transfer.** Everything resolves from this file's
own location plus, for the few things that live outside the repo (the Bungie
data dir, the Unreal install, the venv), an env var or an optional
`Tools/m1_paths.local.json`. Nothing in `Tools/` should hard-code an absolute
path any more -- import from here instead.

Resolution order for external paths: **env var -> m1_paths.local.json ->
auto-discover -> documented default.**

    Repo-relative (always correct):  PROJECT, WORKSPACE, REFERENCE, SOURCE,
                                     CONTENT, TOOLS, UPROJECT, BUILD
    Bungie data (env M1_SOURCE):     M1SOURCE, MAP_SCEN, SHAPES, APPL,
                                     SOUNDS_SRC, PHYSICS_SRC
    Unreal (env M1_UE):              UE_ROOT, UE_CMD, UE_PYTHON, RUNUAT
    Python venv (env M1_VENV_PYTHON):VENV_PYTHON

Run `python Tools/m1_paths.py` to print what resolved and what's missing.
Importable from both the project venv and the UE editor python (stdlib only).
"""
import json
import os
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
PROJECT = TOOLS.parent                 # <repo>/M1UE5
WORKSPACE = PROJECT.parent             # <repo> (holds M1SOURCE/, .venv/)

# --- Repo-relative (never machine-specific) --------------------------------
REFERENCE = PROJECT / "Reference"
SOURCE = PROJECT / "Source" / "M1UE5"
CONTENT = PROJECT / "Content"
BUILD = PROJECT / "Build"
UPROJECT = PROJECT / "M1UE5.uproject"

# --- Optional local overrides file (gitignored) ----------------------------
_CONFIG = {}
_cfg_path = TOOLS / "m1_paths.local.json"
if _cfg_path.exists():
    try:
        _CONFIG = json.loads(_cfg_path.read_text(encoding="utf-8"))
    except Exception:
        _CONFIG = {}


def _override(env_key, cfg_key):
    val = os.environ.get(env_key) or _CONFIG.get(cfg_key)
    return Path(val) if val else None


# --- Bungie data dir (M1SOURCE) --------------------------------------------
def _find_m1source():
    o = _override("M1_SOURCE", "m1_source")
    if o:
        return o
    base = WORKSPACE / "M1SOURCE"
    if (base / "Map.scen").exists():
        return base
    if base.is_dir():
        for sub in sorted(base.iterdir()):
            if sub.is_dir() and (sub / "Map.scen").exists():
                return sub
    return base / "Marathon-20250829"  # documented default layout


M1SOURCE = _find_m1source()
MAP_SCEN = M1SOURCE / "Map.scen"
SHAPES = M1SOURCE / "Shapes.shps"
APPL = M1SOURCE / "Marathon.appl"
SOUNDS_SRC = M1SOURCE / "Sounds.sndz"
PHYSICS_SRC = M1SOURCE / "Physics.phys"


# --- Unreal Engine ---------------------------------------------------------
def _find_ue():
    o = _override("M1_UE", "ue_root")
    if o:
        return o
    candidates = []
    for drive in ("C", "D", "E", "F", "G", "I"):
        for pf in (r"{d}:\Epic Games", r"{d}:\Program Files\Epic Games"):
            root = Path(pf.format(d=drive))
            if root.is_dir():
                candidates += sorted(root.glob("UE_5.*"), reverse=True)
    for c in candidates:
        if (c / "Engine" / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe").exists():
            return c
    return Path(r"I:\Epic Games\UE_5.7")  # documented default


UE_ROOT = _find_ue()
UE_CMD = UE_ROOT / "Engine" / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe"
UE_PYTHON = UE_ROOT / "Engine" / "Binaries" / "ThirdParty" / "Python3" / "Win64" / "python.exe"
RUNUAT = UE_ROOT / "Engine" / "Build" / "BatchFiles" / "RunUAT.bat"


# --- Project venv python ---------------------------------------------------
def _find_venv():
    o = _override("M1_VENV_PYTHON", "venv_python")
    if o:
        return o
    for c in (WORKSPACE / ".venv" / "Scripts" / "python.exe",
              PROJECT / ".venv" / "Scripts" / "python.exe"):
        if c.exists():
            return c
    return WORKSPACE / ".venv" / "Scripts" / "python.exe"


VENV_PYTHON = _find_venv()


def campaign_maps():
    """Cook map list: L_MainMenu + every L_NN_* leaf under Content/Levels/M1."""
    levels = CONTENT / "Levels" / "M1"
    leaves = sorted(p.stem for p in levels.glob("*.umap")) if levels.is_dir() else []
    return [m for m in leaves
            if m == "L_MainMenu" or (m.startswith("L_") and m[2:4].isdigit())]


def _report():
    rows = [
        ("PROJECT", PROJECT, True), ("WORKSPACE", WORKSPACE, True),
        ("REFERENCE", REFERENCE, False), ("SOURCE", SOURCE, True),
        ("CONTENT", CONTENT, True), ("UPROJECT", UPROJECT, True),
        ("M1SOURCE", M1SOURCE, False), ("  MAP_SCEN", MAP_SCEN, False),
        ("  SHAPES", SHAPES, False), ("  APPL", APPL, False),
        ("UE_ROOT", UE_ROOT, False), ("  UE_CMD", UE_CMD, False),
        ("  UE_PYTHON", UE_PYTHON, False), ("  RUNUAT", RUNUAT, False),
        ("VENV_PYTHON", VENV_PYTHON, False),
    ]
    print("m1_paths resolution (env var / m1_paths.local.json / auto-discover):\n")
    missing = 0
    for name, path, required in rows:
        ok = Path(path).exists()
        if not ok and (required or name in ("M1SOURCE", "UE_ROOT", "VENV_PYTHON")):
            missing += 1
        print(f"  {'ok ' if ok else 'MISS'}  {name:<14} {path}")
    print(f"\n  campaign maps: {len(campaign_maps())}")
    if missing:
        print(f"\n  {missing} key path(s) missing. Set env (M1_SOURCE / M1_UE / "
              "M1_VENV_PYTHON) or create Tools/m1_paths.local.json "
              "(see m1_paths.example.json).")
    return 1 if missing else 0


if __name__ == "__main__":
    import sys
    sys.exit(_report())
