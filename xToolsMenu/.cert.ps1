try {
    Write-Host "Generating exportable code-signing certificate..." -ForegroundColor Cyan
    
    $cert = New-SelfSignedCertificate `
        -Type CodeSigningCert `
        -Subject "CN=xToolsMenu" `
        -CertStoreLocation "Cert:\CurrentUser\My" `
        -KeyExportPolicy Exportable `
        -NotAfter (Get-Date).AddYears(5)

    Write-Host "Certificate created. Thumbprint: $($cert.Thumbprint)" -ForegroundColor Green

    $securePassword = Read-Host -AsSecureString "Enter a password to secure xToolsMenu_Local.pfx (leave blank for no password)"

    $outputPath = Join-Path $PSScriptRoot "xToolsMenu_Local.pfx"
    Write-Host "Exporting to $outputPath..." -ForegroundColor Cyan
    
    Export-PfxCertificate `
        -Cert $cert `
        -FilePath $outputPath `
        -Password $securePassword `
        -Force

    Write-Host "Success! File saved at: $outputPath" -ForegroundColor Green
}
catch {
    Write-Host "[ERROR] Failed to create or export certificate:" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Yellow
}

Write-Host "`nPress any key to exit..." -ForegroundColor Gray
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")