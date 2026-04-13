@echo off
setlocal

set "NIOS_SHELL=F:\altera\13.0sp1\nios2eds\Nios_Shell.bat"
set "APP_DIR=%~dp0"
set "PROJECT_ROOT=%APP_DIR%..\..\..\.."
for %%I in ("%PROJECT_ROOT%") do set "PROJECT_ROOT=%%~fI"
set "HARDWARE_DIR=%PROJECT_ROOT%\hardware"
set "HEX_OUT_WIN=%HARDWARE_DIR%\onchip_mem.hex"
set "HEX_SUB_WIN=%HARDWARE_DIR%\NiosII_ps2\synthesis\submodules\niosII_ps2_onchip_memory2_0.hex"
set "APP_DIR_SLASH=%APP_DIR:\=/%"
set "APP_DIR_CYG=/cygdrive/%APP_DIR_SLASH:~0,1%%APP_DIR_SLASH:~2%"
set "HEX_OUT_SLASH=%HEX_OUT_WIN:\=/%"
set "HEX_OUT=/cygdrive/%HEX_OUT_SLASH:~0,1%%HEX_OUT_SLASH:~2%"
set "HEX_SUB_SLASH=%HEX_SUB_WIN:\=/%"

cd /d "%~dp0"
if not exist "%NIOS_SHELL%" (
    echo ERRO: Shell do Nios nao encontrado: %NIOS_SHELL%
    exit /b 1
)

if not exist "main.elf" (
    echo ERRO: main.elf nao encontrado. Compile o flashboot primeiro.
    exit /b 1
)

call "%NIOS_SHELL%" sh -c "cd \"%APP_DIR_CYG%\" && elf2hex --input=main.elf --output=\"%HEX_OUT_SLASH%\" --base=0x0180C000 --end=0x0180FFFF --width=32"
if %errorlevel% neq 0 (
    echo ERRO: falha ao gerar onchip_mem.hex
    exit /b %errorlevel%
)

copy /y "%HEX_OUT_WIN%" "%HEX_SUB_WIN%" >nul
if %errorlevel% neq 0 (
    echo ERRO: falha ao atualizar hex do submodule da on-chip
    exit /b %errorlevel%
)

echo onchip_mem.hex atualizado em %HEX_OUT_WIN%
echo onchip hex do submodule atualizado em %HEX_SUB_WIN%
endlocal
