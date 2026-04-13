@echo off
REM --- AJUSTE O CAMINHO SE NECESSARIO ---
set "NIOS_SHELL=F:\altera\13.0sp1\nios2eds\Nios_Shell.bat"
set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\\..\\hardware") do set "HARDWARE_DIR=%%~fI"

cd /d "%HARDWARE_DIR%"

echo.
echo ==========================================
echo [QSYS] Gerando arquivos HDL (VHDL/Verilog)...
echo ==========================================
echo Isso pode levar alguns minutos. Aguarde...
echo.

if not exist "%NIOS_SHELL%" (
    echo ERRO: Shell do Nios nao encontrado.
    pause
    exit /b 1
)

REM --- O COMANDO MÁGICO ---
REM qsys-generate: A ferramenta que substitui o botão "Generate"
REM --synthesis=VHDL: Diz para gerar arquivos para síntese VHDL
REM NiosII_ps2.qsys: O nome do seu sistema (ajuste se for diferente)

"%NIOS_SHELL%" sh -c "qsys-generate NiosII_ps2.qsys --synthesis=VHDL"

if %errorlevel% neq 0 (
    echo.
    echo [ERRO] Falha na geracao do Qsys!
    echo Verifique se o arquivo .qsys existe e se nao ha erros de sintaxe nos componentes.
    pause
    exit /b %errorlevel%
)

echo.
echo [SUCESSO] Hardware gerado! Agora pode Sintetizar o VHDL.
