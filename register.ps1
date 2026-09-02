$manifestPath = Join-Path $PSScriptRoot "AppxManifest.xml"
$externalLocation = $PSScriptRoot

Write-Host "Registering xToolsMenu as a Sparse Package..."
Write-Host "Manifest: $manifestPath"
Write-Host "External Location: $externalLocation"

# Note: This requires Developer Mode to be enabled in Windows Settings
Add-AppxPackage -Register $manifestPath -ExternalLocation $externalLocation

if ($?) {
    Write-Host "Successfully registered! Restarting Explorer..." -ForegroundColor Green
    Stop-Process -Name explorer -Force
} else {
    Write-Host "Failed to register. Make sure you are running as Administrator and Developer Mode is enabled." -ForegroundColor Red
}
pause
