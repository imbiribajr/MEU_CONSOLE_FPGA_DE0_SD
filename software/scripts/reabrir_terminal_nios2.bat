@echo off
setlocal

set "NIOS_SHELL=F:\altera\13.0sp1\nios2eds\Nios_Shell.bat"

echo Fechando instancias antigas do terminal Nios II...
taskkill /F /IM nios2-terminal.exe /T >nul 2>&1

if not exist "%NIOS_SHELL%" (
    echo ERRO: Nios_Shell.bat nao encontrado em:
    echo %NIOS_SHELL%
    pause
    exit /b 1
)

echo Reabrindo nios2-terminal...
call "%NIOS_SHELL%" sh -c "nios2-terminal"

