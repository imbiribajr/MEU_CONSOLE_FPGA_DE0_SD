@echo off
REM --- AJUSTE O CAMINHO ABAIXO CONFORME SUA VERSAO (13.0 ou 13.0sp1) ---
set "NIOS_SHELL=F:\altera\13.0sp1\nios2eds\Nios_Shell.bat"

set "SCRIPT_DIR=%~dp0"
set "APP_DIR=%CD%"
set "APP_DIR_SLASH=%APP_DIR:\=/%"
set "APP_DIR_CYG=/cygdrive/%APP_DIR_SLASH:~0,1%%APP_DIR_SLASH:~2%"

if not exist "%APP_DIR%\\main.c" (
    echo ERRO: Rode este .bat dentro da pasta app do jogo: %APP_DIR%
    pause
    exit /b 1
)

echo.
echo ==========================================
echo Iniciando Ambiente Nios II...
echo ==========================================

REM Verifica se o arquivo existe para dar um erro amigavel
if not exist "%NIOS_SHELL%" (
    echo ERRO: Nao encontrei o arquivo do Nios em:
    echo %NIOS_SHELL%
    echo Verifique se o caminho esta correto no arquivo gravar.bat
    pause
    exit /b 1
)

REM Chama o Shell do Nios e passa o comando de gravacao
REM O "sh -c" diz para rodar os comandos Linux dentro do Windows
"%NIOS_SHELL%" sh -c "cd \"%APP_DIR_CYG%\" && nios2-download -g main.elf --accept-bad-sysid && nios2-terminal"

echo.
echo Fim.
