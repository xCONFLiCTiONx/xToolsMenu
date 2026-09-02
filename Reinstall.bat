@echo off
setlocal
:: Ensure we are in the script's directory
cd /d "%~dp0"

echo ==========================================
echo Step 1: Unregistering xToolsMenu...
echo ==========================================

:: 1. Remove Sparse Package (Windows 11 Menu)
echo Removing Sparse Package...
powershell -Command "Get-AppxPackage -Name xToolsMenu.Extension | Remove-AppxPackage"

:: 2. Remove Registry Keys (Legacy Menu and COM Server)
echo Cleaning up Registry...
reg delete "HKCU\Software\Classes\CLSID\{D1B6F6E9-4A9A-4B6A-8A4E-7C2D8D6E5C9A}" /f >nul 2>&1
reg delete "HKCU\Software\Classes\Directory\shell\xToolsMenu" /f >nul 2>&1
reg delete "HKCU\Software\Classes\Directory\Background\shell\xToolsMenu" /f >nul 2>&1
reg delete "HKCU\Software\Classes\Drive\shell\xToolsMenu" /f >nul 2>&1
reg delete "HKCU\Software\Classes\*\shell\xToolsMenu" /f >nul 2>&1

:: 3. Restart Explorer to clear old hooks
echo Restarting Explorer...
taskkill /f /im explorer.exe
start explorer.exe

echo.
echo ==========================================
echo Step 2: Building xToolsMenu Solutions...
echo ==========================================
echo Working Directory: %cd%

:: Locate MSBuild
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath`) do (
    set "VS_PATH=%%i"
)

if "%VS_PATH%"=="" (
    echo Visual Studio MSBuild not found!
    exit /b 1
)

:: Call VsDevCmd but prevent it from changing directory
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=x64 -no_logo

:: Check for solution file
if not exist "xToolsMenu.slnx" (
    echo Error: xToolsMenu.slnx not found!
    dir /b
    exit /b 1
)

:: Build solution
msbuild xToolsMenu.slnx /p:Configuration=Release /p:Platform=x64

:: Build companion exes
echo Building companion executables...
if not exist "x64\Release" mkdir "x64\Release"

if exist "src\Launcher.cpp" (
    cl /nologo /O2 /Fe:x64\Release\Launcher.exe src\Launcher.cpp /link /SUBSYSTEM:WINDOWS user32.lib
) else (
    echo Error: src\Launcher.cpp not found!
)

if exist "src\AttributesDialog.cpp" (
    cl /nologo /O2 /Fe:x64\Release\AttributesDialog.exe src\AttributesDialog.cpp /link /SUBSYSTEM:WINDOWS user32.lib dwmapi.lib advapi32.lib shell32.lib
) else (
    echo Error: src\AttributesDialog.cpp not found!
)

echo Build complete!

echo.
echo ==========================================
echo Step 3: Registering xToolsMenu...
echo ==========================================
set "ScriptDir=%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%ScriptDir%register.ps1"

:: Final Explorer restart to force Windows shell to load the fresh package/extensions
echo Restarting Explorer to apply registration...
taskkill /f /im explorer.exe
start explorer.exe

echo.
echo All tasks completed successfully!
pause
endlocal