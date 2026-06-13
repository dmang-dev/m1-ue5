# Marathon UE5 Remake — Transfer Handoff

A self-contained guide for picking this project up on another machine/agent.
For deeper dives see the sibling docs (`PROJECT_STATUS.md`,
`scen_to_ue5_import.md`, `mesh_import_guide.md`, `pipeline_roadmap.md`).
Authoritative conventions live in [`../CLAUDE.md`](../CLAUDE.md) and the repo
root [`../../CLAUDE.md`](../../CLAUDE.md) — read those first.

---

## 1. What this is

A from-scratch, **automation-first** remake of Bungie's **Marathon 1** in
Unreal Engine 5. Nothing is hand-modeled: geometry, sprites, sounds, physics,
terminals, doors, panels, and chapter art are all extracted/derived
programmatically from the original M1 data and imported into UE5. It is a
C++/Blueprint hybrid (gameplay is C++ under `Source/M1UE5`; no Blueprint logic
of note yet).

**Current state:** a playable vertical slice across all 37 levels — main menu →
chapter splash → first-person combat (7 weapons + Pfhor enemies) → readable lore
terminals → opening doors → shield chargers / save buffers → save & continue
with shield carry-over → an authentic PICT-128 framed HUD. See §6 for the status
matrix.

---

## 2. Repository layout

```
C:\m1-ue5\                         # repo root
├── CLAUDE.md                      # root conventions
├── M1UE5/                         # the UE5 project + tooling + docs  <- all work
│   ├── M1UE5.uproject
│   ├── CLAUDE.md                  # authoritative project guide
│   ├── Source/M1UE5/              # C++ gameplay (see §5)
│   ├── Content/                   # committed UE assets (sprites, audio, UI, levels)
│   ├── Tools/                     # Python pipeline (see §4)
│   ├── Reference/                 # GITIGNORED regenerable extract output
│   ├── Build/                     # GITIGNORED packaged output (the .exe)
│   └── docs/                      # this file + design notes
├── M1SOURCE/Marathon-20250829/    # GITIGNORED Bungie IP (Aleph One M1 release)
│   ├── Map.scen  Shapes.shps  Sounds.sndz  Physics.phys  Marathon.appl
└── M2SOURCE/ MISOURCE/            # GITIGNORED M2/Infinity reference
```

**Never commit** `M1SOURCE/`, `M2SOURCE/`, `MISOURCE/`, `Reference/`, `Build/`,
`Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`. The first two are
Bungie IP; the rest are regenerable. `Content/` **is** committed (derived dev
assets) — that is the established pattern.

---

## 3. Environment & prerequisites

> ⚠ **Two machines seen so far — paths differ.** The original author's box (per
> the rows below) put UE on `I:\`. The current box (Windows 10) has UE at
> **`C:\Program Files\Epic Games\UE_5.7`** and the project `.venv` is **broken**
> (its shim points at a missing `C:\Users\dusti\...python.exe`). On the current
> box: editor python = `C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\ThirdParty\Python3\Win64\python.exe`;
> for PNG/measurement work, system `python` (3.12, `py -3`) has `py-marathon-utils`
> and `pip install pillow numpy` covers the rest. Adjust the `I:\` rows below to
> whichever box you're on.

| Thing | Original author's box | Notes |
|-------|-------------|-------|
| Unreal Engine | **5.7.4** at `I:\Epic Games\UE_5.7` | Editor-Cmd + RunUAT live here (current box: `C:\Program Files\Epic Games\UE_5.7`) |
| Project python (extract/render) | `C:\m1-ue5\.venv\Scripts\python.exe` | has **py-marathon-utils 0.1.0** + Pillow (broken on current box — use system python) |
| Editor python (header/asset gen, UE import) | `I:\Epic Games\UE_5.7\Engine\Binaries\ThirdParty\Python3\Win64\python.exe` | Py 3.11; handles Windows paths |
| Shell | PowerShell (primary) + bash | Win 11 (author) / Win 10 (current) |

**Pitfalls when porting to a new machine:**
- All tool paths are currently **hard-coded absolute** (`C:\m1-ue5\...`,
  `I:\Epic Games\UE_5.7\...`) in the `Tools/*.py` scripts and the build command.
  Grep + update them for a new layout. (For HUD tuning above I compiled the
  `M1UE5` game target directly with `Engine\Build\BatchFiles\Build.bat` and ran
  the editor's `UnrealEditor.exe <proj> <map> -game` — neither needs the cooked
  `.exe` nor the Tools scripts, so the `I:\` hard-coding didn't block it.)
- **`py-marathon-utils`** is the whole extraction backbone
  (`pip install py-marathon-utils[images]`, or `-r Tools/requirements.txt`).
  Published at https://github.com/dmang-dev/py-marathon-utils.
- Avoid the MSYS python (`c:\devkitPro\msys2`) for anything the editor consumes —
  it mangles Windows paths to `/c/...` and breaks import manifests.
- The `*SOURCE/` folders must be present (Bungie IP, not in git) for any
  extraction step. Without them you can still build from committed `Content/`.

---

## 4. Asset pipeline (Tools/)

Two stages everywhere: a **gen** script (project venv, reads `M1SOURCE` /
`Reference`, emits PNGs + a JSON manifest or a generated `.h` table) → an
**import/editor** script (UE Editor-Cmd python, imports to `/Game/...`). Generated
`*Table.h` headers are committed; the PNGs they reference are gitignored.

| Script | Stage | Produces |
|--------|-------|----------|
| `extract_reference.py` | venv | `Reference/` maps, sounds, physics, shapes |
| `scen_to_obj.py` | venv | per-level textured OBJ + `*_placements.json` (geometry) |
| `ue5_headless_build.py` | editor | imports level OBJs → procedural sector meshes |
| `gen_monster_assets.py` → `render_sprites.py` → `ue5_import_sprites.py` | both | enemy/weapon sprites → `/Game/Sprites` + `M1SpawnTable.h`, `M1WeaponSpriteTable.h` |
| `gen_hud_assets.py` | both | `M1HudTable.h` + HUD icon sprites |
| `gen_terminal_assets.py` → `ue5_import_terminals.py` | both | `M1TerminalTable.h` + page art → `/Game/UI/Terminals` |
| `gen_chapter_assets.py` → `ue5_import_chapters.py` | both | chapter splashes → `/Game/UI/Chapters` |
| `gen_platform_assets.py` | venv | `M1PlatformTable.h` (doors) |
| `gen_panel_assets.py` | venv | `M1PanelTable.h` (chargers + save buffers + oxygen rechargers) |
| `gen_level_env.py` | venv | `M1LevelEnvTable.h` (per-level `song_index` + vacuum flag from Minf) |
| `gen_music_assets.py` → `ue5_import_music.py` | both | 16 M1 OGG→WAV → looping SoundWaves `/Game/Audio/Music/A_music_NN` |
| `ue5_import_vfx.py` | editor | procedural muzzle-flash sprite → `/Game/Sprites/VFX/T_muzzle_flash` (source PNG generated, no M1 extraction) |
| `ue5_import_menu.py` / `ue5_import_hud_bezel.py` | editor | `/Game/UI/Menu/T_menu_main`, `/Game/UI/HUD/T_hud_bezel` |
| `ue5_mrq_render.py` | editor | top-down / FP debug renders (QA) |

**Headless editor invocation** (PowerShell call operator, NOT `cmd /c`):
```
& "I:\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "C:\m1-ue5\M1UE5\M1UE5.uproject" -ExecutePythonScript="<script>" `
  -RenderOffScreen -unattended -nosplash -nopause -stdout
```
(`-RenderOffScreen` is required for asset imports; exit 5 / `-1073741819` on
teardown is a benign editor-shutdown crash, not a failure.)

---

## 5. Source architecture (`Source/M1UE5/`)

**Gameplay actors/components**
- `AM1Character` — first-person pawn: WASD/mouse/jump, weapon, shields (`Health`,
  default 150, no regen). Owns level-order table `GM1Levels[37]` + the static
  campaign helpers (`M1LevelPath`, `M1LevelIndexForMap`, `M1Write/ReadCheckpoint`)
  and the bezel `SceneCaptureComponent2D` → `SceneRT`. `[`/`]` cycle levels.
- `AM1GameMode` — spawns the pawn + `SpawnMonsters()` + `SpawnDoors()` +
  `SpawnPanels()` from the generated tables on `BeginPlay`.
- `AM1PlayerController` — Slate menus (no UMG): bitmap main menu
  (`BuildMainMenuScreen`, hotspots over `T_menu_main`), pause menu, chapter
  splash (`MaybeShowChapterScreen`), New Game / Continue / Quit.
- `AM1HUD` — Canvas HUD. `DrawFramedBezel` composites `SceneRT` into the
  PICT-128 hole, frame on top, shield/oxygen meters + ammo; **falls back** to
  `DrawInterfacePanel` (fullscreen bottom strip) if bezel/RT missing.
- `UM1WeaponComponent` — 7 weapons from `Physics.phys`, primary + secondary
  triggers, hitscan + `AM1Projectile`.
- `AM1Enemy` — Pfhor sprite billboards, 8-dir view frames, melee + ranged AI.
- `AM1Projectile` — bolts/rockets/grenades (radial/point damage).
- `AM1Platform` — movable door panel (open on proximity, auto-close).
- `AM1Panel` — wall control panel (shield charger / pattern-buffer save), used
  via `AM1Character::OnUse`.
- `UM1SaveGame` — checkpoint (level index + shields).
- `MenuGameMode` — spectator pawn for `L_MainMenu`.
- `M1Tuning.h` (constants), `M1Audio.h` (`M1DefaultAttenuation()`).

**Generated tables (committed, do not hand-edit):** `M1SpawnTable.h`,
`M1MonsterTable.h`, `M1WeaponSpriteTable.h`, `M1HudTable.h`, `M1TerminalTable.h`,
`M1PlatformTable.h`, `M1PanelTable.h`. Each is `gen_*.py` output with a
`M1<Thing>ForLevel(MapName)` lookup; empty levels carry a sentinel row
(C++ forbids zero-size arrays) excluded by `Count`.

---

## 6. System status

| System | State | Notes |
|--------|-------|-------|
| Extraction / converter / headless import | ✅ | all 37 levels, 0 errors |
| Sprites (enemies, weapons, HUD icons) | ✅ | CLUT-correct, index-0 transparent |
| Audio | ✅ v1 | 104 sounds + **16 looping level-music tracks**, played per level by the canonical **`Minf.song_index`** (`M1LevelEnvTable.h` → `AM1GameMode::PlayLevelMusic`); door open/close SFX wired; platform-move loop SFX TODO |
| Weapons | ✅ v1 | 7 + secondaries; **muzzle-flash overlay** + **impact sparks** (`AM1Impact`, emissive engine-sphere pop on hitscan/projectile hits). Persistent scorch decals still TODO; debug hit spheres/lines now `#if WITH_EDITOR` |
| Enemies | ✅ v1 | 2232 placements, ranged + 8-dir |
| Terminals (lore) | ✅ | readable pages, paging |
| Main menu + chapter screens | ✅ | authentic bitmaps |
| **Doors** (#11) | ✅ v1 | 473 narrow authored-open doors as panels |
| **Save / Continue** (#12) | ✅ | shield carry-over, menu Continue |
| **Control panels** (#13) | ✅ v1 | 42 chargers + 48 save buffers (press E) |
| **HUD bezel** (#14) | ✅ | PICT-128 framed view; meter rects verified in-engine. **Oxygen gauge dynamic + drains in vacuum** (`AM1Character::Oxygen`, drain in `Tick` on vacuum levels) |
| Polish (#15) | ✅ v1 | music (song_index), muzzle flash + impact sparks, dynamic O2 meter + **vacuum drain & oxygen rechargers**, **exit-terminal progression** (data-flagged), **epilogue**, movement-param consolidation. TODO: persistent decals, platform-move SFX, movement feel pass, O2 checkpoint persistence |

**Deferred / known follow-ups** (tracked in memory + commit bodies):
- Doors: authored-**closed** doors (need converter floor-rebake), **ride-on
  elevators** (floor itself must move), **switch→door** wiring (panel types
  4/5/7 extracted but inert), door panel **textures** (plain box now).
- Campaign exit terminal: finishing a level's exit terminal calls
  `AM1Character::AdvanceLevel` (last level → `AM1PlayerController::ShowEpilogue`);
  `[`/`]` cycling kept as a fallback. M1 carries **no teleport directive** in the
  data (verified: terminal scripts have only logon/information/checkpoint
  groupings), so the exit is identified by a **`bExit` flag** the terminal
  generator sets from teleport-action TEXT ("teleport now", "teleporter pad", …;
  7 levels), falling back to the most-paged terminal. A per-level curation pass
  would make the other ~14 levels exact.
- Oxygen: drains on **vacuum levels only** (`Minf` env bit 0 — in M1 just L08
  "G4 Sunbathing"); refill at oxygen rechargers (panel Kind 2, 3 on L08); 0 O2 →
  suffocation damage. NOT yet checkpointed (`M1SaveGame.Oxygen` is a follow-up).
  `M1SetOxygen <v>` console exec exercises the gauge.
- Movement: params consolidated in `M1Tuning.h` (`GWalkSpeed` etc.); a feel pass
  may lower `GWalkSpeed` (600) toward Marathon's canonical ~430 cm/s.

---

## 7. Build & run

Packaged build (cooks `L_MainMenu` + the 37 campaign levels):
```
& "I:\Epic Games\UE_5.7\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun `
  -project="C:\m1-ue5\M1UE5\M1UE5.uproject" `
  -noP4 -platform=Win64 -clientconfig=Development `
  -cook "-map=<+joined leaf map names>" -stage -pak `
  -archive -archivedirectory="C:\m1-ue5\M1UE5\Build" -build -utf8output -unattended
```
- Map list = `L_MainMenu` + every `L_NN_*` leaf (from `Content/Levels/M1`).
- Success = exit 0 + `BUILD SUCCESSFUL` + 0 compile errors.
- Output: **`C:\m1-ue5\M1UE5\Build\Windows\M1UE5.exe`** (double-click to play).
- Assets ship via **IoStore** (`.utoc`/`.ucas`), so the `.pak` is nearly empty —
  to verify an asset cooked, check `Saved/Cooked/.../Content/...`, not the pak.
- Close the editor before packaging (Live Coding locks). Don't pass
  `-nocompileeditor`.
- ⚠ **Editor target build needs the .NET Framework SDK (≥4.6).** On a box without
  it, `BuildCookRun -build` dies with `Unable to instantiate module 'SwarmInterface'
  ... Could not find NetFxSDK install dir` (RulesError) while compiling the
  **editor** target it uses to run the cook commandlet. Install the .NET Framework
  4.x Developer Pack to do a full repackage there. The **game** target compiles
  fine without it (`Engine\Build\BatchFiles\Build.bat M1UE5 Win64 Development`).
- **C++-only change shortcut (no asset edits → no re-cook needed):** the shipped
  HUD/gameplay logic lives in the monolithic game binary, not in cooked assets.
  So after a code-only change you can skip the full (editor-dependent) repackage:
  rebuild the game target, then copy the fresh `Binaries\Win64\M1UE5.exe` over the
  staged `Build\Windows\M1UE5\Binaries\Win64\M1UE5.exe`. The existing cooked
  `Content\Paks\*.utoc/.ucas/.pak` stay valid (same engine version, unchanged
  assets). Used this for the HUD-meter commit; verified the packaged build boots
  on Arrival and renders the tuned bezel. (Do a real BuildCookRun once assets
  change or the .NET SDK is available.)
- Running a build for HUD/QA without the cooked `.exe`: `UnrealEditor.exe <proj>
  <map> -game -windowed -ResX=1280 -ResY=720` runs uncooked from `Content/` — fast
  for iterating C++ (see the HUD-capture gotchas in §8).

**Coordinate / data conventions** (constant across the pipeline):
- M1 unit → UE cm: `S = 100/1024`. Final imported frame: `X = mx*S`, `Y = my*S`
  (+y), `Z = height*S`. Facing: M1 512 units = 360°.
- Sounds: id = literal Mac `'snd '` id; asset `/Game/Audio/<Family>/A_<name>`
  (family folder is cosmetic). Map in `Tools/m1_sound_names.json`.
- Sprites: palette index 0 is transparent (M1 predates the 0x4000 flag — force
  `bm.transparent=True` in `render_sprites.py`).
- `GetMapName()` is PIE-prefixed in-editor (`UEDPIE_0_L_00_Arrival`); all lookups
  match by `Contains`/`EndsWith` the clean stem.

---

## 8. Gotchas / lessons (carried)

- **PowerShell path guard / hooks** block: combining `git` with `Remove-Item` or
  an `I:\Epic` launch in one command; `sleep` loops in the orchestrator; writing
  source via bash `>` redirect; `>=` inside an inline `python -c` (reads as
  redirect). Split commands; use Write/Edit for source.
- **FVector_NetQuantize ternary** won't compile — wrap `Hit.ImpactPoint` in
  `FVector(...)`.
- **Coll_10 is title/menu art only** — there is NO in-game status bar there; the
  real in-game HUD frame is **PICT 128 in `Marathon.appl`** (now used).
- M1 full-screen PICTs split across forks: **chapter screens in `Shapes.shps`**
  (`id = 10000 + level`), **HUD/title/credits in `Marathon.appl`** (128 / 1000 /
  1111 / 1114). Decode with `marathon_utils.images.extract`.
- M1 **control panels** = `SIDS` sides with flag `0x2`; `panel_type`: 0 oxygen,
  1/2/3 shield 1x/2x/3x, 4 light-switch, 5 platform-switch, 6 pattern-buffer,
  7 tag-switch, 8 terminal.
- M1 **platforms** = narrow `plat`-chunk polygons whose floor rises to seal a
  passage (door); `min/max_height = -1` means "use native polygon heights".
- **Capturing the HUD for QA:** the framed bezel only draws AFTER the chapter
  splash is dismissed. On level entry `AM1PlayerController::MaybeShowChapterScreen`
  puts a full-viewport "click to continue" Slate button up (`FInputModeUIOnly`,
  game paused). A screenshot taken before clicking shows the splash (a dark scene
  + fullscreen gun), NOT the HUD — this looks like a bezel failure but isn't.
  Click anywhere (or it has keyboard focus for Enter/Space) to reveal the HUD.
- **`HighResShot` does NOT capture the Canvas HUD** (`AM1HUD::DrawHUD`) — it grabs
  only the 3D scene. To screenshot the HUD headlessly, run
  `UnrealEditor.exe <proj> <map> -game -windowed -ResX=1280 -ResY=720`, then GDI
  CopyFromScreen the window client rect (PowerShell + System.Drawing). Foreground
  the window first via `(New-Object -ComObject WScript.Shell).AppActivate($pid)`
  (`SetForegroundWindow` alone is blocked by Windows' foreground lock). The
  computer-use `request_access` resolver only matches Start-menu apps, so it can't
  target the running game window — GDI capture is the working path.

---

## 9. Recent commit arc (newest first)

```
e89ba40 [HUD] Authentic PICT 128 bezel framing a 4:3 viewport
fa7ede0 [Control panels] Shield rechargers + pattern-buffer save stations
1b9fc9f [Campaign] Save/Continue checkpoint with shield carry-over
ff9a406 [Interactables] Marathon doors as movable panels
b45021f [Menu] Marathon chapter screens on level entry
4ff7222 [Menu] Authentic M1 title-screen bitmap as the main menu
```
Commit format `[System] description`; author `dmang-dev` (no PII — public repo);
end bodies with `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`.

## 10. Immediate next step

The **polish tier (#15)** is done v1: faithful music (song_index), muzzle flash +
impact sparks, vacuum oxygen (drain + rechargers + suffocation), exit-terminal
progression + epilogue, movement-param consolidation — built and the high-value
ones verified in-engine (music log = track 7 on Arrival; O2 drains full→~40% over
~40s on L08). `Build\Windows` is current via an exe-swap (all changes were
code-only — no re-cook).

Remaining, in rough priority (see §6 deferred):
1. **Persistent impact decals** (scorch/blood) — needs a decal material; sparks
   are the v1 stand-in.
2. **Oxygen checkpoint persistence** (`M1SaveGame.Oxygen`) + tune
   `GOxygenDrainPerSec` (L08 currently empties in ~75–100s).
3. **Movement feel pass** — lower `GWalkSpeed` (600) toward canonical ~430 cm/s.
4. **Door follow-ups** — closed doors / ride-on elevators / switch→door wiring.
5. **Platform-move loop SFX**; per-level exit-terminal curation for the ~14
   levels without a teleport-action phrase.
