@echo off
setlocal
cd /d "%~dp0"

:: Configuration
set "CERT_PATH=E:\.Vault\Certs\xToolsMenu_Local.pfx"
set "CERT_PASS="
set "SIGNTOOL=C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe"
set "ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"

echo ==========================================
echo [1/5] Building Solution & Companion EXEs...
echo ==========================================
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath`) do (
    set "VS_PATH=%%i"
)
call "%VS_PATH%\Common7\Tools\VsDevCmd.bat" -arch=x64 -no_logo
msbuild xToolsMenu.slnx /p:Configuration=Release /p:Platform=x64 /t:Rebuild

:: Rebuild companion binaries
if not exist "x64\Release" mkdir "x64\Release"
if exist "src\app.rc" rc.exe /fo src\app.res src\app.rc
if exist "src\Launcher.cpp" cl /nologo /O2 /D UNICODE /D _UNICODE /Fe:x64\Release\Launcher.exe src\Launcher.cpp src\app.res /link /SUBSYSTEM:WINDOWS user32.lib
if exist "src\AttributesDialog.cpp" cl /nologo /O2 /D UNICODE /D _UNICODE /Fe:x64\Release\AttributesDialog.exe src\AttributesDialog.cpp src\app.res /link /SUBSYSTEM:WINDOWS user32.lib advapi32.lib shell32.lib comctl32.lib gdi32.lib
if exist "src\EditWithDialog.cpp" cl /nologo /O2 /D UNICODE /D _UNICODE /Fe:x64\Release\EditWithDialog.exe src\EditWithDialog.cpp src\app.res /link /SUBSYSTEM:WINDOWS user32.lib advapi32.lib shell32.lib comctl32.lib comdlg32.lib gdi32.lib
if exist "src\SystemFoldersDialog.cpp" cl /nologo /O2 /D UNICODE /D _UNICODE /Fe:x64\Release\SystemFoldersDialog.exe src\SystemFoldersDialog.cpp src\app.res /link /SUBSYSTEM:WINDOWS user32.lib advapi32.lib shell32.lib comctl32.lib gdi32.lib
if exist "src\TakeOwnershipDialog.cpp" cl /nologo /O2 /D UNICODE /D _UNICODE /Fe:x64\Release\TakeOwnership.exe src\TakeOwnershipDialog.cpp src\app.res /link /SUBSYSTEM:WINDOWS user32.lib advapi32.lib shell32.lib comctl32.lib comdlg32.lib gdi32.lib
if exist "src\SettingsDialog.cpp" cl /nologo /O2 /D UNICODE /D _UNICODE /Fe:x64\Release\Settings.exe src\SettingsDialog.cpp src\app.res /link /SUBSYSTEM:WINDOWS user32.lib advapi32.lib shell32.lib comctl32.lib comdlg32.lib gdi32.lib

echo ==========================================
echo [2/5] Staging Files into AppPackage...
echo ==========================================
set "StageDir=%~dp0AppPackage"
if not exist "%StageDir%" mkdir "%StageDir%"
copy /y "AppxManifest.xml" "%StageDir%\" >nul
if exist "Assets" xcopy /y /s /i "Assets" "%StageDir%\Assets" >nul
if exist "Icons" xcopy /y /s /i "Icons" "%StageDir%\Icons" >nul
copy /y "x64\Release\*.exe" "%StageDir%\" >nul
copy /y "x64\Release\*.dll" "%StageDir%\" >nul

echo ==========================================
echo [3/5] Signing xToolsMenu.dll...
echo ==========================================
"%SIGNTOOL%" sign /f "%CERT_PATH%" /p "%CERT_PASS%" /fd SHA256 /v "AppPackage\xToolsMenu.dll"

echo ==========================================
echo [4/5] Compiling Inno Setup Installer...
echo ==========================================
"%ISCC%" installer.iss
if %errorlevel% neq 0 exit /b %errorlevel%

echo ==========================================
echo [5/5] Signing Installer Executable...
echo ==========================================
"%SIGNTOOL%" sign /f "%CERT_PATH%" /p "%CERT_PASS%" /fd SHA256 /v "xToolsMenu_Setup.exe"

echo ==========================================
echo Build, Package, and Sign Complete!
echo ==========================================
endlocal
pause