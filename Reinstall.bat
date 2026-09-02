@echo off
setlocal
cd /d "%~dp0"

echo ==========================================
echo Step 1: Building xToolsMenu Solutions...
echo ==========================================

:: Attempt building via build.bat first WITHOUT touching Explorer
call :RUN_BUILD
if %BUILD_SUCCESS%==1 goto :CHECK_PACKAGE

:: If build failed (likely due to DLL lock by Explorer), kill Explorer and retry once
echo.
echo [!] Build failed (likely due to file lock). Terminating Explorer and retrying...
taskkill /f /im explorer.exe >nul 2>&1

call :RUN_BUILD
if %BUILD_SUCCESS%==0 (
    echo Build failed even after terminating Explorer!
    start explorer.exe
    pause
    exit /b 1
)

:: Restart Explorer since we had to kill it to clear the lock
echo Restarting Explorer...
start explorer.exe

:CHECK_PACKAGE
echo.
echo ==========================================
echo Step 2: Checking Sparse Package Registration...
echo ==========================================
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$pkg = Get-AppxPackage -Name xToolsMenu.Extension; if (-not $pkg) { Write-Host 'Package not registered. Running initial registration...'; & '%~dp0register.ps1' }"

echo.
echo Done!
endlocal
pause
exit /b 0

:: --- Build Subroutine ---
:RUN_BUILD
set "BUILD_SUCCESS=1"

if not exist "build.bat" (
    echo Error: build.bat not found in directory!
    set "BUILD_SUCCESS=0"
    goto :eof
)

call build.bat
if errorlevel 1 set "BUILD_SUCCESS=0"
goto :eof

pause
