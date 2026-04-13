#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "system.h"
#include "io.h"
#include "altera_avalon_pio_regs.h"
#include "altera_avalon_uart_regs.h"
#include "tetris_module.h"

/* =========================
 * Configuracoes de Tela
 * ========================= */
#define PHY_W 128
#define PHY_H 64
#define VIEW_W 64
#define VIEW_H 128

#define HUD_LINE_Y 14
#define GAME_TOP_Y 15

#define GRID_W 16
#define GRID_H 28
#define CELL_PX 4

/* =========================
 * Timing & Fluidez
 * ========================= */
#define MS_TICK_US      1000
#define LOGIC_DT_MS     3
#define RENDER_DT_MS    8

#define DROP_SPEED_NORMAL 100
#define DROP_SPEED_FAST   30

#define DAS_DELAY_MS    60
#define DAS_SPEED_MS    16

/* =========================
 * Dificuldade
 * ========================= */
#define LINES_PER_LEVEL   3
#define SPEED_DECREMENT   2
#define MIN_DROP_SPEED    LOGIC_DT_MS
#define NUM_LIVES         1

#define HEIGHT_START   (GRID_H /3)
#define HEIGHT_MAX_BOOST_MS  15

/* Cores */
#define RGB(r,g,b)  (uint8_t)(((r)?1:0) | ((g)?2:0) | ((b)?4:0))
#define COLOR_BLACK 0
#define COLOR_WHITE 7

/* =========================
 * Globais e Estruturas
 * ========================= */
static const uint16_t SHAPES[7][4] = {
    {0x0F00, 0x2222, 0x00F0, 0x4444},
    {0x8E00, 0x6440, 0x0E20, 0x44C0},
    {0x2E00, 0x4460, 0x0E80, 0xC440},
    {0xCC00, 0xCC00, 0xCC00, 0xCC00},
    {0x6C00, 0x4620, 0x06C0, 0x8C40},
    {0x4E00, 0x4640, 0x0E40, 0x4C40},
    {0xC600, 0x2640, 0x0C60, 0x4C80}
};

static const uint16_t FONT_NUM[10] = {
    0x7B6F, 0x2C97, 0x73E7, 0x73CF, 0x5BC9,
    0x79CF, 0x79EF, 0x7249, 0x7BEF, 0x7BCF
};

static const uint8_t FONT_VHDL[7][7] = {
    {0x0E, 0x11, 0x10, 0x13, 0x11, 0x11, 0x0E},
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    {0x11, 0x1B, 0x15, 0x11, 0x11, 0x11, 0x11},
    {0x1F, 0x10, 0x10, 0x1F, 0x10, 0x10, 0x1F},
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    {0x11, 0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04},
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}
};

static const uint8_t GAMMA_TABLE[256] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,   1,   1,
    1,   1,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,   4,   4,   4,   4,
    5,   5,   5,   5,   6,   6,   6,   7,   7,   7,   8,   8,   8,   9,   9,   9,
   10,  10,  11,  11,  12,  12,  13,  13,  14,  14,  15,  15,  16,  16,  17,  18,
   18,  19,  19,  20,  21,  21,  22,  22,  23,  24,  25,  25,  26,  27,  27,  28,
   29,  30,  31,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,
   44,  45,  46,  47,  48,  50,  51,  52,  53,  54,  55,  57,  58,  59,  60,  62,
   63,  64,  66,  67,  68,  70,  71,  72,  74,  75,  77,  78,  80,  81,  83,  84,
   86,  88,  89,  91,  92,  94,  96,  97,  99, 101, 103, 104, 106, 108, 110, 112,
  114, 116, 118, 120, 122, 124, 126, 128, 130, 132, 134, 136, 138, 140, 143, 145,
  147, 149, 152, 154, 156, 159, 161, 163, 166, 168, 171, 173, 176, 178, 181, 183,
  186, 189, 191, 194, 197, 199, 202, 205, 208, 210, 213, 216, 219, 222, 225, 228,
  231, 234, 237, 240, 243, 246, 250, 253, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255
};

static uint8_t viewbuf[VIEW_W * VIEW_H];

typedef enum { STATE_PLAYING, STATE_LOST_LIFE, STATE_GAME_OVER_FADE, STATE_GAME_OVER } game_state_t;
static game_state_t current_state = STATE_PLAYING;

typedef struct { int id; int rot; int x; int y; } piece_t;
static piece_t cur;

static uint8_t grid[GRID_H][GRID_W];
static int next_id = 0;
static uint16_t lfsr = 0xA5A5;

static int bag[7] = {0, 1, 2, 3, 4, 5, 6};
static int bag_idx = 7;

static uint32_t ms_global = 0;
static uint32_t fall_acc_ms = 0;
static uint32_t fall_limit_ms = DROP_SPEED_NORMAL;

static uint8_t score_bcd[6] = {0,0,0,0,0,0};
static uint8_t lines_mod10 = 0;
static uint8_t lives = 3;

static int timer_das_l = 0;
static int timer_das_r = 0;
static uint8_t prev_rot_key = 0;
static uint8_t prev_any_key = 0;

/* =========================
 * Sincronismo de video
 * ========================= */
#define LED_STATUS_ADDR 8193
#define LED_FRAME_DONE  0x01
#define LED_STREAM_LINE_ADDR 8194
#define LED_STREAM_DATA_ADDR 8195
#define LED_STREAM_CTRL_ADDR 8196
#define LED_STREAM_CTRL_FULL 0x02

#define USE_STREAM_BLIT 0

static inline void wait_frame(void) {
    while ((IORD_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_STATUS_ADDR) & LED_FRAME_DONE) == 0)
        ;
    IOWR_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_STATUS_ADDR, LED_FRAME_DONE);
}

/* =========================
 * Controle de Brilho
 * ========================= */
#define LED_BRIGHTNESS_OFFSET 8192
#define DEFAULT_BRIGHTNESS 100
#define PS2_BASE PS2_INTERFACE_0_BASE

static uint8_t fade_brightness = DEFAULT_BRIGHTNESS;
static uint32_t fade_acc_ms = 0;

#define FADE_STEP_MS    5
#define FADE_STEP_VAL   1

#define PS2_VALID_MASK    (1u << 31)
#define PS2_RELEASED_MASK (1u << 30)
#define PS2_EXTENDED_MASK (1u << 29)
#define PS2_CODE_MASK     0xFF

#define K_RIGHT   0x01
#define K_DOWN    0x02
#define K_ROT     0x04
#define K_LEFT    0x08
#define K_SPACE   0x10

static volatile uint8_t kb_held     = 0;
static volatile uint8_t kb_pressed  = 0;
static volatile uint8_t kb_released = 0;
static volatile uint8_t ps2_held    = 0;
static volatile uint8_t uart_held   = 0;
static volatile uint8_t launcher_exit_req = 0;

static volatile uint8_t ps2_kb_mask = 0;

#define effects_trigger_click()      ((void)0)
#define effects_trigger_line_clear() ((void)0)
#define effects_trigger_game_over()  ((void)0)
#define effects_trigger_rotate()     ((void)0)
#define effects_music_start()        ((void)0)
#define effects_music_stop()         ((void)0)

#define FLASHBOOT_START_ADDR 0x0180C040u

/* =========================
 * Funcoes Prototipos
 * ========================= */
static void set_display_brightness(uint8_t level);

/* =========================
 * Funcoes Auxiliares
 * ========================= */
static int max_stack_height(void)
{
    int maxh = 0;
    for (int x = 0; x < GRID_W; x++) {
        int y;
        for (y = 0; y < GRID_H; y++) {
            if (grid[y][x] != 0) break;
        }
        int h = GRID_H - y;
        if (h > maxh) maxh = h;
    }
    return maxh;
}

static uint32_t height_boost_ms(void)
{
    int h = max_stack_height();
    if (h < HEIGHT_START) return 0;

    int danger = h - HEIGHT_START;
    int range  = GRID_H - HEIGHT_START;

    return (uint32_t)((danger * HEIGHT_MAX_BOOST_MS) / range);
}

static int grid_is_empty(void)
{
    for (int y = 0; y < GRID_H; y++)
        for (int x = 0; x < GRID_W; x++)
            if (grid[y][x] != 0) return 0;
    return 1;
}

static void score_add_100(void)
{
    int i = 2;
    score_bcd[i]++;
    while (i < 6 && score_bcd[i] >= 10) {
        score_bcd[i] = 0;
        i++;
        if (i < 6) score_bcd[i]++;
    }
}

static void lfsr_step(void) {
    uint16_t bit = (uint16_t)(((lfsr >> 15) ^ (lfsr >> 13)) & 1);
    lfsr = (uint16_t)((lfsr << 1) | bit);
}

static void shuffle_bag(void) {
    int i, j , temp;
    for (i = 6; i > 0; i--) {
        lfsr_step();
        j = lfsr % (i + 1);
        temp = bag[i]; bag[i] = bag[j]; bag[j] = temp;
    }
    bag_idx = 0;
}

static int get_next_piece_from_bag(void) {
    if (bag_idx >= 7) shuffle_bag();
    return bag[bag_idx++];
}

/* =========================
 * Hardware e Video
 * ========================= */
static void ps2_update_held(uint8_t mask, int is_released) {
    uint8_t before = kb_held;
    if (is_released) ps2_held &= ~mask; else ps2_held |= mask;
    if (!is_released && !(before & mask)) kb_pressed |= mask;
}

static void ps2_make_code(uint8_t code, int extended) {
    if (!extended) {
        if (code == 0x1C) ps2_update_held(K_LEFT, 0);
        if (code == 0x23) ps2_update_held(K_RIGHT, 0);
        if (code == 0x1D) ps2_update_held(K_ROT, 0);
        if (code == 0x1B) ps2_update_held(K_DOWN, 0);
        if (code == 0x29) ps2_update_held(K_SPACE, 0);
    } else {
        if (code == 0x6B) ps2_update_held(K_LEFT, 0);
        if (code == 0x74) ps2_update_held(K_RIGHT, 0);
        if (code == 0x75) ps2_update_held(K_ROT, 0);
        if (code == 0x72) ps2_update_held(K_DOWN, 0);
    }
}

static void ps2_break_code(uint8_t code, int extended) {
    if (!extended) {
        if (code == 0x1C) ps2_update_held(K_LEFT, 1);
        if (code == 0x23) ps2_update_held(K_RIGHT, 1);
        if (code == 0x1D) ps2_update_held(K_ROT, 1);
        if (code == 0x1B) ps2_update_held(K_DOWN, 1);
        if (code == 0x29) ps2_update_held(K_SPACE, 1);
    } else {
        if (code == 0x6B) ps2_update_held(K_LEFT, 1);
        if (code == 0x74) ps2_update_held(K_RIGHT, 1);
        if (code == 0x75) ps2_update_held(K_ROT, 1);
        if (code == 0x72) ps2_update_held(K_DOWN, 1);
    }
}

static void handle_ps2(void) {
    for (int i = 0; i < 32; i++) {
        uint32_t d = IORD_32DIRECT(PS2_BASE, 0);
        if ((d & PS2_VALID_MASK) == 0) break;
        int released = (d & PS2_RELEASED_MASK) ? 1 : 0;
        int extended = (d & PS2_EXTENDED_MASK) ? 1 : 0;
        uint8_t code = (uint8_t)(d & PS2_CODE_MASK);
        if (released) ps2_break_code(code, extended);
        else ps2_make_code(code, extended);
    }
}

static void handle_uart(void) {
    while (IORD_ALTERA_AVALON_UART_STATUS(UART_0_BASE) & ALTERA_AVALON_UART_STATUS_RRDY_MSK) {
        uint8_t rx = (uint8_t)IORD_ALTERA_AVALON_UART_RXDATA(UART_0_BASE);
        switch (rx) {
            case 'L':
                uart_held |= K_LEFT;
                kb_pressed |= K_LEFT;
                break;
            case 'l':
                uart_held &= (uint8_t)~K_LEFT;
                kb_released |= K_LEFT;
                break;
            case 'R':
                uart_held |= K_RIGHT;
                kb_pressed |= K_RIGHT;
                break;
            case 'r':
                uart_held &= (uint8_t)~K_RIGHT;
                kb_released |= K_RIGHT;
                break;
            case 'U':
                uart_held |= K_ROT;
                kb_pressed |= K_ROT;
                break;
            case 'u':
                uart_held &= (uint8_t)~K_ROT;
                kb_released |= K_ROT;
                break;
            case 'D':
                uart_held |= K_DOWN;
                kb_pressed |= K_DOWN;
                break;
            case 'd':
                uart_held &= (uint8_t)~K_DOWN;
                kb_released |= K_DOWN;
                break;
            default:
                break;
        }
    }
}

static void handle_input_merge(void) {
    handle_ps2();
    handle_uart();
    uint32_t raw = IORD_ALTERA_AVALON_PIO_DATA(PIO_0_BASE) & 0xF;
    uint8_t p = (uint8_t)((~raw) & 0xF);
    if (p & 0x4) launcher_exit_req = 1;
    kb_held = ps2_held | uart_held;
}

static void launcher_soft_reset(void)
{
    __asm__ __volatile__("wrctl status, zero");
    __asm__ __volatile__("wrctl ienable, zero");
    ((void (*)(void))FLASHBOOT_START_ADDR)();
    for (;;) {
    }
}

static void view_clear(void) {
    memset(viewbuf, 0, sizeof(viewbuf));
}

static void view_put(int xv, int yv, uint8_t rgb) {
    if ((unsigned)xv >= VIEW_W || (unsigned)yv >= VIEW_H) return;
    viewbuf[yv * VIEW_W + xv] = (uint8_t)(rgb & 7);
}

/* =========================
 * LUT + BLIT DIFERENCIAL
 * ========================= */
static uint16_t base_addr_xv[VIEW_W];
static uint8_t  prevbuf[VIEW_W * VIEW_H];

static void init_panel_lut_small(void)
{
    for (int xv = 0; xv < VIEW_W; xv++) {
        int yp = 63 - xv;
        uint32_t base;
        if (yp < 32) base = (uint32_t)(yp * 128);
        else         base = 4096u + (uint32_t)((yp - 32) * 128);
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

            uint16_t addr = (uint16_t)(base_addr_xv[xv] + xp);
            IOWR_8DIRECT(LED_MATRIX_AVALON_0_BASE, addr, v);
        }
    }
}

static void blit_stream_diff(void)
{
    for (int row = 0; row < 32; row++) {
        int upper_xv = 63 - row;
        int lower_xv = 31 - row;
        int changed = 0;
        for (int col = 0; col < VIEW_H; col++) {
            int idx_upper = col * VIEW_W + upper_xv;
            int idx_lower = col * VIEW_W + lower_xv;
            if (viewbuf[idx_upper] != prevbuf[idx_upper] ||
                viewbuf[idx_lower] != prevbuf[idx_lower]) {
                changed = 1;
                break;
            }
        }
        if (!changed) {
            continue;
        }
        IOWR_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_STREAM_LINE_ADDR, (uint8_t)row);
        for (int col = 0; col < VIEW_H; col++) {
            int idx_upper = col * VIEW_W + upper_xv;
            int idx_lower = col * VIEW_W + lower_xv;
            uint8_t upper = (uint8_t)(viewbuf[idx_upper] & 7);
            uint8_t lower = (uint8_t)(viewbuf[idx_lower] & 7);
            uint8_t data = (uint8_t)(upper | (lower << 3));
            while (IORD_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_STREAM_CTRL_ADDR) & LED_STREAM_CTRL_FULL) {
                ;
            }
            IOWR_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_STREAM_DATA_ADDR, data);
            prevbuf[idx_upper] = upper;
            prevbuf[idx_lower] = lower;
        }
    }
}

static int font_get_bit(const uint16_t *font_table, int glyph_idx, int x, int y) {
    if(x<0||x>2||y<0||y>4) return 0;
    uint16_t v = font_table[glyph_idx];
    int bit_idx = 14 - (y * 3 + x);
    return (v >> bit_idx) & 1;
}

static void draw_digit_tall(int digit, int start_x, int start_y, uint8_t color) {
    int gx, gy;
    for (gy = 0; gy < 5; gy++) {
        for (gx = 0; gx < 3; gx++) {
            if (font_get_bit(FONT_NUM, digit, gx, gy)) {
                view_put(start_x + gx, start_y + (gy * 2), color);
                view_put(start_x + gx, start_y + (gy * 2) + 1, color);
            }
        }
    }
}

static uint8_t color_piece(int id) {
    switch (id) {
        case 0: return RGB(0,1,1); case 1: return RGB(0,0,1);
        case 2: return RGB(1,1,0); case 3: return RGB(1,1,0);
        case 4: return RGB(0,1,0); case 5: return RGB(1,0,1);
        default:return RGB(1,0,0);
    }
}

static int shape_filled(int id, int rot, int r, int c) {
    return (SHAPES[id][rot & 3] >> ((3-r)*4 + (3-c))) & 1;
}

/* =========================
 * Logica do Jogo
 * ========================= */
static int collide_piece(int id, int rot, int x, int y) {
    int r, c, gx, gy;
    for (r = 0; r < 4; r++) {
        for (c = 0; c < 4; c++) {
            if (!shape_filled(id, rot, r, c)) continue;
            gx = x + c; gy = y + r;
            if (gx < 0 || gx >= GRID_W || gy >= GRID_H) return 1;
            if (gy >= 0 && grid[gy][gx] != 0) return 1;
        }
    }
    return 0;
}

static void lock_piece(const piece_t *p) {
    int r, c, gx, gy;
    for (r = 0; r < 4; r++) {
        for (c = 0; c < 4; c++) {
            if (shape_filled(p->id, p->rot, r, c)) {
                gx = p->x + c; gy = p->y + r;
                if (gy >= 0 && gy < GRID_H && gx >= 0 && gx < GRID_W)
                    grid[gy][gx] = (uint8_t)(p->id + 1);
            }
        }
    }
    effects_trigger_click();
}

static void clear_lines(void) {
    int y, x, k, full;
    uint8_t cleared = 0;
    for (y = GRID_H - 1; y >= 0; y--) {
        full = 1;
        for (x = 0; x < GRID_W; x++) if (grid[y][x] == 0) { full = 0; break; }

        if (full) {
            cleared = 1;
            if (score_bcd[0]==9) {
                score_bcd[0]=0;
                if(score_bcd[1]==9) {score_bcd[1]=0; score_bcd[2]++;}
                else score_bcd[1]++;
            } else {
                score_bcd[0]++;
            }

            lines_mod10++;
            if(lines_mod10 >= LINES_PER_LEVEL) {
                lines_mod10 = 0;
                if(fall_limit_ms > (MIN_DROP_SPEED + SPEED_DECREMENT)) {
                    fall_limit_ms -= SPEED_DECREMENT;
                } else {
                    fall_limit_ms = MIN_DROP_SPEED;
                }
            }

            for (k = y; k >= 1; k--)
                for (x = 0; x < GRID_W; x++) grid[k][x] = grid[k-1][x];

            for (x = 0; x < GRID_W; x++) grid[0][x] = 0;

            y++;
        }
    }
    if (cleared) {
        effects_trigger_line_clear();
    }
}

static void reset_board_only(void) {
    memset(grid, 0, sizeof(grid));
    timer_das_l = 0; timer_das_r = 0; prev_rot_key = 0;
    fall_limit_ms = DROP_SPEED_NORMAL;
    lines_mod10 = 0;
    fall_acc_ms = 0;
}

static void reset_full_game(void) {
    reset_board_only();
    lives = NUM_LIVES;
    memset(score_bcd, 0, sizeof(score_bcd));
    lines_mod10 = 0;
    fall_limit_ms = DROP_SPEED_NORMAL;

    shuffle_bag();
    next_id = get_next_piece_from_bag();
}

static void handle_death(void) {
    if (lives > 1) {
        lives--;
        current_state = STATE_LOST_LIFE;
    } else {
        lives = 0;
        effects_trigger_game_over();
        effects_music_stop();

        current_state = STATE_GAME_OVER_FADE;
        fade_brightness = DEFAULT_BRIGHTNESS;

        fade_acc_ms = 0;
        prev_any_key = 1;

        set_display_brightness(fade_brightness);
    }
}

static void spawn_piece(void) {
    cur.id = next_id;
    next_id = get_next_piece_from_bag();
    cur.rot = 0; cur.x = 6; cur.y = -1;
    fall_acc_ms = 0;

    if (collide_piece(cur.id, cur.rot, cur.x, cur.y)) {
        handle_death();
    }
}

static void try_rotate(void) {
    int nr = (cur.rot + 1) & 3;

    if (!collide_piece(cur.id, nr, cur.x, cur.y)) {
        cur.rot = nr;
        effects_trigger_rotate();
        return;
    }
    if (!collide_piece(cur.id, nr, cur.x + 1, cur.y)) {
        cur.x += 1; cur.rot = nr;
        effects_trigger_rotate();
        return;
    }
    if (!collide_piece(cur.id, nr, cur.x - 1, cur.y)) {
        cur.x -= 1; cur.rot = nr;
        effects_trigger_rotate();
        return;
    }
}

static void update_play_state_gold(uint8_t held, uint8_t pressed_edge)
{
    int move;
    uint32_t limit;

    if (held & K_LEFT) {
        if (timer_das_l == 0) {
            move = -1;
            if (!collide_piece(cur.id, cur.rot, cur.x + move, cur.y)) cur.x += move;
            timer_das_l = DAS_DELAY_MS;
        } else {
            timer_das_l -= LOGIC_DT_MS;
            if (timer_das_l <= 0) {
                move = -1;
                if (!collide_piece(cur.id, cur.rot, cur.x + move, cur.y)) cur.x += move;
                timer_das_l = DAS_SPEED_MS;
            }
        }
    } else {
        timer_das_l = 0;
    }

    if (held & K_RIGHT) {
        if (timer_das_r == 0) {
            move = 1;
            if (!collide_piece(cur.id, cur.rot, cur.x + move, cur.y)) cur.x += move;
            timer_das_r = DAS_DELAY_MS;
        } else {
            timer_das_r -= LOGIC_DT_MS;
            if (timer_das_r <= 0) {
                move = 1;
                if (!collide_piece(cur.id, cur.rot, cur.x + move, cur.y)) cur.x += move;
                timer_das_r = DAS_SPEED_MS;
            }
        }
    } else {
        timer_das_r = 0;
    }

    if (pressed_edge & K_ROT) {
        try_rotate();
    }

    limit = (held & K_DOWN) ? DROP_SPEED_FAST : fall_limit_ms;

    uint32_t boost = height_boost_ms();
    if (boost) {
        limit = (limit > boost) ? (limit - boost) : 1;
        if (limit < LOGIC_DT_MS) limit = LOGIC_DT_MS;
    }

    fall_acc_ms += LOGIC_DT_MS;
    if (fall_acc_ms >= limit) {
        fall_acc_ms = 0;
        if (!collide_piece(cur.id, cur.rot, cur.x, cur.y + 1)) {
            cur.y++;
        } else {
            lock_piece(&cur);
            if (cur.y < 0) { handle_death(); return; }

            clear_lines();

            if (grid_is_empty()) {
                score_add_100();
                fall_limit_ms = DROP_SPEED_NORMAL;
                lines_mod10 = 0;
            }
            spawn_piece();
        }
    }
}

static inline void draw_block4(int x0, int y0, uint8_t color)
{
    int y;
    for (y = 0; y < CELL_PX; y++) {
        int idx = (y0 + y) * VIEW_W + x0;
        viewbuf[idx + 0] = color;
        viewbuf[idx + 1] = color;
        viewbuf[idx + 2] = color;
        viewbuf[idx + 3] = color;
    }
}

static void render_grid(void)
{
    for (int gy = 0; gy < GRID_H; gy++) {
        int y0 = GAME_TOP_Y + gy * CELL_PX;
        for (int gx = 0; gx < GRID_W; gx++) {
            uint8_t v = grid[gy][gx];
            if (!v) continue;
            int x0 = gx * CELL_PX;
            draw_block4(x0, y0, color_piece((int)v - 1));
        }
    }
}

/* =========================
 * Renderizacao
 * ========================= */
static void render_play_screen(uint8_t pressed) {
    int xv, yv, i;
    int r, c, px, py;
    int offset_y_px, base_y_px, base_x_px, draw_y, draw_x;
    uint32_t limit;
    int dig;
    uint8_t color;
    for (i = 0; i < 6; i++) {
        dig = score_bcd[5 - i];
        draw_digit_tall(dig, 2 + (i * 4), 1, RGB(1,1,0));
    }
    dig = (lives > 9) ? 9 : lives;
    draw_digit_tall(dig, 56, 1, RGB(1,0,0));

    for(yv = 2; yv < 18; yv++) {
        for(xv = 32; xv < 48; xv++) {
            int r = (yv - 2) / CELL_PX;
            int c = (xv - 32) / CELL_PX;
            if(r < 4 && c < 4 && shape_filled(next_id, 0, r, c)) {
                view_put(xv, yv, RGB(1,0,1));
            }
        }
    }

    for(xv = 0; xv < VIEW_W; xv++) {
        view_put(xv, HUD_LINE_Y, RGB(0,0,1));
    }

    render_grid();

    piece_t ghost = cur;
    while (!collide_piece(ghost.id, ghost.rot, ghost.x, ghost.y + 1)) {
        ghost.y++;
    }
    if (ghost.y > cur.y) {
        base_y_px = GAME_TOP_Y + (ghost.y * CELL_PX);
        base_x_px = ghost.x * CELL_PX;
        for (r = 0; r < 4; r++) {
            for (c = 0; c < 4; c++) {
                if (shape_filled(ghost.id, ghost.rot, r, c)) {
                    for(py=0; py<CELL_PX; py++) {
                        for(px=0; px<CELL_PX; px++) {
                            draw_x = base_x_px + c*CELL_PX + px;
                            draw_y = base_y_px + r*CELL_PX + py;
                            if (draw_y < GAME_TOP_Y || draw_y >= VIEW_H) continue;
                            if ((draw_x + draw_y) % 2 == 0) {
                                view_put(draw_x, draw_y, COLOR_WHITE);
                            }
                        }
                    }
                }
            }
        }
    }

    offset_y_px = 0;
    limit = (pressed & K_DOWN) ? DROP_SPEED_FAST : fall_limit_ms;

    if (limit > 0) {
        if (!collide_piece(cur.id, cur.rot, cur.x, cur.y + 1)) {
            offset_y_px = (fall_acc_ms * CELL_PX) / limit;
            if (offset_y_px >= CELL_PX) offset_y_px = CELL_PX - 1;
        }
    }

    base_y_px = GAME_TOP_Y + (cur.y * CELL_PX) + offset_y_px;
    base_x_px = cur.x * CELL_PX;

    for (r = 0; r < 4; r++) {
        for (c = 0; c < 4; c++) {
            if (shape_filled(cur.id, cur.rot, r, c)) {
                color = color_piece(cur.id);
                for(py=0; py<CELL_PX; py++) {
                    for(px=0; px<CELL_PX; px++) {
                        draw_x = base_x_px + c*CELL_PX + px;
                        draw_y = base_y_px + r*CELL_PX + py;
                        if (draw_y < GAME_TOP_Y) continue;
                        if (draw_y >= VIEW_H) continue;
                        view_put(draw_x, draw_y, color);
                    }
                }
            }
        }
    }
}

static void draw_vhdl_char(int char_idx, int start_x, int start_y, uint8_t color) {
    int row, col;
    for (row = 0; row < 7; row++) {
        uint8_t line = FONT_VHDL[char_idx][row];
        for (col = 0; col < 5; col++) {
            if ((line >> (4 - col)) & 1) {
                int px = start_x + (col * 2);
                int py = start_y + (row * 2);
                view_put(px,   py,   color);
                view_put(px+1, py,   color);
                view_put(px,   py+1, color);
                view_put(px+1, py+1, color);
            }
        }
    }
}

static void set_display_brightness(uint8_t level)
{
    uint8_t corrected_pwm;
    corrected_pwm = GAMMA_TABLE[level];

    IOWR_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_BRIGHTNESS_OFFSET, corrected_pwm);
}

static int update_game_over_fade(void)
{
    fade_acc_ms += LOGIC_DT_MS;
    if (fade_acc_ms < FADE_STEP_MS) return 0;
    fade_acc_ms = 0;
    if (fade_brightness > FADE_STEP_VAL) fade_brightness -= FADE_STEP_VAL;
    else fade_brightness = 0;

    set_display_brightness(fade_brightness);

    if (fade_brightness == 0) {
        memset(grid, 0, sizeof(grid));
        current_state = STATE_GAME_OVER;
        return 1;
    }

    return 0;
}

static void tetris_run(void)
{
    uint32_t last_logic  = ms_global;
    uint32_t last_render = ms_global;
    uint32_t acc_logic   = 0;

    uint8_t held_snapshot;
    uint8_t pressed_snapshot;

    set_display_brightness(DEFAULT_BRIGHTNESS);

    init_panel_lut_small();
    reset_full_game();
    spawn_piece();

    static uint8_t game_over_armed = 0;

    while (1)
    {
        usleep(MS_TICK_US);
        ms_global++;
        lfsr_step();
        handle_input_merge();

        if (launcher_exit_req) {
            launcher_soft_reset();
        }

        const uint32_t MAX_DT_MS = 40;
        const uint32_t MAX_STEPS = 8;

        uint32_t dt = ms_global - last_logic;
        if (dt > 0)
        {
            last_logic = ms_global;
            if (dt > MAX_DT_MS) dt = MAX_DT_MS;
            acc_logic += dt;

            uint32_t steps = 0;
            while (acc_logic >= LOGIC_DT_MS && steps < MAX_STEPS)
            {
                acc_logic -= LOGIC_DT_MS;
                steps++;

                held_snapshot    = kb_held;
                pressed_snapshot = kb_pressed;
                kb_pressed = 0;

                switch (current_state)
                {
                    case STATE_PLAYING:
                        update_play_state_gold(held_snapshot, pressed_snapshot);
                        prev_any_key = (held_snapshot != 0);
                        break;

                    case STATE_LOST_LIFE:
                        if (prev_any_key && held_snapshot == 0) prev_any_key = 0;
                        if (!prev_any_key && held_snapshot != 0) {
                            reset_board_only();
                            spawn_piece();
                            current_state = STATE_PLAYING;
                        }
                        if (held_snapshot != 0) prev_any_key = 1;
                        break;

                    case STATE_GAME_OVER_FADE:
                        if (update_game_over_fade()) {
                            prev_any_key = 1;
                        }
                        break;

                    case STATE_GAME_OVER:
                        if (prev_any_key && held_snapshot == 0) prev_any_key = 0;

                        if (!prev_any_key && held_snapshot != 0) {
                            reset_full_game();
                            spawn_piece();
                            current_state = STATE_PLAYING;
                            game_over_armed = 0;
                        }
                        if (held_snapshot != 0) prev_any_key = 1;
                        break;
                }
            }

            if (steps == MAX_STEPS)
                acc_logic = 0;
        }

        if ((ms_global - last_render) >= RENDER_DT_MS)
        {
            last_render = ms_global;

            if (current_state == STATE_GAME_OVER_FADE)
                continue;

            view_clear();

            if (current_state == STATE_PLAYING)
            {
                render_play_screen(kb_held);
            }
            else if (current_state == STATE_LOST_LIFE)
            {
                render_play_screen(kb_held);
                for (int y = 40; y < 80; y++)
                    for (int x = 10; x < 54; x++)
                        if (y == x + 30 || y == 80 - (x - 10))
                            view_put(x, y, RGB(1,0,0));
            }
            else if (current_state == STATE_GAME_OVER)
            {
                int game_ids[] = {0, 1, 2, 3};
                for (int i = 0; i < 4; i++)
                    draw_vhdl_char(game_ids[i], 8 + (i * 12), 45, COLOR_WHITE);

                int over_ids[] = {4, 5, 3, 6};
                for (int i = 0; i < 4; i++)
                    draw_vhdl_char(over_ids[i], 8 + (i * 12), 65, COLOR_WHITE);

                if (!game_over_armed) {
                    wait_frame();
                    #if USE_STREAM_BLIT
                    blit_stream_diff();
                    #else
                    blit_diff_to_panel_small_lut();
                    #endif
                    set_display_brightness(DEFAULT_BRIGHTNESS);
                    game_over_armed = 1;
                }
            }
            wait_frame();
            #if USE_STREAM_BLIT
            blit_stream_diff();
            #else
            blit_diff_to_panel_small_lut();
            #endif
        }
    }
}

void tetris_module_entry(void)
{
    tetris_run();
}

const launcher_builtin_module_t tetris_module = {
    "builtin:tetris",
    "TETRIS MOD",
    tetris_module_entry
};
