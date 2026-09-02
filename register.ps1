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

# 3. Copy Compiled Binaries from MSBuild output to Staging
$BuildOut = Join-Path $Root "x64\Release"
Copy-Item (Join-Path $BuildOut "*.exe") $StageDir -Force
Copy-Item (Join-Path $BuildOut "*.dll") $StageDir -Force

$ManifestPath = Join-Path $StageDir "AppxManifest.xml"

Write-Host "Unregistering previous version if exists..."
try { Remove-AppxPackage -Package "xToolsMenu.Extension_1.0.0.0_neutral__1b7q5sa4bwdpa" -ErrorAction SilentlyContinue } catch {}

Write-Host "Registering Sparse Package with External Location pointing to StageDir..."
Add-AppxPackage -Register -Path $ManifestPath -ExternalLocation $StageDir

Stop-Process -Name explorer -Force

Write-Host "Done! All binaries and manifest staged and registered."