# Changelog

All notable changes to the Marathon UE5 remake. This is a pre-1.0, automation-first
re-creation of Bungie's *Marathon* (1994) in Unreal Engine 5; entries below are grouped
by system and ordered roughly in the order they were built (see `git log` for the
authoritative per-commit history). The project ships **no Bungie data** — it loads the
user's own Marathon 1 data at runtime (see `docs/INSTALLER_DESIGN.md`).

## [Unreleased]

### Pipeline & headless build
- Initial automation-first scaffold: extract M1 data → `Reference/`, convert `.scen` →
  per-level OBJ + placements, headless UE5 import loop (all 37 levels, shared
  texture/material library, zero manual editor steps).
- MRQ render loop (top-down / first-person QA renders), parameterized for any level.
- Walkable sectors: complex-as-simple collision + a real `PlayerStart` (correct facing)
  from the OBJS player object; fixed spawn Y-mirror + world-origin fall.
- Faithful Marathon wall texturing (per-side UVs from the Aleph One format), z-fighting
  + camera-clipping fixes; legacy OBJ importer to stop terminal-niche hole culling.
- Closed-platform "doors" rebaked as walkable passages; faithful ride-on elevators
  (the movable slab *is* the floor; baked floor dropped).

### Player, movement & game flow
- First-person C++ pawn (`AM1Character`) + `AM1GameMode`: WASD/mouse/jump, gravity,
  collision; double-click `.exe` packaged build.
- All 37 levels roamable via an in-game switcher (`[` / `]`); narrow 75° Marathon FOV.
- Fit the pawn to Marathon's tight rooms; main menu + pause menu (Slate, no UMG); fixed
  the menu→level transition crash.
- Save/Continue checkpoint with shield carry-over; **Preferences** (mouse sensitivity,
  invert look) and **Save/Restore Game** menu entries.
- Movement feel consolidated into `M1Tuning.h`; walk speed tuned to canonical ~430 cm/s.

### Weapons & combat
- Full 7-weapon M1 arsenal with canonical `Physics.phys` stats; secondary triggers
  (AR grenade, Fusion charge, Alien burst); `AM1Projectile` (radial + point damage,
  rocket-jumps). Later made **data-driven** from a generated `M1WeaponTable.h`.
- Tuning knobs: 150 shields, incoming-damage scale, shared constants.

### Enemies
- Pfhor sprite billboards from `Shapes.shps` (canonical stats, AI, melee + ranged,
  8-direction view frames, visible projectiles, corpses persist).
- **Attack-pose animation** (Stand/Walk/Dead/**Attack** states; multicast so it shows
  in net play).

### HUD
- Weapon-in-hand sprite viewmodels; authentic **PICT-128 bezel** framing a 4:3 viewport
  (SceneCapture); shield/oxygen meters matched to Aleph One's render; **motion-sensor
  radar** in the bezel oval; net **scoreboard** tinted by team/player color.

### Lighting
- Iterated point lights → settled on **per-surface baked emissive**: `scen_to_obj`
  bakes each surface's LITE brightness into the material name (`tex_C_S_b<pct>`); manual
  exposure so eye-adaptation can't wash out the room-by-room mood. Rolled out to all 37.

### Terminals, audio, interactables
- Readable lore terminals (paged green screens from the canonical renders); exit-terminal
  level progression (epilogue on the final level).
- All 104 M1 sounds imported with canonical names + distance attenuation; 16 looping
  music tracks per level (`Minf.song_index`).
- Doors as movable panels; switch-operated doors; shield rechargers + pattern-buffer
  save stations; cosmetic light-switch panels.

### Networking, save & replay (UE-native)
- **Save-anywhere**: full world-state snapshot (player + every living enemy).
- **Co-op** (Phase 2): player/enemy replication, server-authoritative AI + weapon fire,
  replicated doors/elevators + a visible co-op body, replicated ammo/weapon-select.
- **Record/replay** ("films") via the UE Replay/DemoNetDriver system.
- **Deathmatch**: Carnage (FFA frags), KOTH, Tag, Team Carnage; teams + friendly-fire;
  authored hill/ball actors; per-player/team body colors + sprite.

### Distribution — bring-your-own-assets installer
- **Phase 0** — runtime OBJ/PNG/material loader (`AM1RuntimeLevel`): builds a level's
  geometry at runtime from intermediates, no cook.
- **Phase 1** — all 37 levels load via the runtime path into one blank `L_Runtime` map
  (now the campaign default); table-driven actors + player start; checkpoints preserved.
- **Phase 2** — Asset Setup wizard (`m1_asset_setup.py`): locate-only + SHA-256 verify +
  convert the user's data → per-user `%LOCALAPPDATA%\M1UE5\Data` cache; frozen to
  `M1AssetSetup.exe` (PyInstaller, no Python needed).
- **Phase 3** — Inno Setup installer (`M1UE5.iss`): per-user/no-admin, EULA gate,
  locate-data page, runs the converter during setup, uninstaller; `Setup.exe` compiled +
  verified end-to-end against a real Steam *Classic Marathon* install.
- **Phase 4** — runtime audio (SFX + music load from the cache, not cooked Bungie audio);
  bundled `<install>\M1Data` tier + clean-room original-texture generator (self-contained
  framework); lean release cook (no Bungie geometry in the pak).

### Docs & repo
- `docs/PROJECT_STATUS.md` / `HANDOFF.md`, `INSTALLER_DESIGN.md`, MIT `LICENSE` + `NOTICE`
  (Bungie IP boundary); in-game `M1SelfTest` console harness; `.gitignore` hardened so no
  generated/Bungie asset can be committed.
