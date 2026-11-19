@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0inject.ps1" %*
set "inject_result=%errorlevel%"
if not "%inject_result%"=="0" pause
exit /b %inject_result%
