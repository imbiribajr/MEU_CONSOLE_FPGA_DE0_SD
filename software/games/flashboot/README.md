# Flashboot

Bootloader minimo que roda na on-chip e carrega o `launcher` da NOR para SDRAM.

## Estado Atual

Fluxo validado:

1. o Nios II sai do reset real em `0x0180C000`
2. o stub em `.reset` salta para `_start`
3. o `flashboot` roda na `onchip_memory2_0`
4. mostra `LOADING...`
5. espera a NOR estabilizar no power-on
6. forca `read array` com `0xF0`
7. le e valida o header `LNCH`
8. copia o payload do `launcher` para SDRAM
9. ajusta `gp/sp`
10. salta para `entry`

## Endereco Da Imagem Na NOR

O `flashboot` atual espera o `launcher` em:

- `FLASHBOOT_IMAGE_OFFSET = 0x010000`

## Arquivos Principais

- [main.c](F:/Jogos_FPGA/MEU_CONSOLE_FPGA_AUDIO_DE0/software/games/flashboot/app/main.c)
- [linker_flashboot.x](F:/Jogos_FPGA/MEU_CONSOLE_FPGA_AUDIO_DE0/software/games/flashboot/app/linker_flashboot.x)
- [flashboot_image.h](F:/Jogos_FPGA/MEU_CONSOLE_FPGA_AUDIO_DE0/software/games/flashboot/app/flashboot_image.h)

## Scripts Relacionados

- gerar imagem do launcher:
  [gerar_launcher_flash_image.ps1](F:/Jogos_FPGA/MEU_CONSOLE_FPGA_AUDIO_DE0/software/scripts/gerar_launcher_flash_image.ps1)
- gerar `onchip_mem.hex`:
  [gerar_onchip_hex.bat](F:/Jogos_FPGA/MEU_CONSOLE_FPGA_AUDIO_DE0/software/games/flashboot/app/gerar_onchip_hex.bat)

## Documentacao Completa

- [BOOT_FLOW.md](F:/Jogos_FPGA/MEU_CONSOLE_FPGA_AUDIO_DE0/software/games/flashboot/BOOT_FLOW.md)
