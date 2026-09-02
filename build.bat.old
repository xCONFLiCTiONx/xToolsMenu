@echo off
setlocal
echo Building xToolsMenu Solutions...

:: Locate MSBuild
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath`) do (
    set "VS_PATH=%%i"
)

if "%VS_PATH%"=="" (
    echo Visual Studio MSBuild not found!
    exit /b 1
)

call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=x64

:: Build solution
msbuild xToolsMenu.slnx /p:Configuration=Release /p:Platform=x64

:: Build companion exes
echo Building companion executables...
cl /nologo /O2 /Fe:x64\Release\Launcher.exe src\Launcher.cpp /link /SUBSYSTEM:WINDOWS user32.lib
cl /nologo /O2 /Fe:x64\Release\AttributesDialog.exe src\AttributesDialog.cpp /link /SUBSYSTEM:WINDOWS user32.lib dwmapi.lib advapi32.lib shell32.lib

echo Build complete!
pause
endlocal