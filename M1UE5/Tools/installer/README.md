# Marathon UE5 installer (Inno Setup) — Phase 3

A per-user (no-admin) `Setup.exe` that installs the runtime-loading game, gates on the
EULA, lets the user **locate** their own Marathon 1 data (never downloads it), runs the
frozen converter into `%LOCALAPPDATA%\M1UE5\Data`, and launches the game.

## Files
- `M1UE5.iss` — the Inno Setup script (Welcome → EULA → locate-data page → install →
  prerequisites → convert → launch → uninstaller).
- `EULA.txt` — the agreement the wizard requires accepting (open-source, non-commercial,
  bring-your-own-data, no warranty).
- `build_installer.ps1` — verifies the payload, finds `iscc.exe`, compiles, prints the
  output path + its SHA-256.
- `redist/` *(you add this)* — `UEPrereqSetup_x64.exe` and/or `vc_redist.x64.exe` from
  `<Engine>\Engine\Extras\Redist\en-us`. Bundled + run silently if present; skipped if not.

## Build steps (on the packaging machine)
1. **Game payload** — cook + exe-swap so `Build\Windows\M1UE5.exe` is current.
   Since runtime mode is the campaign default, the **release cook ships only the menu
   + the blank runtime map** (no Bungie-baked per-level geometry in the pak):
   `RunUAT BuildCookRun … -cook "-map=L_MainMenu+L_Runtime" -stage -pak -archive -archivedirectory=Build`
   then copy the fresh `Binaries\Win64\M1UE5.exe` over
   `Build\Windows\M1UE5\Binaries\Win64\M1UE5.exe`. (Levels build at runtime from the
   per-user cache; `/Game/Sprites|Audio|UI|Runtime` still cook via DirectoriesToAlwaysCook.)
2. **Frozen converter** —
   `pip install pyinstaller py-marathon-utils[images] soundfile`
   `cd Tools && pyinstaller m1_asset_setup.spec --noconfirm --distpath ..\Build\AssetSetup\dist --workpath ..\Build\AssetSetup\work`
3. **(optional) redist** — copy the UE/VC redists into `Tools\installer\redist\`.
4. **Compile** — `Tools\installer\build_installer.ps1` → `Build\Installer\M1UE5-Setup.exe`.

## Install modes
- **Interactive** — `M1UE5-Setup.exe`: Welcome → EULA → pick the data folder → install.
- **Silent / scripted** — `M1UE5-Setup.exe /VERYSILENT /DATADIR="<Marathon data folder>"`
  installs without prompts and converts that data during setup (the `.iss` reads
  `{param:DataDir}`). Omit `/DATADIR` to defer (the in-game "run Asset Setup" screen
  + the Start-Menu shortcut cover it). `/LOG="setup.log"` captures a setup transcript.

## Code signing / SmartScreen
The installer is unsigned by default, so Windows SmartScreen shows "Windows protected
your PC". Options, best first:
1. **Free OSS signing** — apply to the **SignPath Foundation** (free certs for OSS) or
   OSSign; sign `M1UE5-Setup.exe` (and `M1AssetSetup.exe`) as a post-build step.
2. **Paid OV/EV** code-signing cert.
3. **Unsigned + transparency** — publish the **SHA-256** (printed by `build_installer.ps1`)
   on the download page and document the "More info → Run anyway" bypass.

## Verification (Phase 3 exit criterion)
On a **clean Windows VM** with no UE, no Python, and no dev tree:
`Setup.exe` → accept EULA → pick the Marathon data folder → install → the converter
runs → the game launches and a level builds at runtime from `%LOCALAPPDATA%\M1UE5\Data`.
If the data folder is left blank, the game shows "Marathon data not found — run Asset
Setup" (`AM1PlayerController::ShowDataMissing`), and the Start-Menu **Run Asset Setup**
shortcut completes setup.

> **Legal:** the EULA is a reasonable fan-project posture, not legal advice. Have counsel
> review it before any public release (see `docs/INSTALLER_DESIGN.md` §2 / open decisions).
