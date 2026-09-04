@echo off
setlocal
cd /d "%~dp0"

echo ==========================================
echo [1/4] Unregistering Existing Extension...
echo ==========================================
powershell -NoProfile -Command "Get-AppxPackage -Name xToolsMenu.Extension | Remove-AppxPackage -ErrorAction SilentlyContinue"
reg delete "HKCU\Software\Classes\CLSID\{D1B6F6E9-4A9A-4B6A-8A4E-7C2D8D6E5C9A}" /f >nul 2>&1
powershell.exe -NoProfile -Command "Stop-Process -Name explorer -Force"

echo Unregistering complete!
endlocal
pause