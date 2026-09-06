#define AppName "TuinDoom RT"
#define AppVersion "1.4.7.9"
#define RepoRoot SourcePath + "\.."

[Setup]
AppId={{7A96D15E-3C3B-49B0-A529-AB8DA90970C5}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=TuinDoom RT Project
DefaultDirName={localappdata}\Programs\TuinDoom RT
DisableDirPage=no
DefaultGroupName=TuinDoom RT
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir={#RepoRoot}\dist
OutputBaseFilename=TuinDoomRT-Setup-{#AppVersion}
SetupIconFile={#RepoRoot}\launcher\TuinDoomRT\assets\TuinDoomRT-v2.ico
UninstallDisplayIcon={app}\TuinDoomRT.exe
LicenseFile={#RepoRoot}\LICENSE
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"; Flags: unchecked

[Files]
Source: "{#RepoRoot}\build\launcher\TuinDoomRT.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#RepoRoot}\build\launcher\Assets\*"; DestDir: "{app}\Assets"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#RepoRoot}\build\test-release\*"; DestDir: "{app}\Game"; Excludes: "*.log,gzdoom*.exe,rt\scenes\*,Mods\*"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#RepoRoot}\build\test-release\gzdoom-stable.exe"; DestDir: "{app}\Game"; DestName: "gzdoom.exe"; Flags: ignoreversion
Source: "{#RepoRoot}\compat\spider-death\zscript\tuindoom\spider_death.zs"; DestDir: "{app}\Game\rt\wad\zscript\tuindoom"; Flags: ignoreversion
Source: "{#RepoRoot}\compat\spider-death\filter\doom.id.doom2\zscript.zc"; DestDir: "{app}\Game\rt\wad\filter\doom.id.doom2"; Flags: ignoreversion
Source: "{#RepoRoot}\installer\game\tuindoom-bindings.cfg"; DestDir: "{app}\Game"; Flags: ignoreversion
Source: "{#RepoRoot}\compat\realistic-lights\textures.lmp"; DestDir: "{app}\Game\rt\wad"; Flags: ignoreversion
Source: "{#RepoRoot}\compat\realistic-lights\textures\tuindoom\*"; DestDir: "{app}\Game\rt\wad\textures\tuindoom"; Flags: ignoreversion
Source: "{#RepoRoot}\build\test-release\rt\scenes\mainmenu\*"; DestDir: "{app}\Game\rt\scenes\mainmenu"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#RepoRoot}\build\test-release\rt\scenes\rtempty\*"; DestDir: "{app}\Game\rt\scenes\rtempty"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#RepoRoot}\installer\mods\nashgore.pk3"; DestDir: "{app}\Game\Mods"; Flags: ignoreversion
Source: "{#RepoRoot}\installer\mods\nashgore_rt_voxel_compat.pk3"; DestDir: "{app}\Game\Mods"; Flags: ignoreversion
Source: "{#RepoRoot}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#RepoRoot}\README.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\TuinDoom RT"; Filename: "{app}\TuinDoomRT.exe"
Name: "{autodesktop}\TuinDoom RT"; Filename: "{app}\TuinDoomRT.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\TuinDoomRT.exe"; Description: "Launch TuinDoom RT"; Flags: nowait postinstall skipifsilent
