# Chapter 01 — Tau Ceti: "Arrival"
## Level Design Brief

**Map asset name**: `L_TC1_Arrival`
**Sublevels**: `L_TC1_Arrival_Geo`, `L_TC1_Arrival_Lighting`
**Estimated play time**: 8–12 minutes (first run)

---

## Narrative setup

The year is 2794. The UESC Marathon — a hollowed-out asteroid converted into a colony
ship — has just arrived at Tau Ceti IV after a 300-year voyage. The player is a
security officer stationed aboard.

The Pfhor, an alien slaving race, have launched a boarding action. The attack was
sudden and precise. Most of the crew in this section is dead or fleeing. The ship's
AI, **Leela**, is still online but her sensor coverage in this section is fragmented —
she can see the player through intermittent cameras but not much else. The other ship
AI, **Durandal**, is active but his motives are already unclear.

The player wakes up in a maintenance corridor in **Ring Habitation Section 7**,
midway between the crew quarters and the cargo processing deck. The lights are on
emergency red. The Pfhor are already inside.

---

## Player path — beat-by-beat

1. **Spawn / Orientation** — Player appears in a narrow maintenance corridor (300 UU
   ceiling, single point of entry behind them sealed by a collapsed bulkhead). One
   dead security officer nearby. Dim red emergency lighting. A Pfhor Fighter is
   visible at the far end of the corridor facing away — telegraphed first kill.

2. **First Terminal (Leela)** — Ten seconds down the corridor, in a small monitoring
   alcove on the right. Leela's message establishes the situation: the Pfhor have
   breached three sections, crew casualties are heavy, the player's objective is to
   reach Habitation Control on Deck 7-C and restore internal communications so she
   can coordinate a response. Tone: clipped, urgent, information-dense.

3. **Crew Quarters Gauntlet** — A short corridor opens into a damaged crew bunk room
   (first room with width to breathe — roughly 800×600 UU). Two Pfhor Fighters
   patrol here. A third drops from a ceiling grate after the first two are engaged.
   This teaches the player: enemies use the environment, never assume a room is clear.

4. **First Pattern Buffer** — In the far corner of the crew quarters, in a recessed
   alcove (`SM_Pfhor_Alcove_200x300` or similar UESC-era equivalent). Distinctive
   sound and screen flash on activation. No explanation — the player learns by doing.

5. **Vacuum Corridor** — A short connecting passage (one `SM_Wall_Window_400_300h`
   panel showing void and the exterior hull) where the oxygen bar begins depleting.
   Duration: 20–30 seconds at normal movement speed. Forces the player to commit —
   no lingering. No enemies here; the vacuum is the threat.

6. **Cargo Processing Bay** — The level's main combat arena. Roughly 1600×1200 UU,
   600 UU ceiling. Stacked crates provide cover. Enemy composition: three Fighters,
   one Trooper (armored, shoots back), patrolling in a loose circuit. A catwalk on
   the east side (accessible via a ramp) lets the player get elevation. The Trooper
   is near the far exit — the player has to push through.

7. **Second Pattern Buffer** — At the exit of the cargo bay, before the control room.
   Reward for clearing the arena; checkpoint before the level's hardest fight.

8. **Habitation Control Room** — Smaller space (~600×600 UU, 400 UU ceiling —
   Pfhor have partially converted this area). An **Enforcer** waits here. It can
   teleport; the player needs to use the room's cover pillars and stay mobile.
   A terminal in the room can be read mid-fight or after — it interfaces with a
   power relay. Interacting with it triggers the "comms restored" event that opens
   the level exit.

9. **Second Terminal (Durandal)** — On the control room's main terminal panel.
   Durandal's message acknowledges the situation but his tone is wrong — too calm,
   mildly contemptuous of the crew's fate, oddly fascinated by the Pfhor boarding
   technique. He gives the player the exit code but adds an unsolicited observation
   about Leela's "limited perspective." First hint that not all AIs are on the same
   side. Tone: verbose, sardonic, slightly threatening.

10. **Level Exit** — A blast door at the north end of the control room slides open
    after the terminal interaction. Fade to black / level transition to Chapter 2.

---

## Key encounters summary

| Location | Enemies | Notes |
|---|---|---|
| Opening corridor | 1 Fighter | Tutorial kill; enemy facing away |
| Crew quarters | 2 Fighters + 1 ambush Fighter | Ceiling-drop ambush teaches room-clearing |
| Cargo bay | 3 Fighters + 1 Trooper | First Trooper; catwalk elevation option |
| Control room | 1 Enforcer | Teleports; room must have cover geometry |

---

## Terminal locations and message notes

### Terminal 1 — Leela (Monitoring Alcove, Habitation Corridor)
- **In-world location**: Side alcove off the main corridor, between spawn and the crew
  quarters. Should feel like a staff workstation — not prominent, not hidden.
- **Leela's message content** (tone reference): She addresses the player by security
  designation. Short sentences. Active verbs. States facts, gives one clear objective,
  closes with a tactical note ("the cargo bay cameras are offline — proceed with
  caution"). No small talk. No philosophy.
- **Design note**: Place this so the player can read it and still see the entrance to
  the crew quarters — it should create mild tension, not a safe pause.

### Terminal 2 — Durandal (Habitation Control Room)
- **In-world location**: Central panel of the control room, facing the entrance. The
  player sees it the moment they enter — but the Enforcer is between them and it.
- **Durandal's message content** (tone reference): Begins with a compliment that is
  actually an insult ("You survived. Remarkable, given your unit's historical
  performance metrics."). Provides the exit code embedded inside a longer digression
  about the Pfhor's methodology. Ends with something unsettling and unresolved.
- **Design note**: The player should feel compelled to read this fully but also aware
  that doing so mid-combat is risky. The terminal is a reward and a lore delivery
  mechanism, not a required gameplay step.

---

## Mood and lighting direction

**Overall palette**: Cold blue-white base light temperature (~6500K) — UESC
industrial spaces feel clinical and unlived-in, like a ship in an emergency state.

| Zone | Primary light | Accent lights | Notes |
|---|---|---|---|
| Spawn corridor | Dim red emergency strips | None | Near-dark; player must adjust |
| Monitoring alcove | Terminal screen glow (green) | One flickering overhead | Terminal is the only real light source |
| Crew quarters | Red emergency + one intact overhead | Emissive bunks, personal screens | More readable than corridor; contrast matters |
| Vacuum corridor | No interior light | Starfield through window | The window provides the only ambient fill — cold, dead |
| Cargo bay | Work lights overhead (amber) | Emissive crate labels, catwalk edge strips | Warmer than crew areas; industrial feel |
| Control room | Pfhor amber-green | Glowing alien conduit trim | First strong Pfhor aesthetic — should feel wrong |

**Lumen settings**: One dominant directional light (cold blue, intensity low — simulates
reflected starlight through hull) handles broad GI. Each zone's character comes from
point/spot emissives, not from the directional. Keep shadow settings sharp for the
narrow corridors — soft shadows lose depth in tight spaces.

**Art direction note**: The UESC Marathon is human-built but old. Surfaces should read
as functional, not designed. The Pfhor sections (control room) feel like an infection —
their amber-green emissives and angled geometry intrude on the orthogonal UESC structure.
