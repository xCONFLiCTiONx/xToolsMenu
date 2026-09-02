@echo off
set VS_PATH="C:\Program Files\Microsoft Visual Studio\18\Community"
set VCVARS=%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat

if not exist %VCVARS% (
    echo Error: Visual Studio 18 Community not found at %VS_PATH%
    echo Please edit build.bat with your correct Visual Studio path.
    pause
    exit /b 1
)

echo Setting up environment...
call %VCVARS% x64

echo Building Launcher.exe...
cl /nologo /O2 /Fe:Launcher.exe src\Launcher.cpp /link /SUBSYSTEM:WINDOWS user32.lib

echo Building AttributesDialog.exe...
cl /nologo /O2 /Fe:AttributesDialog.exe src\AttributesDialog.cpp /link /SUBSYSTEM:WINDOWS user32.lib dwmapi.lib advapi32.lib shell32.lib

echo Building xToolsMenu.dll...
cl /nologo /O2 /LD /Fe:xToolsMenu.dll src\ShellExtension.cpp /link /DLL /DEF:src\ShellExtension.def shlwapi.lib runtimeobject.lib shell32.lib ole32.lib advapi32.lib user32.lib

echo.
echo Done! If there were no errors, you can now run register.ps1 as Administrator.
pause
