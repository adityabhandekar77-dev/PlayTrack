; PlayTrack 1.0.0 installer template
; Build the project first, then copy ONLY release binaries/runtime DLLs to .\release.
; Do not put your personal config.json, playtime.json, backups, or shortcuts in .\release.
#define MyAppName "PlayTrack"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "PlayTrack"
#define MyAppExeName "PlayTrack.exe"

[Setup]
AppId={{A6F2A6AB-4A3D-4C0D-9D9E-123456789001}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={localappdata}\Programs\PlayTrack
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=output
OutputBaseFilename=PlayTrack-Setup-1.0.0
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "release\PlayTrack.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "release\PlayTrackTracker.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "release\*.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{autoprograms}\PlayTrack"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"
Name: "{autodesktop}\PlayTrack"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch PlayTrack"; Flags: postinstall nowait skipifsilent
