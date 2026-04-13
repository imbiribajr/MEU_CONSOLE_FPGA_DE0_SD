#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "system.h"
#include "io.h"
#include "launcher_flash_payload.h"

#ifndef GENERIC_TRISTATE_CONTROLLER_0_BASE
#error "Regenerate the BSP so system.h defines GENERIC_TRISTATE_CONTROLLER_0_BASE."
#endif

#define FLASH_BASE         GENERIC_TRISTATE_CONTROLLER_0_BASE
#define FLASH_IMAGE_OFFSET 0x010000u
#define FLASH_TOTAL_SIZE   (2u * 1024u * 1024u)

static unsigned flash_read8(unsigned addr) {
    return IORD_8DIRECT(FLASH_BASE, addr) & 0xFFu;
}

static void flash_write8(unsigned addr, unsigned value) {
    IOWR_8DIRECT(FLASH_BASE, addr, value & 0xFFu);
}

static void flash_reset_read_array(void) {
    flash_write8(0x000, 0xF0);
    usleep(1000);
}

static void flash_erase_sector(unsigned sector_addr) {
    flash_reset_read_array();
    flash_write8(0x555, 0xAA);
    flash_write8(0x2AA, 0x55);
    flash_write8(0x555, 0x80);
    flash_write8(0x555, 0xAA);
    flash_write8(0x2AA, 0x55);
    flash_write8(sector_addr, 0x30);
}

/* * OTIMIZAÇÃO PRINCIPAL: Rotina de gravação sem usleep, 
 * baseada em interrogação de hardware (Tight Loop)
 */
static int flash_program_fast(unsigned addr, unsigned value) {
    // Escreve os comandos CFI diretamente para máxima velocidade
    IOWR_8DIRECT(FLASH_BASE, 0x555, 0xAA);
    IOWR_8DIRECT(FLASH_BASE, 0x2AA, 0x55);
    IOWR_8DIRECT(FLASH_BASE, 0x555, 0xA0);
    IOWR_8DIRECT(FLASH_BASE, addr, value & 0xFFu);

    // Na arquitetura de Data# Polling, o DQ7 (Bit 7) inverte durante a gravação
    unsigned expected_bit7 = value & 0x80u;

    // Laço "Tight-Loop" infinito (quebra em microssegundos)
    while (1) {
        unsigned status = IORD_8DIRECT(FLASH_BASE, addr) & 0xFFu;
        
        // Verifica se terminou com sucesso comparando o bit 7
        if ((status & 0x80u) == expected_bit7) {
            return 0; // Sucesso!
        }
        
        // Verifica o Bit 5 (DQ5) - Indicador de erro de hardware ou timeout
        if (status & 0x20u) {
            // Se DQ5=1, a folha de dados (datasheet) manda ler mais uma última vez
            status = IORD_8DIRECT(FLASH_BASE, addr) & 0xFFu;
            if ((status & 0x80u) == expected_bit7) {
                return 0; // Terminou no último instante
            }
            
            // Falha irrecuperável na gravação deste byte
            flash_write8(0x000, 0xF0); // Faz reset à Flash para limpar o erro
            return -1;
        }
    }
}

// Mantido o poll_value original apenas para o apagamento (que é um processo lento por natureza)
static int flash_poll_value(unsigned addr, unsigned expected, unsigned attempts, unsigned delay_us) {
    unsigned i;
    for (i = 0; i < attempts; ++i) {
        if (flash_read8(addr) == (expected & 0xFFu)) {
            return 0;
        }
        usleep(delay_us);
    }
    return -1;
}

static unsigned sector_size_for_addr(unsigned addr) {
    (void)addr;
    return 0x8000u; // O barramento está configurado em 8 bits a aceder à Flash em 16 bits
}

static unsigned next_sector_base(unsigned addr) {
    return (addr + 0x8000u) & ~0x7FFFu;
}

static int erase_range(unsigned start, unsigned size) {
    unsigned addr = start;
    unsigned end = start + size;

    while (addr < end) {
        printf("A apagar setor @ 0x%06X\n", addr);
        flash_erase_sector(addr);
        // Apagar demora algum tempo, logo usar usleep aqui é aceitável
        if (flash_poll_value(addr, 0xFF, 4000, 1000) != 0) {
            printf("Timeout ao apagar @ 0x%06X read=0x%02X\n", addr, flash_read8(addr));
            return -1;
        }
        addr = next_sector_base(addr);
    }
    return 0;
}

static int verify_erased(unsigned start, unsigned size) {
    unsigned addr;
    for (addr = start; addr < start + size; ++addr) {
        if (flash_read8(addr) != 0xFFu) {
            printf("Falha na verificacao de apagamento @ 0x%06X read=0x%02X\n", addr, flash_read8(addr));
            return -1;
        }
    }
    return 0;
}

static int program_image(unsigned dst, const uint8_t *src, unsigned size) {
    unsigned i;
    unsigned next_report = 0x4000u;

    for (i = 0; i < size; ++i) {
        // Ignora bytes 0xFF, pois a Flash já fica em 0xFF após ser apagada (ganho extra de velocidade!)
        if (src[i] != 0xFFu) {
            if (flash_program_fast(dst + i, src[i]) != 0) {
                printf("Erro na programacao @ 0x%06X read=0x%02X exp=0x%02X\n", dst + i, flash_read8(dst + i), src[i]);
                return -1;
            }
        }
        
        // Reportar progresso a cada ~16 KB para não inundar o terminal
        if (i >= next_report) {
            printf("Progresso da gravacao: 0x%06X / 0x%06X\n", i, size);
            next_report += 0x4000u;
        }
    }

    flash_reset_read_array(); // Faz o reset apenas UMA VEZ no final de toda a gravação!
    return 0;
}

static int verify_image(unsigned dst, const uint8_t *src, unsigned size) {
    unsigned i;

    for (i = 0; i < size; ++i) {
        if (flash_read8(dst + i) != src[i]) {
            printf("Falha na verificacao @ 0x%06X read=0x%02X exp=0x%02X\n",
                   dst + i, flash_read8(dst + i), src[i]);
            return -1;
        }
    }
    return 0;
}

static void dump_flash_region(unsigned addr, unsigned count) {
    unsigned i;
    printf("flash dump @ 0x%06X\n", addr);
    for (i = 0; i < count; i += 16) {
        unsigned j;
        printf("%06X :", addr + i);
        for (j = 0; j < 16 && (i + j) < count; ++j) {
            printf(" %02X", flash_read8(addr + i + j));
        }
        printf("\n");
    }
}

int main(void) {
    unsigned erase_size;

    printf("flashwrite: inicio\n");
    printf("flashwrite: tamanho da imagem = %u bytes\n", (unsigned)launcher_flash_payload_size);

    if (launcher_flash_payload_size == 0u ||
        launcher_flash_payload_size > (FLASH_TOTAL_SIZE - FLASH_IMAGE_OFFSET)) {
        printf("flashwrite: tamanho de imagem invalido\n");
        return 1;
    }

    // Calcula o tamanho a apagar alinhado com o tamanho dos setores (0x8000)
    erase_size = launcher_flash_payload_size;
    erase_size = (erase_size + sector_size_for_addr(FLASH_IMAGE_OFFSET) - 1u) & ~(sector_size_for_addr(FLASH_IMAGE_OFFSET) - 1u);

    if (erase_range(FLASH_IMAGE_OFFSET, erase_size) != 0) {
        printf("flashwrite: falha ao apagar\n");
        return 1;
    }

    if (verify_erased(FLASH_IMAGE_OFFSET, erase_size) != 0) {
        printf("flashwrite: falha ao verificar apagamento\n");
        return 1;
    }

    printf("flashwrite: a gravar...\n");
    if (program_image(FLASH_IMAGE_OFFSET, launcher_flash_payload, launcher_flash_payload_size) != 0) {
        printf("flashwrite: falha na gravacao\n");
        return 1;
    }

    printf("flashwrite: a verificar...\n");
    if (verify_image(FLASH_IMAGE_OFFSET, launcher_flash_payload, launcher_flash_payload_size) != 0) {
        printf("flashwrite: falha na verificacao\n");
        return 1;
    }

    dump_flash_region(FLASH_IMAGE_OFFSET, 64u);
    if (launcher_flash_payload_size >= 64u) {
        unsigned tail = FLASH_IMAGE_OFFSET + launcher_flash_payload_size - 64u;
        dump_flash_region(tail, 64u);
    }

    printf("flashwrite: OK. Gravacao concluida com sucesso!\n");
    while (1) {
        usleep(500000);
    }

    return 0;
}