@echo off
setlocal
:: Ensure we are in the script's directory
cd /d "%~dp0"

echo Unregistering xToolsMenu...

:: 1. Remove Sparse Package (Windows 11 Menu)
echo Removing Sparse Package...
powershell -Command "Get-AppxPackage -Name xToolsMenu.Extension | Remove-AppxPackage"

:: 2. Remove Registry Keys (Legacy Menu and COM Server)
echo Cleaning up Registry...
reg delete "HKCU\Software\Classes\CLSID\{D1B6F6E9-4A9A-4B6A-8A4E-7C2D8D6E5C9A}" /f >nul 2>&1

:: 3. Restart Explorer to clear hooks
echo Restarting Explorer...
taskkill /f /im explorer.exe
start explorer.exe

echo.
echo Unregistration complete!
pause
endlocal
