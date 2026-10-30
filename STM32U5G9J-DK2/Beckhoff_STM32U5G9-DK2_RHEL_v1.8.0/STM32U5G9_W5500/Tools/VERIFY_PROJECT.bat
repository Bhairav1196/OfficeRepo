@echo off
setlocal
set "ROOT=%~dp0.."
where py >nul 2>nul && (py -3 "%~dp0verify_project.py" "%ROOT%" & exit /b %ERRORLEVEL%)
where python >nul 2>nul && (python "%~dp0verify_project.py" "%ROOT%" & exit /b %ERRORLEVEL%)
where python3 >nul 2>nul && (python3 "%~dp0verify_project.py" "%ROOT%" & exit /b %ERRORLEVEL%)
echo ERROR: Python 3 not found.
exit /b 1
