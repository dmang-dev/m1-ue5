# Project Status & Handoff — Marathon 1 → UE5

_Last updated: 2026-06-08_

This is the **single source of truth for where the port stands** and how to drive
it. Read this plus `M1UE5/CLAUDE.md` (conventions) before continuing.

---

## TL;DR — what works end to end today

The whole Marathon 1 campaign now imports into UE5 as **textured geometry, fully
headless** — no manual editor steps. The agentic loop is:

```
M1SOURCE  ──extract──►  Reference/(Maps, Shapes, …)
Reference/Maps/*.json  ──scen_to_obj.py──►  Reference/Geometry/*.obj + *.mtl + *_placements.json
*.obj + shared bitmaps  ──ue5_headless_build.py (UnrealEditor-Cmd)──►  37 UE levels + shared material library
a level  ──ue5_mrq_render.py (UnrealEditor-Cmd)──►  top-down PNG (read back by the agent)
```

All 37 levels built in one editor launch, **0 errors**, 100% material-slot
assignment. Spot-rendered Arrival and Colony Ship For Sale, Cheap — textures
correct on both (different shape collections), confirming the shared library
works campaign-wide.

---

## The pipeline, in order (commands)

Paths below are relative to `C:\m1-ue5\M1UE5`. Python is launched with `py`
(3.12). The UE editor is `UE_5.7` (see `MIGRATION.md` for the full exe path).

1. **Extract** original data → `Reference/` (regenerable, gitignored):
   ```
   py Tools/extract_reference.py            # maps, sounds, physics
   py Tools/extract_reference.py --shapes   # also dump per-collection bitmap PNGs
   ```

2. **Convert geometry** → per-level OBJ + MTL + placements:
   ```
   py Tools/scen_to_obj.py --all Reference/Maps --out Reference/Geometry
   ```
   Produces `NN_<name>.obj` (UVs + usemtl groups), `NN_<name>.mtl` (each
   `tex_<coll>_<shape>` → `../Shapes/Coll_<coll>/bitmaps/<shape>.png`), and
   `NN_<name>_placements.json` (player_start, monsters, objects, lights).

3. **Headless build** — import all levels + build the shared material library:
   ```
   UnrealEditor-Cmd <proj> -ExecutePythonScript="Tools\ue5_headless_build.py" \
       -RenderOffScreen -unattended -nosplash -nopause -stdout
   ```
   - `M1_LIMIT=N` env caps the count (validate on 1–2 before the full 37).
   - Writes a checkpointed `Reference/Geometry/ue5_build_result.json` after every
     level (so a crash is diagnosable).

4. **Headless render** — top-down PNG of one level (verification / agent feedback):
   ```
   set M1_LEVEL=13_Colony_Ship_For_Sale_Cheap
   UnrealEditor-Cmd <proj> -ExecCmds="py Tools\ue5_mrq_render.py" \
       -RenderOffScreen -nosplash -nopause -stdout
   ```
   - `M1_LEVEL` is the UE-safe level name (default `00_Arrival`).
   - Output → `Reference/Geometry/mrq/<level>_topdown.png`, plus `mrq_result.json`.

---

## How the headless UE5 loop works (and why it's built this way)

These were hard-won; don't "simplify" them without re-reading the gotchas.

- **Run mode:** use `-ExecutePythonScript=<file>` (build) and
  `-ExecCmds="py <file>"` (render), both with `-RenderOffScreen`. Do **not** use
  `-run=pythonscript` — it crashes with `Assertion failed:
  CurrentApplication.IsValid()` (no Slate).
- **Materials are built in Python, not by the OBJ importer.** UE's Interchange
  OBJ/.mtl importer creates materials but leaves base color unwired → flat
  black/white render. `ue5_headless_build.py` instead imports each bitmap as a
  `Texture2D` and builds an **unlit material** (texture on Emissive, nearest
  filter). Unlit = reads correctly at any light angle and matches Marathon's
  flat-shaded look. Slots are reassigned by `material_slot_name`.
- **Shared library.** Each Marathon shape is imported once into `/Game/Textures`
  + `/Game/Materials` and reused by every level that references it (134
  materials / 133 textures across the campaign). Per-level meshes live in
  `/Game/Meshes/M1/<level>/`, maps in `/Game/Levels/M1/L_<level>`.
- **Rendering uses Movie Render Queue (MRQ)**, not SceneCapture/HighResShot.
  MRQ renders through an exact camera (Level Sequence + Camera Cut) with proper
  warm-up. A straight-down camera blows out to white and hides walls, so the
  camera is an **angled bird's-eye** (~50° overhead) computed from the Sector
  actor bounds.
- **MRQ exit is fragile** — handled. The async finished-delegate gets GC'd if the
  executor is only a local, so the editor hangs open (observed: 20 min idle
  after the PNG was written). Fix: keep a module-level reference to the executor
  **and** a post-tick watchdog that force-quits on done or after a hard timeout.

---

## Repo & asset layout

| Path | What | Committed? |
|------|------|------------|
| `M1UE5/Tools/*.py` | the whole pipeline (extract, convert, headless build, render) | ✅ yes |
| `M1UE5/Reference/` | extracted data + generated OBJ/MTL/PNG | ❌ gitignored (regenerable) |
| `M1UE5/Content/Meshes/M1/`, `Content/Levels/M1/`, `Content/Textures/`, `Content/Materials/` | auto-generated UE assets | ❌ gitignored (regenerable) |
| `M1SOURCE/`, `M2SOURCE/`, `MISOURCE/` | Bungie game data | ❌ gitignored (IP; never commit) |
| `M1UE5/docs/` | design + status notes | ✅ yes |

Asset pipeline is the published package **`py-marathon-utils`**
(github.com/dmang-dev/py-marathon-utils, on PyPI). Install via
`pip install -r Tools/requirements.txt`.

---

## Done ✅

- Full Python extraction toolkit, published as `py-marathon-utils` (M1/M2/Infinity,
  read + write: maps, shapes, sounds, physics, terminals, strings, patches, images).
- `.scen` → OBJ converter with UVs, per-shape materials, and `.mtl` (all 37 levels).
- Texture extraction (Shapes → per-collection bitmap PNGs).
- **Fully headless UE5 loop**: build + MRQ render, agent reads results back, no
  manual editor steps.
- **All 37 levels imported** with textures + shared material library (0 errors).
- Parameterized render (any level via `M1_LEVEL`).
- **Walkable geometry (all 37):** sector meshes carry complex-as-simple
  collision (component `QueryAndPhysics` + `BlockAll`), and each level gets a
  real `PlayerStart` (`PlayerStart_M1`) at the Marathon spawn with the correct
  facing, read from `<level>_placements.json`. Headless verify:
  `M1_VIEW=fp M1_LEVEL=<safe> ue5_mrq_render.py` renders a first-person eye-level
  frame from the spawn and asserts the collision config (`fp_verify_result.json`).
- **Double-clickable WALKING build:** C++ `Source/M1UE5` provides `AM1Character`
  (first-person — WASD walk, mouse look, Space jump, gravity + collision against
  the sector meshes) + `AM1GameMode`; legacy input maps are in
  `Config/DefaultInput.ini`, and `Config/DefaultEngine.ini` sets the default map
  (`L_00_Arrival`) + GameMode (`/Script/M1UE5.M1GameMode`). The project is now a
  C++/Blueprint hybrid (needs VS Build Tools to compile, present on this machine).
  A Win64 Development package is produced by:
  ```
  "<UE>\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="<proj>\M1UE5.uproject" ^
    -noP4 -platform=Win64 -clientconfig=Development -cook -map=L_00_Arrival ^
    -stage -pak -archive -archivedirectory="<proj>\Build" -build -utf8output -unattended
  ```
  Output: `Build/Windows/M1UE5.exe` (double-click). Verified: boots, loads
  `L_00_Arrival` with `M1GameMode`, 0 errors. **Packaging gotchas:** (1) UAT builds
  the editor target — **fails if any `UnrealEditor.exe` is open holding Live
  Coding** (close them all first, incl. the Project Browser). (2) Do **not** pass
  `-nocompileeditor` — the cook needs the `M1UE5Editor.target` receipt. (3) Target
  rules must use `DefaultBuildSettings = BuildSettingsVersion.V6` to match the
  installed engine's shared build environment (V5 → `UndefinedIdentifier` mismatch).

## Open items / next up 🔲

- **Campaign flow + movement polish (next milestone):** all 37 levels are now
  cooked into the build and reachable in-game via the `[` / `]` (or PageUp/Dn)
  level switcher (`AM1Character::CycleLevel` → `UGameplayStatics::OpenLevel`, with
  an on-screen level name); the `L_00_Arrival` default loads on launch. Replace
  free-roam cycling with real story progression (terminals, level exits), and
  layer movement polish (sprint, crouch, head-bob, mouse sensitivity) onto
  `AM1Character`. (Cook all maps via the quoted array arg
  `"-map=$maps"` — a bare `-map=$maps` does NOT expand in PowerShell 5.1.)
  NOTE: the **headless editor world can't raycast** — line/shape
  traces return `None` under `-ExecutePythonScript`/`-ExecCmds` (no physics query
  scene), so a headless "drop test" is a collision-config assertion; a real
  settle-test needs PIE/game. (`unreal.Rotator` positional order is
  `(roll, pitch, yaw)` — use keyword args.)
- Enemy **sprite billboards** from Shapes (placements already extracted).
- Doors / platforms / elevators from polygon+platform data.
- Liquids (lava/water media), audio import + cues, terminals (renderer exists in
  the package).

## Known issues / gotchas ⚠️

- **OBJ import MUST use the LEGACY importer, not Interchange.** UE5's Interchange
  OBJ translator silently culls ~17% of each level's triangles at the *translate*
  stage (Arrival 2738→2208), punching holes around every terminal niche. No
  import/build flag stops it: `remove_degenerates` is already off, and explicit
  `vn` normals + `s off` smoothing groups are both ignored. Fix:
  `Interchange.FeatureFlags.Import.OBJ=0` — set in `Config/DefaultEngine.ini`
  `[ConsoleVariables]` AND executed at the top of `ue5_headless_build.py`'s
  driver — routes import through the legacy `UStaticMeshFactory`, which keeps
  every real face (Arrival 2208→2678; only ~60 genuinely zero-area tris drop).
- **Per-level OBJ/MTL filenames MUST be space-free.** Corollary of the above: the
  legacy importer's `mtllib` line is whitespace-delimited, so a spaced filename
  (`01_Bigger Guns Nearby.mtl`) only reads the first token, fails to load the
  `.mtl`, and collapses the mesh to one `defaultMat` slot. `scen_to_obj` now emits
  underscore stems (obj/mtl/mtllib/placements all consistent); keep it that way.
- **Headless builds need `-RenderOffScreen`** in the `UnrealEditor-Cmd` args, or
  an editor window pops up during level generation. (`-nullrhi` is the harder
  fallback if a window still appears.)
- **`L_TC1_Arrival.umap` shows perpetually "modified" in git.** It's the legacy
  hand-made single-level map (pre-batch), and the editor re-saves it on open.
  Superseded by `/Game/Levels/M1/L_00_Arrival`. Safe to ignore, or
  `git rm --cached` it + gitignore to stop the churn (deferred).
- **Interchange leaves orphan `TEX_*` / per-level material assets** in the
  per-level mesh folder (its own import artifacts; we reassign to the shared
  library and ignore them). The dest folder is wiped each run so they don't
  accumulate. Harmless.
- **A few black surfaces** in renders = the `default` (no-texture) material or
  wall back-faces. Cosmetic; revisit during the art/material pass.
- **`/_nul`** stray file at repo root (from a shell redirect to `nul`).
  Gitignored; safe to delete.
- **Render grep pipes buffer** until the process exits — to watch progress,
  read the checkpointed `*_result.json`, not the live stdout.
- Two editor instances on the same project at once can lock assets — run build
  and render sequentially, not in parallel.

---

## Privacy / authorship constraint (keep enforcing)

This repo is intended for public release someday. **No PII in committed files:**
no real name, no personal email, no `C:\Users\...` home paths. Git author is
`dmang-dev <dmang-dev@users.noreply.github.com>`. Treat everything under
`*SOURCE/` and `Reference/` as unlicensed third-party reference — ship original
art before any public release.
