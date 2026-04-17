#ifndef ALT_NO_C_PLUS_PLUS
#define ALT_NO_C_PLUS_PLUS
#endif
#ifndef ALT_NO_EXIT
#define ALT_NO_EXIT
#endif

#include <stdint.h>
#include <stdio.h>

#include "system.h"
#include "io.h"
#include "sys/alt_cache.h"
#include "altera_avalon_jtag_uart_regs.h"
#include "flashboot_image.h"
#include "unifor_splash.h"

#ifndef GENERIC_TRISTATE_CONTROLLER_0_BASE
#error "Regenerate the BSP so system.h defines GENERIC_TRISTATE_CONTROLLER_0_BASE."
#endif

#define FLASHBOOT_FLASH_BASE   GENERIC_TRISTATE_CONTROLLER_0_BASE
#define FLASHBOOT_IMAGE_OFFSET 0x010000u
#define FLASHBOOT_MAX_PAYLOAD  (1536u * 1024u)
#define FLASHBOOT_POWERUP_DELAY_MS 250u
#define FLASHBOOT_HEADER_RETRIES 8u
#define FLASHBOOT_HEADER_RETRY_DELAY_MS 100u
#define VIEW_W 64
#define VIEW_H 128
#define COLOR_BLACK 0
#define COLOR_RED 1
#define COLOR_GREEN 2
#define LED_BRIGHTNESS_OFFSET 8192

typedef void (*flashboot_entry_fn)(void);

void flashboot_reset_stub(void) __attribute__((section(".reset"), naked));
void flashboot_reset_stub(void)
{
    __asm__ __volatile__(
        "movia r2, _start\n"
        "jmp r2\n"
    );
}

static void dbg_putc(char c) {
    uint32_t ctrl = IORD_ALTERA_AVALON_JTAG_UART_CONTROL(JTAG_UART_0_BASE);
    if ((ctrl & 0xFFFF0000u) != 0u) {
        IOWR_ALTERA_AVALON_JTAG_UART_DATA(JTAG_UART_0_BASE, (uint32_t)(uint8_t)c);
    }
}

static void dbg_puts(const char *s) {
    while (*s) dbg_putc(*s++);
}

static void busy_delay_ms(uint32_t ms)
{
    volatile uint32_t outer;
    volatile uint32_t inner;

    for (outer = 0; outer < ms; ++outer) {
        for (inner = 0; inner < 25000u; ++inner) {
            __asm__ __volatile__("nop");
        }
    }
}

static void set_display_brightness(uint8_t level)
{
    IOWR_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_BRIGHTNESS_OFFSET, level);
}

static void panel_put(int xv, int yv, uint8_t rgb)
{
    uint16_t xp;
    uint32_t base;

    if ((unsigned)xv >= VIEW_W || (unsigned)yv >= VIEW_H) return;

    xp = (uint16_t)yv;
    if (xv < 32) {
        base = (uint32_t)((63 - xv) * 128);
    } else {
        base = 4096u + (uint32_t)(((63 - xv) - 32) * 128);
    }

    IOWR_8DIRECT(LED_MATRIX_AVALON_0_BASE, (uint16_t)(base + xp), rgb);
}

static uint8_t glyph_row(char c, int row)
{
    switch (c) {
        case 'A': { static const uint8_t g[7] = {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}; return g[row]; }
        case 'C': { static const uint8_t g[7] = {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}; return g[row]; }
        case 'D': { static const uint8_t g[7] = {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}; return g[row]; }
        case 'G': { static const uint8_t g[7] = {0x0F,0x10,0x10,0x17,0x11,0x11,0x0E}; return g[row]; }
        case 'I': { static const uint8_t g[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x1F}; return g[row]; }
        case 'L': { static const uint8_t g[7] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}; return g[row]; }
        case 'N': { static const uint8_t g[7] = {0x11,0x19,0x15,0x13,0x11,0x11,0x11}; return g[row]; }
        case 'O': { static const uint8_t g[7] = {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}; return g[row]; }
        case 'P': { static const uint8_t g[7] = {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}; return g[row]; }
        case 'F': { static const uint8_t g[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}; return g[row]; }
        case 'R': { static const uint8_t g[7] = {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}; return g[row]; }
        case 'S': { static const uint8_t g[7] = {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}; return g[row]; }
        case 'T': { static const uint8_t g[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}; return g[row]; }
        case 'U': { static const uint8_t g[7] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}; return g[row]; }
        case '.': { static const uint8_t g[7] = {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}; return g[row]; }
        case ' ': return 0;
        default:  return 0x1F;
    }
}

static void draw_char5x7(int x0, int y0, char c, uint8_t color)
{
    int row, col;
    for (row = 0; row < 7; row++) {
        uint8_t bits = glyph_row(c, row);
        for (col = 0; col < 5; col++) {
            if ((bits >> (4 - col)) & 1) {
                panel_put(x0 + col, y0 + row, color);
            } else {
                panel_put(x0 + col, y0 + row, COLOR_BLACK);
            }
        }
        panel_put(x0 + 5, y0 + row, COLOR_BLACK);
    }
}

static void draw_text5x7(int x0, int y0, const char *text, uint8_t color)
{
    while (*text) {
        draw_char5x7(x0, y0, *text, color);
        x0 += 6;
        text++;
    }
}

static void draw_unifor_splash(void)
{
    int x;
    int y;

    for (y = 0; y < UNIFOR_SPLASH_HEIGHT; y++) {
        for (x = 0; x < UNIFOR_SPLASH_WIDTH; x++) {
            panel_put(x, y, unifor_splash[y * UNIFOR_SPLASH_WIDTH + x]);
        }
    }
}

static void show_loading_screen(void)
{
    set_display_brightness(100);
    draw_unifor_splash();
    draw_text5x7((VIEW_W - 6 * 6) / 2, 4, "UNIFOR", COLOR_GREEN);
    draw_text5x7((VIEW_W - 6 * 5) / 2, 12, "C C T", COLOR_GREEN);
    draw_text5x7((VIEW_W - 6 * 10) / 2 + 1, VIEW_H - 14, "LOADING...", COLOR_RED);
}

static unsigned char flash_read8(uint32_t flash_offset) {
    return (unsigned char)(IORD_8DIRECT(FLASHBOOT_FLASH_BASE, flash_offset) & 0xFFu);
}

static void flash_write8(uint32_t flash_offset, uint32_t value) {
    IOWR_8DIRECT(FLASHBOOT_FLASH_BASE, flash_offset, value & 0xFFu);
}

static void flash_reset_read_array(void) {
    flash_write8(0x000u, 0xF0u);
}

static uint32_t flash_read32(uint32_t flash_offset) {
    return (uint32_t)flash_read8(flash_offset + 0u) |
           ((uint32_t)flash_read8(flash_offset + 1u) << 8) |
           ((uint32_t)flash_read8(flash_offset + 2u) << 16) |
           ((uint32_t)flash_read8(flash_offset + 3u) << 24);
}

static void flashboot_read_header(uint32_t flash_offset, flashboot_image_header_t *hdr) {
    hdr->magic = flash_read32(flash_offset + 0u);
    hdr->version = flash_read32(flash_offset + 4u);
    hdr->load_addr = flash_read32(flash_offset + 8u);
    hdr->entry_addr = flash_read32(flash_offset + 12u);
    hdr->gp_addr = flash_read32(flash_offset + 16u);
    hdr->sp_addr = flash_read32(flash_offset + 20u);
    hdr->payload_size = flash_read32(flash_offset + 24u);
    hdr->payload_checksum = flash_read32(flash_offset + 28u);
}

static uint32_t flashboot_checksum32(uint32_t flash_offset, uint32_t size) {
    uint32_t sum = 0;
    uint32_t i;

    for (i = 0; i < size; ++i) {
        sum += flash_read8(flash_offset + i);
    }

    return sum;
}

static void flashboot_copy_payload(uint32_t flash_offset, uint32_t dst_addr, uint32_t size) {
    uint32_t i;
    volatile unsigned char *dst = (volatile unsigned char *)dst_addr;

    for (i = 0; i < size; ++i) {
        dst[i] = flash_read8(flash_offset + i);
    }
}

static int flashboot_validate(const flashboot_image_header_t *hdr) {
    uint32_t payload_offset;
    uint32_t actual_checksum;

    if (hdr->magic != FLASHBOOT_IMAGE_MAGIC) {
        dbg_puts("FB_MG\r\n");
        printf("flashboot: bad magic 0x%08X\n", (unsigned)hdr->magic);
        return -1;
    }

    if (hdr->version != FLASHBOOT_IMAGE_VERSION) {
        dbg_puts("FB_VR\r\n");
        printf("flashboot: bad version %u\n", (unsigned)hdr->version);
        return -1;
    }

    if (hdr->payload_size == 0u || hdr->payload_size > FLASHBOOT_MAX_PAYLOAD) {
        dbg_puts("FB_SZ\r\n");
        printf("flashboot: bad size %u\n", (unsigned)hdr->payload_size);
        return -1;
    }

    payload_offset = FLASHBOOT_IMAGE_OFFSET + (uint32_t)sizeof(flashboot_image_header_t);
    actual_checksum = flashboot_checksum32(payload_offset, hdr->payload_size);
    if (actual_checksum != hdr->payload_checksum) {
        dbg_puts("FB_CK\r\n");
        printf("flashboot: checksum fail exp=0x%08X got=0x%08X\n",
               (unsigned)hdr->payload_checksum,
               (unsigned)actual_checksum);
        return -1;
    }

    return 0;
}

static void flashboot_jump(uint32_t entry_addr, uint32_t gp_addr, uint32_t sp_addr) {
    flashboot_entry_fn entry = (flashboot_entry_fn)entry_addr;

    __asm__ __volatile__ ("wrctl status, zero");
    __asm__ __volatile__ ("wrctl ienable, zero");
    __asm__ __volatile__ ("mov sp, %0" :: "r"(sp_addr));
    __asm__ __volatile__ ("mov gp, %0" :: "r"(gp_addr));

#if defined(ALT_CPU_DCACHE_SIZE) && (ALT_CPU_DCACHE_SIZE > 0)
    alt_dcache_flush_all();
#endif
#if defined(ALT_CPU_ICACHE_SIZE) && (ALT_CPU_ICACHE_SIZE > 0)
    alt_icache_flush_all();
#endif

    entry();
}

int main(void) {
    flashboot_image_header_t hdr_local;
    const flashboot_image_header_t *hdr = &hdr_local;
    uint32_t payload_offset = FLASHBOOT_IMAGE_OFFSET + (uint32_t)sizeof(flashboot_image_header_t);
    uint32_t attempt;

    show_loading_screen();
    dbg_puts("FB0\r\n");
    printf("flashboot: start\n");
    printf("flashboot: powerup delay %u ms\n", (unsigned)FLASHBOOT_POWERUP_DELAY_MS);
    busy_delay_ms(FLASHBOOT_POWERUP_DELAY_MS);

    for (attempt = 0; attempt < FLASHBOOT_HEADER_RETRIES; ++attempt) {
        flash_reset_read_array();
        flashboot_read_header(FLASHBOOT_IMAGE_OFFSET, &hdr_local);
        if (flashboot_validate(hdr) == 0) {
            break;
        }

        printf("flashboot: retry %u/%u\n",
               (unsigned)(attempt + 1u),
               (unsigned)FLASHBOOT_HEADER_RETRIES);
        busy_delay_ms(FLASHBOOT_HEADER_RETRY_DELAY_MS);
    }

    dbg_puts("FB1\r\n");
    printf("flashboot: hdr magic=0x%08X size=%u load=0x%08X entry=0x%08X\n",
           (unsigned)hdr->magic,
           (unsigned)hdr->payload_size,
           (unsigned)hdr->load_addr,
           (unsigned)hdr->entry_addr);
    dbg_puts("FB2\r\n");

    if (attempt == FLASHBOOT_HEADER_RETRIES) {
        dbg_puts("FB_BAD\r\n");
        printf("flashboot: image invalid\n");
        for (;;) {
        }
    }

    dbg_puts("FB3\r\n");
    printf("flashboot: copy\n");
    flashboot_copy_payload(payload_offset, hdr->load_addr, hdr->payload_size);

    dbg_puts("FB4\r\n");
    printf("flashboot: jump gp=0x%08X sp=0x%08X\n",
           (unsigned)hdr->gp_addr,
           (unsigned)hdr->sp_addr);
    dbg_puts("FB5\r\n");
    flashboot_jump(hdr->entry_addr, hdr->gp_addr, hdr->sp_addr);

    dbg_puts("FB_RET\r\n");
    printf("flashboot: returned\n");
    for (;;) {
    }
}
