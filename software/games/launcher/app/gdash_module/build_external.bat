@echo off
setlocal

set "NIOS_SHELL=F:\altera\13.0sp1\nios2eds\Nios_Shell.bat"
set "MODULE_DIR=%~dp0"
set "ROOT_DIR=%MODULE_DIR%..\..\..\..\.."
for %%I in ("%ROOT_DIR%") do set "ROOT_DIR=%%~fI"

set "BUILD_DIR=%MODULE_DIR%build_external"
set "GMOD_PATH=%ROOT_DIR%\software\games\launcher\app\gmods\gdash.gmod"
set "SCRIPT_GMOD=%ROOT_DIR%\software\scripts\gerar_gmod.ps1"

set "BUILD_DIR_SLASH=%BUILD_DIR:\=/%"
set "BUILD_DIR_CYG=/cygdrive/%BUILD_DIR_SLASH:~0,1%%BUILD_DIR_SLASH:~2%"

set "MODULE_DIR_SLASH=%MODULE_DIR:\=/%"
set "MODULE_DIR_CYG=/cygdrive/%MODULE_DIR_SLASH:~0,1%%MODULE_DIR_SLASH:~2%"

set "ROOT_DIR_SLASH=%ROOT_DIR:\=/%"
set "ROOT_DIR_CYG=/cygdrive/%ROOT_DIR_SLASH:~0,1%%ROOT_DIR_SLASH:~2%"

if not exist "%NIOS_SHELL%" exit /b 1
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

cd /d "%BUILD_DIR%"
if exist "Makefile" del /f /q "Makefile"

call "%NIOS_SHELL%" sh -c "cd \"%BUILD_DIR_CYG%\" && nios2-app-generate-makefile --app-dir=. --bsp-dir=\"%ROOT_DIR_CYG%/software/bsp\" --elf-name=main.elf --src-files=\"%MODULE_DIR_CYG%/external_main.c\" --src-files=\"%MODULE_DIR_CYG%/main.c\" --inc-dir=\"%MODULE_DIR_CYG%\" --inc-dir=\"%ROOT_DIR_CYG%/software/games/launcher/app\" --inc-dir=\"%ROOT_DIR_CYG%/software/terasic_lib\" --inc-dir=\"%ROOT_DIR_CYG%/hardware\""
if not exist "Makefile" exit /b 1

call "%NIOS_SHELL%" sh -c "cd \"%BUILD_DIR_CYG%\" && cp \"%ROOT_DIR_CYG%/software/games/launcher/app/linker_external_game.x\" ./linker_external_game.x && make clean all \"APP_CFLAGS_USER_FLAGS=-std=c99 -G0\" LINKER_SCRIPT=./linker_external_game.x"
if %errorlevel% neq 0 exit /b %errorlevel%

powershell -ExecutionPolicy Bypass -File "%SCRIPT_GMOD%" ^
  -ElfPath "%BUILD_DIR%\main.elf" ^
  -Title "GDASH MOD" ^
  -OutputPath "%GMOD_PATH%"
if %errorlevel% neq 0 exit /b %errorlevel%

echo GMOD gerado em %GMOD_PATH%
endlocal
