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
CloseApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "AppPackage\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "ICON.ico"; DestDir: "{app}"; Flags: ignoreversion

[UninstallRun]
; Run the reliable Uninstall wrapper which references .unregister.bat
Filename: "{app}\Uninstall.exe"; StatusMsg: "Unregistering Shell Extension..."; Flags: runhidden

[Code]
procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  if CurStep = ssPostInstall then
  begin
    WizardForm.StatusLabel.Caption := 'Registering Shell Extension...';
    if not Exec(ExpandConstant('{app}\Install.exe'), '', ExpandConstant('{app}'), SW_HIDE, ewWaitUntilTerminated, ResultCode) then
    begin
      MsgBox('Failed to launch Install.exe (error ' + IntToStr(ResultCode) + ').' + #13#10 +
             'Check log: ' + GetEnv('TEMP') + '\xToolsMenu_Install.log', mbError, MB_OK);
    end
    else if ResultCode <> 0 then
    begin
      MsgBox('Shell Extension registration failed (exit code ' + IntToStr(ResultCode) + ').' + #13#10 +
             'Check log: ' + GetEnv('TEMP') + '\xToolsMenu_Install.log', mbError, MB_OK);
    end;
  end;
end;

// Helper to ensure we don't leave things behind if the wrapper fails
procedure CurUninstallStepChanged(UninstallStep: TUninstallStep);
begin
  if UninstallStep = usPostUninstall then
  begin
    // Optional: Add manual cleanup here if needed, but Uninstall.exe should handle it
  end;
end;
