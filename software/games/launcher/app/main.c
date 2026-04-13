#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "system.h"
#include "io.h"
#include "altera_avalon_pio_regs.h"
#include "altera_avalon_uart_regs.h"

#include "launcher_image.h"
#include "launcher_module.h"
#include "launcher_storage.h"

#define VIEW_W 64
#define VIEW_H 128

#define MS_TICK_US   1000
#define LOGIC_DT_MS  10
#define RENDER_DT_MS 20

#define LED_BRIGHTNESS_OFFSET 8192
#define LED_STATUS_ADDR 8193
#define LED_FRAME_DONE  0x01
#define DEFAULT_BRIGHTNESS 100

#define PS2_BASE PS2_INTERFACE_0_BASE

#define PS2_VALID_MASK    (1u << 31)
#define PS2_RELEASED_MASK (1u << 30)
#define PS2_EXTENDED_MASK (1u << 29)
#define PS2_CODE_MASK     0xFF

#define K_RIGHT   0x01
#define K_DOWN    0x02
#define K_UP      0x04
#define K_LEFT    0x08
#define K_SELECT  0x10
#define UART_RX_BUDGET_PER_TICK 16

#define COLOR_BLACK   0
#define COLOR_RED     1
#define COLOR_GREEN   2
#define COLOR_BLUE    4
#define COLOR_CYAN    6
#define COLOR_WHITE   7

#define MAX_MENU_ITEMS 12
#define ROW_HEIGHT 10
#define VISIBLE_ROWS 6

typedef struct {
    uint8_t held;
    uint8_t pressed;
    uint8_t ps2_held;
    uint8_t uart_held;
} input_state_t;

static uint8_t viewbuf[VIEW_W * VIEW_H];
static uint8_t prevbuf[VIEW_W * VIEW_H];
static uint16_t base_addr_xv[VIEW_W];

static input_state_t input_state;
static launcher_storage_entry_t menu_entries[MAX_MENU_ITEMS];
static int menu_count = 0;
static int menu_selected = 0;
static int menu_top = 0;
static launcher_storage_status_t storage_status = LAUNCHER_STORAGE_NO_MEDIA;
static launcher_storage_status_t launch_status = LAUNCHER_STORAGE_OK;

static inline void wait_frame(void)
{
    while ((IORD_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_STATUS_ADDR) & LED_FRAME_DONE) == 0) ;
    IOWR_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_STATUS_ADDR, LED_FRAME_DONE);
}

static void set_display_brightness(uint8_t level)
{
    IOWR_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_BRIGHTNESS_OFFSET, level);
}

static void view_clear(void)
{
    memset(viewbuf, 0, sizeof(viewbuf));
}

static void init_panel_lut_small(void)
{
    for (int xv = 0; xv < VIEW_W; xv++) {
        int yp = 63 - xv;
        uint32_t base = (yp < 32) ? (uint32_t)(yp * 128) : (4096u + (uint32_t)((yp - 32) * 128));
        base_addr_xv[xv] = (uint16_t)base;
    }
    memset(prevbuf, 0xFF, sizeof(prevbuf));
}

static void blit_diff_to_panel_small_lut(void)
{
    for (int yv = 0; yv < VIEW_H; yv++) {
        uint16_t xp = (uint16_t)yv;
        int row = yv * VIEW_W;
        for (int xv = 0; xv < VIEW_W; xv++) {
            int i = row + xv;
            uint8_t v = (uint8_t)(viewbuf[i] & 7);
            if (v == prevbuf[i]) continue;
            prevbuf[i] = v;
            IOWR_8DIRECT(LED_MATRIX_AVALON_0_BASE, (uint16_t)(base_addr_xv[xv] + xp), v);
        }
    }
}

static void view_put(int xv, int yv, uint8_t rgb)
{
    if ((unsigned)xv >= VIEW_W || (unsigned)yv >= VIEW_H) return;
    viewbuf[yv * VIEW_W + xv] = rgb;
}

static void draw_rect(int x0, int y0, int w, int h, uint8_t color)
{
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            view_put(x0 + x, y0 + y, color);
        }
    }
}

static uint8_t glyph_row(char c, int row)
{
    switch (c) {
        case 'A': { static const uint8_t g[7] = {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}; return g[row]; }
        case 'B': { static const uint8_t g[7] = {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}; return g[row]; }
        case 'C': { static const uint8_t g[7] = {0x0F,0x10,0x10,0x10,0x10,0x10,0x0F}; return g[row]; }
        case 'D': { static const uint8_t g[7] = {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}; return g[row]; }
        case 'E': { static const uint8_t g[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}; return g[row]; }
        case 'F': { static const uint8_t g[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}; return g[row]; }
        case 'G': { static const uint8_t g[7] = {0x0F,0x10,0x10,0x17,0x11,0x11,0x0E}; return g[row]; }
        case 'H': { static const uint8_t g[7] = {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}; return g[row]; }
        case 'I': { static const uint8_t g[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x1F}; return g[row]; }
        case 'K': { static const uint8_t g[7] = {0x11,0x12,0x14,0x18,0x14,0x12,0x11}; return g[row]; }
        case 'L': { static const uint8_t g[7] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}; return g[row]; }
        case 'M': { static const uint8_t g[7] = {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}; return g[row]; }
        case 'N': { static const uint8_t g[7] = {0x11,0x19,0x15,0x13,0x11,0x11,0x11}; return g[row]; }
        case 'O': { static const uint8_t g[7] = {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}; return g[row]; }
        case 'P': { static const uint8_t g[7] = {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}; return g[row]; }
        case 'R': { static const uint8_t g[7] = {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}; return g[row]; }
        case 'S': { static const uint8_t g[7] = {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}; return g[row]; }
        case 'T': { static const uint8_t g[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}; return g[row]; }
        case 'U': { static const uint8_t g[7] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}; return g[row]; }
        case 'V': { static const uint8_t g[7] = {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}; return g[row]; }
        case 'Y': { static const uint8_t g[7] = {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}; return g[row]; }
        case '^': { static const uint8_t g[7] = {0x04,0x0E,0x15,0x04,0x04,0x04,0x04}; return g[row]; }
        case 'v': { static const uint8_t g[7] = {0x04,0x04,0x04,0x04,0x15,0x0E,0x04}; return g[row]; }
        case ' ': return 0;
        default:  return 0x1F;
    }
}

static void draw_char5x7(int x0, int y0, char c, uint8_t color)
{
    for (int row = 0; row < 7; row++) {
        uint8_t bits = glyph_row(c, row);
        for (int col = 0; col < 5; col++) {
            if ((bits >> (4 - col)) & 1) {
                view_put(x0 + col, y0 + row, color);
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

static void ps2_update_held(uint8_t mask, int is_released)
{
    uint8_t before = input_state.held;
    if (is_released) input_state.ps2_held &= (uint8_t)~mask;
    else input_state.ps2_held |= mask;
    if (!is_released && !(before & mask)) input_state.pressed |= mask;
}

static void ps2_make_code(uint8_t code, int extended)
{
    if (!extended) {
        if (code == 0x1D) ps2_update_held(K_UP, 0);
        if (code == 0x1B) ps2_update_held(K_DOWN, 0);
        if (code == 0x1C) ps2_update_held(K_LEFT, 0);
        if (code == 0x23) ps2_update_held(K_RIGHT, 0);
        if (code == 0x29) ps2_update_held(K_SELECT, 0);
        if (code == 0x2C) ps2_update_held(K_SELECT, 0); /* T */
        if (code == 0x2B) ps2_update_held(K_SELECT, 0); /* F */
        if (code == 0x4B) ps2_update_held(K_SELECT, 0); /* L */
        if (code == 0x5A) ps2_update_held(K_SELECT, 0); /* Enter */
    } else {
        if (code == 0x75) ps2_update_held(K_UP, 0);
        if (code == 0x72) ps2_update_held(K_DOWN, 0);
        if (code == 0x6B) ps2_update_held(K_LEFT, 0);
        if (code == 0x74) ps2_update_held(K_RIGHT, 0);
    }
}

static void ps2_break_code(uint8_t code, int extended)
{
    if (!extended) {
        if (code == 0x1D) ps2_update_held(K_UP, 1);
        if (code == 0x1B) ps2_update_held(K_DOWN, 1);
        if (code == 0x1C) ps2_update_held(K_LEFT, 1);
        if (code == 0x23) ps2_update_held(K_RIGHT, 1);
        if (code == 0x29) ps2_update_held(K_SELECT, 1);
        if (code == 0x2C) ps2_update_held(K_SELECT, 1); /* T */
        if (code == 0x2B) ps2_update_held(K_SELECT, 1); /* F */
        if (code == 0x4B) ps2_update_held(K_SELECT, 1); /* L */
        if (code == 0x5A) ps2_update_held(K_SELECT, 1); /* Enter */
    } else {
        if (code == 0x75) ps2_update_held(K_UP, 1);
        if (code == 0x72) ps2_update_held(K_DOWN, 1);
        if (code == 0x6B) ps2_update_held(K_LEFT, 1);
        if (code == 0x74) ps2_update_held(K_RIGHT, 1);
    }
}

static void handle_ps2(void)
{
    for (int i = 0; i < 32; i++) {
        uint32_t d = IORD_32DIRECT(PS2_BASE, 0);
        if ((d & PS2_VALID_MASK) == 0) break;
        if (d & PS2_RELEASED_MASK) ps2_break_code((uint8_t)(d & PS2_CODE_MASK), (d & PS2_EXTENDED_MASK) != 0);
        else ps2_make_code((uint8_t)(d & PS2_CODE_MASK), (d & PS2_EXTENDED_MASK) != 0);
    }
}

static void handle_uart(void)
{
    uint32_t budget = UART_RX_BUDGET_PER_TICK;
    while (budget-- > 0) {
        uint32_t status = IORD_ALTERA_AVALON_UART_STATUS(UART_0_BASE);
        if (status & (ALTERA_AVALON_UART_STATUS_ROE_MSK |
                      ALTERA_AVALON_UART_STATUS_TOE_MSK |
                      ALTERA_AVALON_UART_STATUS_FE_MSK  |
                      ALTERA_AVALON_UART_STATUS_PE_MSK  |
                      ALTERA_AVALON_UART_STATUS_BRK_MSK)) {
            IOWR_ALTERA_AVALON_UART_STATUS(UART_0_BASE, 0);
            input_state.uart_held = 0;
            if ((status & ALTERA_AVALON_UART_STATUS_RRDY_MSK) == 0) continue;
        }
        if ((status & ALTERA_AVALON_UART_STATUS_RRDY_MSK) == 0) break;

        switch ((char)IORD_ALTERA_AVALON_UART_RXDATA(UART_0_BASE)) {
            case 'U': input_state.uart_held |= K_UP; input_state.pressed |= K_UP; break;
            case 'u': input_state.uart_held &= (uint8_t)~K_UP; break;
            case 'D': input_state.uart_held |= K_DOWN; input_state.pressed |= K_DOWN; break;
            case 'd': input_state.uart_held &= (uint8_t)~K_DOWN; break;
            case 'L': input_state.uart_held |= K_SELECT; input_state.pressed |= K_SELECT; break;
            case 'l': input_state.uart_held &= (uint8_t)~K_SELECT; break;
            case 'R': input_state.uart_held |= K_RIGHT; input_state.pressed |= K_RIGHT; break;
            case 'r': input_state.uart_held &= (uint8_t)~K_RIGHT; break;
            case 'T':
            case 'F': input_state.uart_held |= K_SELECT; input_state.pressed |= K_SELECT; break;
            case 't':
            case 'f': input_state.uart_held &= (uint8_t)~K_SELECT; break;
            default: break;
        }
    }
}

static void handle_input_merge(void)
{
    handle_ps2();
    handle_uart();

    {
        uint32_t raw = IORD_ALTERA_AVALON_PIO_DATA(PIO_0_BASE) & 0xF;
        uint8_t p = (uint8_t)((~raw) & 0xF);
        uint8_t pio_held = 0;
        if (p & 0x8) pio_held |= K_UP;
        if (p & 0x1) pio_held |= K_DOWN;
        if (p & 0x4) pio_held |= K_LEFT;
        if (p & 0x2) pio_held |= K_RIGHT;
        input_state.pressed |= (uint8_t)(pio_held & ~input_state.held);
        input_state.held = input_state.ps2_held | input_state.uart_held | pio_held;
    }
}

static const char *storage_status_text(launcher_storage_status_t st)
{
    switch (st) {
        case LAUNCHER_STORAGE_OK: return "READY";
        case LAUNCHER_STORAGE_NO_MEDIA: return "NO SD";
        case LAUNCHER_STORAGE_NOT_FOUND: return "NOFILE";
        case LAUNCHER_STORAGE_BAD_IMAGE: return "BADIMG";
        case LAUNCHER_STORAGE_IO_ERROR: return "IOERR";
        default: return "UNK";
    }
}

static void menu_refresh_catalog(void)
{
    if (storage_status == LAUNCHER_STORAGE_OK) {
        menu_count = launcher_storage_list(menu_entries, MAX_MENU_ITEMS);
    } else {
        menu_count = 0;
    }
    if (menu_count <= 0) {
        strcpy(menu_entries[0].title, "EMPTY");
        strcpy(menu_entries[0].path, "none");
        menu_count = 1;
    }
    if (menu_selected >= menu_count) menu_selected = menu_count - 1;
    if (menu_selected < 0) menu_selected = 0;
    if (menu_top > menu_selected) menu_top = menu_selected;
}

static void menu_launch_selected(void)
{
    if (strcmp(menu_entries[menu_selected].path, "none") == 0) {
        launch_status = LAUNCHER_STORAGE_NOT_FOUND;
        return;
    }

    {
        launcher_loaded_image_t image;
        launch_status = launcher_storage_load_image(menu_entries[menu_selected].path, &image);
        if (launch_status == LAUNCHER_STORAGE_OK) {
            if (launcher_image_boot(&image) != LAUNCHER_IMAGE_OK) {
                launch_status = LAUNCHER_STORAGE_BAD_IMAGE;
            }
            return;
        }
    }
}

static void update_menu(void)
{
    if (input_state.pressed & K_UP) {
        if (menu_selected > 0) menu_selected--;
    }
    if (input_state.pressed & K_DOWN) {
        if (menu_selected + 1 < menu_count) menu_selected++;
    }
    if (menu_selected < menu_top) menu_top = menu_selected;
    if (menu_selected >= menu_top + VISIBLE_ROWS) menu_top = menu_selected - VISIBLE_ROWS + 1;
    if (input_state.pressed & K_SELECT) menu_launch_selected();
    input_state.pressed = 0;
}

static void render_menu(void)
{
    view_clear();
    draw_text5x7(2, 4, "MENU SD", COLOR_CYAN);
    draw_text5x7(2, 14, storage_status_text(storage_status), COLOR_WHITE);

    for (int i = 0; i < VISIBLE_ROWS; i++) {
        int idx = menu_top + i;
        int y = 24 + i * ROW_HEIGHT;
        if (idx >= menu_count) break;
        draw_rect(1, y - 1, VIEW_W - 2, 8, (idx == menu_selected) ? COLOR_GREEN : COLOR_BLUE);
        draw_text5x7(4, y, menu_entries[idx].title, (idx == menu_selected) ? COLOR_BLACK : COLOR_WHITE);
    }

    draw_text5x7(2, 92, "^ v NAV", COLOR_WHITE);
    draw_text5x7(2, 102, "L LOAD", COLOR_WHITE);
    draw_text5x7(2, 112, storage_status_text(launch_status), COLOR_RED);
    draw_text5x7(2, 120, "SD MOD", COLOR_RED);
}

int main(void)
{
    init_panel_lut_small();
    set_display_brightness(DEFAULT_BRIGHTNESS);
    storage_status = launcher_storage_init();
    launch_status = storage_status;
    menu_refresh_catalog();

    uint32_t logic_acc_ms = 0;
    uint32_t render_acc_ms = 0;
    while (1) {
        usleep(MS_TICK_US);
        logic_acc_ms++;
        render_acc_ms++;
        if (logic_acc_ms >= LOGIC_DT_MS) {
            logic_acc_ms -= LOGIC_DT_MS;
            handle_input_merge();
            update_menu();
        }
        if (render_acc_ms >= RENDER_DT_MS) {
            render_acc_ms -= RENDER_DT_MS;
            render_menu();
            wait_frame();
            blit_diff_to_panel_small_lut();
        }
    }
}
