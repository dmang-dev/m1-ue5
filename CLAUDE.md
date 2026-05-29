# Marathon UE5 Remake — repository root

This repo holds a from-scratch, **automation-first** remake of Bungie's
Marathon 1 in Unreal Engine 5. Original game data is interpreted/extracted
programmatically rather than hand-authored.

## Layout

```
C:\m1-ue5\
├── M1UE5/            # the UE5 project + Python tooling + docs  ← active work
│   └── CLAUDE.md     # **authoritative project guide** — read this
├── M1SOURCE/         # Marathon 1 Aleph One data (Bungie IP, gitignored)
├── M2SOURCE/         # Marathon 2 data (reference; gitignored)
├── MISOURCE/         # Marathon Infinity data (reference; gitignored)
└── CLAUDE.md         # this file
```

**The authoritative project conventions, pipeline, and status live in
[`M1UE5/CLAUDE.md`](M1UE5/CLAUDE.md).** Read that before making changes.

## Quick orientation

- **Asset pipeline** is the published package
  [`py-marathon-utils`](https://github.com/dmang-dev/py-marathon-utils)
  (`pip install -r M1UE5/Tools/requirements.txt`). It reads/writes Marathon
  1/2/Infinity maps, shapes, sounds, physics, terminals, etc.
- **Extraction**: `python M1UE5/Tools/extract_reference.py` → `M1UE5/Reference/`
- **Geometry**: `python M1UE5/Tools/scen_to_obj.py --all M1UE5/Reference/Maps`
  → per-level OBJ + placements in `M1UE5/Reference/Geometry/`
- **UE5 import**: `M1UE5/Tools/ue5_import_level.py` (editor Python) or the
  headless `UnrealEditor-Cmd` path — see `M1UE5/docs/scen_to_ue5_import.md`.

## Conventions

- The `*SOURCE/` folders are read-only Bungie IP and are **never committed**.
- `M1UE5/Reference/` is regenerable output (gitignored).
- Commit author for this repo is `dmang-dev` (no personal info in history).
