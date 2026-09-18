# Ensure script is run as Administrator
if (!([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Error "Run this script in an Administrator PowerShell window."
    exit
}

$CurrentDir = Split-Path -Parent $MyInvocation.MyCommand.Definition

# Scenario Detection
# If xToolsMenu.dll and AppxManifest.xml exist in the current directory, we are likely in the installation/staged folder.
$isStaged = (Test-Path (Join-Path $CurrentDir "xToolsMenu.dll")) -and (Test-Path (Join-Path $CurrentDir "AppxManifest.xml"))

if ($isStaged) {
    Write-Host "Detected installation/staged directory. Skipping file staging."
    $RegistrationDir = $CurrentDir
} else {
    Write-Host "Detected development environment. Staging files to AppPackage..."
    $RegistrationDir = Join-Path $CurrentDir "AppPackage"

    # 1. Create Staging Directory
    if (!(Test-Path $RegistrationDir)) { New-Item -ItemType Directory -Path $RegistrationDir | Out-Null }

    # 2. Copy Manifest, Assets, and Icons to Staging
    if (Test-Path (Join-Path $CurrentDir "AppxManifest.xml")) {
        Copy-Item (Join-Path $CurrentDir "AppxManifest.xml") $RegistrationDir -Force
    }
    if (Test-Path (Join-Path $CurrentDir "Assets")) {
        Copy-Item (Join-Path $CurrentDir "Assets") $RegistrationDir -Recurse -Force
    }
    if (Test-Path (Join-Path $CurrentDir "Icons")) {
        Copy-Item (Join-Path $CurrentDir "Icons") $RegistrationDir -Recurse -Force
    }

    # 3. Copy Compiled Binaries
    $BuildOut = Join-Path $CurrentDir "x64\Release"
    if (Test-Path $BuildOut) {
        Get-ChildItem -Path $BuildOut -Filter "*.exe" | Copy-Item -Destination $RegistrationDir -Force
        Get-ChildItem -Path $BuildOut -Filter "*.dll" | Copy-Item -Destination $RegistrationDir -Force
    } else {
        Write-Warning "Build output directory not found at $BuildOut. Skipping binary copy."
    }
}

$DllPath = Join-Path $RegistrationDir "xToolsMenu.dll"
$ManifestPath = Join-Path $RegistrationDir "AppxManifest.xml"

# Validation
if (!(Test-Path $DllPath)) {
    Write-Error "Could not find xToolsMenu.dll at $DllPath. Ensure the project is built or files are staged correctly."
    exit
}
if (!(Test-Path $ManifestPath)) {
    Write-Error "Could not find AppxManifest.xml at $ManifestPath. Ensure the manifest exists."
    exit
}

$Clsid = "{D1B6F6E9-4A9A-4B6A-8A4E-7C2D8D6E5C9A}"
$ClassicClsid = "{E5F7B8C0-5B0B-4D7B-9F1F-8C3D9E7F6A1B}"

Write-Host "Registering COM Server for $Clsid (Windows 11 Menu)..."
$ClsidKey = "HKCU:\Software\Classes\CLSID\$Clsid"
if (!(Test-Path $ClsidKey)) { New-Item -Path $ClsidKey -Force | Out-Null }
$InProcKey = Join-Path $ClsidKey "InprocServer32"
if (!(Test-Path $InProcKey)) { New-Item -Path $InProcKey -Force | Out-Null }
Set-ItemProperty -Path $InProcKey -Name "(Default)" -Value $DllPath
Set-ItemProperty -Path $InProcKey -Name "ThreadingModel" -Value "Apartment"

Write-Host "Registering Classic COM Server for $ClassicClsid (Classic Menu)..."
$ClassicClsidKey = "HKCU:\Software\Classes\CLSID\$ClassicClsid"
if (!(Test-Path $ClassicClsidKey)) { New-Item -Path $ClassicClsidKey -Force | Out-Null }
$ClassicInProcKey = Join-Path $ClassicClsidKey "InprocServer32"
if (!(Test-Path $ClassicInProcKey)) { New-Item -Path $ClassicInProcKey -Force | Out-Null }
Set-ItemProperty -Path $ClassicInProcKey -Name "(Default)" -Value $DllPath
Set-ItemProperty -Path $ClassicInProcKey -Name "ThreadingModel" -Value "Apartment"

Write-Host "Registering Classic Context Menu Handlers..."
$Handlers = @(
    "HKCU:\Software\Classes\*\shellex\ContextMenuHandlers\xToolsMenu",
    "HKCU:\Software\Classes\Directory\shellex\ContextMenuHandlers\xToolsMenu",
    "HKCU:\Software\Classes\Directory\Background\shellex\ContextMenuHandlers\xToolsMenu"
)
foreach ($key in $Handlers) {
    if (!(Test-Path $key)) { New-Item -Path $key -Force | Out-Null }
    Set-ItemProperty -Path $key -Name "(Default)" -Value $ClassicClsid
}

Write-Host "Unregistering Sparse Package if exists..."
try {
    $pkg = Get-AppxPackage -Name xToolsMenu.Extension
    if ($pkg) { Remove-AppxPackage -Package $pkg.PackageFullName -ErrorAction SilentlyContinue }
} catch {}

Write-Host "Registering Sparse Package..."
Add-AppxPackage -Register -Path $ManifestPath -ExternalLocation $RegistrationDir

Write-Host "Restarting Explorer..."
Stop-Process -Name explorer -Force

Write-Host "Done! If it doesn't show up immediately, it will be in the 'Show more options' menu."
