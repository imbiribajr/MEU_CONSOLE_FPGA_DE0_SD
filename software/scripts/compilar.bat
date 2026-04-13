@echo off
REM --- AJUSTE O CAMINHO CONFORME SUA VERSAO (13.0 ou 13.0sp1) ---
set "NIOS_SHELL=F:\altera\13.0sp1\nios2eds\Nios_Shell.bat"

echo.
echo ==========================================
echo Compilando Game ...
echo ==========================================

set "SCRIPT_DIR=%~dp0"
set "APP_DIR=%CD%"
set "APP_DIR_SLASH=%APP_DIR:\=/%"
set "APP_DIR_CYG=/cygdrive/%APP_DIR_SLASH:~0,1%%APP_DIR_SLASH:~2%"

REM Verifica se o Shell existe
if not exist "%NIOS_SHELL%" (
    echo ERRO: Nao encontrei o arquivo do Nios em:
    echo %NIOS_SHELL%
    pause
    exit /b 1
)

if not exist "%APP_DIR%\\main.c" (
    echo ERRO: Rode este .bat dentro da pasta app do jogo: %APP_DIR%
    pause
    exit /b 1
)

echo.
echo [INFO] Regenerando Makefile para garantir caminhos do workspace atual...
call "%SCRIPT_DIR%init_makefile.bat"

if not exist "Makefile" (
    echo.
    echo [ERRO] Makefile nao encontrado em %CD%
    pause
    exit /b 1
)

REM --- O COMANDO MÁGICO ---
REM sh -c: Abre o linux embutido
REM make all: Compila
REM APP_CFLAGS...: Força o padrão C99
"%NIOS_SHELL%" sh -c "cd \"%APP_DIR_CYG%\" && make clean all APP_CFLAGS_USER_FLAGS=-std=c99"

REM Verifica se houve erro na compilação
if %errorlevel% neq 0 (
    echo.
    echo [ERRO] Falha na compilacao! Verifique o codigo.
    pause
    exit /b %errorlevel%
)

echo.
echo [SUCESSO] Compilação finalizada.
