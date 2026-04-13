# Flashwrite

Aplicativo utilitario para gravar a imagem `LNCH` do `launcher` na NOR.

## Fluxo Atual

1. gerar a imagem do launcher:
   - `software/scripts/gerar_launcher_flash_image.ps1`
2. isso atualiza:
   - `software/games/flashwrite/app/launcher_flash_payload.h`
3. compilar o `flashwrite`:
   - `software/games/flashwrite/app/compila_grava.bat`
4. executar o `flashwrite`
5. o app:
   - apaga
   - programa
   - verifica

## Mapeamento Validado

No fluxo final estabilizado:

- a imagem do launcher e gravada a partir de `0x010000`
- a geometria efetiva tratada pelo software usa setores de `0x8000`
- o espaco efetivo usado pelo software ficou em `2 MB`

## Arquivos Principais

- [main.c](F:/Jogos_FPGA/MEU_CONSOLE_FPGA_AUDIO_DE0/software/games/flashwrite/app/main.c)
- [launcher_flash_payload.h](F:/Jogos_FPGA/MEU_CONSOLE_FPGA_AUDIO_DE0/software/games/flashwrite/app/launcher_flash_payload.h)

## Relacao Com O Boot

Depois de gravar a NOR com sucesso, o `flashboot` pode carregar o `launcher` automaticamente no power-on.

Fluxo completo:

- [BOOT_FLOW.md](F:/Jogos_FPGA/MEU_CONSOLE_FPGA_AUDIO_DE0/software/games/flashboot/BOOT_FLOW.md)
