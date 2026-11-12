@echo off
rem PushFPS installer: runs Install-PushFPS.ps1 next to this file. Extra arguments are passed on
rem (for example: Install-PushFPS.cmd -Uninstall).
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-PushFPS.ps1" %*
set "code=%ERRORLEVEL%"
echo.
pause
exit /b %code%
