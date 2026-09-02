@echo off
setlocal
cd /d "%~dp0"

echo ==========================================
echo Step 1: Building xToolsMenu Solutions...
echo ==========================================
if not exist "build.bat" (
    echo Error: build.bat not found in directory!
    pause
    exit /b 1
)

call build.bat
if errorlevel 1 (
    echo [X] Build failed! Exiting.
    pause
    exit /b 1
)

echo.
echo ==========================================
echo Step 2: Terminating Explorer (Freeing DLL Locks)
echo ==========================================
taskkill /f /im explorer.exe >nul 2>&1

echo.
echo ==========================================
echo Step 3: Cleaning old DLL and updating AppPackage...
echo ==========================================
if not exist "AppPackage" mkdir "AppPackage"

:: Explicitly delete the old xToolsMenu.dll from AppPackage first
if exist "AppPackage\xToolsMenu.dll" (
    del /f /q "AppPackage\xToolsMenu.dll"
    echo Deleted old AppPackage\xToolsMenu.dll
)

:: Copy fresh binaries directly into the ExternalLocation AppPackage folder
xcopy /y /q "x64\Release\*.exe" "AppPackage\" >nul
xcopy /y /q "x64\Release\*.dll" "AppPackage\" >nul
echo Fresh binaries successfully copied to AppPackage.

echo.
echo ==========================================
echo Step 4: Checking Sparse Package Registration...
echo ==========================================
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$pkg = Get-AppxPackage -Name xToolsMenu.Extension; if (-not $pkg) { Write-Host 'Package not registered. Running initial registration...'; & '%~dp0register.ps1' } else { Write-Host 'Sparse Package is active. External location is live.' }"

echo.
echo ==========================================
echo Step 5: Restarting Explorer...
echo ==========================================
start explorer.exe

echo.
echo ==========================================
echo All tasks completed successfully!
echo ==========================================
endlocal
pause
exit /b 0