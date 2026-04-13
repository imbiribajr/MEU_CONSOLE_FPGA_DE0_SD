@echo off
REM --- AJUSTE O CAMINHO DA SUA VERSAO SE NECESSARIO ---
set "NIOS_SHELL=F:\altera\13.0sp1\nios2eds\Nios_Shell.bat"
set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\\bsp") do set "BSP_DIR=%%~fI"

echo.
echo ==========================================
echo [1/3] Liberando JTAG (Fechando processos antigos)...
echo ==========================================

REM Mata o terminal e o debug server se estiverem rodando
REM O ">nul 2>&1" esconde o erro caso eles nao estejam abertos (o que eh normal)
taskkill /F /IM nios2-terminal.exe /T >nul 2>&1
taskkill /F /IM nios2-gdb-server.exe /T >nul 2>&1
taskkill /F /IM openocd.exe /T >nul 2>&1

REM Hack de delay compativel (1 segundo)
ping -n 2 127.0.0.1 >nul

echo.
echo ==========================================
echo [BSP] Limpando e Compilando Bibliotecas...
echo ==========================================

if not exist "%NIOS_SHELL%" (
    echo ERRO: Shell do Nios nao encontrado.
    pause
    exit /b 1
)

REM --- Navega para a pasta BSP ---
cd /d "%BSP_DIR%"

REM --- Compila ---
REM "make clean all" garante que ele apague lixo velho e compile do zero.
REM Isso é vital para o BSP, pois ele contém endereços de hardware.
call "%NIOS_SHELL%" sh -c "make clean all"

if %errorlevel% neq 0 (
    echo.
    echo [ERRO] O BSP falhou. Verifique se o hardware mudou e se regenerou os arquivos.
    pause
    exit /b %errorlevel%
)

echo.
echo [SUCESSO] BSP pronto. Agora compile a Aplicacao.
