param(
    [string]$Configuration = "Release",
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

$RootDir = $PSScriptRoot
$BinDir = Join-Path $RootDir "$Platform\$Configuration"
$StageDir = Join-Path $RootDir "AppPackage"
$IssPath = Join-Path $RootDir "installer.iss"

$RequiredBinaries = @(
    "xToolsMenu.dll",
    "AttributesDialog.exe",
    "EditWithDialog.exe",
    "Install.exe",
    "Launcher.exe",
    "Settings.exe",
    "SystemFoldersDialog.exe",
    "TakeOwnership.exe",
    "Uninstall.exe"
)

if (-not (Test-Path $BinDir)) {
    throw "Build output directory not found: $BinDir. Build $Configuration|$Platform first."
}

foreach ($file in $RequiredBinaries) {
    $src = Join-Path $BinDir $file
    if (-not (Test-Path $src)) {
        throw "Required build artifact missing: $src"
    }
}

# Locate Inno Setup compiler
$Iscc = $null
$IsccCmd = Get-Command iscc -ErrorAction SilentlyContinue
if ($IsccCmd) {
    $Iscc = $IsccCmd.Source
} else {
    $Candidates = @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
        "${env:ProgramFiles(x86)}\Inno Setup 5\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 5\ISCC.exe"
    )
    foreach ($candidate in $Candidates) {
        if (Test-Path $candidate) {
            $Iscc = $candidate
            break
        }
    }
}

if (-not $Iscc) {
    throw "Inno Setup compiler (ISCC.exe) not found. Install Inno Setup 6 or add ISCC.exe to PATH."
}

Write-Host "Staging AppPackage from $BinDir..."
if (Test-Path $StageDir) {
    Remove-Item -Path $StageDir -Recurse -Force
}
New-Item -Path $StageDir -ItemType Directory | Out-Null

foreach ($file in $RequiredBinaries) {
    Copy-Item -Path (Join-Path $BinDir $file) -Destination $StageDir -Force
}

Copy-Item -Path (Join-Path $RootDir "AppxManifest.xml") -Destination $StageDir -Force
Copy-Item -Path (Join-Path $RootDir "Assets") -Destination (Join-Path $StageDir "Assets") -Recurse -Force
Copy-Item -Path (Join-Path $RootDir "Icons") -Destination (Join-Path $StageDir "Icons") -Recurse -Force

Write-Host "Compiling installer with $Iscc..."
& $Iscc $IssPath
if ($LASTEXITCODE -ne 0) {
    throw "ISCC.exe failed with exit code $LASTEXITCODE."
}

$OutputExe = Join-Path $RootDir "xToolsMenu_Setup.exe"
Write-Host "Installer created: $OutputExe"

