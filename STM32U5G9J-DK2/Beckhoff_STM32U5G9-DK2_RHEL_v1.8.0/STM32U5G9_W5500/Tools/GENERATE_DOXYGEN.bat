@echo off
setlocal
set "ROOT=%~dp0.."
where doxygen >nul 2>&1 || (echo ERROR: doxygen was not found in PATH. & exit /b 1)
pushd "%ROOT%"
doxygen Doxyfile
set "RC=%ERRORLEVEL%"
popd
exit /b %RC%
