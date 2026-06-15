<#
  build_installer.ps1 -- assemble + compile the Marathon UE5 Setup.exe (Phase 3).

  Prereqs (must already exist):
    * Build\Windows\M1UE5.exe                     -- the cooked, exe-swapped game
        (RunUAT BuildCookRun ... then exe-swap the fresh Binaries\Win64\M1UE5.exe)
    * Build\AssetSetup\dist\M1AssetSetup\M1AssetSetup.exe  -- the frozen converter
        (cd Tools; pyinstaller m1_asset_setup.spec --noconfirm
                   --distpath ..\Build\AssetSetup\dist --workpath ..\Build\AssetSetup\work)
    * Inno Setup 6 (iscc.exe) on PATH or in Program Files.
    * (optional) Tools\installer\redist\UEPrereqSetup_x64.exe / vc_redist.x64.exe
        from <Engine>\Engine\Extras\Redist\en-us -- bundled + run silently if present.

  Run:  Tools\installer\build_installer.ps1
  Out:  Build\Installer\M1UE5-Setup.exe  (+ its SHA-256, to publish on the download page)
#>
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$proj = (Resolve-Path "$here\..\..").Path        # M1UE5\
$game = Join-Path $proj 'Build\Windows'
$wiz  = Join-Path $proj 'Build\AssetSetup\dist\M1AssetSetup'

if (-not (Test-Path (Join-Path $game 'M1UE5.exe'))) {
  throw "Game payload not found at $game\M1UE5.exe. Cook + exe-swap first (see docs/INSTALLER_DESIGN.md Phase 0-1)."
}
if (-not (Test-Path (Join-Path $wiz 'M1AssetSetup.exe'))) {
  throw "Frozen converter not found at $wiz. Build it: cd Tools; pyinstaller m1_asset_setup.spec --noconfirm --distpath ..\Build\AssetSetup\dist --workpath ..\Build\AssetSetup\work"
}

# Locate Inno Setup's iscc.exe.
$iscc = (Get-Command iscc.exe -ErrorAction SilentlyContinue).Source
if (-not $iscc) {
  foreach ($c in @(
      "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
      "${env:ProgramFiles}\Inno Setup 6\ISCC.exe",
      "${env:LOCALAPPDATA}\Programs\Inno Setup 6\ISCC.exe")) {  # winget per-user install
    if (Test-Path $c) { $iscc = $c; break }
  }
}
if (-not $iscc) {
  throw "Inno Setup 6 not found. Install it (https://jrsoftware.org/isdl.php) so iscc.exe is on PATH."
}

if (-not (Test-Path (Join-Path $here 'redist'))) {
  Write-Warning "No redist\ folder -- the installer will skip UE/VC prerequisites. Copy UEPrereqSetup_x64.exe + vc_redist.x64.exe from <Engine>\Engine\Extras\Redist\en-us into Tools\installer\redist\ to bundle them."
}

Write-Host "Compiling installer with $iscc ..."
& $iscc (Join-Path $here 'M1UE5.iss')
if ($LASTEXITCODE -ne 0) { throw "iscc failed ($LASTEXITCODE)" }

$setup = Join-Path $proj 'Build\Installer\M1UE5-Setup.exe'
if (Test-Path $setup) {
  $sha = (Get-FileHash $setup -Algorithm SHA256).Hash
  Write-Host "`nBuilt: $setup"
  Write-Host "SHA-256: $sha   (publish this on the download page for SmartScreen-wary users)"
}
