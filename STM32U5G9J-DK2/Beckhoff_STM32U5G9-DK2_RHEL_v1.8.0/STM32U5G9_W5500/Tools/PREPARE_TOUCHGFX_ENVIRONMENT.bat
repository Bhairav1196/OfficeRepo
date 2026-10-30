@echo off
setlocal

echo ============================================================
echo TouchGFX 4.26.1 one-time project environment preparation
echo No recursive filesystem scan is performed in v2.0.0-alpha.9.0.
echo ============================================================
echo.
echo [1/2] Preparing official TouchGFX 4.26.1 asset converters...
call "%~dp0PREPARE_TOUCHGFX_TOOLS.bat"
if errorlevel 1 exit /b %ERRORLEVEL%

echo [2/2] Preparing Roboto source fonts (installed or official download)...
call "%~dp0PREPARE_TOUCHGFX_FONT.bat"
if errorlevel 1 exit /b %ERRORLEVEL%

echo.
echo TouchGFX generation environment is ready.
echo You can now open the .touchgfx project and use Generate Code.
exit /b 0
