# MEU_CONSOLE_FPGA_DE0_SD

Console de games em FPGA baseado em **Nios II**, com **launcher residente em flash NOR**, jogos carregados a partir de **cartão SD**, entrada por **teclado PS/2** e suporte a controle via **UART/Bluetooth**.

English version: [README.en.md](./README.en.md)

O projeto foi desenvolvido para a familia **Intel/Altera Cyclone III** usando **Quartus II 13.0 SP1** e **Nios II EDS 13.0 SP1**. O hardware integra CPU Nios II, SDRAM, interface PS/2, UART, SPI para SD card, controlador de flash paralela e saída para matriz de LED.

## Visao Geral

Ao energizar a placa:

1. a FPGA carrega o hardware
2. o Nios II inicia um bootloader mínimo em memória on-chip
3. esse bootloader lê o `launcher` gravado na flash NOR
4. o `launcher` sobe em SDRAM
5. o `launcher` monta FAT32 no cartão SD
6. os jogos `.gmod` são listados no menu
7. o jogo selecionado é carregado para SDRAM e executado

## Principais Recursos

- Boot autonomo do sistema sem depender de `nios2-download`
- `launcher` persistido em flash paralela
- Jogos armazenados no SD em formato `.gmod`
- Navegacão por teclado **PS/2**
- Controle alternativo via **UART** para módulo Bluetooth/controller serial
- Renderizaçãoo em **matriz LED HUB75**
- Uso de **SDRAM** para execução do launcher e dos jogos
- Infraestrutura para atualizar launcher, boot e jogos separadamente

## Fotos

Pac-Man rodando na matriz LED com a placa em primeiro plano:

![Pac-Man na matriz LED](./fotos/IMG_20260413_175017.jpg)

Launcher em execução com seleção de jogos no SD e controle Bluetooth via UART:

![Launcher com controle Bluetooth](./fotos/IMG_20260413_180009.jpg)

## Hardware

O hardware principal esta em [`hardware/`](./hardware) e usa como topo o arquivo [`hardware/Hardware.vhd`](./hardware/Hardware.vhd).

Blocos relevantes presentes no projeto:

- **Nios II Qsys** gerado em `hardware/NiosII_ps2/`
- **PS/2** via `ps2_avalon_interface.vhd`
- **UART** para recepção de comandos seriais/Bluetooth
- **SPI SD card** para leitura dos jogos no SD
- **Flash NOR paralela** para armazenamento do launcher
- **SDRAM** como memória principal de execução
- **Matriz LED** controlada por `led_matrix_avalon.vhd`

Projeto Quartus:

- arquivo de projeto: [`hardware/DriverVHDL.qpf`](./hardware/DriverVHDL.qpf)
- constraints e pinagem: [`hardware/DriverVHDL.qsf`](./hardware/DriverVHDL.qsf)

## Arquitetura De Software

O software está organizado em três camadas principais:

- **flashboot**
  Bootloader mínimo executado a partir da memória on-chip. Carrega o launcher da NOR para SDRAM.
- **flashwrite**
  Aplicativo utilitário que grava ou atualiza a imagem do launcher na flash paralela.
- **launcher**
  Menu principal que monta FAT32 no SD, lista os jogos e carrega arquivos `.gmod`.

Pastas principais:

- [`software/bsp/`](./software/bsp): BSP gerado pelo Nios II EDS
- [`software/games/flashboot/`](./software/games/flashboot): bootloader
- [`software/games/flashwrite/`](./software/games/flashwrite): gravaçãoo da imagem do launcher na NOR
- [`software/games/launcher/`](./software/games/launcher): launcher e módulos de jogo
- [`software/scripts/`](./software/scripts): scripts auxiliares de build e programação
- [`software/workspace/`](./software/workspace): área de app Nios usada no fluxo de desenvolvimento

## Fluxo De Boot

Fluxo validado no projeto:

1. o Nios II sai do reset na memória on-chip
2. o `flashboot` é executado
3. a flash NOR é estabilizada e colocada em `read array`
4. o header `LNCH` do launcher é validado
5. o payload do launcher é copiado para SDRAM
6. `gp`, `sp` e `entry` são ajustados
7. a execução é transferida ao launcher
8. o launcher monta o SD FAT32 e lista os jogos

Documentação de apoio:

- [`software/games/flashboot/BOOT_FLOW.md`](./software/games/flashboot/BOOT_FLOW.md)
- [`software/games/flashboot/README.md`](./software/games/flashboot/README.md)
- [`software/games/flashwrite/README.md`](./software/games/flashwrite/README.md)

## Formato Dos Jogos

Os jogos atuais no SD usam arquivos binários `.gmod`.

Características do formato atual:

- múltiplos segmentos carregáveis
- `entry_addr`, `stack_addr` e `gp_addr` no cabeçalho
- carga direta em SDRAM pelo launcher

Definições:

- [`software/games/launcher/app/launcher_image.h`](./software/games/launcher/app/launcher_image.h)
- gerador: [`software/scripts/gerar_gmod.ps1`](./software/scripts/gerar_gmod.ps1)

Jogos atualmente presentes no repositório:

- Arkanoid
- Boulder Dash (`gdash`)
- Pac-Man
- Pong
- River Raid
- Snake
- Space Invaders
- Tetris

Os binários prontos de exemplo estao em:

- [`software/games/launcher/app/gmods/`](./software/games/launcher/app/gmods)

## Controles

O launcher aceita entrada por:

- **PS/2**
- **UART**
- botões locais mapeados no PIO

No launcher, o mapeamento principal e:

- `Up/Down`: navegar no menu
- `L`, `Enter`, `Space` ou equivalente serial: selecionar/carregar

Nos jogos, o codigo mostra leitura combinada de:

- scan codes de **PS/2**
- bytes recebidos pela **UART**
- sw0 na DE0 reset o game
- button2 na DE0 recarrega o launcher

Isso permite usar teclado PS/2 ou um controle Bluetooth que entrega comandos seriais à UART do sistema.

## Como Compilar

### Requisitos

- Quartus II **13.0 SP1**
- Nios II EDS **13.0 SP1**
- Windows com acesso ao `Nios_Shell.bat`
- Cabo **USB-Blaster**

Caminhos usados nos scripts atuais:

- `F:\altera\13.0sp1\quartus`
- `F:\altera\13.0sp1\nios2eds`

Se sua instalacao estiver em outro local, ajuste os `.bat` e `.ps1` em [`software/scripts/`](./software/scripts) e nas configurações do VS Code em [`.vscode/settings.json`](./.vscode/settings.json).

### Hardware

Gerar Qsys:

```powershell
cd software\scripts
.\Generate_Qsys.bat
```

Sintetizar no Quartus:

```powershell
cd hardware
quartus_sh --flow compile DriverVHDL
```

Gravar FPGA:

```powershell
cd hardware\output_files
quartus_pgm -m jtag -c USB-Blaster -o "p;DriverVHDL.sof"
```

### BSP

Regenerar arquivos do BSP:

```powershell
cd software\bsp
nios2-bsp-generate-files --settings=settings.bsp --bsp-dir=.
```

Compilar BSP:

```powershell
cd software\scripts
.\compila_BSP.bat
```

### Launcher

Compilar e baixar o launcher para desenvolvimento:

```powershell
cd software\games\launcher\app
.\compila_grava.bat
```

### Gerar A Imagem Do Launcher Para Flash

```powershell
powershell -ExecutionPolicy Bypass -File software\scripts\gerar_launcher_flash_image.ps1
```

Esse script gera:

- `software/games/launcher/app/launcher_flash.img`
- `software/games/flashwrite/app/launcher_flash_payload.h`

### Gravar O Launcher Na NOR

Depois de gerar a imagem:

1. compile o `flashwrite`
2. execute o `flashwrite` na placa
3. ele apaga, grava e verifica a imagem na flash

Documentação:

- [`software/games/flashwrite/README.md`](./software/games/flashwrite/README.md)

### Atualizar O Bootloader On-Chip

Sempre que o `flashboot` mudar:

1. compile `software/games/flashboot/app`
2. gere `hardware/onchip_mem.hex`
3. recompile o hardware Quartus para embutir o novo HEX no bitstream

Fluxo detalhado:

- [`software/games/flashboot/BOOT_FLOW.md`](./software/games/flashboot/BOOT_FLOW.md)

## Como Gerar Um Novo Jogo `.gmod`

Cada jogo externo possui um módulo próprio em:

- `software/games/launcher/app/<nome>_module/`

Normalmente cada módulo contém:

- `main.c`
- `external_main.c`
- `build_external.bat`

Exemplo de build de um jogo:

```powershell
cd software\games\launcher\app\pacman_module
.\build_external.bat
```

O `.gmod` resultante e salvo em:

- [`software/games/launcher/app/gmods/`](./software/games/launcher/app/gmods)

Documentação:

- [`software/games/launcher/EXTERNAL_BUILD.md`](./software/games/launcher/EXTERNAL_BUILD.md)

## Estrutura Do Repositório

```text
MEU_CONSOLE_FPGA_DE0_SD/
|- hardware/                      -> projeto Quartus e componentes HDL
|- software/
|  |- bsp/                        -> BSP do Nios II
|  |- games/
|  |  |- flashboot/               -> bootloader em on-chip
|  |  |- flashwrite/              -> gravacao da imagem do launcher na NOR
|  |  |- launcher/                -> launcher e jogos
|  |- scripts/                    -> automacao de build, flash e geracao de imagens
|  |- workspace/                  -> app/workspace de desenvolvimento
|- .vscode/                       -> tasks e botoes de acao do VS Code
```

## VS Code

O repositório inclui configuração para trabalhar com VS Code:

- botões de açao em [`.vscode/settings.json`](./.vscode/settings.json)
- tasks em [`.vscode/tasks.json`](./.vscode/tasks.json)
- debug em [`.vscode/launch.json`](./.vscode/launch.json)

O fluxo foi preparado para chamar o ambiente da Intel/Altera via `Nios_Shell.bat`.

## Estado Atual Do Projeto

Pelo estado atual do repositório:

- o hardware Quartus esta presente e compilável
- o `flashboot` esta integrado ao fluxo de boot
- o `launcher` carrega imagens `.gmod` do SD
- a leitura de FAT32 está implementada no próprio launcher
- os jogos já possuem módulos e artefatos `.gmod`
- o controle por PS/2 e UART está presente no launcher e nos jogos

## Observações

- O projeto inclui arquivos gerados do Qsys/synthesis que fazem parte do fluxo atual.
- Artefatos pesados de compilação do Quartus e objetos temporários do Nios estão filtrados pelo [`.gitignore`](./.gitignore).
- Alguns documentos internos ainda refletem fases anteriores do desenvolvimento; este README descreve o fluxo validado a partir do estado atual do código.

## Licença

Este projeto usa a licença [MIT](./LICENSE).
