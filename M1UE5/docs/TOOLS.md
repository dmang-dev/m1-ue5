# Tools & asset-generation pipeline

Every shippable asset in this project is **generated from the original Marathon 1 data**,
never hand-authored. This doc explains the pipeline end-to-end and every tool in
`M1UE5/Tools/`.

## Prerequisites

- **Python 3.11+** with `pip install -r M1UE5/Tools/requirements.txt`
  ([`py-marathon-utils[images]`](https://github.com/dmang-dev/py-marathon-utils) — the
  published reader/writer for M1/2/Infinity maps, shapes, sounds, physics, terminals,
  images — plus `soundfile` for music OGG→WAV).
- **Unreal Engine 5.7** — `UnrealEditor-Cmd.exe` (for the `ue5_*` editor stages) +
  `Build.bat`/`RunUAT.bat`.
- Your **Marathon 1 data** in `M1SOURCE/Marathon-20250829/` (`Map.scen`, `Shapes.shps`,
  `Sounds.sndz`, `Physics.phys`, and `Music/NN.ogg`).

There are **two tracks** that share the same converters:

- **Dev / cooked build** — generate into `Reference/`, import into `/Game/...` via the
  editor stages, cook a packaged build.
- **Runtime / installer** (the shipped path) — `m1_asset_setup.py` converts the user's
  data into the per-user cache `%LOCALAPPDATA%\M1UE5\Data`, which the game loads at
  runtime with **no editor and no cook** (`AM1RuntimeLevel`).

---

## 1. Extraction & geometry (standalone Python — no UE)

| Tool | In → Out | Notes |
|---|---|---|
| `extract_reference.py` | `M1SOURCE` → `Reference/{Maps,Sounds,Physics,Shapes}` | `--shapes` also dumps sprite/texture PNGs. `--source/--dest` override paths. |
| `scen_to_obj.py` | `Reference/Maps/*.json` → `Reference/Geometry/<NN>_<name>.obj` + `.mtl` + `_placements.json` | One mesh per sector; UVs + per-surface LITE brightness baked into material names (`tex_C_S_b<pct>`); `--all <dir> --out <dir>`. Coordinates already in final UE cm. |

## 2. Generators (standalone Python → committed `*Table.h` + import lists)

These read `Reference/` + `M1SOURCE` and emit **committed** C++ headers (the game reads
these tables) and/or PNG/import lists for the editor stages. Re-run after changing the
source data or a generator.

| Tool | Emits |
|---|---|
| `gen_monster_assets.py` | `M1MonsterTable.h` (30 monster types: stats + Stand/Walk/Dead/Attack sprite grids) + `M1SpawnTable.h` (2232 placements) + the sprite render/import lists |
| `gen_weapon_stats.py` | `M1WeaponTable.h` — the 7-weapon arsenal (damage/fire/mag/range/projectile/secondary + SFX paths), from `Physics.phys` |
| `gen_weapon_sprites.py` | weapon-in-hand viewmodel frame list → `M1WeaponSpriteTable.h` |
| `gen_platform_assets.py` | `M1PlatformTable.h` — doors + ride-on elevators |
| `gen_panel_assets.py` | `M1PanelTable.h` — wall panels (shield chargers, pattern-buffer saves, switches) |
| `gen_light_assets.py` | `M1LightTable.h` — per-level LITE records (index, mode, intensity) |
| `gen_level_env.py` | `M1LevelEnvTable.h` — per-level `Minf` song index + vacuum flag |
| `gen_terminal_assets.py` | `M1TerminalTable.h` (placements, page counts, exit flag) + the terminal page-art import list (renders from `Marathon.appl`) |
| `gen_chapter_assets.py` | chapter-splash PICT import list (`Shapes.shps` id 10000+level) |
| `gen_hud_assets.py` | `M1HudTable.h` + HUD chrome / inventory-icon import list |
| `gen_music_assets.py` | `M1SOURCE/Music/NN.ogg` → `Reference/Music/A_music_NN.wav` (`--source/--dest`) |
| `render_sprites.py` | renders the CLUT-correct, alpha-forced sprite PNGs for the monster/weapon/HUD import lists |
| `gen_original_textures.py` | **clean-room** original wall textures (zero Bungie pixels) for the (coll, shape) IDs a cache uses — for a self-contained build (Phase 4) |

## 3. Editor import stages (run inside `UnrealEditor-Cmd`)

Invoke each as:
```
"<UE>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" M1UE5.uproject \
  -ExecutePythonScript="Tools/<script>.py" -RenderOffScreen -unattended -nosplash -nopause -stdout
```
(Exit 5 / `-1073741819` on teardown is a benign editor-shutdown crash, not a failure.)

| Tool | Builds into `/Game/...` |
|---|---|
| `ue5_headless_build.py` | imports all 37 level OBJs → `StaticMesh` + the shared unlit material library + `.umap`s (the cooked-build geometry) |
| `ue5_import_sprites.py` | monster + weapon + HUD sprite textures + the master `M_M1Sprite` |
| `ue5_import_player.py` | the Coll_06 player-figure billboard textures + tintable `M_M1Player` |
| `ue5_import_sounds.py` / `ue5_import_music.py` | the 104 SFX / 16 music WAVs → `SoundWave`s |
| `ue5_import_terminals.py` | terminal page art → `/Game/UI/Terminals` |
| `ue5_import_chapters.py` / `ue5_import_menu.py` / `ue5_import_hud_bezel.py` | chapter splashes / main-menu PICT / HUD bezel |
| `ue5_import_vfx.py` / `ue5_import_decal.py` | muzzle-flash sprite / scorch-decal texture |
| `ue5_make_runtime_material.py` | the runtime master material `M_M1Runtime` (Sprite×Emissive) |
| `ue5_make_runtime_map.py` | the blank persistent `L_Runtime` map (installer Phase 1) |
| `ue5_make_menu_level.py` / `ue5_import_level.py` / `ue5_setup_player.py` | menu level / single-level import / PlayerStart placement helpers |
| `ue5_mrq_render.py` | QA renders (top-down / first-person) of any level (`M1_LEVEL` env) |

## 4. Runtime / installer (the shipped bring-your-own-assets path)

| Tool | Role |
|---|---|
| `m1_asset_setup.py` | **The Asset Setup wizard.** Locate-only (no download) + SHA-256 verify the 4 data files → `extract_reference --shapes` + `scen_to_obj --all` + music + `sound_names.json` into `%LOCALAPPDATA%\M1UE5\Data`; writes `.m1_assets_done` (manifest + skip marker). Frozen to `M1AssetSetup.exe` via `m1_asset_setup.spec` (PyInstaller). |
| `installer/build_installer.ps1` | Stages the payload, finds `iscc.exe`, compiles `installer/M1UE5.iss` → `Build/Installer/M1UE5-Setup.exe`, prints its SHA-256. |
| `installer/M1UE5.iss` + `EULA.txt` | The Inno Setup installer (per-user/no-admin, EULA gate, locate-data page, runs the converter during setup). See `installer/README.md`. |

`m1_sound_names.json` — canonical `'snd '` id → name map; shipped into the cache so the
runtime WAV loader resolves `/Game/Audio/A_<name>` → `Sounds/<id>.wav`.

---

## End-to-end (from scratch)

```text
# A. Convert the source data (dev tree)
python Tools/extract_reference.py --shapes
python Tools/scen_to_obj.py --all Reference/Maps --out Reference/Geometry
python Tools/gen_monster_assets.py        # + gen_weapon_stats / platform / panel / light / level_env / terminal / chapter / hud / music
python Tools/render_sprites.py

# B. Import into the editor (each via UnrealEditor-Cmd, see above)
ue5_headless_build.py ; ue5_import_sprites.py ; ue5_import_player.py ; ue5_import_sounds.py ;
ue5_import_music.py ; ue5_import_terminals.py ; ue5_import_chapters.py ; ue5_import_menu.py ;
ue5_import_hud_bezel.py ; ue5_import_vfx.py ; ue5_make_runtime_material.py ; ue5_make_runtime_map.py

# C. Build + cook (lean release cook = no Bungie geometry in the pak)
Build.bat M1UE5 Win64 Development -project=M1UE5.uproject
RunUAT.bat BuildCookRun ... -cook "-map=L_MainMenu+L_Runtime" -stage -pak -archive -archivedirectory=Build -skipbuild

# D. (Shipped path) — the player never does A/B/C; the installer's M1AssetSetup.exe runs
#    the converters from A into %LOCALAPPDATA%\M1UE5\Data, and the game builds levels at runtime.
```

> **Gotchas** (full list in `docs/HANDOFF.md`): force-touch source mtimes before
> `Build.bat` (clock-skew no-op); build the cook `-map=` list with PowerShell-native
> paths (a Bash `/tmp` path is unreadable from PowerShell → empty `-map` prunes the cook).
