# Modular Level Geometry Kit — First-Pass Spec

All meshes snap to the 100 UU grid. Dimensions in asset names follow the pattern
`SM_<Group>_<Variant>_<Width>[x<Depth>][_<Height>h]`. Width and depth are always
the footprint; height is only appended when a group has multiple height variants.
The default wall heights in this kit are **300 UU** (UESC minimum) and **400 UU**
(standard Pfhor corridor). Build at 500 UU where you need an imposing open section.

Pivot points: walls pivot at the bottom-left corner on the XY plane. Floors/ceilings
pivot at one corner. Corners pivot at the vertex. Trim pivots at the near end, flush
with the wall face.

---

## Group 1 — Walls

These are the workhorses. Every wall in the game is assembled from these segments.
Build the 300h variants first; the 400h variants are needed before Chapter 1 can be
dressed.

| Asset name | Width (UU) | Height (UU) | Depth (UU) | Purpose |
|---|---|---|---|---|
| `SM_Wall_Straight_100_300h` | 100 | 300 | 20 | Narrow fill segment, plugs gaps between longer pieces |
| `SM_Wall_Straight_200_300h` | 200 | 300 | 20 | Half-grid wall, used for alcoves and short runs |
| `SM_Wall_Straight_400_300h` | 400 | 300 | 20 | **Primary wall unit** — the kit's most-used piece |
| `SM_Wall_Straight_800_300h` | 800 | 300 | 20 | Long corridor span, reduces piece count in straight halls |
| `SM_Wall_Straight_100_400h` | 100 | 400 | 20 | Narrow fill for Pfhor-height corridors |
| `SM_Wall_Straight_200_400h` | 200 | 400 | 20 | Short Pfhor-height wall |
| `SM_Wall_Straight_400_400h` | 400 | 400 | 20 | **Primary Pfhor corridor wall** |
| `SM_Wall_Straight_800_400h` | 800 | 400 | 20 | Long Pfhor-height span |
| `SM_Wall_Straight_400_500h` | 400 | 500 | 20 | Tall wall for sub-arenas and imposing Pfhor chambers |
| `SM_Wall_Half_200_150h` | 200 | 150 | 20 | Half-height cover wall, breaks up open combat arenas |
| `SM_Wall_Half_400_150h` | 400 | 150 | 20 | Long cover wall, chest-high barrier |
| `SM_Wall_Window_400_300h` | 400 | 300 | 20 | Wall with a centered viewport cutout (vacuum sections, lookouts) |

---

## Group 2 — Floors and Ceilings

Floors and ceilings share the same geometry — flip Z to use a floor piece as a
ceiling. Build as flat panels 10 UU thick. UV islands should tile every 400 UU so
the same material works at all scales.

| Asset name | Width (UU) | Depth (UU) | Thickness (UU) | Purpose |
|---|---|---|---|---|
| `SM_Floor_100x100` | 100 | 100 | 10 | Fine-fill tile for irregular nooks |
| `SM_Floor_200x200` | 200 | 200 | 10 | Small room fill, raised platform surface |
| `SM_Floor_200x400` | 200 | 400 | 10 | Rectangular run for corridors |
| `SM_Floor_400x400` | 400 | 400 | 10 | **Primary floor unit** |
| `SM_Floor_400x800` | 400 | 800 | 10 | Wide corridor or entry hall |
| `SM_Floor_800x800` | 800 | 800 | 10 | Combat arena base tile |
| `SM_Floor_Grate_200x200` | 200 | 200 | 10 | Metal grating variant — catwalks, drainage channels |
| `SM_Floor_Grate_400x400` | 400 | 400 | 10 | Large grating panel — engine rooms, maintenance bays |
| `SM_Floor_Raised_200x200` | 200 | 200 | 50 | Raised pad for Pattern Buffer / terminal alcove floor |

---

## Group 3 — Corners

Inner corners fill the concave vertex where two walls meet. Outer corners cap the
convex vertex on the outside of a room. Build each at every height variant that
has a matching wall in Group 1 (300h and 400h minimum).

| Asset name | Height (UU) | Type | Purpose |
|---|---|---|---|
| `SM_Corner_Inner_300h` | 300 | Concave 90° | Inside wall junction, UESC-height rooms |
| `SM_Corner_Outer_300h` | 300 | Convex 90° | Outside wall junction, pillars, exposed corners |
| `SM_Corner_Inner_400h` | 400 | Concave 90° | Inside wall junction, Pfhor-height rooms |
| `SM_Corner_Outer_400h` | 400 | Convex 90° | Outside wall junction, Pfhor-height rooms |
| `SM_Corner_Inner_500h` | 500 | Concave 90° | Inside junction for tall combat chambers |
| `SM_Corner_Outer_500h` | 500 | Convex 90° | Outside junction for tall combat chambers |
| `SM_Corner_Inner_150h` | 150 | Concave 90° | Joins half-height cover walls |
| `SM_Corner_Outer_150h` | 150 | Convex 90° | Caps half-height cover walls |

---

## Group 4 — Trim

Trim breaks visual monotony, hides seams between wall and floor/ceiling, and gives
Marathon's industrial aesthetic its character. All trim is 10–20 UU deep and lies
flush with the face of the wall or the edge of the floor.

| Asset name | Length (UU) | Height/Depth (UU) | Purpose |
|---|---|---|---|
| `SM_Trim_Base_100` | 100 | 20 | Floor-level baseboard strip, short fill |
| `SM_Trim_Base_200` | 200 | 20 | Floor-level baseboard strip, half-grid |
| `SM_Trim_Base_400` | 400 | 20 | **Primary floor trim** — runs along base of walls |
| `SM_Trim_Ceiling_100` | 100 | 20 | Ceiling-level cove trim, short fill |
| `SM_Trim_Ceiling_200` | 200 | 20 | Ceiling-level cove trim, half-grid |
| `SM_Trim_Ceiling_400` | 400 | 20 | **Primary ceiling trim** |
| `SM_Trim_Corner_Base_Inner` | — | 20 | Mitered inner corner cap for base trim |
| `SM_Trim_Corner_Base_Outer` | — | 20 | Mitered outer corner cap for base trim |
| `SM_Trim_Column_300h` | — | 30×30 | Vertical accent column strip, UESC height |
| `SM_Trim_Column_400h` | — | 30×30 | Vertical accent column strip, Pfhor height |
| `SM_Trim_Panel_400x200` | 400 | 200 | Recessed wall panel detail — breaks up large flat walls |
| `SM_Trim_Conduit_400` | 400 | 15 | Horizontal pipe/conduit run — industrial detail |

---

## Group 5 — Doors

Door pieces are split into frames (static, embedded in the wall) and panels (the
moving door leaf, driven by BP_Door). Frame depth matches the wall thickness of
20 UU. Build frames first; panels come after BP_Door is wired up.

| Asset name | Opening Width (UU) | Opening Height (UU) | Purpose |
|---|---|---|---|
| `SM_Door_Frame_200x300` | 200 | 300 | Standard interior doorway, UESC areas |
| `SM_Door_Frame_200x400` | 200 | 400 | Standard interior doorway, Pfhor areas |
| `SM_Door_Frame_300x400` | 300 | 400 | Wide Pfhor doorway — command rooms, arenas |
| `SM_Door_Frame_400x400` | 400 | 400 | Large bay door opening |
| `SM_Door_Panel_200x300` | 200 | 300 | Door leaf for 200×300 frame — slides on Z or Y |
| `SM_Door_Panel_200x400` | 200 | 400 | Door leaf for 200×400 frame |
| `SM_Door_Panel_300x400` | 300 | 400 | Door leaf for 300×400 frame |
| `SM_Door_Panel_400x400` | 400 | 400 | Bay door leaf — two halves if needed for split-open anim |

---

## Group 6 — Special (Pfhor-Specific Shapes)

These pieces define the alien visual language of Pfhor interiors. They break from
the orthogonal UESC grid with beveled edges, asymmetric cutouts, and organic-ish
curves. Still snap to the 100 UU grid at connection points — the irregularity is
in the middle of the piece, not at the edges.

| Asset name | Footprint (UU) | Height (UU) | Purpose |
|---|---|---|---|
| `SM_Pfhor_Arch_400x400` | 400 wide | 400 | Alien archway with tapered sides — replaces door frames in Pfhor zones |
| `SM_Pfhor_Arch_300x400` | 300 wide | 400 | Narrower Pfhor arch for secondary passages |
| `SM_Pfhor_Column_400h` | 100×100 base | 400 | Freestanding alien pillar, decorative or structural |
| `SM_Pfhor_Column_600h` | 100×100 base | 600 | Tall Pfhor pillar for large chambers |
| `SM_Pfhor_Wall_Angled_400` | 400 (face) | 400 | 45-degree diagonal wall segment — breaks orthogonal monotony in Pfhor rooms |
| `SM_Pfhor_Wall_Angled_200` | 200 (face) | 400 | Short 45-degree diagonal fill piece |
| `SM_Pfhor_Alcove_200x300` | 200 wide | 300 | Shallow wall recess — fits a terminal or Pattern Buffer |
| `SM_Pfhor_Alcove_200x400` | 200 wide | 400 | Taller alcove for Pfhor-height rooms |
| `SM_Pfhor_Platform_400x400` | 400×400 | 100 rise | Raised dais — alien altars, commander platforms |
| `SM_Pfhor_Platform_200x400` | 200×400 | 100 rise | Narrower raised walkway |
| `SM_Pfhor_Ramp_400x200` | 400 long × 200 wide | 100 rise | Sloped connector, one-grid rise over two-grid run (26.6°) |
| `SM_Pfhor_Ceiling_Vault_400` | 400×400 | — | Arched or faceted ceiling panel replacing flat ceiling in Pfhor rooms |
| `SM_Pfhor_Ceiling_Vault_800` | 800×400 | — | Wide vaulted ceiling for large Pfhor chambers |

---

## Build priority for first-pass Chapter 1

Build in this order to unblock level assembly as fast as possible:

1. `SM_Wall_Straight_400_300h`, `SM_Floor_400x400`, `SM_Corner_Inner_300h`, `SM_Corner_Outer_300h` — bare minimum to rough in any room
2. `SM_Wall_Straight_100_300h`, `SM_Wall_Straight_200_300h`, `SM_Floor_200x400` — gap-fill and corridors
3. `SM_Trim_Base_400`, `SM_Trim_Ceiling_400`, `SM_Trim_Corner_*` — seam cleanup
4. `SM_Door_Frame_200x300`, `SM_Door_Panel_200x300` — first playable door
5. `SM_Wall_Window_400_300h` — vacuum section lookout
6. `SM_Pfhor_Arch_400x400`, `SM_Pfhor_Wall_Angled_400`, `SM_Pfhor_Ceiling_Vault_400` — Pfhor zone dressing
7. Remaining heights (400h, 500h) and Pfhor special pieces as needed
