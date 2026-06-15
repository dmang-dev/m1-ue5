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
  splash (`MaybeShowChapterScreen`), New Game / Continue / Quit. **Pause menu also
  has Save Game / Restore Game (`M1SaveFull`/`M1LoadFull`) + Preferences**
  (`BuildPreferencesMenu` / `ShowPreferences`: mouse sensitivity + invert look,
  persisted to the `M1Settings` slot via `UM1SettingsSave`, consumed by
  `AM1Character::Turn/LookUp`; look is frozen while a terminal is open).
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
| Enemies | ✅ v1 | 2232 placements, ranged + 8-dir; **attack-pose animation** (Stand/Walk/Dead/**Attack** sprite states from each monster's melee/ranged attack shape; `MulticastAttackAnim` plays it on server+clients during `PerformAttack`) |
| Terminals (lore) | ✅ | readable pages, paging |
| Main menu + chapter screens | ✅ | authentic bitmaps |
| **Doors** (#11) | ✅ v1 | 473 door panels; **switch→door wiring** (119 switches); **ride-on elevators** (35 wide platforms via `AM1Elevator` — movable floor slab, rises to neighbour floor, carries the rider). All harness-verified (switch target 244 opened door poly 244; player rode elevator 100→200) |
| **Save / Continue** (#12) | ✅ | shield carry-over, menu Continue |
| **Control panels** (#13) | ✅ v1 | 42 chargers + 48 save buffers (press E) |
| **HUD bezel** (#14) | ✅ | PICT-128 framed view; meter rects verified in-engine. **Oxygen gauge dynamic + drains in vacuum** (`AM1Character::Oxygen`, drain in `Tick` on vacuum levels) |
| Polish (#15) | ✅ v1 | music (song_index), muzzle flash + impact sparks, dynamic O2 meter + **vacuum drain & oxygen rechargers**, **exit-terminal progression** (data-flagged), **epilogue**, movement-param consolidation. TODO: persistent decals, platform-move SFX, movement feel pass, O2 checkpoint persistence |

**Deferred / known follow-ups** (tracked in memory + commit bodies):
- Doors: **switch→door** + **ride-on elevators** done (v1). Elevators are a runtime
  movable slab (`AM1Elevator`, engine box, no geometry rebake) that rises to the
  highest neighbour floor and carries the rider; the baked static floor still sits
  at the slab's rest height (a faithful version would author the floor as a movable
  mesh in the converter, dropping the static one). Remaining: authored-**closed**
  doors + the **11 closed-platform** polygons (need the converter floor-rebake),
  **light-switch** (type 4; no lights), door/elevator **textures** (plain box now).
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

## 9b. Test harness (drive + verify gameplay headlessly)

`AM1Character` exposes `Exec` console commands (Development only) so a headless
run can DRIVE and INSPECT gameplay and verify features by reading the log — no
manual navigation. Console commands inject reliably (open `~`, type, Enter);
in-game key binds via SendInput are flaky, so route through these instead.

- `M1Where` — pos / level / shields / oxygen / vacuum flag
- `M1Doors` / `M1Panels` / `M1Terms` — list spawned doors (open state), panels
  (kind), terminals (id, pages, which is the exit)
- `M1Goto X Y Z` — teleport
- `M1Activate N` — fire the Nth control panel (bypasses proximity)
- `M1OpenTerm N` + `M1Use` (repeat) — open a terminal and page it; finishing the
  exit terminal calls `AdvanceLevel`
- `M1Elevators` — list ride-on lifts (pos, travel, state)
- `M1SetOxygen V` — set the gauge
- `M1SelfTest` — **end-to-end per-level check**: logs spawn counts (monsters /
  doors / elevators / panels / terminals), exit terminal, player+oxygen state,
  and a `PASS/FAIL`. Sweep all levels by launching each and running it (verified
  PASS on L00/L07/L08/L36; L08 shows `vac=1` oxygen draining).

Verified via this harness: exit-terminal → `AdvanceLevel 0→1` (shields carry,
L_01 plays its song_index track 8); vacuum O2 drain on L08 + oxygen-recharger
refill (19→99). Driving recipe: launch the packaged exe windowed on a map, click
to dismiss the chapter splash, then inject `~`-console commands and read
`Build\Windows\M1UE5\Saved\Logs\M1UE5.log`.

## 9c. Side-by-side fidelity vs Classic Marathon

`M1SOURCE\Marathon-20250829\Classic Marathon.exe` is the bundled Aleph One
Windows build of the ORIGINAL game — run it for ground-truth fidelity checks.
Recipe (PowerShell GDI, like the §9b harness): `Start-Process "Classic
Marathon.exe"` → click through the intro → the main menu renders right-of-centre
in the 1536×864 window (BEGIN NEW GAME ≈ (945,408)) → ARRIVAL teleport screen →
first-person HUD. Full-screen `CopyFromScreen` captures it. Then capture the same
spot in the UE5 build and diff/compose with PIL.

First finding it caught: **the M1 HUD meters are OXYGEN = blue (left well),
SHIELDS = red (right well)** — the remake had them green/cyan. Fixed in
`M1HUD.cpp` (sampled originals: oxygen ~RGB(0,0,200), shields ~RGB(200,0,0)).
Saved reference shots live in `Saved\classic_*.png` (Bungie IP — gitignored).

## 10. Immediate next step

The **polish tier (#15)** is done v1: faithful music (song_index), muzzle flash +
impact sparks, vacuum oxygen (drain + rechargers + suffocation), exit-terminal
progression + epilogue, movement-param consolidation — built and the high-value
ones verified in-engine (music log = track 7 on Arrival; O2 drains full→~40% over
~40s on L08). `Build\Windows` is current via an exe-swap (all changes were
code-only — no re-cook).

Done since: movement tuned to canonical `GWalkSpeed`=430; vacuum drain softened
to `GOxygenDrainPerSec`=0.5 (oxygen intentionally resets per level → not
checkpointed, matching M1's per-teleport suit reset).

Remaining — these are larger / not cleanly doable headless, hence deferred:
1. **Persistent scorch decals** — `Tools/ue5_import_decal.py` + `T_scorch_decal`
   exist, but the deferred-decal material **won't compile in the headless
   (`-RenderOffScreen`) editor** ("Default Material will be used"); it needs the
   material authored/fixed in the GUI editor (or a cook-time compile check). The
   impact **spark** (`AM1Impact`) is the working stand-in.
2. **Switch→door wiring** — DONE (types 5/7 force-open their target door(s);
   verified via the §9b harness). Optional refinement: make switch-targeted doors
   switch-*only* (disable proximity) where the M1 `plat` flags say so — needs
   playtest to avoid soft-locks.
3. **Geometry pipeline — DONE.** Closed-door rebake (11 polys reopened to walkable
   passages) + **faithful elevators** (`scen_to_obj.py` drops the baked floor for
   elevator polys so the `AM1Elevator` slab IS the floor; drop ⟺ slab via the
   shared detection). Regenerated all 37 OBJs → re-imported (37/37, 0 errors) →
   cooked. **Full 37-level `M1SelfTest` sweep: 37/37 PASS** (every level grounded,
   weapon + exit-terminal present, all systems spawned, no holes).
   **Cosmetic niceties — DONE.** Doors/elevators now wear their M1 floor_shape
   texture (`tex_<coll>_<shape>` from the shared library) instead of the grey
   engine box — generators emit TexColl/TexShape, the actors SetMaterial on init;
   verified via `M1Doors` reporting `mat=tex_18_18 / tex_18_10 / tex_18_7` on
   Arrival. **Light switches** (panel_type 4) now spawn as interactive Kind-5
   panels (`M1Panels` shows `kind=light-switch`, 14 across the campaign); they
   click + message but do NOT toggle lights — the build uses one global
   DirectionalLight, so a per-room/per-LITE point-light system is the prerequisite
   for real light switching (deferred, see #6).
4. **Per-level exit-terminal curation** (~14 levels with no teleport-action
   phrase) — needs Marathon walkthrough knowledge per level; manual research.
5. **Platform-move loop SFX** — moot until ride-on elevators exist; doors already
   play open/close one-shots that cover their motion.
6. **Marathon lighting — DONE via PER-SURFACE brightness (all 37, shipped).** The
   point-light approach (`AM1Light` + dimmed directional/fill) was abandoned: it
   made hotspots AND black gaps (the user saw "too bright" + "black corridors" at
   once), which no tuning fixes. Marathon lights each *surface* flat at its LITE
   level (Doom sector lighting), so that's what we bake now:
   `scen_to_obj.py` writes each floor/ceiling/wall material as `tex_C_S_b<pct>`
   (from POLY `floor_light`/`ceiling_light`; walls use the poly's floor_light),
   curve 0→35%, 1.0→100%; `ue5_headless_build.py` emissive mode scales the emissive
   by `<pct>` (screens/coll-8 stay full so CRTs glow). Structurally can't wash out
   (emissive ≤ texture) or go pure black (≥35%), needs zero dynamic lights.
   `AM1GameMode::SpawnLights` now only locks **manual exposure** (no eye-adaptation
   to undo the baked mood). Doors/elevators load the bucketed material matching
   their room (`FM1Door/FM1Elevator.TexBucket`). Rolled out: regen all 37 OBJs →
   full emissive rebake (881 materials) → recook into `Build\Windows`. Verified:
   L_00 + L_06 `M1SelfTest` PASS, start rooms moody+readable, dim rooms dim-not-black.
   `AM1Light`/`M1LightTable`/`gen_light_assets.py` are RETAINED (unused) for a future
   MPC-based switchable-light system. REMAINING (Phase 2): functional light switches
   (per-LITE-channel brightness via a Material Parameter Collection so Kind-5 toggles
   work again — currently cosmetic); animated `type`!=0 lights (flicker/strobe);
   ceiling-only lights (`ceiling_light` ≠ `floor_light`, ~20% of polys).
7. **Motion sensor / radar — DONE (Phase 1).** `AM1HUD::DrawMotionSensor` fills the
   bezel's bottom-left oval: green grid (3 rings + crosshair) + monster blips,
   player-centered and rotated so view-forward is up; red Pfhor / amber Bobs,
   brighter when active; green player ping. `AM1Enemy::IsAlive/IsActive/IsFriendly`
   feed it; `M1Radar` Exec logs in-range counts. Range 5000 cm.

## 11. Finish roadmap (4 phases, agreed)

1. **HUD & feel** — motion sensor ✅; remaining: bezel-native ammo/weapon readout,
   movement-feel pass.
2. **Lighting interactivity** — functional light switches (MPC), animated lights,
   ceiling-only lights.
3. **Fidelity polish** — per-level exit-terminal curation; platform-move SFX; O2
   checkpoint persistence; scorch decals (GUI material); enemy-AI depth; media/liquid
   damage; ambient poly sounds.
4. **Distribution & release** — implement the runtime-loading architecture
   (`docs/INSTALLER_DESIGN.md`) so the game runs from converted intermediates with no
   UE/cook; Inno installer + Asset Setup wizard; begin original-art replacement
   (license requirement before public release).
   - **Phase 0 (runtime loader) DONE + verified.** `AM1RuntimeLevel` builds a level's
     geometry at runtime from OBJ/PNG (no cook): `M1Runtime <stem>` exec /
     `M1_RUNTIME_SELFTEST` hook. Packaged self-test: Arrival = 92 sections / 2738
     tris, PASS. Decisions locked: per-user **AppData** content cache, **locate-only**
     data (no bundled downloader).
   - **Phase 1 (all 37 via runtime path) DONE (core) + verified.** Blank persistent
     `L_Runtime` map + runtime-mode statics (`GRuntimeLevelName`, `M1LevelKey`); enter
     via `M1RunLevel <idx>` exec / `M1_RUNLEVEL` env; `AM1GameMode::BuildRuntimeGeometry`
     builds geometry before table-driven actors spawn; `ResolveGeoBasename` matches the
     OBJ by `NN_` prefix (cooked vs OBJ name sanitization differ); player teleports to
     `placements.player_start` on Tick 1; checkpoints unchanged (index-keyed). Packaged
     sweep built 00/05/13/20/28/36 at runtime, no crash.
   - **Phase 2 (Asset Setup wizard + per-user cache) DONE (core) + verified.**
     `Tools/m1_asset_setup.py` (locate-only + SHA-256 + `extract_reference --shapes` +
     `scen_to_obj --all` -> `%LOCALAPPDATA%\M1UE5\Data` + `.m1_assets_done` manifest/skip
     marker, `@PROGRESS` lines). `AM1RuntimeLevel::ResolveDataRoot` prefers that AppData
     cache. Frozen via `Tools/m1_asset_setup.spec` (PyInstaller `--onedir`, bundles
     py-marathon-utils + Pillow + soundfile/libsndfile) -> `M1AssetSetup.exe`. VERIFIED:
     wizard converted M1SOURCE -> AppData cache (37 levels + manifest); the packaged game
     built Arrival at runtime FROM the AppData cache (no dev `Reference/`, no env).
     **NEXT = Phase 3: Inno `.iss` (EULA gate + run `M1AssetSetup.exe` during setup +
     bundle UE redists) + a "data not found, run setup" in-game message; plus the WAV
     runtime loader and flipping runtime mode to the campaign default.**

> **MULTIPLAYER PLAYTEST — TABLED (do later).** The networked *gameplay* (PvP
> fragging, KOTH hill / Tag ball scoring accumulating, the scoreboard widget
> visually, teammate bodies, doors opening for the client) can't be script-verified
> here (dual paused splash screens, precise aim, two live players). Everything
> compiles, boots in net mode, and ticks without crashing (KOTH even ran its full
> hill-time -> ScoreLimit -> round-restart loop solo). When ready, playtest a mode:
> `M1UE5.exe /Game/Levels/M1/L_30_Arena?game=/Script/M1UE5.<Mode>?listen` + a second
> `M1UE5.exe 127.0.0.1`, where `<Mode>` =
> `M1DeathmatchGameMode` (Carnage) / `M1TeamGameMode` (team) / `M1KOTHGameMode` /
> `M1TagGameMode`. Check: teammate body sprites + team colors, friendly-fire off,
> hill/ball scoring on the scoreboard, ball drop on death.

## 12. Net / Save / Replay track (UE-native, agreed)

These three M1 features map to mature UE systems (NOT a deterministic-lockstep
rewrite — the project is a modern UE5 FPS). Decision + order confirmed by the user.

1. **Save-anywhere — DONE.** Full world-state snapshot (pattern buffer): player
   pose/shields/oxygen/loadout + every living enemy (dead stay dead). `UM1SaveGame`
   full-state fields; `AM1Character::M1SaveFull` / `M1LoadFull` (Exec `M1Save`/`M1Load`,
   + pattern buffers); restore applies player state on the first Tick and respawns
   the saved enemies via `AM1GameMode::SpawnMonsters`. Harness-verified. FOLLOW-UP:
   wire a menu "Restore Game" entry to `M1LoadFull`; optionally save door/platform/
   light states (currently transient/auto).
2. **Replication foundation + co-op — DONE (core).** Verified with a real
   listen-server + client (`M1UE5.exe <map>?listen` + `M1UE5.exe 127.0.0.1`, separate
   `-LOG` files): both instances stay alive (the earlier "crash" was the test's
   `Stop-Process -Force` closing both windows at once), client is `Welcomed by server`,
   `Join succeeded`, and `M1Net` (new Exec) reports `players=2`.
   - (2a) `AM1Character` replicates: bReplicates + ReplicateMovement (ACharacter
     movement auto-replicates) + Replicated Health/Oxygen.
   - (2b) `AM1Enemy` replicates; AI runs server-only (HasAuthority); clients render
     from replicated mirrors RepState/RepFacingYaw (no more client double-sim).
   - (2c) `AM1Platform`/`AM1Elevator` replicate; state machines authority-gated;
     PlayerNear/PlayerOn check every connected player.
   - (2d) Visible co-op body (bOwnerNoSee placeholder box) + server-authoritative
     weapon fire (client routes OnFire/etc. through Server RPCs).
   REMAINING POLISH (needs interactive 2-player playtest): client-side fire feedback
   (muzzle-flash/ammo HUD replication), per-player spawn points, multicast door/lift
   SFX, player sprite billboard instead of the box. NOTE: verification needs TWO
   instances, not the single-instance console harness.
3. **Record/replay — DONE.** `M1Rec [name]` / `M1RecStop` / `M1Play [name]` wrap
   UGameInstance Start/Stop/PlayReplay (DemoNetDriver); films land in
   Saved/Demos/<name>.replay. Verified single-player: recorded film4351.replay
   (10 KB, "Demo stopped at frame 284"), played back ("ReplayStreamingReady").
   Works because Phase 2 replication feeds the demo driver.
4. **Deathmatch — DONE (Carnage v1).** `AM1DeathmatchGameMode` (free-for-all frags +
   instant respawn + round-robin spawns); players shootable (capsule blocks the
   Visibility trace -> PvP + co-op friendly fire); AM1Character death routes to the
   DM mode (frag/respawn) vs campaign (level reload). Select per-map:
   `...L_30_Arena?game=/Script/M1UE5.M1DeathmatchGameMode?listen`. `M1Frags` logs the
   scoreboard. Verified the mode loads + 2 players connect; PvP fragging needs a
   2-human playtest.
   - **Modes (DONE):** `AM1DeathmatchGameMode` is now a subclassable base (replicated
     APlayerState::Score, ScoreLimit=10 -> EndRound announce + ServerTravel restart,
     virtual AwardKill + ScoreTick). `AM1KOTHGameMode` (score by time on the hill =
     PlayerStarts centroid r=500; verified ScoreTick runs) and `AM1TagGameMode` (Kill
     the Man with the Ball: frag to grab it, carrier scores/sec). Select via
     `?game=/Script/M1UE5.M1KOTHGameMode` / `M1TagGameMode`.
   - **Scoreboard (DONE):** scores on the replicated APlayerState::Score; `M1Frags`
     reads GameState->PlayerArray (clients too); `AM1HUD::DrawScoreboard` draws a
     top-right board in net sessions, **each row tinted by the player's team/FFA color**.
   - **Teams (DONE).** `AM1PlayerState` adds a replicated `TeamNum` (-1 = FFA) +
     `GetPlayerColor()` (4 team colors, else a stable per-id hue). `AM1DeathmatchGameMode`
     now sets `PlayerStateClass`, exposes `NumTeams`, and assigns teams round-robin in
     `OnPostLogin`. `AM1TeamGameMode` (NumTeams=2, "TEAM CARNAGE") is the team variant.
     `AM1Character::TakeDamage` skips same-team hits (friendly fire off; self/environment
     still hurt). `AM1GameMode` (campaign/co-op) also uses `AM1PlayerState` so co-op
     bodies are color-distinct.
   - **Authored hill / ball actors (DONE).** `AM1Hill` (visible gold disc, `Radius`) —
     KOTH uses an authored one if placed, else **spawns one** at the PlayerStarts
     centroid and scores by time inside it. `AM1Ball` (visible floating skull) — Tag
     **grab-by-touch**, carrier scores/sec, and the ball **DROPS where the carrier dies**
     (neutral, anyone can grab) instead of auto-transferring. Both replicate.
   - **Per-player body colors + real player sprite (DONE).** The visible body is now a
     **camera-facing billboard of the Coll_06 "stationary" Marathon figure** (5 unique
     views + mirror = 8 dirs, same machinery as `AM1Enemy`), tinted by team/FFA color.
     Assets: `Tools/ue5_import_player.py` imports `/Game/Sprites/Player/T_player_v0..4`
     + the tintable `M_M1Player` (Sprite×Tint). `AM1Character` falls back to a colored
     box if those assets aren't cooked (verified: the pre-recook packaged build ran the
     box path without crashing). **Generated (Bungie-derived) → not committed; ships via
     `DirectoriesToAlwaysCook=/Game/Sprites`. RECOOK DONE** — the packaged
     `Build\Windows` now cooks all 38 maps + `T_player_v0..4` + `M_M1Player`, and boots
     clean with the sprite assets loaded (no load errors).
   - **VERIFIED (boot + scoring, single instance):** KOTH/Tag/Team all load
     (`Game class is 'M1*GameMode'`), spawn their hill/ball (`[M1Net] KOTH hill at
     (2125,1988,96) r=500`, `[M1Net] TAG ball at (2125,1988,156)`), no crash. KOTH ran
     the **full scoring loop**: the listen player accrued hill-time to ScoreLimit ->
     `ROUND OVER` -> `ServerTravel` restart. PvP fragging, ball-steal, and the sprite
     *visual* still need the 2-human playtest (the body is bOwnerNoSee).

BUILD GOTCHA (important): on this box the build can silently NO-OP (source mtimes not
seen as newer than the .obj -> "Result: Succeeded" but the exe isn't relinked, so
exe-swap ships stale code). Force-touch edited sources before building, and confirm
`Binaries\Win64\M1UE5.exe` LastWriteTime advanced before exe-swapping.

COOK GOTCHA (destructive): build the `-map=` list with PowerShell-native paths only.
A Bash-tool `/tmp/...` file is NOT readable from the PowerShell tool (`/tmp` resolves
differently), so `$ml = Get-Content /tmp/maps.txt` silently yields `$null` ->
`-map=` (empty) -> the cooker prunes the cooked dir down to ~1 map and the archived
`Build\Windows` ships almost no levels (the run still says BUILD SUCCESSFUL). Generate
the leaf list with `Get-ChildItem` inside the same PowerShell call, assert it's
non-empty, then verify `find Saved\Cooked ... -name "L_*.umap" | wc -l == 38` after.
