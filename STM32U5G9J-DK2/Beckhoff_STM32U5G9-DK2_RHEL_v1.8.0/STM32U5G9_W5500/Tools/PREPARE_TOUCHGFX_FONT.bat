@echo off
setlocal

rem PowerShell handles HTTPS, ZIP extraction, and font validation.
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0prepare_touchgfx_font.ps1"
exit /b %ERRORLEVEL%
