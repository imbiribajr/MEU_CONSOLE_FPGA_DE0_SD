@echo off
REM --- AJUSTE O CAMINHO SE NECESSARIO ---
set "NIOS_SHELL=F:\altera\13.0sp1\nios2eds\Nios_Shell.bat"

echo.
echo ==========================================
echo [MAKEFILE] Gerando Makefile para o Jogo...
echo ==========================================

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\workspace") do set "APP_DIR=%%~fI"
set "APP_DIR_SLASH=%APP_DIR:\=/%"
set "APP_DIR_CYG=/cygdrive/%APP_DIR_SLASH:~0,1%%APP_DIR_SLASH:~2%"
for %%I in ("%APP_DIR%\..") do set "ROOT_DIR=%%~fI"
set "ROOT_DIR_SLASH=%ROOT_DIR:\=/%"
set "ROOT_DIR_CYG=/cygdrive/%ROOT_DIR_SLASH:~0,1%%ROOT_DIR_SLASH:~2%"
for %%I in ("%ROOT_DIR%\bsp") do set "BSP_DIR=%%~fI"
set "BSP_DIR_SLASH=%BSP_DIR:\=/%"
set "BSP_DIR_CYG=/cygdrive/%BSP_DIR_SLASH:~0,1%%BSP_DIR_SLASH:~2%"
for %%I in ("%ROOT_DIR%\hardware") do set "HARDWARE_DIR=%%~fI"
set "HARDWARE_DIR_SLASH=%HARDWARE_DIR:\=/%"
set "HARDWARE_DIR_CYG=/cygdrive/%HARDWARE_DIR_SLASH:~0,1%%HARDWARE_DIR_SLASH:~2%"
for %%I in ("%ROOT_DIR%\terasic_lib") do set "TERASIC_DIR=%%~fI"
set "TERASIC_DIR_SLASH=%TERASIC_DIR:\=/%"
set "TERASIC_DIR_CYG=/cygdrive/%TERASIC_DIR_SLASH:~0,1%%TERASIC_DIR_SLASH:~2%"

if not exist "%NIOS_SHELL%" (
    echo ERRO: Shell do Nios nao encontrado.
    pause
    exit /b 1
)

REM --- Usa o workspace fixo ---
if not exist "%APP_DIR%\main.c" (
    echo ERRO: Pasta workspace nao encontrada ou sem main.c: %APP_DIR%
    pause
    exit /b 1
)

call "%NIOS_SHELL%" sh -c "cd \"%APP_DIR_CYG%\" && nios2-app-generate-makefile --app-dir=. --bsp-dir=\"%BSP_DIR_CYG%\" --elf-name=main.elf --src-rdir=. --src-rdir=\"%HARDWARE_DIR_CYG%\" --inc-dir=\"%HARDWARE_DIR_CYG%\" --src-rdir=\"%TERASIC_DIR_CYG%\" --inc-dir=\"%TERASIC_DIR_CYG%\" --inc-dir=\"%ROOT_DIR_CYG%\""

if %errorlevel% neq 0 (
    echo.
    echo [ERRO] Falha ao gerar Makefile. Verifique se a pasta BSP existe.
    pause
    exit /b %errorlevel%
)

echo.
echo [SUCESSO] Makefile gerado! Agora voce pode compilar.
