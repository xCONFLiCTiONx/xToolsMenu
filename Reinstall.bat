@echo off
setlocal
cd /d "%~dp0"

echo ==========================================
echo Step 1: Building xToolsMenu Solutions...
echo ==========================================

:: Locate MSBuild
set "VS_PATH="
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath`) do (
    set "VS_PATH=%%i"
)

if "%VS_PATH%"=="" (
    echo Visual Studio MSBuild not found!
    exit /b 1
)

call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=x64 -no_logo

if not exist "xToolsMenu.slnx" (
    echo Error: xToolsMenu.slnx not found!
    exit /b 1
)

:: Attempt build first WITHOUT touching Explorer
call :DO_BUILD
if %BUILD_SUCCESS%==1 goto :BUILD_SUCCESSFUL

:: If build failed (likely due to DLL lock by Explorer), kill Explorer and retry once
echo.
echo [!] Build failed (likely due to file lock). Terminating Explorer and retrying...
taskkill /f /im explorer.exe >nul 2>&1

call :DO_BUILD
if %BUILD_SUCCESS%==0 (
    echo Build failed even after terminating Explorer!
    start explorer.exe
    pause
    exit /b 1
)

:: Restart Explorer since we had to kill it to clear the lock
echo Restarting Explorer...
start explorer.exe
goto :CHECK_PACKAGE

:BUILD_SUCCESSFUL
echo Build completed with zero Explorer interference!

:CHECK_PACKAGE
echo.
echo ==========================================
echo Step 2: Checking Sparse Package Registration...
echo ==========================================
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$pkg = Get-AppxPackage -Name xToolsMenu.Extension; if (-not $pkg) { Write-Host 'Package not registered. Running initial registration...'; & '%~dp0register.ps1' }"

echo.
echo Done!
endlocal
exit /b 0

:: --- Build Subroutine ---
:DO_BUILD
set "BUILD_SUCCESS=1"

msbuild xToolsMenu.slnx /p:Configuration=Release /p:Platform=x64
if errorlevel 1 set "BUILD_SUCCESS=0"

if not exist "x64\Release" mkdir "x64\Release"

if exist "src\Launcher.cpp" (
    cl /nologo /O2 /Fe:x64\Release\Launcher.exe src\Launcher.cpp /link /SUBSYSTEM:WINDOWS user32.lib
    if errorlevel 1 set "BUILD_SUCCESS=0"
) else (
    echo Error: src\Launcher.cpp not found!
    set "BUILD_SUCCESS=0"
)

if exist "src\AttributesDialog.cpp" (
    cl /nologo /O2 /Fe:x64\Release\AttributesDialog.exe src\AttributesDialog.cpp /link /SUBSYSTEM:WINDOWS user32.lib dwmapi.lib advapi32.lib shell32.lib
    if errorlevel 1 set "BUILD_SUCCESS=0"
) else (
    echo Error: src\AttributesDialog.cpp not found!
    set "BUILD_SUCCESS=0"
)
goto :eof