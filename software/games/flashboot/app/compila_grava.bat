@echo off
setlocal
set "NIOS_SHELL=F:\altera\13.0sp1\nios2eds\Nios_Shell.bat"
set "APP_DIR=%~dp0"
set "APP_DIR_SLASH=%APP_DIR:\=/%"
set "APP_DIR_CYG=/cygdrive/%APP_DIR_SLASH:~0,1%%APP_DIR_SLASH:~2%"
set "LINKER_SCRIPT=./linker_flashboot.x"

taskkill /F /IM nios2-terminal.exe /T >nul 2>&1
taskkill /F /IM nios2-gdb-server.exe /T >nul 2>&1
taskkill /F /IM openocd.exe /T >nul 2>&1
ping -n 2 127.0.0.1 >nul

cd /d "%~dp0"
if not exist "%NIOS_SHELL%" exit /b 1
if exist "Makefile" del /f /q "Makefile"

call "%NIOS_SHELL%" sh -c "cd \"%APP_DIR_CYG%\" && nios2-app-generate-makefile --app-dir=. --bsp-dir=../../../bsp --elf-name=main.elf --src-rdir=. --src-rdir=../../../../hardware --inc-dir=../../../../hardware --src-rdir=../../../terasic_lib --inc-dir=../../../terasic_lib --inc-dir=../../.."
if not exist "Makefile" exit /b 1

call "%NIOS_SHELL%" sh -c "cd \"%APP_DIR_CYG%\" && make clean all \"APP_CFLAGS_USER_FLAGS=-std=c99\" LINKER_SCRIPT=%LINKER_SCRIPT%"
if %errorlevel% neq 0 exit /b %errorlevel%

call "%NIOS_SHELL%" sh -c "cd \"%APP_DIR_CYG%\" && nios2-download -r -g main.elf --accept-bad-sysid && nios2-terminal"
endlocal
