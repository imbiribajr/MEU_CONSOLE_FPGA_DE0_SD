# Compilacao Externa Dos Jogos

Este documento descreve o fluxo correto para transformar um jogo do ambiente de desenvolvimento em um jogo de producao carregavel pelo `launcher` como arquivo `.gmod`.

## Visao Geral

Na arquitetura final validada:

1. o `flashboot` sobe da on-chip
2. o `launcher` sobe da NOR
3. os jogos sao carregados do SD como binarios `.gmod`

O ponto importante e este:

- o ELF usado para gerar o `.gmod` nao deve vir diretamente do app standalone antigo de desenvolvimento
- o ELF deve ser gerado a partir do codigo adaptado ao ambiente do `launcher`

## Diferenca Entre Desenvolvimento E Producao

### Desenvolvimento standalone

Durante o desenvolvimento, o jogo costuma existir como app independente, por exemplo:

- `software/games/pacman/app/main.c`
- `software/games/tetris/app/main.c`

Esse fluxo e util para teste isolado com `nios2-download`, mas nao e a melhor entrada para gerar o `.gmod` final.

### Producao no launcher

No fluxo de producao, cada jogo fica adaptado como modulo dentro do `launcher`, por exemplo:

- `software/games/launcher/app/pacman_module/main.c`
- `software/games/launcher/app/tetris_module/main.c`
- `software/games/launcher/app/pong_module/main.c`
- `software/games/launcher/app/snake_module/main.c`
- `software/games/launcher/app/spaceinvaders_module/main.c`
- `software/games/launcher/app/riverraid_module/main.c`
- `software/games/launcher/app/gdash_module/main.c`

Esses `main.c` de modulo sao a base correta para a versao de producao.

## Estrutura De Cada Jogo Externo

Cada jogo externo usa tres pecas:

1. `main.c`
2. `external_main.c`
3. `build_external.bat`

### 1. `main.c`

Arquivo principal do modulo do jogo.

Exemplo:

- [pacman_module/main.c](./app/pacman_module/main.c)

Ele contem a logica real do jogo e normalmente expoe uma entry do modulo, como:

- `pacman_module_entry()`
- `tetris_module_entry()`

### 2. `external_main.c`

Wrapper minimo para transformar o modulo em app externo linkavel.

Exemplo:

- [pacman_module/external_main.c](./app/pacman_module/external_main.c)

Padrao:

```c
#include "pacman_module.h"

int main(void)
{
    pacman_module_entry();
    return 0;
}
```

Esse arquivo existe porque o build externo precisa de um `main()` normal.

### 3. `build_external.bat`

Script responsavel por:

1. gerar o `Makefile`
2. compilar o `main.elf`
3. empacotar o `main.elf` em `.gmod`

Exemplo:

- [pacman_module/build_external.bat](./app/pacman_module/build_external.bat)

## Linker Externo

O build externo usa um linker especifico:

- [linker_external_game.x](./app/linker_external_game.x)

Esse linker existe para garantir que o ELF gerado seja adequado para:

- carga direta em SDRAM
- salto a partir do `launcher`
- formato compativel com `gerar_gmod.ps1`

Nao substitua esse linker pelo linker padrao do app standalone sem necessidade.

## Fluxo Correto Para Criar Um Novo Jogo Externo

Supondo que voce tenha um jogo novo chamado `meujogo`.

### 1. Criar a pasta do modulo

Estrutura esperada:

- `software/games/launcher/app/meujogo_module/main.c`
- `software/games/launcher/app/meujogo_module/meujogo_module.h`

Esse `main.c` deve conter a logica adaptada ao ambiente do `launcher`.

### 2. Criar o wrapper externo

Arquivo:

- `software/games/launcher/app/meujogo_module/external_main.c`

Modelo:

```c
#include "meujogo_module.h"

int main(void)
{
    meujogo_module_entry();
    return 0;
}
```

### 3. Criar o script de build externo

Arquivo:

- `software/games/launcher/app/meujogo_module/build_external.bat`

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

call "%NIOS_SHELL%" sh -c "cd \"%BUILD_DIR_CYG%\" && nios2-app-generate-makefile --app-dir=. --bsp-dir=\"%ROOT_DIR_CYG%/software/bsp\" --elf-name=main.elf --src-files=\"%MODULE_DIR_CYG%/external_main.c\" --src-files=\"%MODULE_DIR_CYG%/main.c\" --inc-dir=\"%MODULE_DIR_CYG%\" --inc-dir=\"%ROOT_DIR_CYG%/software/games/launcher/app\" --inc-dir=\"%ROOT_DIR_CYG%/software/terasic_lib\" --inc-dir=\"%ROOT_DIR_CYG%/hardware\""
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

## Processo De Compilacao De Um Jogo Ja Existente

### Pac-Man

Codigo de producao:

- [pacman_module/main.c](./app/pacman_module/main.c)

Wrapper:

- [pacman_module/external_main.c](./app/pacman_module/external_main.c)

Build:

```powershell
cd F:\Jogos_FPGA\MEU_CONSOLE_FPGA_DE0_SD\software\games\launcher\app\pacman_module
.\build_external.bat
```

Saida:

- [pacman.gmod](./app/gmods/pacman.gmod)

### Tetris

Codigo de producao:

- [tetris_module/main.c](./app/tetris_module/main.c)

Wrapper:

- [tetris_module/external_main.c](./app/tetris_module/external_main.c)

Build:

```powershell
cd F:\Jogos_FPGA\MEU_CONSOLE_FPGA_DE0_SD\software\games\launcher\app\tetris_module
.\build_external.bat
```

Saida:

- [tetris.gmod](./app/gmods/tetris.gmod)

### Outros Jogos

Mesmo padrao:

- `pong_module`
- `snake_module`
- `spaceinvaders_module`
- `riverraid_module`
- `gdash_module`

Cada um possui:

- `main.c`
- `external_main.c`
- `build_external.bat`

## Onde O `.gmod` E Gerado

Os binarios finais ficam em:

- [software/games/launcher/app/gmods](./app/gmods)

Arquivos atuais esperados:

- `pacman.gmod`
- `tetris.gmod`
- `pong.gmod`
- `snake.gmod`
- `spaceinvaders.gmod`
- `riverraid.gmod`
- `gdash.gmod`

## Copia Para O SD

Depois do build externo:

1. pegue o `.gmod` gerado em `software/games/launcher/app/gmods/`
2. copie para a pasta `games/` do cartao SD
3. ligue a placa
4. o `launcher` vai listar o jogo automaticamente

## Regras Praticas

- nao gere o `.gmod` final a partir do `main.elf` standalone antigo sem necessidade
- use como base o `main.c` de producao dentro de `launcher/app/*_module/`
- mantenha o `external_main.c` o mais simples possivel
- mantenha o `build_external.bat` apontando para `linker_external_game.x`
- use `-G0` no build externo para evitar problemas de `global pointer`

## Problemas Comuns

### 1. Multiplo `main`

Se o build do `launcher` principal tentar compilar `external_main.c`, o script `compila_grava.bat` esta errado. O launcher principal nao deve puxar os wrappers externos.

### 2. Dependencias de audio ausentes

Se um jogo tinha dependencias de audio e a plataforma atual nao usa audio, o modulo deve:

- remover os fontes de audio do `build_external.bat`
- ou manter stubs sem audio no `main.c`

Foi exatamente o caso do `riverraid_module`.

### 3. Jogo externo nao entra

Verifique:

- se o `.gmod` foi gerado do modulo correto
- se o script usou `linker_external_game.x`
- se o `.gmod` novo foi realmente copiado para o SD

## Resumo

O fluxo certo e:

1. pegar o `main.c` de producao em `launcher/app/*_module/`
2. expor a entry do modulo por um `external_main.c`
3. compilar com `build_external.bat`
4. gerar o `.gmod`
5. copiar para `games/` no SD

Esse e o fluxo oficial para sair do codigo de desenvolvimento e chegar no binario final usado pelo launcher na placa.
