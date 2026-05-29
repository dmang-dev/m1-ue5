# CLAUDE.md — Marathon UE5 Remake

This file teaches Claude Code the conventions, structure, and constraints for
this project. Read it fully before making any changes.

---

## Project overview

A reimagined first-person shooter remake of Bungie's Marathon 1, built in
Unreal Engine 5. The game preserves Marathon's world, lore, factions, and
spatial language while rebuilding all systems as a modern FPS. Primary
developer is a beginner-to-intermediate UE5 user working primarily in
Blueprints with VS Code as the external editor.

### Working philosophy: automate over hand-build

The developer is not an artist. We **do not** hand-model walls, panels, or
props if there is data in `M1SOURCE/` we can extract automatically. The
workflow is:

1. Extract everything we can from the original M1 data files into
   `Reference/` (geometry, textures, sprites, sounds, physics constants).
2. Auto-convert level geometry from `.scen` into UE5 procedural meshes — one
   per Marathon sector.
3. Enemies render as **sprite billboards** lifted directly from
   `Shapes.shps` — no 3D modeling required for the first playable pass.
4. Hand-build only what cannot be derived: gameplay logic, player feel, UI,
   and the eventual final-art passes.

If you find yourself about to ask the user to hand-model something, stop and
ask whether the data exists in M1SOURCE first.

---

## Tech stack

- **Engine**: Unreal Engine 5 (latest stable)
- **Primary language**: Blueprints (`.uasset`). Introduce C++ only when
  Blueprint hits a hard limit.
- **Editor**: VS Code with the Unreal Engine extension
- **Extraction pipeline**: Python 3.7+ (stdlib only at this stage; Pillow
  will be added when we extract Shapes to PNG)
- **Version control**: Git with `.gitignore` tuned for UE5
- **Target platform**: Windows (PC-first)

---

## Repository layout

```
m1ue5/
├── M1SOURCE/Marathon-20250829/   # Aleph One data — DO NOT MODIFY
│   ├── Map.scen                  # 37 levels (MacBinary + WAD v0)
│   ├── Shapes.shps               # 26 sprite/texture collections (rsrc fork)
│   ├── Sounds.sndz               # 104 'snd ' resources (rsrc fork)
│   ├── Physics.phys              # monster/weapon/projectile constants
│   ├── Marathon.appl             # Aleph One app + embedded resources
│   └── Music/                    # standard audio
└── M1UE5/                        # the UE5 project
    ├── M1UE5.uproject
    ├── CLAUDE.md                 # this file
    ├── Tools/                    # Python tooling for the port
    │   ├── extract_reference.py  # M1SOURCE -> Reference (uses py-marathon-utils)
    │   └── requirements.txt      # py-marathon-utils[images]
    ├── Reference/                # extracted data (regenerable; gitignored)
    │   ├── Maps/                 # per-level JSON: geometry + lights + objs
    │   ├── Sounds/               # WAVs by family (monster/weapon/ambient/…)
    │   └── Physics/              # raw + manifest
    ├── Content/                  # UE5 assets (see folder structure below)
    └── docs/                     # human-readable design notes
```

`Reference/` is **regenerable output**. It is treated as intermediate build
artifacts: gitignored, rebuilt by `Tools/extract_reference.py`. The data itself
is Bungie IP and not redistributable.

---

## Asset extraction pipeline

Extraction/conversion of the M1 data files is handled by the standalone,
published package **[py-marathon-utils](https://github.com/dmang-dev/py-marathon-utils)**
(`pip install py-marathon-utils`). It reads/writes maps, shapes, sounds,
physics, terminals, strings, patches, and Images for Marathon 1 / 2 / Infinity.

```bash
pip install -r Tools/requirements.txt
python Tools/extract_reference.py            # M1SOURCE -> Reference (defaults)
python Tools/extract_reference.py --shapes   # also dump sprite/texture PNGs
```

This produces:

- **`Reference/Sounds/<family>/<id>[_name].wav`** — 16-bit mono PCM, ready to
  drag into UE5's Content Browser
- **`Reference/Maps/<NN>_<name>.json`** — per-level dump of EPNT/LINS/SIDS/
  POLY/LITE/OBJS/Minf, with terminal text when present
- **`Reference/Physics/`** — raw `Physics.phys` + manifest
- **`Reference/Shapes/`** (with `--shapes`) — per-collection sprite/texture PNGs

Sprites and wall textures are available via `--shapes` (and the package's
`shapes`/`terminals`/`images` modules) — no longer a TODO.

---

## Content folder structure (UE5)

All UE5 assets live under `/Content/`. Never place assets in the engine's
root or in plugin folders.

```
/Content
  /Levels             # .umap files — one folder per chapter
    /Chapter01_Tau_Ceti
    /Chapter02_Pfhor_Ship
  /Meshes
    /Sectors          # Auto-generated procedural sector meshes (one per M1 polygon)
    /Props            # Non-structural objects: crates, terminals, lights
    /Characters       # Player and enemy meshes (mostly billboards initially)
    /Weapons          # Weapon meshes and attachments
  /Materials
    /Master           # Master materials (M_Master_Surface, M_Master_Emissive,
                      #                    M_Master_Sprite_Billboard)
    /Instances        # Per-surface material instances
    /Decals           # Blood, scorch, bullet holes
  /Sprites            # Imported sprite sheets and flipbooks from Shapes.shps
  /Textures           # Imported wall/floor/ceiling textures from Shapes.shps
  /Blueprints
    /Player           # BP_PlayerCharacter, BP_PlayerController, BP_PlayerCamera
    /Weapons          # BP_WeaponBase and one child BP per weapon
    /Enemies          # BP_EnemyBillboardBase and one child BP per enemy type
    /Interactables    # BP_TerminalBase, BP_Door, BP_Switch, BP_PatternBuffer
    /GameFramework    # BP_GameInstance, BP_GameMode, BP_HUD
    /Components       # AC_HealthComponent, AC_OxygenComponent, AC_InventoryComponent
  /UI
    /HUD              # WBP_HUD, WBP_MotionTracker, WBP_ShieldBar
    /Menus            # WBP_MainMenu, WBP_PauseMenu, WBP_Terminal
  /Audio
    /SFX              # Imported from Reference/Sounds — keep family folders
    /Music            # Music assets
    /Voice            # Durandal and Leela terminal voice lines (TBD)
  /VFX                # Niagara systems: muzzle flash, hit sparks, teleport FX
  /Data               # Data tables and data assets (enemy stats, weapon stats, lore text)
```

---

## Naming conventions

Use these prefixes and patterns consistently. Never deviate.

| Asset type            | Prefix  | Example                        |
|-----------------------|---------|--------------------------------|
| Blueprint class       | `BP_`   | `BP_PlayerCharacter`           |
| Widget Blueprint      | `WBP_`  | `WBP_HUD`                      |
| Actor Component       | `AC_`   | `AC_HealthComponent`           |
| Material              | `M_`    | `M_Master_Surface`             |
| Material instance     | `MI_`   | `MI_PfhorWall_01`              |
| Static mesh           | `SM_`   | `SM_Sector_Level00_Poly042`    |
| Skeletal mesh         | `SK_`   | `SK_Pfhor_Fighter`             |
| Texture               | `T_`    | `T_PfhorWall_Coll17_Shape03`   |
| Sprite / flipbook     | `SP_` / `FB_` | `FB_Fighter_Walk`         |
| Sound cue             | `SC_`   | `SC_Pistol_Fire`               |
| Niagara system        | `NS_`   | `NS_TeleportBurst`             |
| Data table            | `DT_`   | `DT_WeaponStats`               |
| Data asset            | `DA_`   | `DA_PfhorFighter`              |
| Level map             | `L_`    | `L_TC1_Arrival`                |

**Procedural sector meshes** follow `SM_Sector_Level<NN>_Poly<NNN>` so the
auto-converter can deterministically rebuild them without colliding with
hand-authored content. **Textures** carry their collection+shape IDs in the
name so we can trace back to the Marathon source.

---

## Blueprint conventions

- **One responsibility per Blueprint.** `BP_PlayerCharacter` handles movement
  and state only. Shooting lives in `BP_WeaponBase`. Health lives in
  `AC_HealthComponent`.
- **Use Actor Components** for any behavior that might be shared across
  multiple actor types (health, oxygen, inventory, damage response).
- **Event dispatchers over direct casting** when communicating upward.
- **Game Instance** (`BP_GameInstance`) is the only place to store data that
  must persist across level loads (current chapter, collected items, lore
  terminal read states).
- **No magic numbers.** Expose every tunable value as a variable with a
  tooltip. Group related variables under a category name.

---

## Level design conventions (post-recalibration)

The original M1 maps are the source of truth for level geometry. The auto
converter (TODO — `Tools/scen_to_ue5/`) reads the JSON in `Reference/Maps/`
and produces procedural sector meshes plus actor placements.

**Coordinate conversion**: Marathon stores `x`, `y`, `floor_height`,
`ceiling_height` as int16 in units of 1/1024 World Unit. World Unit ≈ 1
meter. Convert to UE centimeters with:

```
ue_value_cm = (m1_int16 / 1024.0) * 100
```

That puts a typical Marathon corridor (e.g. 1 WU wide) at 100 UE cm.

**No hand-built BSP** in shipping levels. The procedural sector meshes are
the geometry. Props, decals, and detail meshes are placed on top.

**Lighting**: imported from `LITE` chunks (`intensity` / `period` / `mode`).
The converter emits one `PointLight` per `LITE` record; an art pass can
swap them for area lights, rect lights, etc.

---

## Enemy visual approach

Enemies render as **camera-facing billboard sprite cards** for the first
playable pass:

1. Extract per-enemy sprite sheets from `Shapes.shps` (TODO).
2. Import as flipbook/animation textures in `/Content/Sprites/`.
3. `BP_EnemyBillboardBase` renders the appropriate frame based on view
   angle (Marathon supports 1, 4, 5, or 8 view directions per shape).
4. Frame selection drives off the enemy state machine (walk/attack/death).

This matches the original M1 visual style exactly and ducks all 3D modeling
work. Upgrading to 3D enemies later is a swap of the visual component — the
gameplay BPs don't change.

---

## Git workflow

- **Never commit** `/Binaries/`, `/Intermediate/`, `/Saved/`,
  `DerivedDataCache/`, `*.sln`, or `Reference/` (regenerable from M1SOURCE)
- **Commit message format**: `[System] Short description`
  - e.g. `[Pipeline] Add M1 sound extraction (104 WAVs)`
- **Branch names**: `feature/system-description`
- **Before committing**: verify the project opens and PIE runs without errors
- When Claude Code modifies pipeline scripts, note affected paths in commits

---

## Marathon fidelity notes

These are the design pillars Claude should understand when generating content
or suggesting systems:

- **Pfhor factions**: Fighters (fast melee/ranged grunts), Troopers (armored
  shooters), Hunters (heavy, bouncing projectiles), Enforcers (staff weapon,
  teleport), Cyborgs, S'pht (compilers, floating, electric bolt). Each has
  distinct behavior and threat level.
- **Weapons (reimagined)**: Fists, MA-75 Assault Rifle/Grenade, TOZT
  Flamethrower, SPNKR Missile Launcher, KKV-7 SMG, Fusion Pistol, Alien
  Weapon, Staff. All weapons should feel punchy and physical.
- **Terminals**: The primary lore delivery system. Styled as monochrome
  green-on-black computer terminals. Durandal's messages are sarcastic and
  verbose. Leela's are clipped and urgent. Tycho's are menacing. Terminal
  text should never auto-scroll. Source text lives in the `term` chunk of
  each level's JSON.
- **Pattern Buffers**: Marathon's save system. No autosave except on level
  transition.
- **Oxygen**: Player oxygen depletes in vacuum areas. Use sparingly but
  meaningfully.
- **No regenerating shields**: Shields recharge from pickups or chargers
  only. Non-negotiable — defines Marathon's tension.

---

## License / distribution policy

The user has indicated this project may be released publicly someday.
Therefore:

- Everything under `M1SOURCE/` and `Reference/` is Bungie IP — **internal
  reference only**. Bungie's grant of a limited license to Aleph One does
  not transfer to derivative projects.
- Final shipped art (textures, sprites, sounds, maps) must be **rebuilt
  original** before any public release. Reference data is allowed during
  development.
- Geometry **shape and layout** of M1 levels is harder to claim copyright
  on (functional spatial design has weaker protection than artistic
  expression), but levels should still be polished and re-themed enough
  that they read as original.
- Music: replace with original tracks before release.
- The Aleph One *source code* is GPL — we don't redistribute it; we use the
  format documentation it reveals.

When in doubt, treat extracted content as if it were unlicensed third-party
material in your `vendor/` directory.

---

## What to avoid

- Do not introduce C++ files unless a Blueprint cannot accomplish the task
  (e.g. custom movement component subclass). Always ask before adding C++.
- Do not use deprecated UE4-era nodes (e.g. `Get Player Controller (0)`
  without a cast — use typed getters). Prefer UE5 patterns.
- Do not add third-party plugins without flagging them first.
- Do not reorganize the `/Content` folder structure without explicit
  instruction.
- Do not use world partition for levels.
- Do not modify anything under `M1SOURCE/` — it is read-only source of truth.
- Do not hand-model assets that can be derived from M1SOURCE. Ask the user
  first if you find yourself wanting to.

---

## Current build status

| System              | Status       | Notes |
|---------------------|--------------|-------|
| Asset extraction    | ✅ Done | via py-marathon-utils (maps/sounds/shapes/physics/terminals) |
| `.scen` → UE5 converter | 🟡 In progress | `Tools/scen_to_obj.py` builds OBJ + placements for all 37 levels; UE5 import script + guide done; needs PIE validation |
| Sprite billboard pipeline | 🔲 Not started | Shapes extraction available via package |
| Level geometry kit  | ⛔ Superseded | Replaced by procedural sectors |
| Player movement     | 🔲 Not started ||
| Weapons             | 🔲 Not started ||
| Enemy AI            | 🔲 Not started ||
| Terminal system     | 🔲 Not started | Source text already extracted in Maps JSON |
| HUD                 | 🔲 Not started ||
| Audio               | 🟡 Source extracted | Need UE import + cue setup |
| Save system         | 🔲 Not started ||

Update this table as systems reach milestones (🔲 Not started → 🟡 In progress → ✅ Done → ⛔ Superseded).
