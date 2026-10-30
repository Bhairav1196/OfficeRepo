@echo off
setlocal
set "ROOT=%~dp0.."
where py >nul 2>nul && (py -3 "%~dp0reapply_threadx_hook.py" "%ROOT%" & exit /b %ERRORLEVEL%)
where python >nul 2>nul && (python "%~dp0reapply_threadx_hook.py" "%ROOT%" & exit /b %ERRORLEVEL%)
where python3 >nul 2>nul && (python3 "%~dp0reapply_threadx_hook.py" "%ROOT%" & exit /b %ERRORLEVEL%)
echo ERROR: Python 3 not found. Install Python 3.10+ or add it to PATH.
exit /b 1
