; xToolsMenu Inno Setup Script

#define AppName "xToolsMenu"
#define AppVersion "1.0.0.0"
#define AppPublisher "xCONFLiCTiONx"
#define AppCLSID "{D1B6F6E9-4A9A-4B6A-8A4E-7C2D8D6E5C9A}"

[Setup]
AppId={{D1B6F6E9-4A9A-4B6A-8A4E-7C2D8D6E5C9A}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
AllowNoIcons=yes
UninstallDisplayIcon={app}\ICON.ico
OutputDir=.
OutputBaseFilename=xToolsMenu_Setup
SetupIconFile=ICON.ico
Compression=lzma
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "AppPackage\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "ICON.ico"; DestDir: "{app}"; Flags: ignoreversion

[Run]
; Run the reliable Install wrapper which references .register.ps1
Filename: "{app}\Install.exe"; StatusMsg: "Registering Shell Extension..."; Flags: runhidden

[UninstallRun]
; Run the reliable Uninstall wrapper which references .unregister.bat
Filename: "{app}\Uninstall.exe"; StatusMsg: "Unregistering Shell Extension..."; Flags: runhidden

[Code]
// Helper to ensure we don't leave things behind if the wrapper fails
procedure CurUninstallStepChanged(UninstallStep: TUninstallStep);
begin
  if UninstallStep = usPostUninstall then
  begin
    // Optional: Add manual cleanup here if needed, but Uninstall.exe should handle it
  end;
end;
