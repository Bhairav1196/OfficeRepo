@echo off
setlocal
set "ROOT=%~dp0.."

where powershell.exe >nul 2>&1 || (
  echo ERROR: Windows PowerShell was not found.
  exit /b 1
)

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0prepare_touchgfx_tools.ps1" -ProjectRoot "%ROOT%"
exit /b %ERRORLEVEL%
