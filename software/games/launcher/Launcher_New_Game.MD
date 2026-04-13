# Como Inserir Um Novo Jogo No Launcher

Este documento descreve o fluxo atual do projeto para adicionar um novo jogo ao launcher.

Importante:

- hoje o `.gmod` no SD nao carrega um binario externo real
- hoje o `.gmod` funciona como descritor de um modulo embutido no `launcher`
- portanto, inserir um jogo novo ainda exige recompilar o `launcher` e regravar a NOR

## Fluxo Em 10 Passos

### 1. Criar a pasta do modulo

Crie uma pasta nova dentro de:

- `software/games/launcher/app/`

Exemplo:

- `software/games/launcher/app/meujogo_module/`

## 2. Colocar o codigo do jogo na pasta

Coloque ali os arquivos do jogo.

Estrutura minima recomendada:

- `main.c`
- `meujogo_module.h`

Se o jogo vier de outro projeto, esse e o ponto onde ele entra na arvore do launcher.

## 3. Adaptar o jogo para a plataforma

O `main.c` original normalmente nao entra direto sem ajuste.

Voce precisa adaptar o jogo para:

- display da plataforma
- leitura de teclado/PS2
- temporizacao
- defines do `system.h`
- mapeamento de memoria e perifericos da plataforma

Na pratica, os pontos mais comuns de adaptacao sao:

- render
- input
- loop principal
- delays/ticks
- includes de hardware

## 4. Expor o modulo no formato do launcher

O modulo precisa exportar um `launcher_builtin_module_t`.

Exemplo:

```c
#include "meujogo_module.h"

static void meujogo_entry(void)
{
    /* loop principal do jogo */
}

const launcher_builtin_module_t meujogo_module = {
    "builtin:meujogo",
    "MEU JOGO",
    meujogo_entry
};
```

O campo `path` precisa casar com o `builtin:` que sera usado no `.gmod`.

## 5. Criar o header publico do modulo

Exemplo de `meujogo_module.h`:

```c
#ifndef MEUJOGO_MODULE_H
#define MEUJOGO_MODULE_H

#include "../launcher_module.h"

extern const launcher_builtin_module_t meujogo_module;

#endif
```

Esse header e o contrato que o launcher usa para enxergar o modulo.

## 6. Registrar o modulo no launcher

No arquivo:

- [main.c](F:/Jogos_FPGA/MEU_CONSOLE_FPGA_AUDIO_DE0/software/games/launcher/app/main.c)

voce precisa:

- incluir o header do novo modulo
- adicionar `&meujogo_module` no array `g_builtin_modules`

Sem isso, o launcher nao vai conseguir localizar o modulo referenciado pelo `.gmod`.

## 7. Incluir o modulo no build do launcher

No arquivo:

- [compila_grava.bat](F:/Jogos_FPGA/MEU_CONSOLE_FPGA_AUDIO_DE0/software/games/launcher/app/compila_grava.bat)

adicione a pasta do novo modulo aos argumentos:

- `--src-rdir=./meujogo_module`
- `--inc-dir=./meujogo_module`

Sem isso, os arquivos do modulo nao entram na compilacao.

## 8. Criar o `.gmod` correspondente

O `.gmod` atual e um arquivo de texto simples.

Conteudo exemplo:

```text
builtin:meujogo
MEU JOGO
```

Voce pode criar manualmente ou usar o script:

- [gerar_gmod.ps1](F:/Jogos_FPGA/MEU_CONSOLE_FPGA_AUDIO_DE0/software/scripts/gerar_gmod.ps1)

Exemplo:

```powershell
powershell -ExecutionPolicy Bypass -File software\scripts\gerar_gmod.ps1 `
  -BuiltinPath "builtin:meujogo" `
  -Title "MEU JOGO" `
  -OutputPath "software\games\launcher\app\gmods\meujogo.gmod"
```

## 9. Copiar o `.gmod` para o SD

Copie o arquivo `.gmod` para a pasta de jogos no cartao SD.

Normalmente:

- `games/`

No estado atual do projeto, esse arquivo apenas aponta para um modulo embutido no launcher.

## 10. Recompilar e regravar o launcher

Como o jogo esta embutido no `launcher`, o passo final e:

1. recompilar o `launcher`
2. regenerar a imagem do launcher
3. regravar a NOR

Em outras palavras:

- mudar jogo embutido = mudar launcher
- mudar launcher = regravar a NOR

## Resumo Curto

Hoje o fluxo real e:

1. integrar o jogo como modulo embutido
2. criar um `.gmod` textual que aponta para esse modulo
3. recompilar o launcher
4. regravar a NOR

## Modelo Atual Versus Modelo Futuro

### Modelo atual

- jogo compilado dentro do launcher
- `.gmod` no SD e apenas um descritor textual

### Modelo futuro desejado

- jogo empacotado como modulo externo real
- launcher carregando o binario diretamente do SD
- sem recompilar o launcher para cada jogo novo

Esse modelo futuro ainda nao e o fluxo usado pelos jogos atuais do projeto.
