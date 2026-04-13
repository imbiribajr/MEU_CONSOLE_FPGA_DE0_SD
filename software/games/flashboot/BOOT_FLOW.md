# Boot Flow Do Launcher

Este documento descreve a cadeia real de boot da plataforma, do power-on ate o launcher aparecer no display.

## Objetivo

Ao ligar a placa:

1. a FPGA configura o hardware
2. o Nios II entra no `flashboot`
3. o `flashboot` carrega o `launcher` da flash paralela para a SDRAM
4. o `launcher` sobe
5. o `launcher` lista e executa os jogos do SD

## Blocos Do Sistema

- `launcher`
  Menu principal. Roda em SDRAM e carrega jogos binarios `.gmod` do SD.

- `flashwrite`
  Aplicativo utilitario que grava a imagem empacotada do `launcher` no inicio da flash paralela.

- `flashboot`
  Bootloader minimo que roda na `onchip_memory2_0`, le a imagem do `launcher` da flash paralela, copia para SDRAM e transfere a execucao.

## Estado Validado

### Flash paralela

A flash paralela conectada ao `generic_tristate_controller_0` em `0x01400000` foi validada:

- leitura simples OK
- `CFI QRY` OK
- erase OK
- program OK
- readback OK

Conclusao: a interface com a NOR esta funcional.

### Launcher

O `launcher` ja esta funcional:

- inicializa a plataforma
- acessa o SD
- monta FAT32
- enumera jogos
- mostra o menu no display
- executa jogos binarios do SD e ainda aceita descritores texto legados como fallback

### Flashboot

O `flashboot` ja foi validado manualmente com `nios2-download -g`:

- ele le a imagem da flash
- copia o payload para SDRAM
- ajusta `gp` e `sp`
- salta para o `entry`
- o `launcher` sobe corretamente

## Ponto Critico Descoberto

O boot automatico falhava por dois motivos distintos.

### 1. Diferenca entre reset real e entry point do ELF

No boot real, o Nios II comeca no reset vector configurado no hardware:

- `0x0180C000`

No `nios2-download -g`, a execucao comeca no `_start` do ELF:

- `0x0180C040`

Sem um codigo explicito em `0x0180C000`, o power-on nao chegava ao `flashboot`, mesmo com o ELF correto.

### 2. Flash paralela ainda nao pronta no power-on

Depois que o stub de reset foi corrigido, ainda havia falha no power-on porque a NOR podia nao estar pronta na primeira leitura logo apos a configuracao da FPGA.

Isso foi resolvido no `flashboot` com:

- atraso inicial curto
- comando `0xF0` para forcar `read array`
- retries de leitura/validacao do header

Conclusao: o boot automatico correto depende tanto do stub de reset quanto da robustez na primeira leitura da NOR.

## Fluxo Real De Boot

Sequencia completa:

1. a FPGA carrega o bitstream
2. a `onchip_memory2_0` sobe inicializada com `onchip_mem.hex`
3. o Nios II sai do reset em `0x0180C000`
4. o stub em `.reset` salta para `_start`
5. o `flashboot` inicializa o display e mostra `LOADING...`
6. o `flashboot` espera a NOR estabilizar
7. o `flashboot` envia `0xF0` para garantir `read array`
8. o `flashboot` le e valida o header `LNCH` na flash
9. o `flashboot` copia o payload do `launcher` para SDRAM
10. o `flashboot` ajusta `gp` e `sp`
11. o `flashboot` salta para o `entry` do `launcher`
12. o `launcher` sobe e passa a operar normalmente

## Enderecos Importantes

- flash paralela:
  `GENERIC_TRISTATE_CONTROLLER_0_BASE = 0x01400000`

- on-chip do boot:
  `ONCHIP_MEMORY2_0_BASE = 0x0180C000`

- reset real do Nios II:
  `NIOS2_RESET_ADDR = 0x0180C000`

- exception vector:
  `NIOS2_EXCEPTION_ADDR = 0x0180C020`

- `_start` atual do `flashboot`:
  `0x0180C040`

## Arquitetura Da Imagem Na Flash

O `flashboot` espera uma imagem `LNCH` no inicio da flash paralela.

Header:

```c
typedef struct flashboot_image_header {
    uint32_t magic;
    uint32_t version;
    uint32_t load_addr;
    uint32_t entry_addr;
    uint32_t gp_addr;
    uint32_t sp_addr;
    uint32_t payload_size;
    uint32_t payload_checksum;
} flashboot_image_header_t;
```

Campos principais:

- `magic = 'LNCH'`
- `version = 1`
- `load_addr = endereco de carga do launcher na SDRAM`
- `entry_addr = entry do launcher`
- `gp_addr = gp do launcher`
- `sp_addr = sp do launcher`
- `payload_size = tamanho do binario`
- `payload_checksum = soma simples dos bytes`

Depois do header vem o payload bruto do `launcher`.

## Cadeia Necessaria Para O Launcher Correto

Para o boot automatico funcionar de forma consistente, toda a cadeia abaixo precisa estar alinhada.

### 1. Launcher compilado

O `launcher` precisa ser compilado normalmente.

Artefato principal:

- `software/games/launcher/app/main.elf`

### 2. Imagem LNCH gerada

O script:

- [gerar_launcher_flash_image.ps1](../../scripts/gerar_launcher_flash_image.ps1)

gera:

- [launcher_flash.img](../launcher/app/launcher_flash.img)
- [launcher_flash_payload.h](../flashwrite/app/launcher_flash_payload.h)

Essa imagem precisa corresponder ao `launcher` que voce quer subir no boot.

### 3. Launcher gravado na flash paralela

O `flashwrite` usa:

- [main.c](../flashwrite/app/main.c)
- [launcher_flash_payload.h](../flashwrite/app/launcher_flash_payload.h)

para:

1. apagar os setores iniciais
2. programar a imagem `LNCH`
3. verificar byte a byte

O resultado esperado e:

- `flashwrite: OK`

### 4. Flashboot compilado com stub de reset

O `flashboot` precisa conter o stub explicito em `.reset`:

```c
void flashboot_reset_stub(void) __attribute__((section(".reset"), naked));
void flashboot_reset_stub(void)
{
    __asm__ __volatile__(
        "movia r2, _start\n"
        "jmp r2\n"
    );
}
```

Esse stub garante que o boot real iniciado em `0x0180C000` chegue ao `_start`.

### 5. Linker do flashboot apontando para a on-chip

O linker do `flashboot` precisa manter:

- memoria `onchip` com origem em `0x0180C000`
- secao `.reset` mantida no inicio dessa memoria

Arquivo:

- [linker_flashboot.x](./app/linker_flashboot.x)

### 6. Robustez de power-on na leitura da NOR

O `flashboot` validado agora contem:

- atraso inicial de power-up
- `flash_reset_read_array()`
- retries de leitura/validacao do header

Isso e necessario porque o boot real ocorre imediatamente apos a configuracao, ao contrario do `nios2-download`.

### 7. `onchip_mem.hex` gerado a partir do flashboot correto

Depois de compilar o `flashboot`, e preciso gerar:

- [hardware/onchip_mem.hex](../../../hardware/onchip_mem.hex)

com:

- [gerar_onchip_hex.bat](./app/gerar_onchip_hex.bat)

Esse passo tambem atualiza:

- [NiosII_ps2_onchip_memory2_0.hex](../../../hardware/NiosII_ps2/synthesis/submodules/NiosII_ps2_onchip_memory2_0.hex)

### 8. Hardware recompilado usando esse HEX

O Qsys precisa continuar apontando para:

- `resetSlave = onchip_memory2_0.s1`
- `exceptionSlave = onchip_memory2_0.s1`
- `initializationFileName = onchip_mem.hex`

Depois disso, o hardware Quartus precisa ser recompilado para embutir o novo conteudo da on-chip no bitstream.

### 9. Bitstream final gravado na FPGA / serial flash

O `.sof` ou `.pof` final precisa conter:

- o hardware atual
- o `onchip_mem.hex` gerado a partir do `flashboot` correto

Sem esse passo, a placa pode continuar iniciando com um `flashboot` antigo mesmo que a NOR ja tenha sido atualizada.

## Sequencia Operacional Correta

Use esta ordem quando quiser atualizar o boot completo da plataforma.

### 1. Recompilar o launcher

No PowerShell:

```powershell
cd F:\Jogos_FPGA\MEU_CONSOLE_FPGA_DE0_SD\software\games\launcher\app
.\compila_grava.bat
```

### 2. Regenerar a imagem do launcher para a NOR

```powershell
powershell -ExecutionPolicy Bypass -File "F:\Jogos_FPGA\MEU_CONSOLE_FPGA_DE0_SD\software\scripts\gerar_launcher_flash_image.ps1"
```

Isso atualiza:

- `software/games/launcher/app/launcher_flash.img`
- `software/games/flashwrite/app/launcher_flash_payload.h`

### 3. Recompilar o flashwrite

```powershell
cd F:\Jogos_FPGA\MEU_CONSOLE_FPGA_DE0_SD\software\games\flashwrite\app
.\compila_grava.bat
```

### 4. Gravar a NOR com o launcher

Execute o `flashwrite` e espere terminar com:

- `flashwrite: OK`

No fluxo atual validado, o `launcher` e gravado a partir de:

- `0x010000`

### 5. Recompilar o flashboot

```powershell
cd F:\Jogos_FPGA\MEU_CONSOLE_FPGA_DE0_SD\software\games\flashboot\app
.\compila_grava.bat
```

### 6. Regenerar o onchip_mem.hex

Ainda dentro de `flashboot/app`:

```powershell
.\gerar_onchip_hex.bat
```

### 7. Recompilar o hardware no Quartus

Depois do `onchip_mem.hex` novo, recompile o projeto Quartus para embutir o `flashboot` atualizado na on-chip.

### 8. Carregar o .sof novo na FPGA

Para teste rapido, basta gravar o `.sof` novo por JTAG. Nao e necessario gravar o `.pof` ainda.

### 9. Preparar o SD

No SD devem estar:

- os jogos `.gmod` binarios reais em `games/`

O `launcher` nao precisa ir para o SD nesse fluxo, porque ele ja esta na NOR.

Observacao importante sobre os jogos:

- o `.gmod` binario deve ser gerado a partir de um ELF compativel com o ambiente do `launcher`
- nao use diretamente o ELF standalone de desenvolvimento sem validar compatibilidade
- a base mais segura para gerar esses ELFs e o codigo ja adaptado dentro de:
  - `software/games/launcher/app/pacman_module/`
  - `software/games/launcher/app/tetris_module/`
  - e equivalentes para outros jogos
- em outras palavras, o `main.c` usado como entrada de compilacao para o ELF externo deve vir das pastas `*_module` do `launcher`, ou de uma versao externa derivada delas, e nao do fluxo antigo de desenvolvimento isolado

### 10. Testar o power-on

Fluxo esperado:

1. a FPGA sobe com o `flashboot` novo
2. o `flashboot` le o `launcher` em `0x010000` da NOR
3. o `launcher` sobe
4. o `launcher` carrega os jogos do SD

### 11. So depois gerar/gravar o .pof

Se o teste com `.sof` por JTAG funcionar, ai sim faz sentido fechar o processo com o `.pof` definitivo na serial flash.

Nao basta gerar o novo `onchip_mem.hex`.

E preciso garantir que o `.sof` ou `.pof` realmente gravado na placa foi gerado depois dessa atualizacao.

Se o arquivo de configuracao gravado for antigo, o Nios continuara usando uma on-chip antiga no power-on.

## Sequencia Pratica Recomendada

Fluxo completo para fechar o launcher correto:

1. compilar o `launcher`
2. rodar `gerar_launcher_flash_image.ps1`
3. gravar a imagem do `launcher` na NOR com `flashwrite`
4. compilar o `flashboot`
5. rodar `gerar_onchip_hex.bat`
6. recompilar o hardware Quartus
7. gerar o `.sof` ou `.pof`
8. gravar o arquivo final de configuracao na FPGA ou serial flash
9. energizar a placa e observar `LOADING...`
10. confirmar a subida automatica do `launcher`

## Diagnostico Esperado

### Se tudo estiver correto

No power-on:

- `LOADING...` aparece imediatamente
- o `launcher` sobe sem `nios2-download`

### Se `LOADING...` nao aparecer

O problema tende a estar antes do acesso a flash:

- bitstream antigo
- `onchip_mem.hex` antigo embutido no hardware
- reset vector nao atingindo o `flashboot`

### Se `LOADING...` aparecer mas o launcher nao subir

O problema tende a estar depois da entrada no `flashboot`:

- imagem `LNCH` nao gravada corretamente
- header invalido
- payload inconsistente
- falha de leitura da NOR

## Atualizacao De Jogos Depois Disso

Modelo desejado:

- flash paralela guarda o `launcher`
- SD guarda os jogos

Entao:

- mudar jogo no SD nao exige reflashing da NOR
- mudar o `launcher` exige regenerar a imagem e regravar a NOR

## Arquivos Principais

### Flashboot

- [main.c](./app/main.c)
- [flashboot_image.h](./app/flashboot_image.h)
- [linker_flashboot.x](./app/linker_flashboot.x)
- [gerar_onchip_hex.bat](./app/gerar_onchip_hex.bat)

### Gravacao Da NOR

- [main.c](../flashwrite/app/main.c)
- [launcher_flash_payload.h](../flashwrite/app/launcher_flash_payload.h)
- [gerar_launcher_flash_image.ps1](../../scripts/gerar_launcher_flash_image.ps1)

### Launcher

- [main.c](../launcher/app/main.c)
