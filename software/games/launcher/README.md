# Launcher

Launcher principal da plataforma.

## Arquitetura Atual

No fluxo final validado:

1. o `flashboot` sobe da on-chip
2. o `launcher` e carregado da NOR para SDRAM
3. o `launcher` monta FAT32 no SD
4. o menu enumera os jogos em `games/*.gmod`
5. o `launcher` carrega a imagem externa para SDRAM e salta para o jogo

## Formato De Jogo No SD

Os jogos no SD sao armazenados como arquivos binarios `.gmod`.

Campos principais:

- `version = 2`
- `segment_count`
- `entry_addr`
- `stack_addr`
- `gp_addr`
- `header_size`
- `title[32]`

Depois do cabecalho vem a tabela de segmentos e o payload binario.

## Geracao Dos `.gmod`

O fluxo correto nao usa mais diretamente o `main.elf` standalone antigo de desenvolvimento.

O `.gmod` final deve ser gerado a partir dos modulos de producao em:

- `software/games/launcher/app/*_module/`

Cada jogo externo usa:

- `main.c`
- `external_main.c`
- `build_external.bat`

Os `.gmod` gerados ficam em:

- `software/games/launcher/app/gmods/`

Documentacao detalhada:

- [EXTERNAL_BUILD.md](./EXTERNAL_BUILD.md)

## Arquivos Principais

- [main.c](./app/main.c)
- [launcher_image.h](./app/launcher_image.h)
- [launcher_storage_stub.c](./app/launcher_storage_stub.c)

## Relacao Com O Boot

O fluxo de boot automatico completo foi consolidado em:

- [BOOT_FLOW.md](../flashboot/BOOT_FLOW.md)
