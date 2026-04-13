@echo off
echo.
echo ========================================================
echo   LIMPEZA TOTAL DE PROCESSOS ZUMBIS (NIOS II / QUARTUS)
echo ========================================================

REM 1. Mata as ferramentas do Nios (Debug e Terminal)
echo [1/4] Matando ferramentas Nios...
taskkill /F /IM nios2-terminal.exe /T >nul 2>&1
taskkill /F /IM nios2-gdb-server.exe /T >nul 2>&1
taskkill /F /IM nios2-download.exe /T >nul 2>&1
taskkill /F /IM openocd.exe /T >nul 2>&1

REM 2. Mata ferramentas de Compilacao travadas
echo [2/4] Matando Make e Compiladores...
taskkill /F /IM make.exe /T >nul 2>&1
taskkill /F /IM cc1.exe /T >nul 2>&1
taskkill /F /IM nios2-elf-gcc.exe /T >nul 2>&1
taskkill /F /IM quartus_pgm.exe /T >nul 2>&1

REM 3. Mata os shells do Linux (Cygwin) que rodam por baixo
echo [3/4] Matando Shells do Cygwin...
taskkill /F /IM sh.exe /T >nul 2>&1
taskkill /F /IM bash.exe /T >nul 2>&1

REM 4. O FINAL BOSS: Mata os Hosts de Janela (O que apareceu no seu print)
echo [4/4] Matando janelas de console orfas (conhost)...
REM Cuidado: Isso pode fechar outros CMDs abertos, mas vai limpar a sujeira.
taskkill /F /IM conhost.exe /T >nul 2>&1

echo.
echo [OK] Limpeza concluida. Seu gerenciador de tarefas deve estar limpo.
timeout /t 2 >nul