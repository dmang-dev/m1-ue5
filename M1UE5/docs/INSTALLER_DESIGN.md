# Installer & Distribution Design

**Project:** Marathon UE5 Remake (unofficial, non-commercial, open-source)
**Status:** Design proposal — not yet implemented
**Audience:** Project maintainers and contributors
**Last updated:** 2026-06-13

> **Disclaimer.** This document is engineering and policy guidance, not legal advice. The author is not a lawyer. Before any public release, a qualified IP attorney must review the licensing posture, the EULA text, and the data-acquisition flow described here.

---

## 1. Purpose & Scope

This document specifies how the Marathon UE5 Remake is packaged and delivered to end users. It covers:

- The **legal posture** that governs everything we ship (Section 2).
- The **distribution architecture** — how the game is built so that the user needs neither an Unreal Engine install nor a cook (Section 4).
- The **installer technology and end-to-end UX** — EULA → detect/locate data → verify → convert with progress → launch (Sections 5–6).
- **Prerequisites** the installer must satisfy (Section 7).
- A **phased implementation roadmap**, MVP first (Section 8).
- **Open decisions** that require a human call (Section 9).

The guiding constraint, inherited from the repo's `CLAUDE.md` hard rule, is non-negotiable and shapes the entire design:

> **We ship zero Bungie content and zero assets derived from Bungie content. The user supplies their own Marathon 1 data; all conversion happens locally on the user's machine; the converted output never enters our distribution.**

---

## 2. Legal Posture (verified)

### 2.1 The decisive facts

- Bungie released the Marathon trilogy game **data** as **freeware** — free to download and play. It is **not** open-source and was **not** placed in the public domain. Bungie's own distribution note reads, verbatim: *"Marathon is copyright 1994-present Bungie Studios, all rights reserved. Files are freely downloadable, but are not open-source."* (confirmed at `citadel.lhowon.org/litterbox/trilogy.html`).
- The Aleph One data repository's README is **explicitly a "non-license."** It states the project has a **limited license** to distribute the files, that *"Bungie has not released Marathon game content to the public under a formal, unambiguous content license,"* that *"Bungie still holds the copyrights to these files,"* and *"You can't, for example, sell them."*
- In **2021** Bungie granted **the Aleph One project specifically** a limited license to distribute the content. This grant runs to that project; **it is non-transferable** and does not extend to this project, to our installer, or to the public.
- In **May 2024** the trilogy launched **free on Steam** ("Classic Marathon," app id `2398450`, by "Aleph One Developers"), with the page stating *"Marathon® trilogy game data is distributed with permission from Bungie."* That permission, again, is to that distributor — not to us.
- The Marathon 2 **engine** source was released under GPLv2 (2000) and Marathon Infinity **engine** source under GPLv3 (2011). This covers **code, not game data.** (The often-cited CC-BY-NC-SA 3.0 association with Infinity has, per the Infinity data README, **unclear scope** and may cover only design documents, not game data — do not rely on it for M1.)

### 2.2 What this means for us — the bottom line

**"Free to download and play" is established. "Free to redistribute inside a third-party derivative work" is not granted.** We must treat the data files as proprietary, all-rights-reserved third-party material that we may neither bundle nor derive shipped assets from.

### 2.3 What we MAY distribute

- The project's own from-scratch C++/UE5 source code and Blueprints (original authorship).
- The Python asset pipeline / conversion tools (`py-marathon-utils`-based) that **read** Marathon formats and **write** engine assets. A tool that interprets a format is distributable; the copyrighted data it reads is not.
- The installer/launcher and first-run flow that **reads the user's data and runs conversion locally.**
- Format documentation derived from reverse-engineering or from understanding the GPL Aleph One source (facts and formats are not copyrightable). We use the format knowledge; we do **not** redistribute the GPL Aleph One source inside this non-GPL UE5 project.
- Genuinely **original** art, audio, music, models, and UI created by the project.
- Documentation, READMEs, and EULA text.
- Optionally, in the long term, a fully **original "Freedoom-style" replacement data set** — the only path to a self-contained distributable.

### 2.4 What we MAY NOT distribute

- Bungie's raw M1 data files: `Map.scen`, `Shapes.shps`, `Sounds.sndz`, `Physics.phys`, `Marathon.appl`, and the `Music/` tracks (or any M2 / Infinity equivalents).
- Anything **cooked or derived** from those files: extracted/converted textures, sprites, wall/landscape art, geometry generated from `Map.scen`, monster/weapon/projectile sprite sheets, HUD/terminal PICTs.
- Converted or re-encoded sound effects and music derived from `Sounds.sndz` or the original `Music/`.
- Physics/constants tables shipped **verbatim** if they reproduce Bungie's creative content. (Functional constants are weaker, but ship our own balancing rather than verbatim dumps.)
- The development-time `Reference/` output and the `M1SOURCE/` / `M2SOURCE/` / `MISOURCE/` folders — already gitignored; keep them out of **every** release artifact.
- The GPL Aleph One engine source (do not redistribute it as part of a non-GPL UE5 project).
- Any pre-converted **"asset pack"** that lets users skip supplying their own data — that would put Bungie-derived content in our distribution.
- Bungie logos, box art, or marketing assets.

### 2.5 Cautionary gray zone (NOT on the allow-list)

> **Re-themed level geometry.** Even with no Bungie bytes shipped, do **not** treat "re-themed" level layouts mechanically derived from `Map.scen` as clearly distributable. The specific arrangement of rooms and encounters can carry protectable expression. Re-author layouts as genuinely original, or treat extracted-then-retextured geometry as **risky**, not allow-listed. This is a judgment call that belongs with a human and, ideally, counsel.

### 2.6 Posture decision: NO auto-download as a recommended path

An earlier draft of this design offered "auto-download the data from the Aleph One repo" as a first-class installer option and baked consent to it into the EULA. **That has been removed.** Reasoning:

- Bungie's permission runs to the **Aleph One project**, not to our installer. An installer that fetches those files and pipes them to our users arguably inserts us into the distribution chain **on rights we do not hold.** The repo README is a self-described "non-license."
- The strictest precedents we follow — **OpenMW, OpenRCT2, Ship of Harkinian** — do **not** auto-download copyrighted data. The one precedent that does (OpenRA) can only do so because EA formally released that content as **freeware to the public** — a clean public grant we do not have for Marathon.

**Decision:** Prefer pointing the installer at the user's **existing copy** (a free Steam "Classic Marathon" install, or an existing Aleph One install). An auto-download branch is **disfavored**; see Section 6.5 for the heavily-gated form it would take if a human decides to ship it anyway.

### 2.7 Trademark

Copyright and trademark are distinct. Even with the copyright posture handled, **"Marathon" is a Bungie trademark**, and Bungie is actively shipping a new commercial *Marathon* (2025). This **elevates** brand-confusion risk. Therefore:

- Use the name "Marathon" **only nominatively** ("compatible with Marathon 1 data"), never as our product brand in a way that implies origin.
- **Prominently disclaim** affiliation and endorsement in the installer, the app, and all docs.
- Avoid Bungie marks, logos, and box art entirely.

### 2.8 Residual risks (flagged, not resolved)

- Non-commercial status is **not** a safe harbor; it reduces damages exposure, it does not immunize infringement.
- Bungie's posture can change; goodwill toward a fan port is not a license, and a new commercial *Marathon* raises enforcement likelihood against confusingly-branded fan work.
- Converting copyrighted data into engine-native assets creates a **derivative work**. Doing it locally, on the user's machine, from data they lawfully possess (the Ship of Harkinian pattern) is the defensible posture — but note that decomp/ROM-asset projects sit in an acknowledged legal **gray area**; the precedent supports the *pattern* and should not be cited as proof it is *safe*.
- **Get counsel before public release.**

---

## 3. Precedents We Follow

| Project | Posture | What we take from it |
|---|---|---|
| **OpenMW** (Morrowind) | Ships zero Bethesda content; user points engine at their own install; reads data at runtime. Explicit anti-piracy stance. | The core runtime bring-your-own-data model. |
| **OpenRCT2** (RCT2) | Engine only; user must own RCT2/RCT Classic; **no auto-download**. | The purest "you must own it" stance — our default. |
| **Ship of Harkinian** (OoT) | Ships no Nintendo content; a separate tool re-cooks the user's legally-acquired ROM into engine-native archives **on the user's machine**. | The local-conversion pattern — most relevant to us, since we also re-cook. (Note the gray-area caveat in §2.8.) |
| **devilutionX** (Diablo) | User supplies `DIABDAT.MPQ` from their own copy; supports free shareware `spawn.mpq` for a demo. | Validates a free-content fallback demo path. |
| **GZDoom / Freedoom** | Engine without IWAD; user supplies a commercial IWAD **or** the free, fully-original Freedoom. | The long-term original-content fallback (§8, Phase 4). |
| **OpenRA** (C&C) | Auto-downloads original assets on first run — but **only** because EA released them as freeware to the public (Tiberian Dawn 2007, Red Alert 2008, Tiberian Sun 2010). | A counter-example: we do **not** have the equivalent public grant, so we do **not** copy this behavior. |

---

## 4. Distribution Architecture

### 4.1 The problem: today, playing requires the full editor

The current build chain is editor-bound at every stage after the pure-Python OBJ step:

1. **Extraction (Python, no UE):** `Tools/extract_reference.py` (project venv, uses `py-marathon-utils`) reads the user's `M1SOURCE/.../{Map.scen, Shapes.shps, Sounds.sndz, Physics.phys}` → `Reference/Maps/*.json`, `Reference/Sounds/<family>/*.wav`, `Reference/Physics/`, and with `--shapes`, `Reference/Shapes/Coll_NN/bitmaps/*.png`.
2. **Geometry (Python, no UE):** `Tools/scen_to_obj.py --all Reference/Maps` (pure stdlib) → per-level `Reference/Geometry/<NN>.obj` + `.mtl` (with `map_Kd` pointing at the Shapes PNGs) + `<NN>_placements.json` (player start + object spawns in UE cm).
3. **Import + bake (REQUIRES editor):** `UnrealEditor-Cmd.exe ... -ExecutePythonScript=Tools/ue5_headless_build.py` imports each OBJ as a `StaticMesh` `.uasset`, each Shapes PNG as a `Texture2D` wrapped in an unlit `Material`, sets complex-as-simple collision, and builds/saves 37 `/Game/Levels/M1/L_NN.umap`. Companion editor scripts (`ue5_import_sprites.py`, `ue5_import_terminals.py`, etc.) import the rest.
4. **Compile + cook + pak (REQUIRES editor + .NET SDK):** `RunUAT.bat BuildCookRun ... -cook -stage -pak -build` cooks the `.uasset`/`.umap` into IoStore `.utoc`/`.ucas` and stages `Build/Windows/M1UE5.exe`.

A UE5 install is tens of GB; the editor/`BuildCookRun -build` path additionally needs the .NET Framework 4.x SDK. **It is wholly impractical to ask an end user to install any of this just to play.** The shipped binary is monolithic for *gameplay code*, but **all world content** is consumed as cooked `.uasset`/`.umap` via ~1,074 hardcoded `/Game/...` paths (e.g. `OpenLevel("/Game/Levels/M1/L_00_Arrival")`, `StaticLoadObject("/Game/Sprites/M_M1Sprite")`). So there is currently **no playable build that doesn't pass through an editor cook.**

### 4.2 Options considered

| Option | How | Bungie content in distributable | User deps | Verdict |
|---|---|---|---|---|
| **(a) Ship the cooked pak** | Distribute today's `M1UE5.exe` + cooked `.utoc/.ucas/.pak`. | **Derived** (textures from `Shapes.shps`, sounds from `Sounds.sndz`, geometry from `Map.scen`). | None — double-click. | **Rejected.** Legally disqualifying; exactly what `CLAUDE.md` and `.gitignore` forbid shipping. |
| **(b) Ship project + tools; user cooks locally with full UE** | User installs UE 5.7 + .NET SDK + Python, supplies own data, runs the full pipeline + `BuildCookRun`. | None. | Tens of GB of UE + .NET SDK + 30–60 min cook. | **Keep as the developer / from-source path only.** Unacceptable as an end-user installer. |
| **(c) Runtime-load converted intermediates (NO cook)** | Ship an `.exe` of **original code with empty world content**; a bundled frozen converter turns the user's data into a local cache of OBJ/PNG/WAV/JSON; the game builds meshes/textures/sounds **at runtime** into one persistent level. | None. | Game `.exe` + user's own data. No UE, no cook, no .NET SDK. | **RECOMMENDED.** |
| **(d) Two-pak split (signed engine pak + locally-generated content pak)** | User's machine generates a second content pak mounted via `FCoreDelegates`. | None. | See note. | **Folded into (c).** `UnrealPak`/IoStore **package** already-cooked assets; they do **not cook**. Real local cooking still needs the editor (collapses into (b)). The only viable form wraps (c)'s intermediates in a pak — little benefit over a loose cache. Treat as an optional later skin on (c). |

### 4.3 Recommendation: Option (c) — runtime-load converted intermediates

**(c) is the only option that is simultaneously (1) Bungie-content-free in the distributable and (2) free of any UE/cook dependency for the user.** It is also the natural extension of an automation-first project whose Python tools already emit exactly OBJ/PNG/WAV/JSON.

**Why it is feasible for this codebase specifically:**

- **Geometry.** `scen_to_obj.py` writes plain-ASCII OBJ (`v`/`vt`/`f`, faces grouped `usemtl` per `tex_<coll>_<shape>`) + `_placements.json` already in final UE cm. A runtime OBJ parser feeding `UProceduralMeshComponent::CreateMeshSection` (one section per material slot, UVs already authored, complex-as-simple collision built at load) consumes the **same data** the editor importer consumes today — triangle/normal handling is known-good.
- **Textures.** `extract_reference.py` writes per-collection PNGs. `FImageWrapperModule` → `UTexture2D::CreateTransient`, set `TF_Nearest` + sRGB — mirrors exactly what the editor `_import_texture` step does. No engine cook needed.
- **Materials.** The game is **already unlit/emissive** (`M1_MATMODE` default). A single committed master material + `MaterialInstanceDynamic` reproduces the look. **The biggest cook feature lost — baked lightmaps — is one the project doesn't use.** (Note: the per-room LITE lighting work uses *dynamic* movable lights + `hybrid`/`lit` materials, which also need no lightmap bake, so they remain compatible with runtime loading.)
- **Sounds.** Decode the 16-bit PCM WAV (already what `extract_reference.py` writes) to a transient `USoundWave` at runtime; sprites via the same PNG path.
- **Gameplay tables.** Already compiled into committed headers (`M1SpawnTable.h`, `M1MonsterTable.h`, `M1WeaponSpriteTable.h`, `M1HudTable.h`, `M1TerminalTable.h`, `M1PlatformTable.h`, `M1PanelTable.h`, `M1LevelEnvTable.h`, `M1LightTable.h`). No change needed.

**What must change (the engineering cost):**

- Add `ProceduralMeshComponent` and `ImageWrapper` module deps to `M1UE5.Build.cs` (both **ship with the engine** — no new third-party plugin required).
- Replace the ~1,074 cooked `/Game/...` `StaticLoadObject`/`ConstructorHelpers` references — and especially the `GM1Levels[] OpenLevel("/Game/Levels/M1/...")` flow — with **runtime loaders operating on a single persistent level** (no per-level `.umap`, no cooked `.uasset`).
- Move door / elevator / panel / enemy / light creation **fully** onto the placements-JSON + table spawn path (the table-driven `SpawnMonsters`/`SpawnDoors`/`SpawnPanels`/`SpawnLights` path already does most of this).

**Costs / risks (with mitigations):**

- **First-load hitch.** Runtime mesh/texture/collision build introduces a stall. Mitigate with an **async build** + a **one-time conversion progress screen**, then **cache** the result and skip on subsequent runs.
- **No Nanite / streaming / precomputed visibility.** M1-scale geometry is tiny; acceptable.
- **Runtime collision cook cost** per level on the `ProceduralMesh` (`CreateCollision=true`). Acceptable at M1 scale.

**Net:** a few hundred lines of loader code plus a first-run conversion flow — no new third-party plugin strictly required. **This is the recommended architecture.** Treat (d) as an optional Phase 4 packaging skin; keep (b) as the documented from-source path.

### 4.4 Verification

Per the project's testing convention, validate each phase via the in-game **`M1SelfTest`** console harness (grounded spawn, weapon fires, no holes in geometry), not manual play or screenshots. The current build passes 37/37 levels through the cooked path; the runtime path must reach parity on the same harness.

---

## 5. Installer Technology

### 5.1 Recommended: Inno Setup 6 + a separate first-run "Asset Setup" wizard

**Outer installer: Inno Setup 6 (Unicode).** Its `[Code]` section is full Pascal scripting, so the bespoke logic we need — detect an existing Marathon data folder, present a folder picker, verify checksums, run the conversion with a live progress page — fits in **one self-contained `.iss`** with no third-party plugins (NSIS needs INetC/nsExec to reach parity). It gives us a first-class `LicensePage` for the mandatory EULA, `CreateInputDirPage`/`CreateCustomPage` for data-location steps, a free MIT-style license, trivial CI scripting, a single self-contained `Setup.exe`, and a standard uninstaller in Add/Remove Programs for free.

**Why a separate first-run wizard rather than doing everything in Inno:** the heavy, slow asset conversion depends on user-supplied data that legally cannot be shipped, and its output is large and per-machine. Splitting it means the **EULA + binary install is fast and idempotent**, while the **data-dependent conversion can be re-run or repaired later without reinstalling.** The wizard (`M1Setup.exe`) can be a small self-contained .NET (WPF/WinForms) app or a second Inno wizard; it is launched at the end of install **and** added to the Start Menu so it is re-runnable.

### 5.2 Alternatives (and why not)

- **NSIS** — free and lighter output, but custom wizard pages (nsDialogs), HTTPS (INetC), and captured child-process progress (nsExec) all require assembling plugins. Good fallback for the smallest stub installer.
- **WiX / MSI** — "enterprise correct" (real MSI, ARP, repair, GPO), but MSI's declarative model fights interactive, long-running custom actions; verbose XML; no UX benefit for a hobby-game audience. Overkill.
- **Single custom .NET setup app** (no Inno wrapper) — maximum UX control, but we'd hand-roll install/uninstall, ARP entries, and elevation that Inno gives free. Reasonable **only if** we already want a .NET app and let it *be* the installer.
- **MSIX / packaged app** — sandbox and virtualized filesystem make "point at an arbitrary Marathon folder and write large converted content next to the game" painful. Not suited to user-supplied data + heavy local conversion.
- **Require a system Python venv** — rejected: forces a Python install on a gaming audience and is fragile. Freeze the tools instead (§7).

---

## 6. Installer UX Flow

Two stages: (A) the **Inno base install** (fast, offline, idempotent) and (B) the **Asset Setup wizard** (data-dependent, re-runnable).

### 6.1 Stage A — Inno base install

1. **Welcome.** Title "Marathon UE5 Remake Setup." State plainly: this is an **unofficial fan project**, **not affiliated with or endorsed by Bungie**; it requires **the user's own Marathon 1 data**; **no Bungie content is distributed by this installer.**
2. **EULA / license acceptance** (`LicensePage`). Show the project's own source license **and** the Marathon-data ownership notice. "I accept" is **required** to enable Next; block continuation until accepted. (EULA content in §6.6.)
3. **Choose install location** (`DirPage`) for the game + frozen tools + where converted content will be written. **Warn on low disk space** (converted assets are large; show an estimate).
4. **Install the payload.** Copy the packaged UE5 build (original code, empty world content), the bundled frozen conversion tools (onedir), and the UE prerequisite redists. Run `UEPrereqSetup_x64.exe` / `vc_redist.x64.exe` **silently** if not already present.
5. **Finish base install.** Offer "Run Marathon data setup now" (checked by default), which launches the Asset Setup wizard. Add the wizard to the Start Menu for re-runs.

### 6.2 Stage B — Asset Setup wizard

6. **Step A — Detect.** Auto-scan likely locations for a folder containing all four required files (`Map.scen` + `Shapes.shps` + `Sounds.sndz` + `Physics.phys`), in order: a path remembered in the registry from a prior run; a free **Steam "Classic Marathon"** install (app `2398450`); any existing **Aleph One** scenario folder; the project's expected `Marathon-YYYYMMDD` layout; common Downloads/Documents folders. A folder qualifies **only if all four files exist and their SHA-256 matches the pinned manifest.** If found, skip to Convert.
7. **Step B — Locate.** If not auto-detected, present a choice: **(a) "Locate my files"** opens a native folder picker (`BrowseForFolder`) and re-validates the four files on selection, with a clear error if any are missing or hash-mismatched. (**(b)** the download path — disfavored — appears only if the human decision in §9 enables it; see §6.5.)
8. **Step C — Verify.** Verify every located file against the **pinned SHA-256 manifest** shipped inside the installer. **Hard-fail** on mismatch with a clear message and a retry / re-locate option (catches both corruption and wrong-version data).
9. **Step D — Convert.** Run the bundled converter (`extract_reference.py` → `scen_to_obj.py`, exposed as **one frozen entrypoint** — see §7) against the located data, streaming machine-readable progress into a UI with a **stage label** ("Extracting sounds 42/104", "Converting level 12/37") and a log pane. On success, write the source path + a **conversion-complete marker and manifest** to a per-user location.
10. **Done.** "Setup complete." Buttons: **"Launch Marathon UE5"** (runs the game `.exe`) and **"Finish."** The verified source path is persisted so future repair/re-run can skip re-locating.

### 6.3 Idempotency, repair, re-run

- The wizard is **idempotent and Start-Menu re-launchable.** On re-run it reads the conversion marker/version and **skips stages whose inputs/outputs are unchanged**, offering a **"Reconvert / Repair assets"** action (e.g. after a game update bumps the asset format).
- The base Inno installer also supports re-run for **binary repair.**

### 6.4 Uninstall

- Standard Inno uninstaller in Add/Remove Programs.
- **Default:** remove installer-written files, but **prompt** before deleting large user-generated converted content. **Never touch the user's original Marathon data folder.** Clean up the remembered-path registry key.

### 6.5 The download branch (DISFAVORED — ship only on explicit human decision)

Per §2.6, auto-download is **not** a recommended option. **If** a human decides to ship it anyway, it must be the most heavily-gated path in the wizard:

- No network request fires without an **explicit, unchecked-by-default** "Download Marathon 1 data now" consent checkbox.
- Consent text must state **what** is downloaded, **from where** (`github.com`), and that it is **Bungie content** made freely available for non-commercial use via Aleph One, and that the **user affirms a lawful right** to it.
- Fetch from a **pinned commit/tag** of the official `Aleph-One-Marathon/data-marathon` repo tree (e.g. `https://codeload.github.com/Aleph-One-Marathon/data-marathon/zip/refs/tags/<tag>`, or `raw.githubusercontent.com/.../<commit>/<file>` per file) — **never a moving branch**, so the pinned checksums stay valid. **Note:** the repo's GitHub *Releases* host only hi-res texture plugins (e.g. `TTEP.v7.zip`) — **not** the game data; the data lives in the repo **tree** (treat the specific release-asset filenames as unverified).
- **Never re-host or mirror** the files. Verify the download against the pinned SHA-256 manifest and refuse on mismatch.
- Frame it in copy and logs as **the user's action**, not the project's distribution.

> **Default build ships with this branch disabled.** Prefer detect/locate (the user's existing/Steam copy) exclusively.

### 6.6 EULA — required key points

The acceptance gate must present and record consent to:

1. **You own or have a lawful right** to a copy of Bungie's Marathon game data (e.g. free from Steam "Classic Marathon," the Aleph One project, or an existing legitimate installation).
2. **Marathon and all original game data are copyright Bungie, all rights reserved**; the data is Bungie freeware, which is **not** a grant of rights to redistribute it within third-party works.
3. This is an **unofficial, non-commercial fan project, not affiliated with, endorsed by, or sponsored by Bungie**; "Marathon" and related marks are trademarks of their owner, used **only nominatively** to describe compatibility.
4. You consent to this software **reading data from your copy and converting it LOCALLY** on your machine to generate game-ready assets that **remain on your machine.** *(Include the download clause **only** if the §6.5 branch is enabled in this build.)*
5. Any assets generated from the data are for your **personal, non-commercial use only**; you may **not** redistribute, share, sell, or publish them or this project bundled with them.
6. The software is provided **AS IS**, without warranty; authors disclaim liability and are not responsible for how you acquire the underlying data.
7. You will **not** use this software to facilitate piracy; if you do not lawfully possess the data, obtain it free from an official source (links provided) before proceeding.
8. A checkbox recording that you have read and agree, captured **before** any conversion (or download) runs.

---

## 7. Prerequisites

- **Freeze the Python tools — yes.** The audience is gamers, not Python users. Freeze the conversion pipeline with **PyInstaller** (or Nuitka) so no system Python/pip is required. Bundle `py-marathon-utils[images]` (Pillow) and `soundfile` (per `requirements.txt`). **`soundfile` pulls in the native `libsndfile` DLL — it must be included in the bundle; verify it loads from the frozen tree.**
- **Use PyInstaller `--onedir`, not `--onefile`.** Onefile's self-extracting compressed stub is a well-known **antivirus false-positive trigger** and adds startup extraction time; onedir is far less likely to be flagged and starts faster. **Nuitka** is a strong alternative (native C, fewer AV heuristics) if PyInstaller flags persist; cx_Freeze is a third option. **Avoid UPX.**
- **Expose ONE freeze entrypoint** that drives `extract_reference` → `scen_to_obj` with `--source` and `--dest` args and **machine-readable progress on stdout**, rather than freezing many scripts. The wizard shells out to this single exe.
- **CRITICAL constraint — editor-bound import scripts cannot be frozen.** Several `Tools/ue5_import_*.py` scripts run **inside the UE5 editor's Python** (`UnrealEditor-Cmd`), not standalone CPython. They depend on the editor and cannot be PyInstaller-frozen. **Resolution under architecture (c):** these editor steps **disappear** from the user's machine — runtime loaders replace them. The frozen installer tools perform **only** the non-engine, data-derived conversions (`extract_reference` + `scen_to_obj` → OBJ/PNG/WAV/JSON). **This split is precisely what makes (c) shippable and is a primary reason (c) is recommended over (b).**
- **UE game prerequisites.** Bundle and silently run Unreal's redists: `UEPrereqSetup_x64.exe` and/or `vc_redist.x64.exe` (from `Engine/Extras/Redist/en-us`). DirectX runtime is covered by the UE prereq setup. **No .NET requirement for the game itself** (the .NET SDK was only needed for the editor cook, which (c) eliminates). If the wizard is written in .NET, use a **self-contained / AOT publish** so no framework install is needed.
- **Pin everything** for reproducibility: the `py-marathon-utils` version, the SHA-256 manifest, the frozen-tools build, and (if the §6.5 branch ships) a fixed Aleph One data commit/tag.

---

## 8. Implementation Roadmap (MVP first)

Each phase is independently testable via the `M1SelfTest` harness. **The architecture work (Phases 0–1) is the critical path; the installer (Phases 2–3) is meaningless until the game can run from intermediates.**

### Phase 0 — Runtime-loader proof of concept (MVP core) — ✅ DONE
- ✅ Added `ProceduralMeshComponent` + `ImageWrapper` (+ `RenderCore`/`RHI`) to `M1UE5.Build.cs`.
- ✅ `AM1RuntimeLevel` (`Source/M1UE5/M1RuntimeLevel.{h,cpp}`): runtime **OBJ → `UProceduralMeshComponent`** — parses `v`/`vt`/`f`, one section per `usemtl`, **negates Y + reverses winding** to match the editor OBJ import, computes normals/tangents, builds complex collision at load.
- ✅ **PNG → `UTexture2D`** via `FImageWrapperModule` (BGRA8, nearest, sRGB), deduped; per-surface unlit `MaterialInstanceDynamic` over the committed master `/Game/Runtime/M_M1Runtime` (`Tools/ue5_make_runtime_material.py`) with the `_b<pct>` emissive bucket (coll 8 screens full-bright) — mirrors the cooked look.
- ✅ Data root: env `M1_DATA_ROOT` → `<ProjectDir>/Reference` → dev tree (Phase 2 points it at the per-user AppData cache).
- ✅ Exec **`M1Runtime <stem>`** + `M1_RUNTIME_SELFTEST=<stem>` BeginPlay hook (real-time ticker, fires through the chapter-splash pause).
- **VERIFIED (packaged build):** `[M1Runtime] SELFTEST 00_Arrival: PASS — 92 sections, 5200 verts, 2738 tris, extent 1347x1629x390 cm`, built entirely at runtime from OBJ/PNG on disk, no crash.
- **Remaining for the full exit (folds into Phase 1):** make the runtime mesh the level the player spawns into (vs building alongside the cooked map), so `M1SelfTest`'s grounded-spawn / no-holes / weapon checks run on it.

### Phase 1 — Generalize all 37 levels (MVP complete) — ✅ DONE (core)
- ✅ **Runtime mode**: `AM1Character::GRuntimeLevelName`/`GRuntimeLevelIndex` (survive level travel) + `M1LevelKey()` route every `Contains()`-based table lookup (terminals, env/vacuum, checkpoint index, level cycling, full-save index) off the campaign name while the persistent map is the blank **`L_Runtime`** (`Tools/ue5_make_runtime_map.py`, global-default `M1GameMode`).
- ✅ **Entry**: `M1EnterRuntimeLevel(idx)` (exec `M1RunLevel <idx|stem>`, env `M1_RUNLEVEL`) sets the name + travels to `L_Runtime`; `[`/`]` cycling and exit-terminal `AdvanceLevel` stay in runtime mode once entered.
- ✅ **Build**: `AM1GameMode::BeginPlay` -> `BuildRuntimeGeometry()` builds the level's geometry (`AM1RuntimeLevel`) before the table-driven actors spawn; `AM1RuntimeLevel::ResolveGeoBasename` matches the OBJ/placements by the reliable `NN_` index prefix (the cooked-map and OBJ names sanitize punctuation differently). Player teleports onto `placements.player_start` on the first Tick.
- ✅ Checkpoints unchanged (store the level **index**); `M1CurrentLevelIndex` returns the runtime index so Continue / save-anywhere still resolve.
- **VERIFIED (packaged build)**: `M1_RUNLEVEL` sweep built `00_Arrival` (92 sec/2738 tri), `05_The_Rose` (93/2672), `13_Colony_Ship` (46/2866), `20_Beware_of_Low-Flying…` (55/2518), `28_Carnage_Palace` (28/1008), `36_…I_D` (61/1560) — all entered runtime mode, traveled to `L_Runtime`, and the GameMode built the geometry at runtime, no crash. (Geometry is read from the dev `Reference/` tree; Phase 2 swaps that for the per-user AppData cache.)
- **Remaining**: WAV->`USoundWave` runtime loader (sounds still load cooked `/Game/Audio`, which is fine while shipping cooked audio); making runtime mode the campaign default (currently opt-in via exec/env, cooked path remains the default) once playtested; a `M1SelfTest` grounded/no-holes pass walking the runtime mesh.
- Spawn doors / elevators / panels / enemies / lights from placements JSON + the committed `*Table.h` (extend the existing `SpawnMonsters`/`SpawnDoors`/`SpawnPanels`/`SpawnLights` path).
- Add runtime **WAV → `USoundWave`**; route `/Game/Audio` loads through it.
- Confirm `UM1SaveGame`'s level-index scheme still resolves under the runtime-level identifiers (see §9).
- **Exit criterion:** `M1SelfTest` 37/37 parity with the cooked build; existing checkpoints resolve.

### Phase 2 — First-run conversion UX — ✅ DONE (core)
- ✅ **Asset Setup wizard** `Tools/m1_asset_setup.py`: locate-only (explicit `--source`,
  else `M1SOURCE` / `M1_SOURCE`/`ALEPHONE_DATA` env; never downloads), **SHA-256**
  verifies the four data files, converts via `extract_reference --shapes` +
  `scen_to_obj --all` into the per-user cache, writes `.m1_assets_done` (the manifest +
  skip marker: source hashes, py-marathon-utils version, tools commit, level count).
  Emits `@PROGRESS <json>` lines for the installer GUI; `--force` reconverts.
- ✅ **Per-user cache wiring**: `AM1RuntimeLevel::ResolveDataRoot` now prefers
  `%LOCALAPPDATA%\M1UE5\Data` (then env override, then in-project `Reference/`), keyed on
  the cache actually holding `Geometry/`. No admin elevation.
- ✅ **Frozen converter**: `Tools/m1_asset_setup.spec` (PyInstaller `--onedir`) bundles
  py-marathon-utils + Pillow + soundfile/libsndfile -> `M1AssetSetup.exe` (no Python on
  the user's machine).
- **VERIFIED**: the wizard converted `M1SOURCE` -> `%LOCALAPPDATA%\M1UE5\Data` (37
  levels + Shapes/Sounds/Physics/Maps + manifest), and the **packaged game built
  `00_Arrival` at runtime from that AppData cache** (`built '00_Arrival' from
  C:/Users/.../AppData/Local/M1UE5/Data`), with no dev `Reference/` and no env override.
- **Remaining**: the first-run hand-off is currently manual (run the wizard, then the
  game finds the cache) -- Phase 3's Inno installer runs `M1AssetSetup.exe` during setup
  and the game shows a "data not found, run setup" message if the cache is missing.

### Phase 3 — Inno installer + distribution polish — ✅ DONE (authored; compile/VM = packaging machine)
- ✅ **`.iss` authored** (`Tools/installer/M1UE5.iss`): per-user/no-admin install
  (`PrivilegesRequired=lowest`, so the `%LOCALAPPDATA%` cache + saves stay writable),
  Welcome + **EULA gate** (`EULA.txt`), a **locate-only** custom data page
  (`CreateInputDirPage`, validates `Map.scen`, blank = defer), installs the game +
  the frozen `M1AssetSetup` + (optional) UE/VC redists, runs `M1AssetSetup.exe
  --source <chosen>` during setup, Start-Menu + desktop icons, **Run Asset Setup**
  shortcut, launch option, uninstaller.
- ✅ **EULA** (`Tools/installer/EULA.txt`): open-source / non-commercial, **no Bungie
  data included**, bring-your-own-data + user responsibility, no warranty.
- ✅ **In-game safety net**: `AM1RuntimeLevel::IsDataAvailable()`; on a fresh install
  with no cache, `AM1GameMode::BuildRuntimeGeometry` shows
  `AM1PlayerController::ShowDataMissing()` ("Marathon data not found — run Asset
  Setup") instead of an empty level. Built + regression-verified (the data-present
  path still builds from the cache).
- ✅ **Build tooling**: `Tools/installer/build_installer.ps1` (verifies payload, finds
  `iscc.exe`, compiles, prints the Setup.exe SHA-256) + `Tools/installer/README.md`
  (build steps, code-signing options, the clean-VM exit criterion).
- **Code signing**: pursue **SignPath Foundation / OSSign** (free OSS certs); else
  publish the installer SHA-256 + document the SmartScreen "Run anyway" bypass.
- ✅ **COMPILED + run end-to-end** (Inno Setup 6 via winget): `build_installer.ps1`
  emits `M1UE5-Setup.exe` (~461 MB). A **real silent install against Steam's "Classic
  Marathon" data** was verified on a dev box: `M1UE5-Setup.exe /VERYSILENT
  /DATADIR="…\Classic Marathon"` → per-user/no-admin install → the bundled
  `M1AssetSetup.exe` converted that data to `%LOCALAPPDATA%\M1UE5\Data` (37 levels +
  manifest) **during setup** → the installed game built Arrival at runtime from the
  cache → the uninstaller cleanly removed it. (The run caught + fixed a silent-mode bug:
  the custom data page now pre-fills from `/DATADIR` and is skipped when `WizardSilent`.)
- **Remaining = a literal clean VM** (no UE / no Python) for the formal exit criterion —
  can't provision one here, but the install is per-user/no-admin and the converter is a
  frozen no-Python bundle (Phase 2), so the dev-box run exercises the same paths.

### Phase 4 — Optional packaging & original-content track — framework DONE; full art = long-term
- **(4a) pak skin — SKIPPED (by design).** The loose per-user cache performs fine (verified:
  levels build at runtime in well under a second). `UnrealPak`/IoStore only *packages*
  already-cooked assets — it can't cook the loose intermediates — so wrapping them in a
  pak adds build complexity for ~no benefit. Revisit only if loose-file I/O ever bites.
- **(4b) original-content track — FRAMEWORK + first art step done.**
  - ✅ **Self-contained data path**: `AM1RuntimeLevel::ResolveDataRoot` adds a **bundled**
    tier (`<install>\M1Data`, between the user's AppData cache and the dev tree), so a
    release can ship its own original/re-themed data set and run with **no user data**.
  - ✅ **Clean-room texture generator** `Tools/gen_original_textures.py`: emits ORIGINAL,
    deterministic, **non-derived** wall/floor textures for exactly the (collection, shape)
    IDs the converted levels reference (read from the cache MTLs) into a Shapes-compatible
    layout. Verified: 133 textures across the 6 wall collections. (Placeholder original art
    — a real art pass swaps in hand-authored originals through the identical pipeline.)
  - ✅ **Runtime audio from data** (also removes cooked Bungie SFX from the picture):
    `AM1RuntimeLevel::LoadRuntimeSound` decodes the cache's `Sounds/<id>.wav` (mapped via
    the shipped `sound_names.json`) into transient `USoundWave`s; weapon + monster SFX
    route through it with a cooked fallback. Verified: 20 cache WAVs resolved + decoded.
  - **Remaining for a TRUE zero-Bungie-IP release** (long-term content work, not code):
    original geometry/level re-authoring (the shape is functional/weakly-protected but
    should be re-themed), original sounds + music + terminal art, then assemble an
    `M1Data` bundle from originals only. Counsel review of the geometry posture advised.

---

## 9. Open Decisions (require a human)

**Legal / policy**
1. **Auto-download branch (§6.5):** ship it disabled (recommended), or enable the heavily-gated form? *Recommendation: disabled by default.*
2. **Re-themed geometry (§2.5):** confirm that runtime-built layouts (M1 shape rebuilt from the user's own data, on their machine) stay in the "derived locally from the user's own data" zone, and decide whether the eventual original-art/level-rethemeing pass changes the architecture. Counsel input advised.
3. **Code signing (§ Phase 3):** pursue SignPath/OSSign free OSS signing, buy a low-cost OV cert (~$130–300/yr), or ship unsigned + document the bypass until reputation builds?

**Architecture / engineering**
4. **Lighting model:** the per-room `LITE` lighting work commits to **dynamic movable point lights + `hybrid` materials** (no lightmap bake), which is compatible with runtime loading. Confirm this stays the direction vs. a flatter unlit-everywhere MVP.
5. **Converter delivery:** frozen Python (PyInstaller `--onedir`, recommended), Nuitka, or eventually port OBJ/PNG/WAV conversion to C++ for a true zero-Python claim?
6. **Conversion timing:** pre-convert all 37 levels up front (longer one-time wait, simplest runtime) vs lazily per level (faster first launch, more async-build complexity)? Define an acceptable first-run / level-load latency budget.
7. **Content-pak packaging (Phase 4a):** build it at all, or is a loose per-user intermediates cache sufficient? *Recommendation: defer; `UnrealPak` can wrap but cannot cook, so it buys little.*
8. **Where converted content lives:** under the install dir, or per-user `AppData`/`Documents` (avoids elevation for re-conversion when installed to `Program Files`)? Keep source data untouched and separate; store the manifest alongside output.
9. **Save-game compatibility:** confirm the runtime-level identifier scheme stays stable across the refactor so existing `UM1SaveGame` checkpoints (which store a level **index**) still resolve.

---

## Appendix A — Tooling reference

| Tool | Runs in | Role | Shippable? |
|---|---|---|---|
| `Tools/extract_reference.py` (`py-marathon-utils`) | Standalone CPython (venv) | `Map.scen`/`Shapes.shps`/`Sounds.sndz`/`Physics.phys` → JSON / WAV / PNG | **Yes — frozen** (reads format; writes intermediates) |
| `Tools/scen_to_obj.py` | Standalone CPython (stdlib) | Maps → per-level OBJ/MTL + `_placements.json` (UE cm) | **Yes — frozen** |
| `Tools/gen_*.py` | Standalone CPython | Generate committed `*Table.h` + original source art | Dev-time (outputs are committed) |
| `Tools/ue5_headless_build.py`, `ue5_import_*.py` | **Inside `UnrealEditor-Cmd`** | OBJ/PNG → `.uasset`/`.umap`, cook | **No — editor-bound; eliminated by architecture (c)** |
| `py-marathon-utils` | Library | Reads/writes Marathon 1/2/Infinity formats | **Yes** (bundled into the freeze) |

## Appendix B — Required M1 data files (user-supplied, never shipped)

`Map.scen` · `Shapes.shps` · `Sounds.sndz` · `Physics.phys` (plus optionally `Marathon.appl`, `Music/`). A data folder qualifies only when **all four** are present **and** their SHA-256 matches the pinned manifest. Legitimate sources to point users at: a free **Steam "Classic Marathon"** install (app `2398450`), an existing **Aleph One** install, or the official **`Aleph-One-Marathon/data-marathon`** repo tree. The installer **detects/locates**; it does **not** bundle or (by default) download these files.
