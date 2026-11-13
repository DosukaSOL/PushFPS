@echo off
rem PushFPS for OBS installer: runs Install-PushFPS-OBS.ps1 next to this file.
rem Install-PushFPS-OBS.cmd -Uninstall removes the plugin.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-PushFPS-OBS.ps1" %*
set "code=%ERRORLEVEL%"
echo.
pause
exit /b %code%
