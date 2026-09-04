@echo off
setlocal
cd /d "%~dp0"

echo ==========================================
echo [1/4] Unregistering Existing Extension...
echo ==========================================
powershell -NoProfile -Command "Get-AppxPackage -Name xToolsMenu.Extension | Remove-AppxPackage -ErrorAction SilentlyContinue"
reg delete "HKCU\Software\Classes\CLSID\{D1B6F6E9-4A9A-4B6A-8A4E-7C2D8D6E5C9A}" /f >nul 2>&1

echo ==========================================
echo [2/4] Locating MSBuild and Building Solution...
echo ==========================================
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath`) do (
    set "VS_PATH=%%i"
)

if "%VS_PATH%"=="" (
    echo Visual Studio MSBuild not found!
    pause
    exit /b 1
)

call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=x64 -no_logo

if not exist "xToolsMenu.slnx" (
    echo Error: xToolsMenu.slnx not found!
    pause
    exit /b 1
)

msbuild xToolsMenu.slnx /p:Configuration=Release /p:Platform=x64

echo ==========================================
echo Building Companion Executables & Resources...
echo ==========================================
if not exist "x64\Release" mkdir "x64\Release"

if exist "src\app.rc" (
    rc.exe /fo src\app.res src\app.rc
)

if exist "src\Launcher.cpp" (
    cl /nologo /O2 /D UNICODE /D _UNICODE /Fe:x64\Release\Launcher.exe src\Launcher.cpp src\app.res /link /SUBSYSTEM:WINDOWS user32.lib
)

if exist "src\AttributesDialog.cpp" (
    cl /nologo /O2 /D UNICODE /D _UNICODE /Fe:x64\Release\AttributesDialog.exe src\AttributesDialog.cpp src\app.res /link /SUBSYSTEM:WINDOWS user32.lib advapi32.lib shell32.lib comctl32.lib gdi32.lib
)

if exist "src\EditWithDialog.cpp" (
    cl /nologo /O2 /D UNICODE /D _UNICODE /Fe:x64\Release\EditWithDialog.exe src\EditWithDialog.cpp src\app.res /link /SUBSYSTEM:WINDOWS user32.lib advapi32.lib shell32.lib comctl32.lib comdlg32.lib gdi32.lib
)

if exist "src\SystemFoldersDialog.cpp" (
    cl /nologo /O2 /D UNICODE /D _UNICODE /Fe:x64\Release\SystemFoldersDialog.exe src\SystemFoldersDialog.cpp src\app.res /link /SUBSYSTEM:WINDOWS user32.lib advapi32.lib shell32.lib comctl32.lib gdi32.lib
)

if exist "src\TakeOwnershipDialog.cpp" (
    cl /nologo /O2 /D UNICODE /D _UNICODE /Fe:x64\Release\TakeOwnership.exe src\TakeOwnershipDialog.cpp src\app.res /link /SUBSYSTEM:WINDOWS user32.lib advapi32.lib shell32.lib comctl32.lib gdi32.lib
)

if exist "src\SettingsDialog.cpp" (
    cl /nologo /O2 /D UNICODE /D _UNICODE /Fe:x64\Release\Settings.exe src\SettingsDialog.cpp src\app.res /link /SUBSYSTEM:WINDOWS user32.lib advapi32.lib shell32.lib comctl32.lib comdlg32.lib gdi32.lib
)

echo ==========================================
echo [3/4] Staging Binaries & Assets...
echo ==========================================
set "StageDir=%~dp0AppPackage"
if not exist "%StageDir%" mkdir "%StageDir%"

copy /y "AppxManifest.xml" "%StageDir%\" >nul
if exist "Assets" xcopy /y /s /i "Assets" "%StageDir%\Assets" >nul
if exist "Icons" xcopy /y /s /i "Icons" "%StageDir%\Icons" >nul
copy /y "x64\Release\*.exe" "%StageDir%\" >nul
copy /y "x64\Release\*.dll" "%StageDir%\" >nul

echo ==========================================
echo [4/4] Registering Sparse Package...
echo ==========================================
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0register.ps1"

echo Local deployment complete!
endlocal
pause