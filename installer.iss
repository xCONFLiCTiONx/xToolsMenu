; xToolsMenu Inno Setup Script

#define AppName "xToolsMenu"
#define AppVersion "1.0.0.0"
#define AppPublisher "Developer"
#define AppCLSID "{D1B6F6E9-4A9A-4B6A-8A4E-7C2D8D6E5C9A}"

[Setup]
AppId={{D1B6F6E9-4A9A-4B6A-8A4E-7C2D8D6E5C9A}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
AllowNoIcons=yes
OutputDir=.
OutputBaseFilename=xToolsMenu_Setup
SetupIconFile=ICON.ico
Compression=lzma
SolidCompression=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "AppPackage\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "ICON.ico"; DestDir: "{app}"; Flags: ignoreversion

[Registry]
Root: HKCU; Subkey: "Software\Classes\CLSID\{#AppCLSID}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\CLSID\{#AppCLSID}\InprocServer32"; ValueType: string; ValueName: ""; ValueData: "{app}\xToolsMenu.dll"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\CLSID\{#AppCLSID}\InprocServer32"; ValueType: string; ValueName: "ThreadingModel"; ValueData: "Apartment"; Flags: uninsdeletekey

[Run]
; Register Sparse Package
Filename: "powershell.exe"; Parameters: "-ExecutionPolicy Bypass -Command ""Add-AppxPackage -Register -Path '{app}\AppxManifest.xml' -ExternalLocation '{app}'"""; Flags: runhidden
; Restart Explorer to apply changes
Filename: "taskkill.exe"; Parameters: "/f /im explorer.exe"; Flags: runhidden
Filename: "explorer.exe"; Flags: nowait postinstall skipifsilent

[UninstallRun]
; Unregister Sparse Package
Filename: "powershell.exe"; Parameters: "-ExecutionPolicy Bypass -Command ""$pkg = Get-AppxPackage -Name xToolsMenu.Extension; if ($pkg) {{ Remove-AppxPackage -Package $pkg.PackageFullName }}"""; Flags: runhidden
; Restart Explorer to apply changes
Filename: "taskkill.exe"; Parameters: "/f /im explorer.exe"; Flags: runhidden
Filename: "explorer.exe"; Flags: nowait
