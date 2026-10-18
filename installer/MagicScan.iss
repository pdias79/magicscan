#define MyAppName "MagicScan"
#define MyAppVersion "0.4.0"
#define MyAppPublisher "MagicScan"
#define MyAppExeName "magicscan-tray.exe"

[Setup]
AppId={{12B91BD7-5B4F-492E-8C54-234DC5E2045C}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\MagicScan
DefaultGroupName=MagicScan
OutputDir=..\dist
OutputBaseFilename=MagicScan-{#MyAppVersion}-setup-win64
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\{#MyAppExeName}
CloseApplications=yes
RestartApplications=no

[Files]
Source: "..\build\Release\magicscan-tray.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\magicscan.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\magicscan-usb-service.exe"; DestDir: "{app}\Service"; Flags: ignoreversion
Source: "manage-service.ps1"; DestDir: "{app}\Installer"; Flags: ignoreversion

[Icons]
Name: "{group}\MagicScan"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Désinstaller MagicScan"; Filename: "{uninstallexe}"
Name: "{commonstartup}\MagicScan"; Filename: "{app}\{#MyAppExeName}"

[Run]
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -NonInteractive -ExecutionPolicy Bypass -File ""{app}\Installer\manage-service.ps1"" -Action Install -ServiceExe ""{app}\Service\magicscan-usb-service.exe"""; Flags: runhidden waituntilterminated

[UninstallRun]
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -NonInteractive -ExecutionPolicy Bypass -File ""{app}\Installer\manage-service.ps1"" -Action Uninstall"; Flags: runhidden waituntilterminated
