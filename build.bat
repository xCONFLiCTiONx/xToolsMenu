@echo off
setlocal
:: Ensure we are in the script's directory
cd /d "%~dp0"

echo Building xToolsMenu Solutions...
echo Working Directory: %cd%

:: Locate MSBuild
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath`) do (
    set "VS_PATH=%%i"
)

if "%VS_PATH%"=="" (
    echo Visual Studio MSBuild not found!
    pause
    exit /b 1
)

:: Call VsDevCmd but prevent it from changing directory
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=x64 -no_logo

:: Check for solution file
if not exist "xToolsMenu.slnx" (
    echo Error: xToolsMenu.slnx not found!
    dir /b
    pause
    exit /b 1
)

:: Build solution
msbuild xToolsMenu.slnx /p:Configuration=Release /p:Platform=x64

:: Build companion exes
echo Building companion executables...
if not exist "x64\Release" mkdir "x64\Release"

:: Compile Resources
if exist "src\app.rc" (
    rc.exe /fo src\app.res src\app.rc
)

if exist "src\Launcher.cpp" (
    cl /nologo /O2 /D UNICODE /D _UNICODE /Fe:x64\Release\Launcher.exe src\Launcher.cpp src\app.res /link /SUBSYSTEM:WINDOWS user32.lib
) else (
    echo Error: src\Launcher.cpp not found!
)

if exist "src\AttributesDialog.cpp" (
    cl /nologo /O2 /D UNICODE /D _UNICODE /Fe:x64\Release\AttributesDialog.exe src\AttributesDialog.cpp src\app.res /link /SUBSYSTEM:WINDOWS user32.lib dwmapi.lib advapi32.lib shell32.lib comctl32.lib
) else (
    echo Error: src\AttributesDialog.cpp not found!
)

if exist "src\EditWithDialog.cpp" (
    cl /nologo /O2 /D UNICODE /D _UNICODE /Fe:x64\Release\EditWithDialog.exe src\EditWithDialog.cpp src\app.res /link /SUBSYSTEM:WINDOWS user32.lib dwmapi.lib advapi32.lib shell32.lib comctl32.lib comdlg32.lib
)

if exist "src\SystemFoldersDialog.cpp" (
    cl /nologo /O2 /D UNICODE /D _UNICODE /Fe:x64\Release\SystemFoldersDialog.exe src\SystemFoldersDialog.cpp src\app.res /link /SUBSYSTEM:WINDOWS user32.lib dwmapi.lib advapi32.lib shell32.lib comctl32.lib
)

endlocal
