# Importing auto-converted Marathon levels into UE5

The pipeline turns an original Marathon level into walkable UE5 geometry with
no hand-modeling:

```
M1SOURCE/Map.scen
   └─ Tools/extract_reference.py        →  Reference/Maps/<NN>_<name>.json
        └─ Tools/scen_to_obj.py         →  Reference/Geometry/<NN>_<name>.obj
                                            Reference/Geometry/<NN>_<name>_placements.json
                                                 └─ import into UE5  →  walk it in PIE
```

## Generate the geometry

```bash
pip install -r Tools/requirements.txt
python Tools/extract_reference.py                 # M1SOURCE → Reference/Maps
python Tools/scen_to_obj.py Reference/Maps/00_Arrival.json   # one level
python Tools/scen_to_obj.py --all Reference/Maps             # all 37
```

Each level produces:
- `<level>.obj` — floors, ceilings, and walls as one mesh (cm, Z-up). Walls fill
  the full height where they border the void, and just the floor/ceiling "step"
  between adjacent sectors of different heights (Doom-style).
- `<level>_placements.json` — `player_start` (position + facing) and every
  `OBJS` placement (monsters/items/scenery/sounds) in UE coordinates, for
  spawning actors later.

Scale: `--scale` is **cm per Marathon World-Unit** (default 100). Bump it if the
spaces feel cramped for the UE character capsule. `--no-flip-y` if a level
imports mirrored.

## Option A — automated (UE5 Python, recommended)

Inside the editor, run `Tools/ue5_import_level.py` (set `MARATHON_LEVEL` env var
or edit `LEVEL` at the top first):

- **Tools → Execute Python Script…** → pick the file, **or**
- Output Log → Cmd: *Python* → `exec(open(r"…/M1UE5/Tools/ue5_import_level.py").read())`

It imports the OBJ into `/Game/Meshes/Sectors`, builds collision
(complex-as-simple), drops the mesh at the origin, and spawns a `PlayerStart` at
the Marathon player start. Press **Play** and walk.

> OBJ import options differ slightly across UE 5.x / Interchange settings. If the
> script errors, use the manual steps below — same result.

## Option B — manual import

1. **Content Browser → Import** → select `Reference/Geometry/00_Arrival.obj`
   into `Content/Meshes/Sectors/`.
2. In the import dialog:
   - Import as Static Mesh (not skeletal), **Combine Meshes** on
   - **Generate Lightmap UVs** on
   - **Auto Generate Collision** on (or set the mesh's collision to
     *Use Complex Collision As Simple* afterward — fine for a greybox)
   - Transform scale **1.0** (coordinates are already in cm)
3. Drag the imported mesh into the level at location **(0, 0, 0)**.
4. Open `Reference/Geometry/00_Arrival_placements.json`, find `player_start`,
   and place a **PlayerStart** at its `x, y, z` (add ~88 to Z so the capsule
   spawns above the floor) with `facing_deg` as the **Yaw**.
5. Add a couple of lights (or a Sky Light) so you can see — proper lighting from
   the level's `LITE` data is a later pass.
6. **Play**. You should spawn in the hangar and be able to walk the level.

## Known first-pass limitations

- **Walls are single-sided.** For greybox visibility use a two-sided material,
  or you'll see through back-faces. Collision is unaffected.
- **No textures yet.** Everything uses the default material. Wall/floor texture
  assignment from the `SIDS`/`POLY` shape descriptors + extracted Shapes PNGs is
  the next pipeline step.
- **Lights/enemies not spawned** (their data is in `placements.json` and the
  level JSON's `LITE`/`OBJS`). Sprite-billboard enemies and light placement come
  after geometry is validated in PIE.
- **Liquids, platforms (doors/elevators), 5-D space** are not modeled yet.
