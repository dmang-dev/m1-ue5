# PyInstaller spec -- freeze the M1UE5 Asset Setup wizard into a standalone, no-Python
# bundle for the installer payload (installer Phase 2, "frozen converter").
#
#   cd M1UE5/Tools
#   pip install pyinstaller py-marathon-utils[images] soundfile
#   pyinstaller m1_asset_setup.spec --noconfirm
#   -> dist/M1AssetSetup/M1AssetSetup.exe  (ship the whole dist/M1AssetSetup folder)
#
# --onedir (a folder, not --onefile): faster start, and onefile's temp-extract trips
# some AV. The Inno installer (Phase 3) drops this folder next to the game and runs
# `M1AssetSetup.exe --source <located dir>` during setup.
#
# NOTE (libsndfile): py-marathon-utils' sound extraction (extract_reference --shapes
# pulls sounds too) decodes via `soundfile`, which ships the libsndfile DLL. collect_all
# below bundles it; after building, smoke-test that `M1AssetSetup.exe --source <dir>`
# completes the "extract" step (it would error on the WAV write if libsndfile is missing).

from PyInstaller.utils.hooks import collect_all, collect_submodules

datas, binaries, hiddenimports = [], [], []

# py-marathon-utils: the maps/shapes/sounds/physics reader the converters depend on.
for pkg in ("marathon_utils",):
    d, b, h = collect_all(pkg)
    datas += d; binaries += b; hiddenimports += h

# Pillow (texture/sprite PNG writing) + soundfile/libsndfile (WAV decode).
hiddenimports += collect_submodules("PIL")
for pkg in ("soundfile",):
    try:
        d, b, h = collect_all(pkg)
        datas += d; binaries += b; hiddenimports += h
    except Exception:
        pass  # built without sounds; geometry/textures still convert

# The sibling converter modules the wizard imports at runtime.
hiddenimports += ["extract_reference", "scen_to_obj"]

a = Analysis(
    ["m1_asset_setup.py"],
    pathex=["."],            # run this spec from M1UE5/Tools
    binaries=binaries,
    datas=datas,
    hiddenimports=hiddenimports,
    hookspath=[],
    runtime_hooks=[],
    excludes=["tkinter", "unreal"],
    noarchive=False,
)
pyz = PYZ(a.pure)
exe = EXE(
    pyz, a.scripts, [],
    exclude_binaries=True,
    name="M1AssetSetup",
    console=True,            # the installer captures @PROGRESS lines from stdout
    disable_windowed_traceback=False,
)
coll = COLLECT(exe, a.binaries, a.datas, name="M1AssetSetup")
