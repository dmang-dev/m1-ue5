"""M1UE5 Asset Setup -- the bring-your-own-assets converter (installer Phase 2).

LOCATE-ONLY (no download, per the project's license posture): the user must already
own a copy of the Marathon 1 data (Aleph One's freeware release). This tool finds it,
verifies it, and converts it into the per-user runtime cache the game loads at
runtime -- with NO Unreal editor and NO cook.

Pipeline (all pure Python via py-marathon-utils):
    <source>/{Map.scen,Shapes.shps,Sounds.sndz,Physics.phys}
      --(extract_reference --shapes)-->  <cache>/{Shapes,Sounds,Physics,Maps}
      --(scen_to_obj --all Maps)------->  <cache>/Geometry  (OBJ + MTL + placements)

The game's AM1RuntimeLevel::ResolveDataRoot loads <cache> = %LOCALAPPDATA%\\M1UE5\\Data
(Geometry/ + Shapes/ as siblings; the MTLs reference ../Shapes/...).

Run (dev):   python Tools/m1_asset_setup.py --source <Marathon data dir>
Frozen:      M1AssetSetup.exe --source <dir>            (PyInstaller, no Python needed)

Emits machine-readable progress as `@PROGRESS <json>` lines so the installer GUI can
show a progress bar. Writes <cache>\\.m1_assets_done (the manifest + skip marker).
"""
import argparse
import hashlib
import json
import os
import sys
import time
from pathlib import Path

THIS = Path(__file__).resolve()
TOOLS = THIS.parent
sys.path.insert(0, str(TOOLS))  # import the sibling converters (works frozen too)

DATA_FILES = ["Map.scen", "Shapes.shps", "Sounds.sndz", "Physics.phys"]
MARKER = ".m1_assets_done"


def progress(step, status, **extra):
    """One machine-readable line the installer wizard can parse + a human line."""
    rec = {"step": step, "status": status, **extra}
    print("@PROGRESS " + json.dumps(rec), flush=True)
    print(f"  [{status:>5}] {step}" + (f"  {extra}" if extra else ""), flush=True)


def default_cache() -> Path:
    base = os.environ.get("LOCALAPPDATA") or str(Path.home() / "AppData" / "Local")
    return Path(base) / "M1UE5" / "Data"


def find_source(explicit: Path | None) -> Path | None:
    """Locate-only: an explicit --source, else common spots. Never downloads."""
    candidates = []
    if explicit:
        candidates.append(explicit)
    repo_default = TOOLS.parents[1] / "M1SOURCE" / "Marathon-20250829"
    candidates.append(repo_default)
    for env in ("M1_SOURCE", "ALEPHONE_DATA"):
        if os.environ.get(env):
            candidates.append(Path(os.environ[env]))
    for c in candidates:
        if c and all((c / f).is_file() for f in DATA_FILES):
            return c
    return None


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def tools_commit() -> str:
    try:
        import subprocess
        return subprocess.check_output(
            ["git", "-C", str(TOOLS), "rev-parse", "--short", "HEAD"],
            text=True, stderr=subprocess.DEVNULL).strip()
    except Exception:
        return "unknown"


def pmu_version() -> str:
    try:
        import marathon_utils
        return getattr(marathon_utils, "__version__", "unknown")
    except Exception:
        return "unavailable"


def main(argv=None) -> int:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--source", type=Path, default=None,
                   help="folder holding Map.scen/Shapes.shps/Sounds.sndz/Physics.phys")
    p.add_argument("--cache", type=Path, default=default_cache(),
                   help="output cache (default %%LOCALAPPDATA%%\\M1UE5\\Data)")
    p.add_argument("--force", action="store_true", help="reconvert even if up to date")
    args = p.parse_args(argv)

    cache: Path = args.cache
    progress("locate", "start")
    source = find_source(args.source)
    if not source:
        progress("locate", "error", message=(
            "Marathon 1 data not found. Point --source at the folder containing "
            + ", ".join(DATA_FILES) + " (Aleph One's freeware M1 release)."))
        return 2
    hashes = {f: sha256(source / f) for f in DATA_FILES}
    progress("locate", "done", source=str(source), hashes=hashes)

    # Skip if already converted from the SAME data, unless --force.
    marker = cache / MARKER
    if marker.is_file() and not args.force:
        try:
            prev = json.loads(marker.read_text(encoding="utf-8"))
            if prev.get("hashes") == hashes and (cache / "Geometry").is_dir():
                progress("convert", "skip", reason="cache up to date", cache=str(cache))
                return 0
        except Exception:
            pass

    cache.mkdir(parents=True, exist_ok=True)

    progress("extract", "start", dest=str(cache))
    import extract_reference
    rc = extract_reference.main(["--source", str(source), "--dest", str(cache), "--shapes"])
    if rc not in (None, 0):
        progress("extract", "error", code=rc)
        return 3
    progress("extract", "done")

    progress("geometry", "start")
    import scen_to_obj
    rc = scen_to_obj.main(["--all", str(cache / "Maps"), "--out", str(cache / "Geometry")])
    if rc not in (None, 0):
        progress("geometry", "error", code=rc)
        return 4
    n_obj = len(list((cache / "Geometry").glob("*.obj")))
    progress("geometry", "done", levels=n_obj)

    # Ship the canonical sound id->name map (flattened) so the game's runtime WAV
    # loader can resolve cooked /Game/Audio/A_<name> paths to Sounds/<id>.wav.
    names_src = TOOLS / "m1_sound_names.json"
    if names_src.is_file():
        try:
            m = json.loads(names_src.read_text(encoding="utf-8")).get("map", {})
            (cache / "sound_names.json").write_text(json.dumps(m), encoding="utf-8")
            progress("sounds", "done", names=len(m))
        except Exception as e:
            progress("sounds", "warn", message=str(e))

    manifest = {
        "hashes": hashes,
        "source": str(source),
        "py_marathon_utils": pmu_version(),
        "tools_commit": tools_commit(),
        "levels": n_obj,
        "completed_unix": int(time.time()),
    }
    marker.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    progress("done", "done", cache=str(cache), levels=n_obj)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
