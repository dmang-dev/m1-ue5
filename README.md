# Marathon UE5

A from-scratch, **automation-first** re-creation of Bungie's *Marathon* (1994) in
**Unreal Engine 5** — a modern FPS that preserves Marathon's world, weapons, enemies,
terminals, and spatial design while rebuilding every system from the ground up.

> **This project ships no Bungie game data.** It is a free, open-source, non-commercial
> fan project, not affiliated with or endorsed by Bungie. You bring your **own** copy of
> the Marathon 1 data (e.g. the freeware *Classic Marathon* release on Steam, or the
> Aleph One distribution); the game converts it **on your machine** and loads it at
> runtime. See [LICENSE](LICENSE), [NOTICE](NOTICE), and
> [`M1UE5/docs/INSTALLER_DESIGN.md`](M1UE5/docs/INSTALLER_DESIGN.md).

## What works today

A near-complete single-player campaign plus multiplayer, built entirely from the
original data via the pipeline:

- **All 37 levels** — procedural sector geometry, faithful wall texturing, per-surface
  baked lighting, built **at runtime** from converted intermediates (no cook needed).
- **Player & weapons** — first-person movement tuned to Marathon; the full 7-weapon
  arsenal with canonical `Physics.phys` stats + secondary triggers.
- **Enemies** — Pfhor sprite billboards (8-direction views, melee + ranged AI, attack
  animations) from `Shapes.shps`.
- **HUD** — the authentic PICT-128 bezel, shield/oxygen meters, motion-sensor radar,
  weapon viewmodels.
- **Terminals** — readable paged lore screens; exit-terminal level progression.
- **Audio** — all 104 M1 sounds + 16 looping music tracks, loaded from your converted
  data.
- **Save** — checkpoints + save-anywhere (pattern buffer).
- **Multiplayer** — co-op, record/replay, and deathmatch modes (Carnage / KOTH / Tag /
  Team), all UE-native.
- **Installer** — a per-user Windows installer that converts your data and runs the game
  with zero Bungie content in the distributable.

See **[CHANGELOG.md](M1UE5/CHANGELOG.md)** for the full development history.

## For players — install & run

1. Obtain your own Marathon 1 data — the four files `Map.scen`, `Shapes.shps`,
   `Sounds.sndz`, `Physics.phys` (and optionally a `Music/` folder of `NN.ogg`). The
   freeware **Classic Marathon** on Steam contains exactly these.
2. Run **`M1UE5-Setup.exe`** → accept the agreement → point it at your Marathon data
   folder → it converts the data into `%LOCALAPPDATA%\M1UE5\Data` and installs the game.
3. Launch **Marathon UE5** and pick *Begin New Game*. (If you skipped the data step, the
   game shows a "run Asset Setup" screen; the Start-Menu **Run Asset Setup** shortcut
   completes it.)

Silent/scripted install: `M1UE5-Setup.exe /VERYSILENT /DATADIR="<your data folder>"`.

## For developers — build from source

**Prerequisites:** Unreal Engine 5.7, Visual Studio 2022 (C++), Python 3.11 with
[`py-marathon-utils`](https://github.com/dmang-dev/py-marathon-utils) + Pillow +
soundfile (`pip install -r M1UE5/Tools/requirements.txt`). Put your Marathon 1 data in
`M1SOURCE/Marathon-20250829/`.

```text
# 1. Extract + convert the source data into the regenerable Reference/ + Geometry/
python M1UE5/Tools/extract_reference.py --shapes
python M1UE5/Tools/scen_to_obj.py --all M1UE5/Reference/Maps --out M1UE5/Reference/Geometry
# 2. Build the C++ game target
"<UE>/Engine/Build/BatchFiles/Build.bat" M1UE5 Win64 Development -project=M1UE5/M1UE5.uproject
# 3. Import assets + cook a packaged build (see docs below), or just run the runtime path:
#    launch and use New Game — levels build at runtime from Reference/ (no per-level cook).
```

Full details — environment, the headless pipeline, the build/cook commands, and the
gotchas — are in **[`M1UE5/docs/HANDOFF.md`](M1UE5/docs/HANDOFF.md)** (developer handoff)
and **[`M1UE5/CLAUDE.md`](M1UE5/CLAUDE.md)** (conventions). The complete asset-generation
pipeline + per-tool reference is in **[`M1UE5/docs/TOOLS.md`](M1UE5/docs/TOOLS.md)**, and
the installer build is in **[`M1UE5/Tools/installer/README.md`](M1UE5/Tools/installer/README.md)**.

## Repository layout

```
m1-ue5/
├── M1UE5/                 # the UE5 project (active work)
│   ├── Source/M1UE5/      # C++ gameplay (pawn, weapons, enemies, HUD, net, runtime loader)
│   ├── Tools/             # Python pipeline + headless-editor import scripts (see TOOLS.md)
│   │   └── installer/     # Inno Setup installer (.iss, EULA, build script)
│   ├── Content/           # UE assets — most are .gitignored (regenerated from M1 data)
│   ├── docs/              # HANDOFF, INSTALLER_DESIGN, TOOLS, ...
│   ├── CLAUDE.md          # project conventions
│   └── CHANGELOG.md
├── M1SOURCE/              # YOUR Marathon 1 data — gitignored, never committed (Bungie IP)
├── LICENSE / NOTICE       # MIT (code) + the Bungie IP boundary
└── README.md             # this file
```

## License

Code & tools: MIT (see [LICENSE](LICENSE)). Marathon game data is Bungie's and is **not**
included or redistributed — see [NOTICE](NOTICE) and the EULA in
[`M1UE5/Tools/installer/EULA.txt`](M1UE5/Tools/installer/EULA.txt).
