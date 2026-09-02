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
endlocal
