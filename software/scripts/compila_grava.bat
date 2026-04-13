@echo off
REM --- AJUSTE O CAMINHO SE NECESSARIO ---
set "NIOS_SHELL=F:\altera\13.0sp1\nios2eds\Nios_Shell.bat"

REM echo.
echo ==========================================
echo [1/3] Liberando JTAG...
echo ==========================================

taskkill /F /IM nios2-terminal.exe /T >nul 2>&1
taskkill /F /IM nios2-gdb-server.exe /T >nul 2>&1
taskkill /F /IM openocd.exe /T >nul 2>&1

REM Hack de delay compativel (1 segundo)
ping -n 2 127.0.0.1 >nul

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\workspace") do set "APP_DIR=%%~fI"
set "APP_DIR_SLASH=%APP_DIR:\=/%"
set "APP_DIR_CYG=/cygdrive/%APP_DIR_SLASH:~0,1%%APP_DIR_SLASH:~2%"

REM --- Navegacao (workspace) ---
if not exist "%APP_DIR%\\main.c" (
    echo ERRO: Pasta workspace nao encontrada ou sem main.c: %APP_DIR%
    pause
    exit /b 1
)
cd /d "%APP_DIR%"

echo.
echo [INFO] Regenerando Makefile para garantir caminhos do workspace atual...
call "%SCRIPT_DIR%init_makefile.bat"

if not exist "Makefile" (
    echo.
    echo [ERRO] Makefile nao encontrado em %CD%
    pause
    exit /b 1
)

echo.
echo ==========================================
echo [2/3] Compilando (Clean Build)...
echo ==========================================

REM "make clean all" força a criacao do .elf
call "%NIOS_SHELL%" sh -c "cd \"%APP_DIR_CYG%\" && make clean all APP_CFLAGS_USER_FLAGS=-std=c99"

if %errorlevel% neq 0 (
    echo.
    echo [ERRO] A compilacao falhou.
    pause
    exit /b %errorlevel%
)

echo.
echo ==========================================
echo [3/3] Gravando e Abrindo Terminal...
echo ==========================================

REM Tenta gravar game.elf (Padrao do template)
REM Se der erro, verifique se o Makefile esta gerando com outro nome
call "%NIOS_SHELL%" sh -c "cd \"%APP_DIR_CYG%\" && nios2-download -g main.elf --accept-bad-sysid && nios2-terminal"
