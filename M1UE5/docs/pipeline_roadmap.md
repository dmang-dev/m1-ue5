# Pipeline Roadmap

The automation-first plan for the Marathon UE5 remake. Updated as milestones
land.

## Current state (2026-05-27)

✅ **Extraction toolchain online.** `Tools/extract/` is a pure-Python
package that reads M1SOURCE binary files into structured `Reference/` output.

✅ **Sounds extracted.** 104 WAVs in `Reference/Sounds/`, organized by family
(monster / weapon / ambient / etc.). Ready to drag into UE5 Content Browser.

✅ **Map geometry extracted.** 37 levels parsed into JSON under
`Reference/Maps/`. Each contains EPNT (vertices), LINS (lines), SIDS (walls),
POLY (sectors with floor/ceiling heights), LITE (lights), OBJS (spawn
placements), and where present, terminal text (de-obfuscated).

✅ **Physics file preserved.** Raw `Physics.phys` copied for later per-record
parsing.

🟡 **CLAUDE.md updated** to reflect the new approach: extract first, hand-
build only what can't be derived.

⛔ **Manual greybox / OBJ-kit workflow retired.** Documents marked
superseded.

## Next milestones

### M1 — Shapes extraction

Pull sprites and wall textures out of `Shapes.shps` as PNGs:

- Install Pillow (`pip install Pillow`) — only added when we get there
- Implement M1 RLE bitmap decoder (`Tools/extract/shapes.py`)
- Per collection: emit a `palette.png` (256-color CLUT), per shape a
  `coll<NN>_shape<NNN>.png`, plus a `frames.json` describing high-level
  shape (sprite-sequence) groupings: views, frames per view, ticks, sounds.
- Organize output under `Reference/Shapes/Coll<NN>_<name>/`. Mapping of
  collection-index → semantic name (Pfhor_Fighter, Walls_Pfhor_Slime, etc.)
  lives in `Tools/extract/shape_collection_names.py`.

### M2 — `.scen` → UE5 converter (the big one)

Read `Reference/Maps/00_Arrival.json` and produce a UE5 level file.

**Approach options to evaluate when this milestone starts:**

1. **JSON → Python-in-UE Editor Utility.** UE5 has an in-editor Python
   interpreter (PythonScriptPlugin). We'd write a `build_level.py` that
   reads our JSON, calls `unreal.EditorStaticMeshLibrary.create_mesh_from_…`
   or uses GeometryScript via Python bindings to extrude each polygon to a
   ProceduralMesh actor. Pros: no intermediate format. Cons: UE5 Python is
   editor-only and reflection-heavy; debugging is slow.

2. **JSON → `.t3d` text export.** UE5 can paste actor data from a text-form
   `.t3d` document. We generate one big `.t3d`, import into an empty level.
   Pros: language-agnostic, easy to inspect. Cons: ProceduralMesh content
   is awkward in t3d; we'd likely target placed static meshes referencing
   pre-baked `.uasset` meshes.

3. **JSON → glTF/FBX per sector + manifest.** Generate one mesh file per
   M1 polygon (extruded floor-to-ceiling), import as static mesh batch, then
   a Python utility script places instances at correct positions. Pros:
   leverages UE5's robust mesh importer. Cons: ~250 meshes per level
   creates Content Browser noise; needs naming discipline (`SM_Sector_…`).

   This is the **probable winner** — easiest to debug and re-run.

First proof-of-concept: convert level 0 ("Arrival") only. Walk it in PIE.

### M3 — Enemy sprite billboard component

Once Shapes are extracted:

- `BP_EnemyBillboardBase` with a `BillboardComponent` (or
  `PaperSpriteComponent`) that picks the right view direction frame
- `DA_EnemyDef` data asset per enemy type, holding sprite sheet refs and
  state-machine frame ranges
- Walk/Idle/Attack/Death flipbooks per enemy

### M4 — Physics constants → DataTables

Decode `Physics.phys` records (deferred from M1 extractor stub). Populate
`DT_MonsterStats`, `DT_WeaponStats`, `DT_ProjectileStats`. These feed enemy
AI and weapon tuning.

### M5 — Terminal pipeline

`term` text is already in the level JSONs. Build `WBP_Terminal` and link to
extracted text. (Terminal text on the Arrival level appears in
`Marathon.appl`'s resources rather than the level WAD — extraction of that
is a small follow-up.)

### M6 — Audio import + cues

Bulk-import the WAVs in `Reference/Sounds/` into UE5 (Python editor script
can do this — it's the same import flow as static meshes). Auto-create
SoundCues per family with appropriate concurrency/attenuation defaults.

## Open questions

- **Cohesive naming for shape collections.** Marathon's 26 `.256` collections
  aren't self-describing. We need a mapping that calls collection 13
  (say) "Pfhor_Walls_Slime" instead of "Coll_13". This will come from
  inspecting extracted PNGs once M1 lands.
- **How accurate is the procedural mesh look?** A flat extruded sector with
  one texture top/bottom/walls may look uncanny without trim and detail
  passes. The first PIE walkthrough of "Arrival" will tell us how much
  art-pass work we need on top.
- **Terminal source for "Arrival".** The level 0 JSON has 0 `term` records
  even though the original level has terminals. Likely they're stored in the
  Marathon.appl resource fork. Investigate when we tackle M5.
