#include <stdint.h>
#include <unistd.h>

#include "system.h"
#include "io.h"

#include "template_module.h"

#define VIEW_W 64
#define VIEW_H 128

#define LED_BRIGHTNESS_OFFSET 8192
#define LED_STATUS_ADDR 8193
#define LED_FRAME_DONE 0x01

#define PS2_BASE PS2_INTERFACE_0_BASE
#define PS2_VALID_MASK    (1u << 31)
#define PS2_RELEASED_MASK (1u << 30)
#define PS2_EXTENDED_MASK (1u << 29)
#define PS2_CODE_MASK     0xFF

#define COLOR_BLACK 0
#define COLOR_GREEN 2
#define COLOR_WHITE 7

static uint16_t g_base_addr_xv[VIEW_W];

static void wait_frame(void)
{
    while ((IORD_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_STATUS_ADDR) & LED_FRAME_DONE) == 0) {
    }
    IOWR_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_STATUS_ADDR, LED_FRAME_DONE);
}

static void set_display_brightness(uint8_t level)
{
    IOWR_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_BRIGHTNESS_OFFSET, level);
}

static void init_panel_lut_small(void)
{
    int xv;
    for (xv = 0; xv < VIEW_W; xv++) {
        int yp = 63 - xv;
        uint32_t base = (yp < 32) ? (uint32_t)(yp * 128) : (4096u + (uint32_t)((yp - 32) * 128));
        g_base_addr_xv[xv] = (uint16_t)base;
    }
}

static void panel_put(int xv, int yv, uint8_t rgb)
{
    uint16_t xp;
    if ((unsigned)xv >= VIEW_W || (unsigned)yv >= VIEW_H) return;
    xp = (uint16_t)yv;
    IOWR_8DIRECT(LED_MATRIX_AVALON_0_BASE, (uint16_t)(g_base_addr_xv[xv] + xp), rgb);
}

static void clear_screen(uint8_t color)
{
    int y;
    int x;
    for (y = 0; y < VIEW_H; y++) {
        for (x = 0; x < VIEW_W; x++) {
            panel_put(x, y, color);
        }
    }
}

static uint8_t glyph_row(char c, int row)
{
    switch (c) {
        case 'A': { static const uint8_t g[7] = {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}; return g[row]; }
        case 'E': { static const uint8_t g[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}; return g[row]; }
        case 'L': { static const uint8_t g[7] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}; return g[row]; }
        case 'M': { static const uint8_t g[7] = {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}; return g[row]; }
        case 'O': { static const uint8_t g[7] = {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}; return g[row]; }
        case 'P': { static const uint8_t g[7] = {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}; return g[row]; }
        case 'R': { static const uint8_t g[7] = {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}; return g[row]; }
        case 'T': { static const uint8_t g[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}; return g[row]; }
        case 'U': { static const uint8_t g[7] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}; return g[row]; }
        case ' ': return 0;
        default: return 0x1F;
    }
}

static void draw_char5x7(int x0, int y0, char c, uint8_t color)
{
    int row;
    int col;
    for (row = 0; row < 7; row++) {
        uint8_t bits = glyph_row(c, row);
        for (col = 0; col < 5; col++) {
            if ((bits >> (4 - col)) & 1) {
                panel_put(x0 + col, y0 + row, color);
            }
        }
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

static int should_exit_to_launcher(void)
{
    uint32_t d;
    int i;

    for (i = 0; i < 32; i++) {
        d = IORD_32DIRECT(PS2_BASE, 0);
        if ((d & PS2_VALID_MASK) == 0) break;
        if ((d & PS2_RELEASED_MASK) != 0) continue;

        if ((d & PS2_EXTENDED_MASK) == 0) {
            uint8_t code = (uint8_t)(d & PS2_CODE_MASK);
            if (code == 0x29 || code == 0x2C || code == 0x2B || code == 0x4B || code == 0x5A) {
                return 1;
            }
        }
    }

    return 0;
}

static void template_entry(void)
{
    init_panel_lut_small();
    set_display_brightness(100);
    clear_screen(COLOR_BLACK);
    draw_text5x7(8, 48, "TEMPLATE", COLOR_GREEN);
    draw_text5x7(8, 62, "MODULE", COLOR_WHITE);
    draw_text5x7(8, 84, "PRESS SEL", COLOR_WHITE);
    draw_text5x7(8, 94, "TO RETURN", COLOR_WHITE);

    for (;;) {
        wait_frame();
        if (should_exit_to_launcher()) {
            usleep(120000);
            return;
        }
    }
}

const launcher_builtin_module_t template_module = {
    "builtin:template",
    "TEMPLATE",
    template_entry
};
