@echo off
set "ScriptDir=%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%ScriptDir%register.ps1"

