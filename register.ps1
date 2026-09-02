# Ensure script is run as Administrator
if (!([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Error "Run this script in an Administrator PowerShell window."
    exit
}

$Root = Split-Path -Parent $MyInvocation.MyCommand.Definition
$StageDir = Join-Path $Root "AppPackage"

# 1. Create Staging Directory
if (!(Test-Path $StageDir)) { New-Item -ItemType Directory -Path $StageDir | Out-Null }

# 2. Copy Manifest and Assets to Staging
Copy-Item (Join-Path $Root "AppxManifest.xml") $StageDir -Force
if (Test-Path (Join-Path $Root "Assets")) {
    Copy-Item (Join-Path $Root "Assets") $StageDir -Recurse -Force
}

# 3. Copy Compiled Binaries
$BuildOut = Join-Path $Root "x64\Release"
Copy-Item (Join-Path $BuildOut "*.exe") $StageDir -Force
Copy-Item (Join-Path $BuildOut "*.dll") $StageDir -Force

$DllPath = Join-Path $StageDir "xToolsMenu.dll"
$ManifestPath = Join-Path $StageDir "AppxManifest.xml"
$Clsid = "{D1B6F6E9-4A9A-4B6A-8A4E-7C2D8D6E5C9A}"

Write-Host "Registering COM Server for $Clsid..."
$ClsidKey = "HKCU:\Software\Classes\CLSID\$Clsid"
if (!(Test-Path $ClsidKey)) { New-Item -Path $ClsidKey -Force | Out-Null }
$InProcKey = Join-Path $ClsidKey "InprocServer32"
if (!(Test-Path $InProcKey)) { New-Item -Path $InProcKey -Force | Out-Null }
Set-ItemProperty -Path $InProcKey -Name "(Default)" -Value $DllPath
Set-ItemProperty -Path $InProcKey -Name "ThreadingModel" -Value "Apartment"

Write-Host "Registering Legacy Context Menu Handler..."
$ShellKeys = @(
    "HKCU:\Software\Classes\Directory\shell\xToolsMenu",
    "HKCU:\Software\Classes\Directory\Background\shell\xToolsMenu",
    "HKCU:\Software\Classes\Drive\shell\xToolsMenu",
    "HKCU:\Software\Classes\*\shell\xToolsMenu"
)

foreach ($key in $ShellKeys) {
    if (!(Test-Path $key)) { New-Item -Path $key -Force | Out-Null }
    Set-ItemProperty -Path $key -Name "ExplorerCommandHandler" -Value $Clsid
    Set-ItemProperty -Path $key -Name "MUIVerb" -Value "xToolsMenu"
    Set-ItemProperty -Path $key -Name "Icon" -Value "shell32.dll,-16769"
}

Write-Host "Unregistering Sparse Package if exists..."
try {
    $pkg = Get-AppxPackage -Name xToolsMenu.Extension
    if ($pkg) { Remove-AppxPackage -Package $pkg.PackageFullName -ErrorAction SilentlyContinue }
} catch {}

Write-Host "Registering Sparse Package..."
Add-AppxPackage -Register -Path $ManifestPath -ExternalLocation $StageDir

Write-Host "Restarting Explorer..."
Stop-Process -Name explorer -Force

Write-Host "Done! If it doesn't show up immediately, it will be in the 'Show more options' menu."
