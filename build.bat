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

echo Build complete!
endlocal