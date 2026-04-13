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
#include "sprites_pacman.h"
#include "pacman_module.h"

/* main.c version: 1.4.5 */

/* =========================
 * Display setup
 * ========================= */
#define PHY_W 128        // panel width
#define PHY_H 64         // panel height
#define VIEW_W 64        // virtual width
#define VIEW_H 128       // virtual height

/* =========================
 * Pac-Man maze (2px walls + 8px paths)
 * ========================= */
#define CELLS_W 6
#define CELLS_H 12
#define CELL_PX 8
#define WALL_PX 2
#define CELL_STEP (CELL_PX + WALL_PX)
#define MAZE_GRID_W (CELLS_W * CELL_STEP + WALL_PX)
#define MAZE_GRID_H (CELLS_H * CELL_STEP + WALL_PX)
#define MAZE_ORIGIN_X 1
#define MAZE_ORIGIN_Y 3
#define MAZE_BORDER_TOP_PX 3
#define MAZE_BORDER_BOTTOM_PX 3
#define MAZE_BORDER_SIDE_PX 5
#define MAZE_SPRITE_X_SHIFT 0
#define MAZE_SPRITE_Y_SHIFT 0
#define USE_MANUAL_TUNNEL 0
#define USE_AUTO_TUNNEL 1
#define TUNNEL_MARK_ENABLE 1
#define TUNNEL_MARK_PX 2
#define TUNNEL_LAND_Y_MIN 32
#define TUNNEL_LAND_Y_MAX 41
#define LAND_Y_TO_VIEW_X(y) (63 - (y))
#define TUNNEL_X_MIN LAND_Y_TO_VIEW_X(TUNNEL_LAND_Y_MAX)
#define TUNNEL_X_MAX LAND_Y_TO_VIEW_X(TUNNEL_LAND_Y_MIN)

#define PELLET_VIEW_SHIFT_X 0
#define PELLET_VIEW_SHIFT_Y 0
#define POWER_VIEW_SHIFT_X 0
#define POWER_VIEW_SHIFT_Y 0

#define MAZE_SPRITE_ROW_SHIFT 0
#define MAZE_SPRITE_COL_SHIFT 0
#define MAZE_COUNT 3

/* =========================
 * Timing
 * ========================= */
#define MS_TICK_US      1000
#define LOGIC_DT_MS     3
#define RENDER_DT_MS    8

/* =========================
 * Pac-Man tuning
 * ========================= */
#define PAC_STEP_MS      11 // tempo por passo de 1 px (acumulador + interpolacao estilo Tetris)
#define PAC_STEP_MS_TURBO 6
#define PAC_TURN_WINDOW   3
#define GHOST_STEP_MS_BASE    11 // base de velocidade dos fantasmas (ms por passo)
#define GHOST_STEP_MS_FR_BASE 15 // base da velocidade quando assustado
#define GHOST_STEP_DEC_MS      1 // decremento por fase (acelera os fantasmas)
#define GHOST_STEP_MIN_MS      5 // limite minimo da velocidade dos fantasmas
#define GHOST_STEP_FR_MIN_MS   7 // limite minimo quando assustado
#define GHOST_SCATTER_1_MS 7000
#define GHOST_CHASE_1_MS   20000
#define GHOST_SCATTER_2_MS 7000
#define GHOST_CHASE_2_MS   20000
#define GHOST_SCATTER_3_MS 5000
#define GHOST_CHASE_3_MS   20000
#define GHOST_SCATTER_4_MS 5000

#define POWER_TIME_MS   6000
#define POWER_BLINK_START_MS 1800
#define POWER_BLINK_PERIOD_MS 120
#define GHOST_COUNT       4
#define PAC_LIVES         3
#define PAC_ANIM_MS     100
#define COLLISION_INSET   3
#define POWER_PELLET_MIN_BITS 1 // minimo de pixels ligados na celula para virar pilula de energia
#define LEVEL_CLEAR_BLINKS 3
#define LEVEL_CLEAR_BLINK_MS 100
#define INTRO_SHOW_MS 1000
#define DEATH_ANIM_MS 480
#define DEATH_STAGE_MS 60
#define BOMB_STEP_MS 30
#define BOMB_PULSE_PHASES 18
#define BOMB_EXPLODE_PHASES 4
#define BOMB_TOTAL_MS ((BOMB_PULSE_PHASES + BOMB_EXPLODE_PHASES) * BOMB_STEP_MS)
#define GAME_OVER_X 10
#define GAME_OVER_Y 10
#define GAME_OVER_STEP 12
#define SCORE_DIGITS 5
#define SCORE_DIGIT_W 3
#define SCORE_DIGIT_H 7
#define SCORE_DIGIT_GAP 1
#define SCORE_X 3
#define SCORE_Y 3
#define SCORE_CLEAR_W (SCORE_DIGITS * SCORE_DIGIT_W + (SCORE_DIGITS - 1) * SCORE_DIGIT_GAP)
#define SCORE_CLEAR_H SCORE_DIGIT_H
static const int POWER_PELLET_COORDS[4][2] = {
    {9, 17},
    {119, 7},
    {9, 57},
    {119, 57}
};
#define PAC_START_CX      2
#define PAC_START_CY     10
#define GHOST_START_CX    2
#define GHOST_START_CY    5

/* =========================
 * Colors
 * ========================= */
#define RGB(r,g,b)  (uint8_t)(((r)?1:0) | ((g)?2:0) | ((b)?4:0))
#define COLOR_BLACK 0
#define COLOR_WHITE 7
#define COLOR_LIVES RGB(0,0,1)
#define COLOR_WALL  RGB(0,1,0)
#define DEBUG_MARKERS 0
#define DEBUG_TUNNEL 0
#define COLOR_PAC   RGB(1,1,0)
#define COLOR_FRIGHT RGB(0,0,1)

/* =========================
 * Fonts
 * ========================= */
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

// 3x5 digits (3 wide, 5 high). Bits use the low 3 bits.
static const uint8_t DIGITS_3X7[10][7] = {
    {0x07, 0x05, 0x05, 0x05, 0x05, 0x05, 0x07}, // 0
    {0x02, 0x06, 0x02, 0x02, 0x02, 0x02, 0x07}, // 1
    {0x07, 0x01, 0x01, 0x07, 0x04, 0x04, 0x07}, // 2
    {0x07, 0x01, 0x01, 0x07, 0x01, 0x01, 0x07}, // 3
    {0x05, 0x05, 0x05, 0x07, 0x01, 0x01, 0x01}, // 4
    {0x07, 0x04, 0x04, 0x07, 0x01, 0x01, 0x07}, // 5
    {0x07, 0x04, 0x04, 0x07, 0x05, 0x05, 0x07}, // 6
    {0x07, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02}, // 7
    {0x07, 0x05, 0x05, 0x07, 0x05, 0x05, 0x07}, // 8
    {0x07, 0x05, 0x05, 0x07, 0x01, 0x01, 0x07}  // 9
};

static uint8_t viewbuf[VIEW_W * VIEW_H];
static uint8_t prevbuf[VIEW_W * VIEW_H];
static uint8_t basebuf[VIEW_W * VIEW_H];
static uint16_t base_addr_xv[VIEW_W];

typedef enum { STATE_INTRO, STATE_PLAYING, STATE_DYING, STATE_LEVEL_CLEAR, STATE_GAME_OVER_FADE, STATE_GAME_OVER } game_state_t;
static game_state_t current_state = STATE_PLAYING;

typedef enum { DIR_NONE, DIR_LEFT, DIR_RIGHT, DIR_UP, DIR_DOWN } dir_t;

typedef struct {
    int x;
    int y;
    int prev_x;
    int prev_y;
    dir_t dir;
    dir_t desired;
    uint32_t acc_ms;
} pac_t;

typedef struct {
    int x;
    int y;
    int prev_x;
    int prev_y;
    dir_t dir;
    uint32_t acc_ms;
    uint8_t fright;
    uint8_t color;
} ghost_t;

static pac_t pac;
static ghost_t ghosts[GHOST_COUNT];
static int maze_index = 0;
static const uint32_t (*maze_list[MAZE_COUNT])[4] = {
    PACMAN_MAZE_1,
    PACMAN_MAZE_2,
    PACMAN_MAZE_3
};
static const uint32_t (*maze_active)[4] = PACMAN_MAZE_1;
static uint32_t ghost_step_ms = GHOST_STEP_MS_BASE;
static uint32_t ghost_step_ms_fr = GHOST_STEP_MS_FR_BASE;
static const uint8_t wall_colors[MAZE_COUNT] = {
    RGB(0,1,0),
    RGB(0,1,1),
    RGB(0,0,1)
};
static uint8_t wall_color = COLOR_WALL;

static uint8_t cell_open[CELLS_H][CELLS_W];
static uint8_t cell_pellet[CELLS_H][CELLS_W];
static uint8_t maze_blocked[VIEW_H][VIEW_W];
static int16_t maze_region[VIEW_H][VIEW_W];
static uint8_t region_tunnel[VIEW_W * VIEW_H];
static uint8_t tunnel_x_allowed[VIEW_W];

static uint32_t score = 0;
static uint8_t lives = PAC_LIVES;
static uint32_t power_timer_ms = 0;
static uint32_t pellets_left = 0;

static uint16_t lfsr = 0xA5A5;
static uint32_t ms_global = 0;

static uint8_t input_locked = 0;
static uint8_t game_started = 0;
static uint8_t prev_any_key = 0;
static uint8_t base_ready = 0;
static uint8_t draw_positions_valid = 0;
static int pac_draw_x_prev = 0;
static int pac_draw_y_prev = 0;
static int ghost_draw_x_prev[GHOST_COUNT];
static int ghost_draw_y_prev[GHOST_COUNT];
static uint8_t score_visible = 1;
static uint8_t level_clear_toggles = 0;
static uint32_t level_clear_acc_ms = 0;
static uint32_t intro_acc_ms = 0;
static uint32_t death_acc_ms = 0;
static uint8_t death_last_life = 0;
static int death_x = 0;
static int death_y = 0;
static dir_t death_dir = DIR_LEFT;
static uint32_t ghost_mode_timer_ms = 0;
static uint8_t bomb_active = 0;
static uint32_t bomb_acc_ms = 0;
static int bomb_ghost_x[GHOST_COUNT];
static int bomb_ghost_y[GHOST_COUNT];
static uint8_t bomb_ghost_color[GHOST_COUNT];

/* =========================
 * Video sync
 * ========================= */
#define LED_STATUS_ADDR 8193
#define LED_FRAME_DONE  0x01
#define PANEL_X_OFFSET 0
#define PANEL_Y_OFFSET 0

static inline void wait_frame(void) {
    while ((IORD_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_STATUS_ADDR) & LED_FRAME_DONE) == 0)
        ;
    IOWR_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_STATUS_ADDR, LED_FRAME_DONE);
}

/* =========================
 * Brightness
 * ========================= */
#define LED_BRIGHTNESS_OFFSET 8192
#define DEFAULT_BRIGHTNESS 100
#define PS2_BASE PS2_INTERFACE_0_BASE

static uint8_t fade_brightness = DEFAULT_BRIGHTNESS;
static uint32_t fade_acc_ms = 0;

#define FADE_STEP_MS    5
#define FADE_STEP_VAL   1

/* =========================
 * Input (Tetris style)
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
#define K_TURBO   0x20
#define K_NEXT_MAZE 0x40
#define K_BOMB    0x80
#define UART_RX_BUDGET_PER_TICK 16

static volatile uint8_t kb_held     = 0;
static volatile uint8_t kb_pressed  = 0;
static volatile uint8_t ps2_held    = 0;
static volatile uint8_t uart_held   = 0;
static volatile uint8_t launcher_exit_req = 0;
static uint8_t pac_turbo_enabled = 0;

#define FLASHBOOT_START_ADDR 0x0180C040u

static void set_display_brightness(uint8_t level);

static uint32_t pac_step_limit_ms(void) {
    return pac_turbo_enabled ? PAC_STEP_MS_TURBO : PAC_STEP_MS;
}

static void trigger_bomb_effect(void)
{
    for (int i = 0; i < GHOST_COUNT; i++) {
        bomb_ghost_x[i] = ghosts[i].x;
        bomb_ghost_y[i] = ghosts[i].y;
        bomb_ghost_color[i] = ghosts[i].fright ? COLOR_FRIGHT : ghosts[i].color;
    }
    bomb_active = 1;
    bomb_acc_ms = 0;
    draw_positions_valid = 0;
    base_ready = 0;
}

static void lfsr_step(void) {
    uint16_t bit = (uint16_t)(((lfsr >> 15) ^ (lfsr >> 13)) & 1);
    lfsr = (uint16_t)((lfsr << 1) | bit);
}

static void view_put(int xv, int yv, uint8_t rgb) {
    if ((unsigned)xv >= VIEW_W || (unsigned)yv >= VIEW_H) return;
    viewbuf[yv * VIEW_W + xv] = (uint8_t)(rgb & 7);
}

static void base_put(int xv, int yv, uint8_t rgb) {
    if ((unsigned)xv >= VIEW_W || (unsigned)yv >= VIEW_H) return;
    basebuf[yv * VIEW_W + xv] = (uint8_t)(rgb & 7);
}


static void draw_digit_3x7_phys(int digit, int start_px, int start_py, uint8_t color)
{
    if (digit < 0 || digit > 9) return;
    for (int y = 0; y < SCORE_DIGIT_H; y++) {
        uint8_t row = DIGITS_3X7[digit][y];
        for (int x = 0; x < 3; x++) {
            if ((row >> (2 - x)) & 1) {
                int px = start_px + x;
                int py = start_py + y;
                int xv = 63 - py;
                int yv = px;
                view_put(xv, yv, color);
            }
        }
    }
}

static int sign_i(int v)
{
    return (v > 0) - (v < 0);
}

static int score_rect_contains_view(int xv, int yv)
{
    int px = yv;
    int py = 63 - xv;
    return (px >= SCORE_X && px < (SCORE_X + SCORE_CLEAR_W) &&
            py >= SCORE_Y && py < (SCORE_Y + SCORE_CLEAR_H));
}

static void view_clear(void) {
    memset(viewbuf, 0, sizeof(viewbuf));
}

static void draw_rect(int x0, int y0, int w, int h, uint8_t color)
{
    for (int y = 0; y < h; y++) {
        int yy = y0 + y;
        if (yy < 0 || yy >= VIEW_H) continue;
        int row = yy * VIEW_W;
        for (int x = 0; x < w; x++) {
            int xx = x0 + x;
            if (xx < 0 || xx >= VIEW_W) continue;
            viewbuf[row + xx] = color;
        }
    }
}

static void restore_rect_from_base(int x0, int y0, int w, int h)
{
    for (int y = 0; y < h; y++) {
        int yy = y0 + y;
        if ((unsigned)yy >= VIEW_H) continue;
        int row = yy * VIEW_W;
        for (int x = 0; x < w; x++) {
            int xx = x0 + x;
            if ((unsigned)xx >= VIEW_W) continue;
            viewbuf[row + xx] = basebuf[row + xx];
        }
    }
}

static void restore_phys_rect_from_base(int px0, int py0, int w, int h)
{
    for (int py = 0; py < h; py++) {
        int pyy = py0 + py;
        for (int px = 0; px < w; px++) {
            int pxx = px0 + px;
            int xv = 63 - pyy;
            int yv = pxx;
            if ((unsigned)xv >= VIEW_W || (unsigned)yv >= VIEW_H) continue;
            viewbuf[yv * VIEW_W + xv] = basebuf[yv * VIEW_W + xv];
        }
    }
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
        xp = (uint16_t)((xp + PANEL_X_OFFSET) & 0x7Fu);
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
        if (code == 0x2C) ps2_update_held(K_TURBO, 0);
        if (code == 0x2B) ps2_update_held(K_NEXT_MAZE, 0);
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
        if (code == 0x2C) ps2_update_held(K_TURBO, 1);
        if (code == 0x2B) ps2_update_held(K_NEXT_MAZE, 1);
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
            /* Recover aggressively from line/FIFO errors so stale states do not stick. */
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
            case 'T':
                pac_turbo_enabled = 1;
                break;
            case 't':
                pac_turbo_enabled = 0;
                break;
            case 'F':
            case 'f':
                kb_pressed |= K_NEXT_MAZE;
                break;
            case 'B':
            case 'b':
                kb_pressed |= K_BOMB;
                break;
            default:
                break;
        }
    }

    /* If the receiver is still reporting a hard error after processing this tick,
       clear it once more and release any latched UART-held directions. */
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
 * Maze (2px walls + 8px paths)
 * ========================= */
static const uint8_t OPEN_RIGHT[CELLS_H][CELLS_W - 1] = {
    {1,1,0,1,1},
    {1,0,1,0,1},
    {1,1,1,0,1},
    {0,1,1,1,0},
    {1,0,1,1,1},
    {1,1,0,1,1},
    {1,0,1,0,1},
    {1,1,1,1,1},
    {1,0,1,1,0},
    {0,1,1,0,1},
    {1,1,0,1,1},
    {1,0,1,1,1}
};

static const uint8_t OPEN_DOWN[CELLS_H - 1][CELLS_W] = {
    {1,0,1,0,1,1},
    {1,1,0,1,0,1},
    {0,1,1,0,1,0},
    {1,0,1,1,0,1},
    {1,1,0,1,1,0},
    {0,1,1,0,1,1},
    {1,0,1,1,0,1},
    {1,1,0,1,1,0},
    {0,1,1,0,1,1},
    {1,0,1,1,0,1},
    {1,1,0,1,1,0}
};


static int cell_to_px_x(int cx)
{
    return MAZE_ORIGIN_X + WALL_PX + (cx * CELL_STEP);
}

static int cell_to_px_y(int cy)
{
    return MAZE_ORIGIN_Y + WALL_PX + (cy * CELL_STEP);
}

static void respawn_ghosts(void)
{
    static const int gx[4] = {GHOST_START_CX, GHOST_START_CX + 1, GHOST_START_CX, GHOST_START_CX + 1};
    static const int gy[4] = {GHOST_START_CY, GHOST_START_CY, GHOST_START_CY + 1, GHOST_START_CY + 1};
    static const uint8_t gc[4] = {RGB(1,0,0), RGB(1,0,1), RGB(0,1,1), RGB(1,1,0)};

    for (int i = 0; i < GHOST_COUNT; i++) {
        ghosts[i].x = cell_to_px_x(gx[i % 4]);
        ghosts[i].y = cell_to_px_y(gy[i % 4]);
        ghosts[i].prev_x = ghosts[i].x;
        ghosts[i].prev_y = ghosts[i].y;
        ghosts[i].dir = DIR_LEFT;
        ghosts[i].acc_ms = 0;
        ghosts[i].fright = 0;
        ghosts[i].color = gc[i % 4];
        ghost_draw_x_prev[i] = ghosts[i].x;
        ghost_draw_y_prev[i] = ghosts[i].y;
    }

    draw_positions_valid = 0;
    base_ready = 0;
}

static void clear_fright_state(void)
{
    power_timer_ms = 0;
    for (int i = 0; i < GHOST_COUNT; i++) {
        ghosts[i].fright = 0;
    }
}

static int is_aligned(int x, int y)
{
    int ox = x - (MAZE_ORIGIN_X + WALL_PX);
    int oy = y - (MAZE_ORIGIN_Y + WALL_PX);
    if (ox < 0 || oy < 0) return 0;
    return ((ox % CELL_STEP) == 0) && ((oy % CELL_STEP) == 0);
}

static int dir_dx(dir_t d);
static int dir_dy(dir_t d);
static int can_move_to(int x, int y, dir_t dir);

static int snap_delta_to_lane(int pos, int base)
{
    int rel = pos - base;
    if (rel < 0) return CELL_STEP;

    int rem = rel % CELL_STEP;
    if (rem > (CELL_STEP / 2)) rem -= CELL_STEP;
    return -rem;
}

static int try_assisted_turn(dir_t desired, int cur_x, int cur_y, int *aligned_x, int *aligned_y)
{
    *aligned_x = cur_x;
    *aligned_y = cur_y;

    if (desired == DIR_NONE) return 0;

    if (dir_dx(desired) != 0) {
        int dy = snap_delta_to_lane(cur_y, MAZE_ORIGIN_Y + WALL_PX);
        if (dy < -PAC_TURN_WINDOW || dy > PAC_TURN_WINDOW) return 0;
        *aligned_y = cur_y + dy;
    } else if (dir_dy(desired) != 0) {
        int dx = snap_delta_to_lane(cur_x, MAZE_ORIGIN_X + WALL_PX);
        if (dx < -PAC_TURN_WINDOW || dx > PAC_TURN_WINDOW) return 0;
        *aligned_x = cur_x + dx;
    } else {
        return 0;
    }

    return 1;
}

static int escape_turn_distance(dir_t travel, dir_t desired, int x, int y)
{
    int probe_x = x;
    int probe_y = y;

    for (int dist = 1; dist <= (VIEW_W + VIEW_H); dist++) {
        probe_x += dir_dx(travel);
        probe_y += dir_dy(travel);
        if (!can_move_to(probe_x, probe_y, travel)) return 0;

        int turn_x = probe_x;
        int turn_y = probe_y;
        if (is_aligned(probe_x, probe_y) ||
            try_assisted_turn(desired, probe_x, probe_y, &turn_x, &turn_y)) {
            int tx = turn_x + dir_dx(desired);
            int ty = turn_y + dir_dy(desired);
            if (can_move_to(tx, ty, desired)) return dist;
        }
    }

    return 0;
}

static dir_t choose_escape_dir(dir_t desired, int x, int y)
{
    dir_t candidates[2];
    int best_dist = 0;
    dir_t best_dir = DIR_NONE;

    if (dir_dx(desired) != 0) {
        candidates[0] = DIR_UP;
        candidates[1] = DIR_DOWN;
    } else if (dir_dy(desired) != 0) {
        candidates[0] = DIR_LEFT;
        candidates[1] = DIR_RIGHT;
    } else {
        return DIR_NONE;
    }

    for (int i = 0; i < 2; i++) {
        dir_t d = candidates[i];
        int nx = x + dir_dx(d);
        int ny = y + dir_dy(d);
        if (!can_move_to(nx, ny, d)) continue;

        int dist = escape_turn_distance(d, desired, x, y);
        if (dist == 0) continue;
        if (best_dir == DIR_NONE || dist < best_dist) {
            best_dir = d;
            best_dist = dist;
        }
    }

    return best_dir;
}

static int is_manual_tunnel_x(int x)
{
#if !USE_MANUAL_TUNNEL
    (void)x;
    return 0;
#else
    if (x < 0) return 0;
    if (x + CELL_PX - 1 > VIEW_W - 1) return 0;
    return (x >= TUNNEL_X_MIN) && ((x + CELL_PX - 1) <= TUNNEL_X_MAX);
#endif
}

static int is_auto_tunnel_x(int center_x)
{
#if !USE_AUTO_TUNNEL
    (void)center_x;
    return 0;
#else
    if (center_x < 0 || center_x >= VIEW_W) return 0;
    return tunnel_x_allowed[center_x] ? 1 : 0;
#endif
}

static int pac_edge_hits_border(dir_t dir, int x, int y)
{
    if (dir == DIR_NONE) return 0;
    int xv_edge = x;
    int yv_edge = y;
    int half = CELL_PX / 2;
    if (dir == DIR_LEFT) {
        xv_edge = x;
        yv_edge = y + half;
    } else if (dir == DIR_RIGHT) {
        xv_edge = x + CELL_PX - 1;
        yv_edge = y + half;
    } else if (dir == DIR_UP) {
        xv_edge = x + half;
        yv_edge = y;
    } else if (dir == DIR_DOWN) {
        xv_edge = x + half;
        yv_edge = y + CELL_PX - 1;
    }

    if (xv_edge < 0 || xv_edge >= VIEW_W) return 1;
    if (yv_edge < 0 || yv_edge >= VIEW_H) return 1;

    int px = yv_edge;
    int py = (PHY_H - 1) - xv_edge;
    return (px == 0 || px == (PHY_W - 1) || py == 0 || py == (PHY_H - 1));
}

static int wrap_tunnel_px(int *x, int *y, dir_t dir)
{
    int center_x = *x + (CELL_PX / 2);
    int center_y = *y + (CELL_PX / 2);
    if (center_x < 0) {
        int allow = is_manual_tunnel_x(0);
        if (!allow) {
            int check_cx = 0;
            allow = is_auto_tunnel_x(check_cx);
        }
        if (!allow && pac_edge_hits_border(dir, *x, *y)) allow = 1;
        if (!allow) return 0;
        *x = VIEW_W - CELL_PX;
        return 1;
    } else if (center_x >= VIEW_W) {
        int allow = is_manual_tunnel_x(VIEW_W - CELL_PX);
        if (!allow) {
            int check_cx = VIEW_W - 1;
            allow = is_auto_tunnel_x(check_cx);
        }
        if (!allow && pac_edge_hits_border(dir, *x, *y)) allow = 1;
        if (!allow) return 0;
        *x = 0;
        return 1;
    }

    if (center_y < 0) {
        int allow = is_manual_tunnel_x(*x);
        if (!allow && pac_edge_hits_border(dir, *x, *y)) allow = 1;
        if (!allow) return 0;
        *y = VIEW_H - CELL_PX;
        return 1;
    } else if (center_y >= VIEW_H) {
        int allow = is_manual_tunnel_x(*x);
        if (!allow && pac_edge_hits_border(dir, *x, *y)) allow = 1;
        if (!allow) return 0;
        *y = 0;
        return 1;
    }
    return 0;
}

static int maze_blocked_px(int x, int y)
{
    if (x < 0 || x >= VIEW_W || y < 0 || y >= VIEW_H) return 1;
    return maze_blocked[y][x] ? 1 : 0;
}

static int can_move_to(int x, int y, dir_t dir)
{
    if (y < 0 || (y + CELL_PX - 1) >= VIEW_H) {
        if (dir == DIR_UP || dir == DIR_DOWN) {
            if (is_manual_tunnel_x(x)) return 1;
            if (pac_edge_hits_border(dir, x, y)) return 1;
            return 0;
        }
        return 0;
    }
    int allow_left = 0;
    int allow_right = 0;
    if (dir == DIR_LEFT || dir == DIR_RIGHT) {
        if (is_manual_tunnel_x(x)) {
            allow_left = 1;
            allow_right = 1;
        } else {
            int center_x = x + (CELL_PX / 2);
            if (is_auto_tunnel_x(center_x)) {
                allow_left = 1;
                allow_right = 1;
            }
        }
    }
    if (x < 0 || (x + CELL_PX - 1) >= VIEW_W) {
        if (dir == DIR_LEFT) return allow_left || pac_edge_hits_border(dir, x, y);
        if (dir == DIR_RIGHT) return allow_right || pac_edge_hits_border(dir, x, y);
        return 0;
    }
    for (int yy = 0; yy < CELL_PX; yy++) {
        for (int xx = 0; xx < CELL_PX; xx++) {
            int xv = x + xx;
            int yv = y + yy;
            if (allow_left && xv < MAZE_BORDER_SIDE_PX) continue;
            if (allow_right && xv >= (VIEW_W - MAZE_BORDER_SIDE_PX)) continue;
            if (maze_blocked_px(xv, yv)) return 0;
        }
    }
    return 1;
}

static int pixel_to_cell_x(int center_x)
{
    int base = MAZE_ORIGIN_X + WALL_PX;
    int rel = center_x - base;
    if (rel < 0) return -1;
    int cx = rel / CELL_STEP;
    if (cx < 0 || cx >= CELLS_W) return -1;
    return cx;
}

static int pixel_to_cell_y(int center_y)
{
    int base = MAZE_ORIGIN_Y + WALL_PX;
    int rel = center_y - base;
    if (rel < 0) return -1;
    int cy = rel / CELL_STEP;
    if (cy < 0 || cy >= CELLS_H) return -1;
    return cy;
}

static int maze_sprite_bit(int row, int col)
{
    int r = row + MAZE_SPRITE_ROW_SHIFT;
    int c = col + MAZE_SPRITE_COL_SHIFT;
    if ((unsigned)r >= 64 || (unsigned)c >= 128) return 0;
    int word = c / 32;
    int bit  = 31 - (c % 32);
    return (maze_active[r][word] >> bit) & 1;
}

static void maze_apply_sprite_blocked(void)
{
    memset(maze_blocked, 0, sizeof(maze_blocked));
    for (int py = 0; py < 64; py++) {
        for (int px = 0; px < 128; px++) {
            int wall = maze_sprite_bit(py, px);
            int xv = (63 - py) + MAZE_SPRITE_X_SHIFT;
            int yv = px + MAZE_SPRITE_Y_SHIFT;
            if ((unsigned)xv < VIEW_W && (unsigned)yv < VIEW_H) {
                maze_blocked[yv][xv] = (uint8_t)wall;
            }
        }
    }

    memset(tunnel_x_allowed, 0, sizeof(tunnel_x_allowed));
#if USE_AUTO_TUNNEL
    for (int x0 = 0; x0 + CELL_PX - 1 < VIEW_W; x0++) {
        int left_open = 1;
        int right_open = 1;
        for (int xx = 0; xx < CELL_PX; xx++) {
            int xv = x0 + xx;
            for (int y = 0; y < MAZE_BORDER_SIDE_PX; y++) {
                if (maze_blocked[y][xv]) { left_open = 0; break; }
            }
            for (int y = VIEW_H - MAZE_BORDER_SIDE_PX; y < VIEW_H; y++) {
                if (maze_blocked[y][xv]) { right_open = 0; break; }
            }
            if (!left_open && !right_open) break;
        }
        if (left_open && right_open) {
            for (int xx = 0; xx < CELL_PX; xx++) {
                tunnel_x_allowed[x0 + xx] = 1;
            }
        }
    }
#endif

    memset(maze_region, 0xFF, sizeof(maze_region));
    memset(region_tunnel, 0, sizeof(region_tunnel));

    int stack_x[VIEW_W * VIEW_H];
    int stack_y[VIEW_W * VIEW_H];
    int region_id = 0;

    for (int yv = 0; yv < VIEW_H; yv++) {
        for (int xv = 0; xv < VIEW_W; xv++) {
            if (maze_blocked[yv][xv]) continue;
            if (maze_region[yv][xv] >= 0) continue;

            int sp = 0;
            stack_x[sp] = xv;
            stack_y[sp] = yv;
            sp++;
            maze_region[yv][xv] = (int16_t)region_id;

            int touch_left = 0;
            int touch_right = 0;

            while (sp > 0) {
                sp--;
                int cx = stack_x[sp];
                int cy = stack_y[sp];

                if (cx == 0) touch_left = 1;
                if (cx == VIEW_W - 1) touch_right = 1;

                if (cx > 0 && !maze_blocked[cy][cx - 1] && maze_region[cy][cx - 1] < 0) {
                    maze_region[cy][cx - 1] = (int16_t)region_id;
                    stack_x[sp] = cx - 1;
                    stack_y[sp] = cy;
                    sp++;
                }
                if (cx + 1 < VIEW_W && !maze_blocked[cy][cx + 1] && maze_region[cy][cx + 1] < 0) {
                    maze_region[cy][cx + 1] = (int16_t)region_id;
                    stack_x[sp] = cx + 1;
                    stack_y[sp] = cy;
                    sp++;
                }
                if (cy > 0 && !maze_blocked[cy - 1][cx] && maze_region[cy - 1][cx] < 0) {
                    maze_region[cy - 1][cx] = (int16_t)region_id;
                    stack_x[sp] = cx;
                    stack_y[sp] = cy - 1;
                    sp++;
                }
                if (cy + 1 < VIEW_H && !maze_blocked[cy + 1][cx] && maze_region[cy + 1][cx] < 0) {
                    maze_region[cy + 1][cx] = (int16_t)region_id;
                    stack_x[sp] = cx;
                    stack_y[sp] = cy + 1;
                    sp++;
                }
            }

            region_tunnel[region_id] = (uint8_t)(touch_left && touch_right);
            region_id++;
        }
    }
}

static int pellet_bit_sprite(int row, int col)
{
    int word = col / 32;
    int bit = 31 - (col % 32);
    return (MAZE_PELLETS[row][word] >> bit) & 1;
}

static int power_bit_sprite(int row, int col)
{
    int word = col / 32;
    int bit = 31 - (col % 32);
    return (MAZE_POWER[row][word] >> bit) & 1;
}

static int pellet_sprite_at_view(int xv, int yv)
{
    int adj_xv = xv - PELLET_VIEW_SHIFT_X;
    int adj_yv = yv - PELLET_VIEW_SHIFT_Y;
    int py = 63 - (adj_xv - MAZE_SPRITE_X_SHIFT);
    int px = adj_yv - MAZE_SPRITE_Y_SHIFT;
    py += MAZE_SPRITE_ROW_SHIFT;
    if (py < 0 || py >= 64 || px < 0 || px >= 128) return 0;
    return pellet_bit_sprite(py, px);
}

static int power_sprite_at_view(int xv, int yv)
{
    int adj_xv = xv - POWER_VIEW_SHIFT_X;
    int adj_yv = yv - POWER_VIEW_SHIFT_Y;
    int py = 63 - (adj_xv - MAZE_SPRITE_X_SHIFT);
    int px = adj_yv - MAZE_SPRITE_Y_SHIFT;
    py += MAZE_SPRITE_ROW_SHIFT;
    if (py < 0 || py >= 64 || px < 0 || px >= 128) return 0;
    return power_bit_sprite(py, px);
}


static void build_basebuf(void)
{
    memset(basebuf, 0, sizeof(basebuf));
#if !DEBUG_MARKERS && !DEBUG_TUNNEL
    for (int py = 0; py < 64; py++) {
        for (int px = 0; px < 128; px++) {
            if (!maze_sprite_bit(py, px)) continue;
            int xv = (63 - py) + MAZE_SPRITE_X_SHIFT;
            int yv = px + MAZE_SPRITE_Y_SHIFT;
            if ((unsigned)xv < VIEW_W && (unsigned)yv < VIEW_H) {
                base_put(xv, yv, wall_color);
            }
        }
    }
#endif
    for (int cy = 0; cy < CELLS_H; cy++) {
        for (int cx = 0; cx < CELLS_W; cx++) {
            if (!cell_open[cy][cx]) continue;
            int x0 = cell_to_px_x(cx);
            int y0 = cell_to_px_y(cy);
            int px = x0 + (CELL_PX / 2);
            int py = y0 + (CELL_PX / 2);
            if (score_rect_contains_view(px, py)) continue;
            if (cell_pellet[cy][cx] == 1) {
                base_put(px, py, COLOR_WHITE);
            } else if (cell_pellet[cy][cx] == 2) {
                for (int y = -1; y <= 1; y++) {
                    for (int x = -1; x <= 1; x++) {
                        base_put(px + x, py + y, COLOR_WHITE);
                    }
                }
            }
        }
    }
#if TUNNEL_MARK_ENABLE
    for (int xv = 0; xv < VIEW_W; xv++) {
        if (!tunnel_x_allowed[xv]) continue;
        for (int y = 0; y < TUNNEL_MARK_PX; y++) {
            base_put(xv, y, RGB(1,0,0));
        }
        for (int y = VIEW_H - TUNNEL_MARK_PX; y < VIEW_H; y++) {
            base_put(xv, y, RGB(1,0,0));
        }
    }
#endif
    base_ready = 0;
}


static void maze_init(void)
{
    pellets_left = 0;
    memset(cell_open, 0, sizeof(cell_open));
    memset(cell_pellet, 0, sizeof(cell_pellet));

    for (int cy = 0; cy < CELLS_H; cy++) {
        for (int cx = 0; cx < CELLS_W; cx++) {
            cell_open[cy][cx] = 1;
        }
    }

    maze_apply_sprite_blocked();

    for (int cy = 0; cy < CELLS_H; cy++) {
        for (int cx = 0; cx < CELLS_W; cx++) {
            int x0 = cell_to_px_x(cx);
            int y0 = cell_to_px_y(cy);
            int count = 0;
            for (int y = 0; y < CELL_PX; y++) {
                for (int x = 0; x < CELL_PX; x++) {
                    int xv = x0 + x;
                    int yv = y0 + y;
                    if (maze_blocked_px(xv, yv)) continue;
                    if (pellet_sprite_at_view(xv, yv)) count++;
                }
            }
            int power_count = 0;
            for (int y = 0; y < CELL_PX; y++) {
                for (int x = 0; x < CELL_PX; x++) {
                    int xv = x0 + x;
                    int yv = y0 + y;
                    if (maze_blocked_px(xv, yv)) continue;
                    if (power_sprite_at_view(xv, yv)) power_count++;
                }
            }
            int center_x = x0 + (CELL_PX / 2);
            int center_y = y0 + (CELL_PX / 2);
            if (score_rect_contains_view(center_x, center_y)) {
                cell_pellet[cy][cx] = 0;
            } else if (power_count >= POWER_PELLET_MIN_BITS) {
                cell_pellet[cy][cx] = 2;
                pellets_left++;
            } else if (count > 0) {
                cell_pellet[cy][cx] = 1;
                pellets_left++;
            }
        }
    }

    for (int i = 0; i < 4; i++) {
        int sx = POWER_PELLET_COORDS[i][0];
        int sy = POWER_PELLET_COORDS[i][1];
        int xv = 63 - sy;
        int yv = sx;
        if (xv < 0 || xv >= VIEW_W || yv < 0 || yv >= VIEW_H) continue;
        int center_x = xv;
        int center_y = yv;
        int cx = pixel_to_cell_x(center_x);
        int cy = pixel_to_cell_y(center_y);
        if (cx < 0 || cy < 0) continue;
        if (cell_pellet[cy][cx] == 0) pellets_left++;
        cell_pellet[cy][cx] = 2;
    }
}

/* =========================
 * Movement
 * ========================= */
static int dir_dx(dir_t d) {
    if (d == DIR_LEFT) return -1;
    if (d == DIR_RIGHT) return 1;
    return 0;
}

static int dir_dy(dir_t d) {
    if (d == DIR_UP) return -1;
    if (d == DIR_DOWN) return 1;
    return 0;
}

static int dir_is_opposite(dir_t a, dir_t b) {
    return (a == DIR_LEFT && b == DIR_RIGHT) ||
           (a == DIR_RIGHT && b == DIR_LEFT) ||
           (a == DIR_UP && b == DIR_DOWN) ||
           (a == DIR_DOWN && b == DIR_UP);
}

static int interp_pos(int prev, int cur, uint32_t acc_ms, uint32_t step_ms)
{
    if (prev == cur) return cur;
    if (step_ms == 0 || step_ms <= LOGIC_DT_MS) return cur;
    if (acc_ms > step_ms) acc_ms = step_ms;
    uint32_t t = (acc_ms * 256u) / step_ms;
    if (t > 255u) t = 255u;
    return prev + ((cur - prev) * (int)t) / 256;
}

static int ghost_scatter_mode(void)
{
    uint32_t t = ghost_mode_timer_ms;
    if (t < GHOST_SCATTER_1_MS) return 1;
    t -= GHOST_SCATTER_1_MS;
    if (t < GHOST_CHASE_1_MS) return 0;
    t -= GHOST_CHASE_1_MS;
    if (t < GHOST_SCATTER_2_MS) return 1;
    t -= GHOST_SCATTER_2_MS;
    if (t < GHOST_CHASE_2_MS) return 0;
    t -= GHOST_CHASE_2_MS;
    if (t < GHOST_SCATTER_3_MS) return 1;
    t -= GHOST_SCATTER_3_MS;
    if (t < GHOST_CHASE_3_MS) return 0;
    t -= GHOST_CHASE_3_MS;
    if (t < GHOST_SCATTER_4_MS) return 1;
    return 0;
}

static void ghost_target_pixel(int ghost_idx, int *tx, int *ty)
{
    static const int scatter_x[GHOST_COUNT] = {
        VIEW_W - CELL_PX, 0, VIEW_W - CELL_PX, 0
    };
    static const int scatter_y[GHOST_COUNT] = {
        0, 0, VIEW_H - CELL_PX, VIEW_H - CELL_PX
    };

    int pac_x = pac.x;
    int pac_y = pac.y;
    int pac_dx = dir_dx(pac.dir) * (CELL_STEP * 4);
    int pac_dy = dir_dy(pac.dir) * (CELL_STEP * 4);

    if (ghost_scatter_mode()) {
        *tx = scatter_x[ghost_idx];
        *ty = scatter_y[ghost_idx];
        return;
    }

    switch (ghost_idx) {
        case 0: /* Blinky */
            *tx = pac_x;
            *ty = pac_y;
            break;
        case 1: /* Pinky */
            *tx = pac_x + pac_dx;
            *ty = pac_y + pac_dy;
            break;
        case 2: { /* Inky */
            int ahead_x = pac_x + dir_dx(pac.dir) * (CELL_STEP * 2);
            int ahead_y = pac_y + dir_dy(pac.dir) * (CELL_STEP * 2);
            int vx = ahead_x - ghosts[0].x;
            int vy = ahead_y - ghosts[0].y;
            *tx = ahead_x + vx;
            *ty = ahead_y + vy;
            break;
        }
        default: { /* Clyde */
            int dx = pac.x - ghosts[ghost_idx].x;
            int dy = pac.y - ghosts[ghost_idx].y;
            int dist2 = (dx * dx) + (dy * dy);
            if (dist2 <= (CELL_STEP * 4) * (CELL_STEP * 4)) {
                *tx = scatter_x[ghost_idx];
                *ty = scatter_y[ghost_idx];
            } else {
                *tx = pac_x;
                *ty = pac_y;
            }
            break;
        }
    }
}

static dir_t choose_ghost_dir(int ghost_idx, int gx, int gy, dir_t cur, uint8_t fright)
{
    dir_t dirs[4] = {DIR_LEFT, DIR_RIGHT, DIR_UP, DIR_DOWN};
    dir_t best = DIR_NONE;
    int best_cost = 0x7FFFFFFF;
    dir_t fallback = DIR_NONE;
    int target_x = pac.x;
    int target_y = pac.y;

    if (!fright) {
        ghost_target_pixel(ghost_idx, &target_x, &target_y);
    }

    for (int i = 0; i < 4; i++) {
        dir_t d = dirs[i];
        int nx = gx + dir_dx(d);
        int ny = gy + dir_dy(d);
        if (!can_move_to(nx, ny, d)) continue;

        if (fallback == DIR_NONE) fallback = d;

        if (fright) {
            if (dir_is_opposite(d, cur) && best != DIR_NONE) continue;
            if ((lfsr & 3u) == (uint32_t)i) return d;
            if (best == DIR_NONE || !dir_is_opposite(d, cur)) best = d;
            continue;
        }

        if (dir_is_opposite(d, cur)) continue;

        int cx = nx + (CELL_PX / 2);
        int cy = ny + (CELL_PX / 2);
        int dx = cx - (target_x + (CELL_PX / 2));
        int dy = cy - (target_y + (CELL_PX / 2));
        int cost = (dx * dx) + (dy * dy);
        if (cost < best_cost) {
            best_cost = cost;
            best = d;
        }
    }

    if (best != DIR_NONE) return best;
    if (fallback != DIR_NONE) return fallback;
    return cur;
}

/* =========================
 * Game state
 * ========================= */
static void reset_positions(void)
{
    clear_fright_state();

    pac.x = cell_to_px_x(PAC_START_CX);
    pac.y = cell_to_px_y(PAC_START_CY);
    pac.prev_x = pac.x;
    pac.prev_y = pac.y;
    pac.dir = DIR_UP;
    pac.desired = DIR_UP;
    pac.acc_ms = 0;
    pac_draw_x_prev = pac.x;
    pac_draw_y_prev = pac.y;

    respawn_ghosts();
}

static void advance_level(void)
{
    maze_index = (maze_index + 1) % MAZE_COUNT;
    maze_active = maze_list[maze_index];
    wall_color = wall_colors[maze_index];
    if (ghost_step_ms > GHOST_STEP_MIN_MS) ghost_step_ms -= GHOST_STEP_DEC_MS;
    if (ghost_step_ms < GHOST_STEP_MIN_MS) ghost_step_ms = GHOST_STEP_MIN_MS;
    if (ghost_step_ms_fr > GHOST_STEP_FR_MIN_MS) ghost_step_ms_fr -= GHOST_STEP_DEC_MS;
    if (ghost_step_ms_fr < GHOST_STEP_FR_MIN_MS) ghost_step_ms_fr = GHOST_STEP_FR_MIN_MS;
    maze_init();
    build_basebuf();
    reset_positions();
}

static void cycle_maze_reset(void)
{
    maze_index = (maze_index + 1) % MAZE_COUNT;
    maze_active = maze_list[maze_index];
    wall_color = wall_colors[maze_index];
    clear_fright_state();
    maze_init();
    build_basebuf();
    reset_positions();
    game_started = 0;
    input_locked = 1;
    score_visible = 1;
    current_state = STATE_PLAYING;
}

static void reset_full_game(void)
{
    score = 0;
    lives = PAC_LIVES;
    clear_fright_state();
    ghost_mode_timer_ms = 0;
    maze_index = 0;
    maze_active = maze_list[maze_index];
    wall_color = wall_colors[maze_index];
    ghost_step_ms = GHOST_STEP_MS_BASE;
    ghost_step_ms_fr = GHOST_STEP_MS_FR_BASE;
    ghost_mode_timer_ms = 0;
    maze_init();
    build_basebuf();
    reset_positions();
    pac_turbo_enabled = 0;
    game_started = 0;
    input_locked = 1;
    score_visible = 1;
    level_clear_toggles = 0;
    level_clear_acc_ms = 0;
}

static void enter_game_over(void)
{
    current_state = STATE_GAME_OVER_FADE;
    fade_brightness = DEFAULT_BRIGHTNESS;
    fade_acc_ms = 0;
    prev_any_key = 1;
    set_display_brightness(fade_brightness);
    base_ready = 0;
    draw_positions_valid = 0;
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

static void enter_intro(void)
{
    current_state = STATE_INTRO;
    intro_acc_ms = 0;
}

static void enter_dying(uint8_t last_life)
{
    current_state = STATE_DYING;
    death_acc_ms = 0;
    death_last_life = last_life;
    death_x = pac.x;
    death_y = pac.y;
    death_dir = pac.dir;
    draw_positions_valid = 0;
    base_ready = 0;
}

static void update_intro(void)
{
    intro_acc_ms += LOGIC_DT_MS;
    if (intro_acc_ms >= INTRO_SHOW_MS) {
        current_state = STATE_PLAYING;
    }
}

static void update_dying(void)
{
    death_acc_ms += LOGIC_DT_MS;
    if (death_acc_ms < DEATH_ANIM_MS) return;

    if (death_last_life) {
        enter_game_over();
    } else {
        reset_positions();
        input_locked = 1;
        current_state = STATE_PLAYING;
    }
}

static void enter_level_clear(void)
{
    current_state = STATE_LEVEL_CLEAR;
    level_clear_toggles = 0;
    level_clear_acc_ms = 0;
    score_visible = 1;
}

static void update_level_clear(void)
{
    level_clear_acc_ms += LOGIC_DT_MS;
    if (level_clear_acc_ms < LEVEL_CLEAR_BLINK_MS) return;
    level_clear_acc_ms = 0;
    score_visible = (uint8_t)(!score_visible);
    level_clear_toggles++;
    if (level_clear_toggles >= (LEVEL_CLEAR_BLINKS * 2)) {
        score_visible = 1;
        advance_level();
        current_state = STATE_PLAYING;
    }
}

static void draw_vhdl_char_rot_right(int char_idx, int start_x, int start_y, uint8_t color)
{
    for (int row = 0; row < 7; row++) {
        uint8_t line = FONT_VHDL[char_idx][row];
        for (int col = 0; col < 5; col++) {
            if ((line >> (4 - col)) & 1) {
                for (int dy = 0; dy < 2; dy++) {
                    for (int dx = 0; dx < 2; dx++) {
                        int ox = (col * 2) + dx;
                        int oy = (row * 2) + dy;
                        int rx = 13 - oy;
                        int ry = ox;
                        view_put(start_x + rx, start_y + ry, color);
                    }
                }
            }
        }
    }
}

static void handle_pac_eat(void)
{
    int center_x = pac.x + (CELL_PX / 2);
    int center_y = pac.y + (CELL_PX / 2);
    int cx = pixel_to_cell_x(center_x);
    int cy = pixel_to_cell_y(center_y);
    if (cx < 0 || cy < 0) return;
    if (cell_pellet[cy][cx] == 1) {
        cell_pellet[cy][cx] = 0;
        pellets_left--;
        score += 10;
        int x0 = cell_to_px_x(cx) + (CELL_PX / 2);
        int y0 = cell_to_px_y(cy) + (CELL_PX / 2);
        base_put(x0, y0, COLOR_BLACK);
        view_put(x0, y0, COLOR_BLACK);
    } else if (cell_pellet[cy][cx] == 2) {
        cell_pellet[cy][cx] = 0;
        pellets_left--;
        score += 50;
        power_timer_ms = POWER_TIME_MS;
        for (int i = 0; i < GHOST_COUNT; i++) {
            ghosts[i].fright = 1;
            ghosts[i].dir = (dir_t)(
                ghosts[i].dir == DIR_LEFT  ? DIR_RIGHT :
                ghosts[i].dir == DIR_RIGHT ? DIR_LEFT  :
                ghosts[i].dir == DIR_UP    ? DIR_DOWN  :
                                             DIR_UP);
            ghosts[i].acc_ms = 0;
        }
        int x0 = cell_to_px_x(cx) + (CELL_PX / 2);
        int y0 = cell_to_px_y(cy) + (CELL_PX / 2);
        for (int y = -1; y <= 1; y++) {
            for (int x = -1; x <= 1; x++) {
                base_put(x0 + x, y0 + y, COLOR_BLACK);
                view_put(x0 + x, y0 + y, COLOR_BLACK);
            }
        }
    }
}

static void handle_collisions(void)
{
    for (int i = 0; i < GHOST_COUNT; i++) {
        int ax0 = pac.x + COLLISION_INSET;
        int ay0 = pac.y + COLLISION_INSET;
        int ax1 = pac.x + CELL_PX - 1 - COLLISION_INSET;
        int ay1 = pac.y + CELL_PX - 1 - COLLISION_INSET;
        int bx0 = ghosts[i].x + COLLISION_INSET;
        int by0 = ghosts[i].y + COLLISION_INSET;
        int bx1 = ghosts[i].x + CELL_PX - 1 - COLLISION_INSET;
        int by1 = ghosts[i].y + CELL_PX - 1 - COLLISION_INSET;
        if (!(ax1 < bx0 || ax0 > bx1 || ay1 < by0 || ay0 > by1)) {
            if (ghosts[i].fright) {
                score += 200;
                ghosts[i].x = cell_to_px_x(GHOST_START_CX);
                ghosts[i].y = cell_to_px_y(GHOST_START_CY);
                ghosts[i].prev_x = ghosts[i].x;
                ghosts[i].prev_y = ghosts[i].y;
                ghosts[i].fright = 0;
                draw_positions_valid = 0;
                base_ready = 0;
            } else {
                if (lives > 1) {
                    lives--;
                    enter_dying(0);
                } else {
                    lives = 0;
                    enter_dying(1);
                }
            }
            return;
        }
    }
}

static void update_pacman(uint8_t held, uint8_t pressed_edge)
{
    uint32_t pac_step_ms = pac_step_limit_ms();
    uint8_t input = held | pressed_edge;
    dir_t next = pac.desired;
    if (input & K_ROT) next = DIR_RIGHT;
    else if (input & K_LEFT) next = DIR_UP;
    else if (input & K_DOWN) next = DIR_LEFT;
    else if (input & K_RIGHT) next = DIR_DOWN;
    pac.desired = next;

    pac.acc_ms += LOGIC_DT_MS;
    if (pac.acc_ms < pac_step_ms) return;

    int moved = 0;
    while (pac.acc_ms >= pac_step_ms) {
        pac.acc_ms -= pac_step_ms;

        dir_t move_dir = pac.dir;
        int move_x = pac.x;
        int move_y = pac.y;
        if (dir_is_opposite(pac.desired, pac.dir)) {
            move_dir = pac.desired;
        } else {
            int turn_x = pac.x;
            int turn_y = pac.y;
            int ready_to_turn = is_aligned(pac.x, pac.y);
            if (!ready_to_turn &&
                pac.desired != pac.dir &&
                !dir_is_opposite(pac.desired, pac.dir)) {
                ready_to_turn = try_assisted_turn(pac.desired, pac.x, pac.y, &turn_x, &turn_y);
            }

            if (ready_to_turn) {
                int tx = turn_x + dir_dx(pac.desired);
                int ty = turn_y + dir_dy(pac.desired);
                if (can_move_to(tx, ty, pac.desired)) {
                    move_dir = pac.desired;
                    move_x = turn_x;
                    move_y = turn_y;
                }
            }
        }

        int nx = move_x + dir_dx(move_dir);
        int ny = move_y + dir_dy(move_dir);
        if (!can_move_to(nx, ny, move_dir) &&
            move_dir == pac.dir &&
            pac.desired != pac.dir &&
            !dir_is_opposite(pac.desired, pac.dir)) {
            dir_t escape_dir = choose_escape_dir(pac.desired, pac.x, pac.y);
            if (escape_dir != DIR_NONE) {
                move_dir = escape_dir;
                move_x = pac.x;
                move_y = pac.y;
                nx = move_x + dir_dx(move_dir);
                ny = move_y + dir_dy(move_dir);
            }
        }

        if (can_move_to(nx, ny, move_dir)) {
            pac.prev_x = pac.x;
            pac.prev_y = pac.y;
            pac.x = nx;
            pac.y = ny;
            pac.dir = move_dir;
            if (wrap_tunnel_px(&pac.x, &pac.y, move_dir)) {
                pac.prev_x = pac.x;
                pac.prev_y = pac.y;
                pac.acc_ms = 0;
            }
            moved = 1;
        } else {
            pac.prev_x = pac.x;
            pac.prev_y = pac.y;
            pac.acc_ms = 0;
            break;
        }
    }
}

static void update_ghosts(void)
{
    if (bomb_active) return;

    for (int i = 0; i < GHOST_COUNT; i++) {
        ghost_t *g = &ghosts[i];
        g->acc_ms += LOGIC_DT_MS;

        uint32_t limit = (g->fright ? ghost_step_ms_fr : ghost_step_ms);
        if (g->acc_ms < limit) continue;

        int moved = 0;
        while (g->acc_ms >= limit) {
            g->acc_ms -= limit;
            dir_t ndir = g->dir;
            if (is_aligned(g->x, g->y)) {
                ndir = choose_ghost_dir(i, g->x, g->y, g->dir, g->fright);
            }
            int nx = g->x + dir_dx(ndir);
            int ny = g->y + dir_dy(ndir);
            if (can_move_to(nx, ny, ndir)) {
                g->prev_x = g->x;
                g->prev_y = g->y;
                g->x = nx;
                g->y = ny;
                g->dir = ndir;
                if (wrap_tunnel_px(&g->x, &g->y, ndir)) {
                    g->prev_x = g->x;
                    g->prev_y = g->y;
                    g->acc_ms = 0;
                }
                moved = 1;
            } else {
                g->prev_x = g->x;
                g->prev_y = g->y;
                g->acc_ms = 0;
                break;
            }
        }
    }
}

static void update_play_state(uint8_t pressed_edge, uint8_t held)
{
    if (pressed_edge & K_NEXT_MAZE) {
        cycle_maze_reset();
        return;
    }

    if (input_locked) {
        if (held == 0) input_locked = 0;
        pressed_edge = 0;
        held = 0;
    }

    if (!game_started) {
        if (pressed_edge) game_started = 1;
        else return;
    }

    if (pressed_edge & K_TURBO) {
        pac_turbo_enabled = (uint8_t)!pac_turbo_enabled;
    }

    if (pressed_edge & K_BOMB) {
        trigger_bomb_effect();
    }

    ghost_mode_timer_ms += LOGIC_DT_MS;

    if (bomb_active) {
        bomb_acc_ms += LOGIC_DT_MS;
        if (bomb_acc_ms >= BOMB_TOTAL_MS) {
            bomb_active = 0;
            bomb_acc_ms = 0;
            respawn_ghosts();
        }
    }

    if (power_timer_ms > 0) {
        if (power_timer_ms > LOGIC_DT_MS) power_timer_ms -= LOGIC_DT_MS;
        else power_timer_ms = 0;
        if (power_timer_ms == 0) {
            for (int i = 0; i < GHOST_COUNT; i++) ghosts[i].fright = 0;
        }
    }

    update_pacman(held, pressed_edge);
    handle_pac_eat();

    // Check collision before ghosts move so Pac-Man can eat frightened
    // ghosts on the same tick he reaches them or grabs a power pellet.
    if (!bomb_active) {
        handle_collisions();
        if (current_state != STATE_PLAYING) return;
    }

    update_ghosts();
    if (!bomb_active) {
        handle_collisions();
        if (current_state != STATE_PLAYING) return;
    }

    if (pellets_left == 0) {
        enter_level_clear();
        return;
    }
}

/* =========================
 * Rendering
 * ========================= */
static void render_score_overlay(void)
{
    if (!score_visible) return;
    uint32_t s = score;
    int start_px = SCORE_X;
    int start_py = SCORE_Y;
    int lives_digit = (lives > 9) ? 9 : (int)lives;

    draw_digit_3x7_phys(lives_digit,
                        start_px,
                        start_py,
                        COLOR_LIVES);

    for (int slot = SCORE_DIGITS - 1; slot >= 1; slot--) {
        int dig = (int)(s % 10);
        s /= 10;
        draw_digit_3x7_phys(dig,
                            start_px + (slot * (SCORE_DIGIT_W + SCORE_DIGIT_GAP)),
                            start_py,
                            COLOR_WHITE);
    }
}

static void render_game_over_text(void)
{
    int msg_ids[] = {0, 1, 2, 3, -1, 4, 5, 3, 6};
    for (int i = 0; i < 9; i++) {
        if (msg_ids[i] < 0) continue;
        draw_vhdl_char_rot_right(msg_ids[i],
                                 GAME_OVER_X,
                                 GAME_OVER_Y + (i * GAME_OVER_STEP),
                                 COLOR_WHITE);
    }
}

static dir_t render_dir(dir_t dir)
{
    if (dir == DIR_UP) return DIR_DOWN;
    if (dir == DIR_DOWN) return DIR_UP;
    return dir;
}

static int sprite_bit(const uint8_t *spr, int x, int y, dir_t dir)
{
    if (dir == DIR_RIGHT) {
        return (spr[y] >> (7 - x)) & 1;
    }
    if (dir == DIR_LEFT) {
        return (spr[y] >> x) & 1;
    }
    if (dir == DIR_UP) {
        return (spr[x] >> (7 - y)) & 1;
    }
    if (dir == DIR_DOWN) {
        return (spr[x] >> y) & 1;
    }
    return (spr[y] >> (7 - x)) & 1;
}

static int sprite_bit_rot_right(const uint8_t *spr, int x, int y)
{
    int src_y = 7 - x;
    int src_x = y;
    return (spr[src_y] >> (7 - src_x)) & 1;
}

static void draw_sprite(int x0, int y0, const uint8_t *spr, dir_t dir, uint8_t color)
{
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (sprite_bit(spr, x, y, dir)) {
                view_put(x0 + x, y0 + y, color);
            }
        }
    }
}

static void draw_sprite_rot_right(int x0, int y0, const uint8_t *spr, uint8_t color)
{
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (sprite_bit_rot_right(spr, x, y)) {
                view_put(x0 + x, y0 + y, color);
            }
        }
    }
}

static void render_entities(void)
{
    uint8_t open = ((ms_global / PAC_ANIM_MS) & 1) ? 1 : 0;
    const uint8_t *spr = open ? PACMAN_SPRITE_OPEN : PACMAN_SPRITE_CLOSED;
    uint32_t pac_step_ms = pac_step_limit_ms();
    int pac_draw_x = interp_pos(pac.prev_x, pac.x, pac.acc_ms, pac_step_ms);
    int pac_draw_y = interp_pos(pac.prev_y, pac.y, pac.acc_ms, pac_step_ms);

    if (draw_positions_valid) {
        restore_rect_from_base(pac_draw_x_prev, pac_draw_y_prev, CELL_PX, CELL_PX);
        for (int i = 0; i < GHOST_COUNT; i++) {
            restore_rect_from_base(ghost_draw_x_prev[i],
                                   ghost_draw_y_prev[i],
                                   CELL_PX,
                                   CELL_PX);
        }
    }

    draw_sprite(pac_draw_x, pac_draw_y, spr, render_dir(pac.dir), COLOR_PAC);
    pac_draw_x_prev = pac_draw_x;
    pac_draw_y_prev = pac_draw_y;

    for (int i = 0; i < GHOST_COUNT; i++) {
        if (bomb_active) {
            int phase = (int)(bomb_acc_ms / BOMB_STEP_MS);
            if (phase < BOMB_PULSE_PHASES) {
                static const int inset_seq[6] = {1, 2, 3, 2, 1, 0};
                int inset = inset_seq[phase % 6];
                for (int y = 0; y < 8; y++) {
                    for (int x = 0; x < 8; x++) {
                        if (x < inset || x >= (8 - inset) || y < inset || y >= (8 - inset)) continue;
                        if (sprite_bit_rot_right(GHOST_SPRITE, x, y)) {
                            view_put(bomb_ghost_x[i] + x, bomb_ghost_y[i] + y, bomb_ghost_color[i]);
                        }
                    }
                }
            } else {
                int stage = phase - BOMB_PULSE_PHASES;
                if (stage > (BOMB_EXPLODE_PHASES - 1)) stage = BOMB_EXPLODE_PHASES - 1;
                for (int y = 0; y < 8; y++) {
                    for (int x = 0; x < 8; x++) {
                        if (!sprite_bit_rot_right(GHOST_SPRITE, x, y)) continue;

                        int dx = x - 3;
                        int dy = y - 3;
                        int dist2 = dx * dx + dy * dy;
                        int keep_limit = stage * 2 + 2;

                        if (dist2 >= keep_limit) {
                            view_put(bomb_ghost_x[i] + x, bomb_ghost_y[i] + y, bomb_ghost_color[i]);
                        } else if (((x + y + stage) & 1) == 0) {
                            int px = bomb_ghost_x[i] + x + sign_i(dx) * stage;
                            int py = bomb_ghost_y[i] + y + sign_i(dy) * stage;
                            view_put(px, py, bomb_ghost_color[i]);
                        }
                    }
                }
            }
            ghost_draw_x_prev[i] = bomb_ghost_x[i];
            ghost_draw_y_prev[i] = bomb_ghost_y[i];
            continue;
        }

        uint8_t c = ghosts[i].color;
        if (ghosts[i].fright) {
            c = COLOR_FRIGHT;
            if (power_timer_ms <= POWER_BLINK_START_MS) {
                uint32_t blink_phase = (power_timer_ms / POWER_BLINK_PERIOD_MS) & 1u;
                if (blink_phase) c = COLOR_WHITE;
            }
        }
        uint32_t limit = ghosts[i].fright ? ghost_step_ms_fr : ghost_step_ms;
        int gx = interp_pos(ghosts[i].prev_x, ghosts[i].x, ghosts[i].acc_ms, limit);
        int gy = interp_pos(ghosts[i].prev_y, ghosts[i].y, ghosts[i].acc_ms, limit);
        draw_sprite_rot_right(gx, gy, GHOST_SPRITE, c);
        ghost_draw_x_prev[i] = gx;
        ghost_draw_y_prev[i] = gy;
    }
    draw_positions_valid = 1;
}

static int death_mouth_open_pixel(int x, int y, dir_t dir, int mouth_stage)
{
    int dx = x - 3;
    int dy = y - 3;

    if (dir == DIR_RIGHT) {
        return dx >= 0 && (abs(dy) * 2) <= (dx * (mouth_stage + 1));
    }
    if (dir == DIR_LEFT) {
        return dx <= 0 && (abs(dy) * 2) <= ((-dx) * (mouth_stage + 1));
    }
    if (dir == DIR_UP) {
        return dy <= 0 && (abs(dx) * 2) <= ((-dy) * (mouth_stage + 1));
    }
    if (dir == DIR_DOWN) {
        return dy >= 0 && (abs(dx) * 2) <= (dy * (mouth_stage + 1));
    }
    return 0;
}

static void render_death_effect(void)
{
    const uint8_t *spr = PACMAN_SPRITE_CLOSED;
    int stage = (int)(death_acc_ms / DEATH_STAGE_MS);
    if (stage > 7) stage = 7;
    dir_t dir = render_dir(death_dir);

    for (int i = 0; i < GHOST_COUNT; i++) {
        uint8_t c = ghosts[i].fright ? COLOR_FRIGHT : ghosts[i].color;
        draw_sprite_rot_right(ghosts[i].x, ghosts[i].y, GHOST_SPRITE, c);
    }

    if (stage < 4) {
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                if (!sprite_bit(spr, x, y, dir)) continue;
                if (death_mouth_open_pixel(x, y, dir, stage)) continue;
                view_put(death_x + x, death_y + y, COLOR_PAC);
            }
        }
        return;
    }

    stage -= 4;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            if (!sprite_bit(spr, x, y, dir)) continue;
            if (death_mouth_open_pixel(x, y, dir, 3)) continue;

            int dx = x - 3;
            int dy = y - 3;
            int dist2 = dx * dx + dy * dy;
            int keep_limit = stage * 2 + 2;

            if (dist2 >= keep_limit) {
                view_put(death_x + x, death_y + y, COLOR_PAC);
            } else if (((x + y + stage) & 1) == 0) {
                int px = death_x + x + sign_i(dx) * stage;
                int py = death_y + y + sign_i(dy) * stage;
                view_put(px, py, COLOR_PAC);
            }
        }
    }
}

static void render_tunnel_debug(void)
{
#if DEBUG_TUNNEL
    for (int xv = 0; xv < VIEW_W; xv++) {
        if (!tunnel_x_allowed[xv]) continue;
        for (int yv = 0; yv < VIEW_H; yv++) {
            viewbuf[yv * VIEW_W + xv] = RGB(1,0,0);
        }
    }
#endif
}

static void render_play_screen(void)
{
#if DEBUG_TUNNEL
    memset(viewbuf, 0, sizeof(viewbuf));
    render_tunnel_debug();
    base_ready = 0;
    draw_positions_valid = 0;
    return;
#endif
    if (!base_ready) {
        memcpy(viewbuf, basebuf, sizeof(viewbuf));
        base_ready = 1;
        draw_positions_valid = 0;
    }
    restore_phys_rect_from_base(SCORE_X, SCORE_Y, SCORE_CLEAR_W, SCORE_CLEAR_H);
    render_tunnel_debug();
    render_entities();
    render_score_overlay();
}

static void render_dying_screen(void)
{
    memcpy(viewbuf, basebuf, sizeof(viewbuf));
    render_tunnel_debug();
    render_death_effect();
    render_score_overlay();
}

/* =========================
 * Entrypoint
 * ========================= */
static void pacman_run(void)
{

    uint32_t last_logic  = ms_global;
    uint32_t last_render = ms_global;
    uint32_t acc_logic   = 0;

    set_display_brightness(DEFAULT_BRIGHTNESS);

    init_panel_lut_small();
    reset_full_game();
    enter_intro();

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
                    case STATE_INTRO:
                        update_intro();
                        prev_any_key = 1;
                        break;
                    case STATE_PLAYING:
                        update_play_state(pressed_snapshot, kb_held);
                        prev_any_key = (kb_held != 0);
                        break;
                    case STATE_DYING:
                        update_dying();
                        prev_any_key = 1;
                        break;
                    case STATE_LEVEL_CLEAR:
                        update_level_clear();
                        prev_any_key = 1;
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

            if (current_state == STATE_PLAYING || current_state == STATE_LEVEL_CLEAR)
            {
                render_play_screen();
            }
            else if (current_state == STATE_DYING)
            {
                render_dying_screen();
            }
            else if (current_state == STATE_INTRO)
            {
                render_play_screen();
            }
            else if (current_state == STATE_GAME_OVER)
            {
                view_clear();
                render_game_over_text();

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

void pacman_module_entry(void)
{
    pacman_run();
}

const launcher_builtin_module_t pacman_module = {
    "builtin:pacman",
    "PACMAN MOD",
    pacman_module_entry
};
