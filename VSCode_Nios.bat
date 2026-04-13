@echo off
echo Configurando ambiente e abrindo VS Code...

REM 1. Define o caminho do Shell do Nios
set "NIOS_PATH=F:\altera\13.0sp1\nios2eds\Nios_Shell.bat"

REM 2. Define o caminho do VS Code usando BARRAS NORMAIS (/) para não dar erro no Linux
REM Isso é o segredo para funcionar dentro do sh -c
set "VSCODE_EXE=C:/Users/Imbiriba/AppData/Local/Programs/Microsoft VS Code/Code.exe"

REM 3. Muda para a pasta raiz do projeto a partir do proprio script
set "PROJECT_ROOT=%~dp0"
for %%I in ("%PROJECT_ROOT%") do set "PROJECT_ROOT=%%~fI"
cd /d "%PROJECT_ROOT%"

REM 4. O COMANDO BLINDADO
REM Explicação:
REM cmd /c start "" "..." -> Abre separado
REM \"%VSCODE_EXE%\"      -> Passa o caminho com aspas (escapadas) para lidar com espaços
REM .                     -> Abre a pasta atual
REM & exec bash           -> Mantém o terminal preto vivo

call "%NIOS_PATH%" sh -c "cmd /c start \"\" \"%VSCODE_EXE%\" . & exec bash"
