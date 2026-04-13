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
#include "snake_module.h"

/* =========================
 * Display setup
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
 * Timing
 * ========================= */
#define MS_TICK_US      1000
#define LOGIC_DT_MS     3
#define RENDER_DT_MS    8 // 125 Hz, set to 16 for ~60 Hz

/* =========================
 * Snake speed
 * ========================= */
#define SNAKE_MOVE_START_MS 45  // — velocidade inicial (menor = mais rápido).
#define SNAKE_MOVE_MIN_MS   15  // — velocidade máxima (limite mínimo).
#define FOODS_PER_SPEEDUP   2   // — quantas comidas para acelerar.
#define SPEEDUP_MS          4   // — quanto reduz a cada aceleração.
#define SNAKE_MOVE_FAST_MS  35  // velocidade base quando começa a segurar.
#define SNAKE_HOLD_ACCEL_MS 100 // intervalo (ms) para cada aumento de velocidade enquanto
#define SNAKE_HOLD_STEP_MS  3   // quanto (ms) reduz a cada degrau.
#define NUM_LIVES           3

/* =========================
 * Colors
 * ========================= */
#define RGB(r,g,b)  (uint8_t)(((r)?1:0) | ((g)?2:0) | ((b)?4:0))
#define COLOR_BLACK 0
#define COLOR_WHITE 7

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
static uint8_t prevbuf[VIEW_W * VIEW_H];
static uint16_t base_addr_xv[VIEW_W];

typedef enum { STATE_PLAYING, STATE_GAME_OVER_FADE, STATE_GAME_OVER } game_state_t;
static game_state_t current_state = STATE_PLAYING;

typedef enum { DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT } dir_t;

typedef struct { uint8_t x; uint8_t y; } point_t;

#define SNAKE_MAX_LEN (GRID_W * GRID_H)

static point_t snake[SNAKE_MAX_LEN];
static point_t prev_snake[SNAKE_MAX_LEN];
static uint16_t snake_len = 0;
static uint16_t prev_len = 0;
static uint16_t snake_head_idx = 0;
static uint8_t snake_occ[GRID_H][GRID_W];
static point_t food;

static dir_t cur_dir = DIR_RIGHT;

static uint16_t lfsr = 0xA5A5;

static uint32_t ms_global = 0;
static uint32_t move_acc_ms = 0;
static uint32_t move_limit_ms = SNAKE_MOVE_START_MS;
static uint32_t render_move_limit_ms = SNAKE_MOVE_START_MS;
static uint32_t hold_acc_ms = 0;

static uint8_t score_bcd[6] = {0,0,0,0,0,0};
static uint8_t foods_mod = 0;
static uint8_t lives = NUM_LIVES;
static uint8_t input_locked = 0;
static uint8_t game_started = 0;

static uint8_t prev_any_key = 0;

/* ========================= 
 * Sincronismo de vídeo
 * ========================= */
#define LED_STATUS_ADDR 8193
#define LED_FRAME_DONE  0x01

static inline void wait_frame(void) {
    while ((IORD_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_STATUS_ADDR) & LED_FRAME_DONE) == 0)
        ;
    IOWR_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_STATUS_ADDR, LED_FRAME_DONE); // clear
}

/* =========================
 * Brightness control
 * ========================= */
#define LED_BRIGHTNESS_OFFSET 8192
#define DEFAULT_BRIGHTNESS 100
#define PS2_BASE PS2_INTERFACE_0_BASE

static uint8_t fade_brightness = DEFAULT_BRIGHTNESS;
static uint32_t fade_acc_ms = 0;

#define FADE_STEP_MS    5
#define FADE_STEP_VAL   1

/* =========================
 * Input
 * ========================= */
#define PS2_VALID_MASK    (1u << 31)
#define PS2_RELEASED_MASK (1u << 30)
#define PS2_EXTENDED_MASK (1u << 29)
#define PS2_CODE_MASK     0xFF

#define K_RIGHT   0x01
#define K_DOWN    0x02
#define K_ROT     0x04
#define K_LEFT    0x08
#define K_SPACE   0x10
#define UART_RX_BUDGET_PER_TICK 16

static volatile uint8_t kb_held     = 0;
static volatile uint8_t kb_pressed  = 0;
static volatile uint8_t ps2_held    = 0;
static volatile uint8_t uart_held   = 0;
static volatile uint8_t launcher_exit_req = 0;

#define FLASHBOOT_START_ADDR 0x0180C040u

/* =========================
 * Helpers
 * ========================= */
static void set_display_brightness(uint8_t level);

static void lfsr_step(void) {
    uint16_t bit = (uint16_t)(((lfsr >> 15) ^ (lfsr >> 13)) & 1);
    lfsr = (uint16_t)((lfsr << 1) | bit);
}

static void score_add_1(void)
{
    int i = 0;
    score_bcd[i]++;
    while (i < 6 && score_bcd[i] >= 10) {
        score_bcd[i] = 0;
        i++;
        if (i < 6) score_bcd[i]++;
    }
}

static int font_get_bit(const uint16_t *font_table, int glyph_idx, int x, int y) {
    if (x < 0 || x > 2 || y < 0 || y > 4) return 0;
    uint16_t v = font_table[glyph_idx];
    int bit_idx = 14 - (y * 3 + x);
    return (v >> bit_idx) & 1;
}

static void draw_digit_tall(int digit, int start_x, int start_y, uint8_t color) {
    int gx, gy;
    for (gy = 0; gy < 5; gy++) {
        for (gx = 0; gx < 3; gx++) {
            if (font_get_bit(FONT_NUM, digit, gx, gy)) {
                viewbuf[(start_y + (gy * 2)) * VIEW_W + (start_x + gx)] = color;
                viewbuf[(start_y + (gy * 2) + 1) * VIEW_W + (start_x + gx)] = color;
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
                viewbuf[py * VIEW_W + px] = color;
                viewbuf[py * VIEW_W + (px + 1)] = color;
                viewbuf[(py + 1) * VIEW_W + px] = color;
                viewbuf[(py + 1) * VIEW_W + (px + 1)] = color;
            }
        }
    }
}

static void view_clear(void) {
    memset(viewbuf, 0, sizeof(viewbuf));
}

static void view_put(int xv, int yv, uint8_t rgb) {
    if ((unsigned)xv >= VIEW_W || (unsigned)yv >= VIEW_H) return;
    viewbuf[yv * VIEW_W + xv] = (uint8_t)(rgb & 7);
}

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

static void set_display_brightness(uint8_t level)
{
    uint8_t corrected_pwm = GAMMA_TABLE[level];
    IOWR_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_BRIGHTNESS_OFFSET, corrected_pwm);
}

/* =========================
 * Input handling
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
    uint32_t budget = UART_RX_BUDGET_PER_TICK;

    while (budget-- > 0) {
        uint32_t status = IORD_ALTERA_AVALON_UART_STATUS(UART_0_BASE);

        if (status & (ALTERA_AVALON_UART_STATUS_ROE_MSK |
                      ALTERA_AVALON_UART_STATUS_TOE_MSK |
                      ALTERA_AVALON_UART_STATUS_FE_MSK  |
                      ALTERA_AVALON_UART_STATUS_PE_MSK  |
                      ALTERA_AVALON_UART_STATUS_BRK_MSK)) {
            IOWR_ALTERA_AVALON_UART_STATUS(UART_0_BASE, 0);
            uart_held = 0;
            if ((status & ALTERA_AVALON_UART_STATUS_RRDY_MSK) == 0) {
                continue;
            }
        }

        if ((status & ALTERA_AVALON_UART_STATUS_RRDY_MSK) == 0) break;

        uint8_t rx = (uint8_t)IORD_ALTERA_AVALON_UART_RXDATA(UART_0_BASE);

        switch (rx) {
            case 'L':
                uart_held |= K_LEFT;
                kb_pressed |= K_LEFT;
                break;
            case 'l':
                uart_held &= (uint8_t)~K_LEFT;
                break;
            case 'R':
                uart_held |= K_RIGHT;
                kb_pressed |= K_RIGHT;
                break;
            case 'r':
                uart_held &= (uint8_t)~K_RIGHT;
                break;
            case 'U':
                uart_held |= K_ROT;
                kb_pressed |= K_ROT;
                break;
            case 'u':
                uart_held &= (uint8_t)~K_ROT;
                break;
            case 'D':
                uart_held |= K_DOWN;
                kb_pressed |= K_DOWN;
                break;
            case 'd':
                uart_held &= (uint8_t)~K_DOWN;
                break;
            default:
                break;
        }
    }

    {
        uint32_t status = IORD_ALTERA_AVALON_UART_STATUS(UART_0_BASE);
        if (status & (ALTERA_AVALON_UART_STATUS_ROE_MSK |
                      ALTERA_AVALON_UART_STATUS_TOE_MSK |
                      ALTERA_AVALON_UART_STATUS_FE_MSK  |
                      ALTERA_AVALON_UART_STATUS_PE_MSK  |
                      ALTERA_AVALON_UART_STATUS_BRK_MSK)) {
            IOWR_ALTERA_AVALON_UART_STATUS(UART_0_BASE, 0);
            uart_held = 0;
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

/* =========================
 * Snake logic
 * ========================= */
static point_t snake_get(int idx_from_head)
{
    uint16_t idx = (uint16_t)((snake_head_idx + SNAKE_MAX_LEN - idx_from_head) % SNAKE_MAX_LEN);
    return snake[idx];
}

static void snapshot_snake(void)
{
    prev_len = snake_len;
    for (uint16_t i = 0; i < snake_len; i++) {
        prev_snake[i] = snake_get(i);
    }
}

static int is_opposite(dir_t a, dir_t b)
{
    return ((a == DIR_UP && b == DIR_DOWN) ||
            (a == DIR_DOWN && b == DIR_UP) ||
            (a == DIR_LEFT && b == DIR_RIGHT) ||
            (a == DIR_RIGHT && b == DIR_LEFT));
}

static void spawn_food(void)
{
    if (snake_len >= SNAKE_MAX_LEN) return;

    for (int tries = 0; tries < 256; tries++) {
        lfsr_step();
        uint8_t x = (uint8_t)(lfsr % GRID_W);
        lfsr_step();
        uint8_t y = (uint8_t)(lfsr % GRID_H);
        if (snake_occ[y][x] == 0) {
            food.x = x;
            food.y = y;
            return;
        }
    }

    for (uint8_t y = 0; y < GRID_H; y++) {
        for (uint8_t x = 0; x < GRID_W; x++) {
            if (snake_occ[y][x] == 0) {
                food.x = x;
                food.y = y;
                return;
            }
        }
    }
}

static void orient_snake(dir_t dir)
{
    memset(snake_occ, 0, sizeof(snake_occ));
    cur_dir = dir;

    uint8_t head_x = GRID_W / 2;
    uint8_t head_y = GRID_H / 2;
    int dx = 0;
    int dy = 0;

    if (dir == DIR_UP) dy = 1;
    else if (dir == DIR_DOWN) dy = -1;
    else if (dir == DIR_LEFT) dx = 1;
    else dx = -1;

    for (uint16_t i = 0; i < snake_len; i++) {
        int offset = (int)(snake_len - 1 - i);
        point_t p = {(uint8_t)(head_x + dx * offset), (uint8_t)(head_y + dy * offset)};
        snake[i] = p;
        snake_occ[p.y][p.x] = 1;
    }
    snake_head_idx = (uint16_t)(snake_len - 1);

    spawn_food();
    snapshot_snake();
}

static void reset_snake_only(void)
{
    snake_len = 3;
    cur_dir = DIR_RIGHT;
    input_locked = 1;
    game_started = 0;

    move_acc_ms = 0;
    move_limit_ms = SNAKE_MOVE_START_MS;
    hold_acc_ms = 0;
    foods_mod = 0;

    orient_snake(cur_dir);
}

static void reset_full_game(void)
{
    memset(score_bcd, 0, sizeof(score_bcd));
    lives = NUM_LIVES;
    reset_snake_only();
}

static void enter_game_over(void)
{
    current_state = STATE_GAME_OVER_FADE;
    fade_brightness = DEFAULT_BRIGHTNESS;
    fade_acc_ms = 0;
    prev_any_key = 1;
    set_display_brightness(fade_brightness);
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
        current_state = STATE_GAME_OVER;
        return 1;
    }

    return 0;
}

static void update_play_state(uint8_t pressed_edge, uint8_t held)
{
    if (input_locked) {
        if (held == 0) input_locked = 0;
        pressed_edge = 0;
        held = 0;
    }

    if (!game_started) {
        if (pressed_edge & (K_ROT | K_DOWN | K_LEFT | K_RIGHT)) {
            if (pressed_edge & K_ROT) cur_dir = DIR_UP;
            else if (pressed_edge & K_DOWN) cur_dir = DIR_DOWN;
            else if (pressed_edge & K_LEFT) cur_dir = DIR_LEFT;
            else if (pressed_edge & K_RIGHT) cur_dir = DIR_RIGHT;
            game_started = 1;
            input_locked = 0;
            move_acc_ms = 0;
            hold_acc_ms = 0;
            orient_snake(cur_dir);
        } else {
            return;
        }
    }

    if (pressed_edge & K_ROT) {
        if (!is_opposite(cur_dir, DIR_UP)) cur_dir = DIR_UP;
    } else if (pressed_edge & K_DOWN) {
        if (!is_opposite(cur_dir, DIR_DOWN)) cur_dir = DIR_DOWN;
    } else if (pressed_edge & K_LEFT) {
        if (!is_opposite(cur_dir, DIR_LEFT)) cur_dir = DIR_LEFT;
    } else if (pressed_edge & K_RIGHT) {
        if (!is_opposite(cur_dir, DIR_RIGHT)) cur_dir = DIR_RIGHT;
    }

    uint32_t limit = move_limit_ms;
    uint8_t holding_same = 0;
    if ((held & K_ROT) && cur_dir == DIR_UP) holding_same = 1;
    else if ((held & K_DOWN) && cur_dir == DIR_DOWN) holding_same = 1;
    else if ((held & K_LEFT) && cur_dir == DIR_LEFT) holding_same = 1;
    else if ((held & K_RIGHT) && cur_dir == DIR_RIGHT) holding_same = 1;

    if (holding_same) {
        hold_acc_ms += LOGIC_DT_MS;
        limit = SNAKE_MOVE_FAST_MS;
        if (hold_acc_ms >= SNAKE_HOLD_ACCEL_MS) {
            uint32_t steps = hold_acc_ms / SNAKE_HOLD_ACCEL_MS;
            uint32_t dec = steps * SNAKE_HOLD_STEP_MS;
            if (limit > dec) limit -= dec;
            else limit = 1;
        }
    } else {
        hold_acc_ms = 0;
    }

    if (limit < SNAKE_MOVE_MIN_MS) limit = SNAKE_MOVE_MIN_MS;

    render_move_limit_ms = limit;
    move_acc_ms += LOGIC_DT_MS;
    if (move_acc_ms < limit) return;
    move_acc_ms -= limit;

    snapshot_snake();

    point_t head = snake_get(0);
    int nx = head.x;
    int ny = head.y;

    if (cur_dir == DIR_UP) ny -= 1;
    else if (cur_dir == DIR_DOWN) ny += 1;
    else if (cur_dir == DIR_LEFT) nx -= 1;
    else if (cur_dir == DIR_RIGHT) nx += 1;

    if (nx < 0 || nx >= GRID_W || ny < 0 || ny >= GRID_H) {
        if (lives > 1) {
            lives--;
            reset_snake_only();
        } else {
            lives = 0;
            enter_game_over();
        }
        return;
    }

    int grow = (nx == food.x && ny == food.y);
    point_t tail = snake_get((int)(snake_len - 1));

    if (snake_occ[ny][nx]) {
        if (!( !grow && tail.x == nx && tail.y == ny)) {
            if (lives > 1) {
                lives--;
                reset_snake_only();
            } else {
                lives = 0;
                enter_game_over();
            }
            return;
        }
    }

    if (!grow) {
        snake_occ[tail.y][tail.x] = 0;
    }

    snake_head_idx = (uint16_t)((snake_head_idx + 1) % SNAKE_MAX_LEN);
    snake[snake_head_idx].x = (uint8_t)nx;
    snake[snake_head_idx].y = (uint8_t)ny;
    snake_occ[ny][nx] = 1;

    if (grow) {
        if (snake_len < SNAKE_MAX_LEN) snake_len++;
        score_add_1();
        foods_mod++;
        if (foods_mod >= FOODS_PER_SPEEDUP) {
            foods_mod = 0;
            if (move_limit_ms > (SNAKE_MOVE_MIN_MS + SPEEDUP_MS))
                move_limit_ms -= SPEEDUP_MS;
            else
                move_limit_ms = SNAKE_MOVE_MIN_MS;
        }
        spawn_food();
    }
}

/* =========================
 * Rendering
 * ========================= */
static inline void draw_block4(int x0, int y0, uint8_t color)
{
    for (int y = 0; y < CELL_PX; y++) {
        int idx = (y0 + y) * VIEW_W + x0;
        viewbuf[idx + 0] = color;
        viewbuf[idx + 1] = color;
        viewbuf[idx + 2] = color;
        viewbuf[idx + 3] = color;
    }
}

static void render_play_screen(void)
{
    for (int i = 0; i < 6; i++) {
        int dig = score_bcd[5 - i];
        draw_digit_tall(dig, 2 + (i * 4), 1, RGB(1,1,0));
    }
    draw_digit_tall((lives > 9) ? 9 : lives, 56, 1, RGB(1,0,0));

    for (int xv = 0; xv < VIEW_W; xv++) {
        view_put(xv, HUD_LINE_Y, RGB(0,0,1));
    }
    int border_top = GAME_TOP_Y - 1;
    int border_bottom = GAME_TOP_Y + (GRID_H * CELL_PX) - 1;
    if (border_top >= 0 && border_top < VIEW_H) {
        for (int xv = 0; xv < VIEW_W; xv++) view_put(xv, border_top, RGB(0,0,1));
    }
    if (border_bottom >= 0 && border_bottom < VIEW_H) {
        for (int xv = 0; xv < VIEW_W; xv++) view_put(xv, border_bottom, RGB(0,0,1));
    }
    for (int yv = border_top; yv <= border_bottom; yv++) {
        if (yv < 0 || yv >= VIEW_H) continue;
        view_put(0, yv, RGB(0,0,1));
        view_put(VIEW_W - 1, yv, RGB(0,0,1));
    }

    int food_x = food.x * CELL_PX;
    int food_y = GAME_TOP_Y + food.y * CELL_PX;
    draw_block4(food_x, food_y, RGB(1,0,0));

    uint16_t interp = 0;
    if (render_move_limit_ms > 0) {
        interp = (uint16_t)((move_acc_ms * 256u) / render_move_limit_ms);
        if (interp > 255) interp = 255;
    }

    for (uint16_t i = 0; i < snake_len; i++) {
        point_t cur = snake_get(i);
        point_t prev = (i < prev_len) ? prev_snake[i] : cur;
        int dx = (int)cur.x - (int)prev.x;
        int dy = (int)cur.y - (int)prev.y;
        int x0 = (int)prev.x * CELL_PX + (dx * CELL_PX * interp) / 256;
        int y0 = (int)prev.y * CELL_PX + (dy * CELL_PX * interp) / 256;
        uint8_t color = (i == 0) ? RGB(0,1,1) : RGB(0,1,0);
        draw_block4(x0, GAME_TOP_Y + y0, color);
    }
}

static void snake_run(void)
{
    uint32_t last_logic  = ms_global;
    uint32_t last_render = ms_global;
    uint32_t acc_logic   = 0;

    set_display_brightness(DEFAULT_BRIGHTNESS);

    init_panel_lut_small();
    reset_full_game();

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

                uint8_t pressed_snapshot = kb_pressed;
                kb_pressed = 0;

                switch (current_state)
                {
                    case STATE_PLAYING:
                        update_play_state(pressed_snapshot, kb_held);
                        prev_any_key = (kb_held != 0);
                        break;

                    case STATE_GAME_OVER_FADE:
                        if (update_game_over_fade()) {
                            prev_any_key = 1;
                        }
                        break;

                    case STATE_GAME_OVER:
                        if (prev_any_key && kb_held == 0) prev_any_key = 0;
                        if (!prev_any_key && kb_held != 0) {
                            reset_full_game();
                            current_state = STATE_PLAYING;
                            game_over_armed = 0;
                        }
                        if (kb_held != 0) prev_any_key = 1;
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
                render_play_screen();
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
                    blit_diff_to_panel_small_lut();
                    set_display_brightness(DEFAULT_BRIGHTNESS);
                    game_over_armed = 1;
                }
            }

            wait_frame();
            blit_diff_to_panel_small_lut();
        }
    }
}

void snake_module_entry(void)
{
    snake_run();
}

const launcher_builtin_module_t snake_module = {
    "builtin:snake",
    "SNAKE MOD",
    snake_module_entry
};
