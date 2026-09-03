@echo off
setlocal

set "ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"

if not exist "%ISCC%" (
    echo Inno Setup 6 not found at %ISCC%
    echo Please install Inno Setup 6 or update the path in this script.
    pause
    exit /b 1
)

echo Compiling xToolsMenu Installer...
"%ISCC%" "installer.iss"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Installer created successfully: xToolsMenu_Setup.exe
) else (
    echo.
    echo Failed to compile installer.
)

pause
