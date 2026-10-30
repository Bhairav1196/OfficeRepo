@echo off
setlocal
for %%I in ("%~dp0..") do set "ROOT=%%~fI"
set "BUILD=%ROOT%\tests\.host_build_win"
if exist "%BUILD%" rmdir /s /q "%BUILD%"
mkdir "%BUILD%" || exit /b 1

where gcc >nul 2>&1 || (echo ERROR: gcc not found in PATH & exit /b 1)
where g++ >nul 2>&1 || (echo ERROR: g++ not found in PATH & exit /b 1)

set "CFG=-I%ROOT%\Application\Common\Inc -I%ROOT%\Application\Config\Inc"
set "IO=%CFG% -I%ROOT%\Application\Services\IO\Inc"
set "DIAG=%IO% -I%ROOT%\Application\Services\Diagnostics\Inc"
set "W5500=-I%ROOT%\Application\Drivers\W5500\Inc -I%ROOT%\Application\Drivers\Platform\Inc"
set "MODBUS=%W5500% -I%ROOT%\Application\Services\Modbus\Inc -I%ROOT%\Application\Config\Inc"
set "HMI=%DIAG% -I%ROOT%\Application\Interfaces\Inc"

call :run_c scaling "%ROOT%\tests\test_scaling.c" "%ROOT%\Application\Config\Src\io_config.c" "%ROOT%\Application\Config\Src\io_config_validate.c" "%ROOT%\Application\Services\IO\Src\io_service.c" -lm %IO% || exit /b 1
call :run_c io_config "%ROOT%\tests\test_io_config.c" "%ROOT%\Application\Config\Src\io_config.c" "%ROOT%\Application\Config\Src\io_config_validate.c" "%ROOT%\Application\Services\IO\Src\io_service.c" -lm %IO% || exit /b 1
call :run_c modbus "%ROOT%\tests\test_modbus_tcp.c" "%ROOT%\Application\Services\Modbus\Src\modbus_tcp_client.c" %MODBUS% || exit /b 1
call :run_c w5500 "%ROOT%\tests\test_w5500_driver.c" "%ROOT%\Application\Drivers\W5500\Src\w5500_driver.c" %W5500% || exit /b 1

echo HOST C TESTS: PASS
exit /b 0

:run_c
set "NAME=%~1"
shift
gcc -std=c11 -Wall -Wextra -Werror -pedantic %* -o "%BUILD%\%NAME%.exe" || exit /b 1
"%BUILD%\%NAME%.exe" || exit /b 1
exit /b 0
