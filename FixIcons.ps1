# Create the AppPackage\Assets directory if it doesn't exist
$assetsPath = Join-Path $PSScriptRoot "AppPackage\Assets"
if (!(Test-Path $assetsPath)) {
    New-Item -ItemType Directory -Path $assetsPath -Force
}

# Source icon file
$sourceIcon = Join-Path $PSScriptRoot "ICON.PNG"

if (Test-Path $sourceIcon) {
    Write-Host "Copying icons to Assets folder..."
    Copy-Item $sourceIcon -Destination (Join-Path $assetsPath "StoreLogo.png") -Force
    Copy-Item $sourceIcon -Destination (Join-Path $assetsPath "Square44x44Logo.png") -Force
    Copy-Item $sourceIcon -Destination (Join-Path $assetsPath "Square150x150Logo.png") -Force
    Copy-Item $sourceIcon -Destination (Join-Path $assetsPath "Wide310x150Logo.png") -Force
    Write-Host "Done."
} else {
    Write-Error "ICON.PNG not found in root directory: $sourceIcon"
}
