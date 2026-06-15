; ===========================================================================
;  Marathon UE5 -- Inno Setup script (installer Phase 3)
;
;  Builds a per-user (no-admin) Setup.exe that:
;    1. shows a Welcome page + the EULA gate (must accept),
;    2. lets the user locate their own Marathon 1 data (LOCATE-ONLY; no download),
;    3. installs the game + the frozen Asset Setup converter,
;    4. installs UE prerequisites (if the redist is bundled),
;    5. runs M1AssetSetup.exe on the located data -> %LOCALAPPDATA%\M1UE5\Data,
;    6. offers to launch the game; provides an uninstaller.
;
;  Build:  iscc M1UE5.iss              (from this folder, after staging the payload)
;          Tools\installer\build_installer.ps1 automates staging + the iscc call.
;  Output: ..\..\Build\Installer\M1UE5-Setup.exe
;
;  Payload roots (override on the command line, e.g.  iscc /DGameDir=...  ):
; ===========================================================================
#define MyAppName    "Marathon UE5"
#define MyAppExe     "M1UE5.exe"
#define MyAppVersion "0.3.0"
#ifndef GameDir
  #define GameDir       "..\..\Build\Windows"
#endif
#ifndef AssetSetupDir
  #define AssetSetupDir "..\..\Build\AssetSetup\dist\M1AssetSetup"
#endif

[Setup]
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher=Marathon UE5 (open-source fan project)
DefaultDirName={autopf}\M1UE5
DefaultGroupName=Marathon UE5
DisableProgramGroupPage=yes
LicenseFile=EULA.txt
OutputDir=..\..\Build\Installer
OutputBaseFilename=M1UE5-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Per-user install: no admin, and %LOCALAPPDATA%\M1UE5\Data stays writable for
; the converter + save games (the locked Phase 2 storage decision).
PrivilegesRequired=lowest

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional icons:"

[Files]
; The cooked, exe-swapped game (Development). Staged into Build\Windows.
Source: "{#GameDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion
; The frozen Asset Setup converter (PyInstaller --onedir; no Python on the user's PC).
Source: "{#AssetSetupDir}\*"; DestDir: "{app}\AssetSetup"; Flags: recursesubdirs createallsubdirs ignoreversion
; Optional UE prerequisite redist -- drop it in this folder's redist\ before building.
Source: "redist\UEPrereqSetup_x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall skipifsourcedoesntexist
Source: "redist\vc_redist.x64.exe";     DestDir: "{tmp}"; Flags: deleteafterinstall skipifsourcedoesntexist

[Icons]
Name: "{group}\Marathon UE5";          Filename: "{app}\{#MyAppExe}"
Name: "{group}\Run Asset Setup";       Filename: "{app}\AssetSetup\M1AssetSetup.exe"
Name: "{group}\Uninstall Marathon UE5"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Marathon UE5";    Filename: "{app}\{#MyAppExe}"; Tasks: desktopicon

[Run]
; UE / VC prerequisites (silent), only if the redist was bundled.
Filename: "{tmp}\UEPrereqSetup_x64.exe"; Parameters: "/quiet"; StatusMsg: "Installing prerequisites..."; Flags: skipifdoesntexist waituntilterminated
Filename: "{tmp}\vc_redist.x64.exe";     Parameters: "/install /quiet /norestart"; StatusMsg: "Installing prerequisites..."; Flags: skipifdoesntexist waituntilterminated
; One-time conversion of the located Marathon data into the per-user cache. Only runs
; if the user picked a valid folder; otherwise they run Asset Setup later.
Filename: "{app}\AssetSetup\M1AssetSetup.exe"; Parameters: "--source ""{code:GetDataDir}"""; StatusMsg: "Converting your Marathon data (one time, may take a minute)..."; Flags: runhidden waituntilterminated; Check: DataDirChosen
; Launch.
Filename: "{app}\{#MyAppExe}"; Description: "Launch Marathon UE5"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Code]
var
  DataPage: TInputDirWizardPage;

procedure InitializeWizard;
begin
  // A locate-only data page (no download). Optional: the user may skip it and run
  // Asset Setup later -- the game shows a "data not found, run setup" screen until then.
  DataPage := CreateInputDirPage(wpSelectDir,
    'Locate your Marathon 1 data',
    'Marathon UE5 includes none of Bungie''s game data.',
    'Point Setup at the folder containing Map.scen, Shapes.shps, Sounds.sndz and '+
    'Physics.phys (the freeware Marathon 1 data from the Aleph One project). '+
    'You may leave this blank and run "Asset Setup" later from the Start Menu.',
    False, '');
  DataPage.Add('Marathon 1 data folder (optional):');
  // Pre-fill from /DATADIR so the page shows it (interactive) and is non-empty
  // (avoids the input-dir validation when proceeding).
  DataPage.Values[0] := Trim(ExpandConstant('{param:DataDir|}'));
end;

// In a silent install the page isn't shown; skip it so its (possibly empty) value
// doesn't trip the built-in "enter a full path" validation. /DATADIR still drives
// the conversion via ResolvedDataDir.
function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := WizardSilent and (PageID = DataPage.ID);
end;

// The chosen data folder: the wizard page if set, else the /DATADIR=<path> command
// line (so a silent/scripted install -- /VERYSILENT /DATADIR=... -- can convert too).
function ResolvedDataDir: string;
begin
  Result := Trim(DataPage.Values[0]);
  if Result = '' then
    Result := Trim(ExpandConstant('{param:DataDir|}'));
end;

function GetDataDir(Param: string): string;
begin
  Result := ResolvedDataDir;
end;

function DataDirChosen: Boolean;
var Dir: string;
begin
  Dir := ResolvedDataDir;
  Result := (Dir <> '') and FileExists(AddBackslash(Dir) + 'Map.scen');
end;

// Validate the folder when the user leaves the data page (allow blank = defer).
function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = DataPage.ID) and (Trim(DataPage.Values[0]) <> '') then
  begin
    if not FileExists(AddBackslash(DataPage.Values[0]) + 'Map.scen') then
    begin
      MsgBox('That folder does not contain Map.scen.' + #13#10 +
             'Choose the folder with your Marathon 1 data, or leave it blank to ' +
             'run Asset Setup later.', mbError, MB_OK);
      Result := False;
    end;
  end;
end;
