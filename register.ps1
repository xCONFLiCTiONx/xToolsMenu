# Register the sparse package
$manifestPath = Join-Path $PSScriptRoot "AppxManifest.xml"
Add-AppPackage -Register $manifestPath
Write-Host "xToolsMenu Shell Extension registered successfully."
