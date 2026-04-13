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

O formato usado no SD e o binario `GIMG`, normalmente salvo com extensao `.gmod`.

Campos principais:

- `magic = GIMG`
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

- [EXTERNAL_BUILD.md](F:/Jogos_FPGA/MEU_CONSOLE_FPGA_AUDIO_DE0/software/games/launcher/EXTERNAL_BUILD.md)

## Arquivos Principais

- [main.c](F:/Jogos_FPGA/MEU_CONSOLE_FPGA_AUDIO_DE0/software/games/launcher/app/main.c)
- [launcher_image.c](F:/Jogos_FPGA/MEU_CONSOLE_FPGA_AUDIO_DE0/software/games/launcher/app/launcher_image.c)
- [launcher_storage.c](F:/Jogos_FPGA/MEU_CONSOLE_FPGA_AUDIO_DE0/software/games/launcher/app/launcher_storage.c)

## Relacao Com O Boot

O fluxo de boot automatico completo foi consolidado em:

- [BOOT_FLOW.md](F:/Jogos_FPGA/MEU_CONSOLE_FPGA_AUDIO_DE0/software/games/flashboot/BOOT_FLOW.md)
