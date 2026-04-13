@echo off
set "NIOS_SHELL=F:\altera\13.0sp1\nios2eds\Nios_Shell.bat"

:loop
echo.
echo ==========================================
echo Iniciando Servidor de Debug (Ctrl+C para sair)...
echo ==========================================
"%NIOS_SHELL%" sh -c "nios2-gdb-server --tcpport 3333 --stop"

echo.
echo Servidor caiu. Reiniciando em 2 segundos...
timeout /t 2 >nul
goto loop