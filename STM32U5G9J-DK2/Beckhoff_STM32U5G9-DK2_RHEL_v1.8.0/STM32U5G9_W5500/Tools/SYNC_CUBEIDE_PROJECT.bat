@echo off
setlocal
set "ROOT=%~dp0.."
where python >nul 2>&1 && (python "%~dp0sync_cubeide_project.py" "%ROOT%" & exit /b %errorlevel%)
where python3 >nul 2>&1 && (python3 "%~dp0sync_cubeide_project.py" "%ROOT%" & exit /b %errorlevel%)
where py >nul 2>&1 && (py -3 "%~dp0sync_cubeide_project.py" "%ROOT%" & exit /b %errorlevel%)
echo ERROR: Python 3 was not found. Install Python 3 or use TouchGFX update_project with .extSettings.
exit /b 1
