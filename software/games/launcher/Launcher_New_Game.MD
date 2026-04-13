# Como Inserir Um Novo Jogo No Launcher

Este documento descreve o fluxo atual para adicionar um novo jogo ao ecossistema do launcher.

Importante:

- o fluxo atual usa jogos externos em arquivo `.gmod`
- o `.gmod` final e gerado a partir de um `main.elf`
- o launcher carrega esse binario do SD para SDRAM e transfere a execucao

## Fluxo Resumido

1. criar a pasta do modulo do jogo
2. adaptar o codigo ao hardware da plataforma
3. criar `external_main.c`
4. criar `build_external.bat`
5. compilar o `main.elf`
6. gerar o `.gmod`
7. copiar o `.gmod` para o SD
8. iniciar o launcher e carregar o jogo

## Estrutura Recomendada

Crie uma pasta nova dentro de:

- `software/games/launcher/app/`

Exemplo:

- `software/games/launcher/app/meujogo_module/`

Estrutura minima recomendada:

- `main.c`
- `external_main.c`
- `meujogo_module.h`
- `build_external.bat`

## 1. Adaptar O Jogo Para A Plataforma

O jogo precisa ser ajustado para:

- display da plataforma
- input via PS/2 e/ou UART
- temporizacao baseada no BSP
- includes de `system.h`
- layout de memoria do ambiente Nios II

Os pontos mais comuns de adaptacao sao:

- render
- leitura de entrada
- loop principal
- delays e ticks
- acesso a periféricos

## 2. Criar O Header Do Modulo

Exemplo de `meujogo_module.h`:

```c
#ifndef MEUJOGO_MODULE_H
#define MEUJOGO_MODULE_H

void meujogo_module_entry(void);

#endif
```

## 3. Criar O `main.c` Do Modulo

O `main.c` contem a logica principal do jogo e deve expor uma entry chamavel pelo wrapper externo.

Exemplo simplificado:

```c
#include "meujogo_module.h"

void meujogo_module_entry(void)
{
    while (1) {
        /* loop do jogo */
    }
}
```

## 4. Criar O `external_main.c`

O build externo precisa de um `main()` convencional.

Exemplo:

```c
#include "meujogo_module.h"

int main(void)
{
    meujogo_module_entry();
    return 0;
}
```

## 5. Criar O `build_external.bat`

Esse script deve:

1. gerar o `Makefile`
2. compilar o `main.elf`
3. empacotar o ELF em `.gmod`

Modelo pratico:

```bat
@echo off
setlocal

set "NIOS_SHELL=F:\altera\13.0sp1\nios2eds\Nios_Shell.bat"
set "MODULE_DIR=%~dp0"
set "ROOT_DIR=%MODULE_DIR%..\..\..\..\.."
for %%I in ("%ROOT_DIR%") do set "ROOT_DIR=%%~fI"

set "BUILD_DIR=%MODULE_DIR%build_external"
set "GMOD_PATH=%ROOT_DIR%\software\games\launcher\app\gmods\meujogo.gmod"
set "SCRIPT_GMOD=%ROOT_DIR%\software\scripts\gerar_gmod.ps1"

set "BUILD_DIR_SLASH=%BUILD_DIR:\=/%"
set "BUILD_DIR_CYG=/cygdrive/%BUILD_DIR_SLASH:~0,1%%BUILD_DIR_SLASH:~2%"

set "MODULE_DIR_SLASH=%MODULE_DIR:\=/%"
set "MODULE_DIR_CYG=/cygdrive/%MODULE_DIR_SLASH:~0,1%%MODULE_DIR_SLASH:~2%"

set "ROOT_DIR_SLASH=%ROOT_DIR:\=/%"
set "ROOT_DIR_CYG=/cygdrive/%ROOT_DIR_SLASH:~0,1%%ROOT_DIR_SLASH:~2%"

if not exist "%NIOS_SHELL%" exit /b 1
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

cd /d "%BUILD_DIR%"
if exist "Makefile" del /f /q "Makefile"

call "%NIOS_SHELL%" sh -c "cd \"%BUILD_DIR_CYG%\" && nios2-app-generate-makefile --app-dir=. --bsp-dir=\"%ROOT_DIR_CYG%/software/bsp\" --elf-name=main.elf --src-files=\"%MODULE_DIR_CYG%/external_main.c\" --src-files=\"%MODULE_DIR_CYG%/main.c\" --inc-dir=\"%MODULE_DIR_CYG%\" --inc-dir=\"%ROOT_DIR_CYG%/hardware\""
if not exist "Makefile" exit /b 1

call "%NIOS_SHELL%" sh -c "cd \"%BUILD_DIR_CYG%\" && cp \"%ROOT_DIR_CYG%/software/games/launcher/app/linker_external_game.x\" ./linker_external_game.x && make clean all \"APP_CFLAGS_USER_FLAGS=-std=c99 -G0\" LINKER_SCRIPT=./linker_external_game.x"
if %errorlevel% neq 0 exit /b %errorlevel%

powershell -ExecutionPolicy Bypass -File "%SCRIPT_GMOD%" ^
  -ElfPath "%BUILD_DIR%\main.elf" ^
  -Title "MEUJOGO" ^
  -OutputPath "%GMOD_PATH%"
if %errorlevel% neq 0 exit /b %errorlevel%

echo GMOD gerado em %GMOD_PATH%
endlocal
```

## 6. Compilar E Gerar O `.gmod`

Exemplo:

```powershell
cd F:\Jogos_FPGA\MEU_CONSOLE_FPGA_DE0_SD\software\games\launcher\app\meujogo_module
.\build_external.bat
```

O arquivo final deve ser gerado em:

- `software/games/launcher/app/gmods/meujogo.gmod`

## 7. Copiar O `.gmod` Para O SD

Copie o `.gmod` para a pasta de jogos do cartao SD.

Exemplo:

- `games/meujogo.gmod`

## 8. Executar Pelo Launcher

Com o launcher rodando:

1. monte o SD
2. selecione o novo item no menu
3. carregue o jogo

## Referencias

- [README.md](./README.md)
- [EXTERNAL_BUILD.md](./EXTERNAL_BUILD.md)
- [main.c do launcher](./app/main.c)
- [linker_external_game.x](./app/linker_external_game.x)
- [gerar_gmod.ps1](../../scripts/gerar_gmod.ps1)
