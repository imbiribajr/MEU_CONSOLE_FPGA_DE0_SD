#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "system.h"
#include "io.h"
#include "altera_avalon_pio_regs.h"
#include "gdash_module.h"

/* main.c version: 1.0.1 */

/* =========================
 * Display setup
 * ========================= */
#define PHY_W 128
#define PHY_H 64

/* Buffer fisico (mantem mapeamento do painel) */
#define VIEW_W 64
#define VIEW_H 128

/* Coordenadas logicas em landscape */
#define LOGIC_W 128
#define LOGIC_H 64

/* =========================
 * Timing
 * ========================= */
#define MS_TICK_US      1000
#define LOGIC_DT_MS     3
#define RENDER_DT_MS    6

/* =========================
 * Geometry Dash style tuning
 * ========================= */
#define PLAYER_SIZE     6
#define PLAYER_X        16
#define GROUND_H        8
#define GROUND_Y        (LOGIC_H - GROUND_H)

#define SCROLL_STEP_MS  8
#define PLAYER_STEP_MS  6
#define GRAVITY_STEP    1
#define JUMP_VEL        -1
#define JUMP_HOLD_MS    120
#define JUMP_HOLD_ACCEL -1

#define OBST_MAX        8
#define OBST_SPAWN_MIN_GAP 18
#define OBST_SPAWN_MAX_GAP 34

#define SPIKE_W         7
#define SPIKE_H         6
#define BLOCK_W         8
#define BLOCK_H         10
#define OBST_W_MAX      8
#define OBST_H_MAX      10

/* =========================
 * Colors
 * ========================= */
#define RGB(r,g,b)  (uint8_t)(((r)?1:0) | ((g)?2:0) | ((b)?4:0))
#define COLOR_BLACK   0
#define COLOR_WHITE   7
#define COLOR_GROUND  RGB(0,1,0)
#define COLOR_GROUND_DARK RGB(0,0,1)
#define COLOR_PLAYER  RGB(1,1,0)
#define COLOR_SPIKE   RGB(1,0,0)
#define COLOR_BLOCK   RGB(0,0,1)
#define COLOR_HORIZON RGB(0,1,1)
#define COLOR_STAR    RGB(1,1,1)

/* =========================
 * Fonts
 * ========================= */
static const uint8_t DIGITS_3X7[10][7] = {
    {0x07, 0x05, 0x05, 0x05, 0x05, 0x05, 0x07},
    {0x02, 0x06, 0x02, 0x02, 0x02, 0x02, 0x07},
    {0x07, 0x01, 0x01, 0x07, 0x04, 0x04, 0x07},
    {0x07, 0x01, 0x01, 0x07, 0x01, 0x01, 0x07},
    {0x05, 0x05, 0x05, 0x07, 0x01, 0x01, 0x01},
    {0x07, 0x04, 0x04, 0x07, 0x01, 0x01, 0x07},
    {0x07, 0x04, 0x04, 0x07, 0x05, 0x05, 0x07},
    {0x07, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02},
    {0x07, 0x05, 0x05, 0x07, 0x05, 0x05, 0x07},
    {0x07, 0x05, 0x05, 0x07, 0x01, 0x01, 0x07}
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
static uint8_t basebuf[VIEW_W * VIEW_H];
static uint16_t base_addr_xv[VIEW_W];

#define SCORE_DIGITS 5
#define SCORE_DIGIT_W 3
#define SCORE_DIGIT_H 7
#define SCORE_DIGIT_GAP 1
#define SCORE_X 2
#define SCORE_Y 2
#define SCORE_CLEAR_W (SCORE_DIGITS * SCORE_DIGIT_W + (SCORE_DIGITS - 1) * SCORE_DIGIT_GAP)
#define SCORE_CLEAR_H SCORE_DIGIT_H

typedef enum { STATE_PLAYING, STATE_GAME_OVER } game_state_t;
static game_state_t current_state = STATE_PLAYING;

typedef struct {
    int y;
    int prev_y;
    uint32_t acc_ms;
    int vel_y;
    uint32_t jump_hold_ms;
} player_t;

typedef enum { OBST_SPIKE = 0, OBST_BLOCK = 1 } obstacle_type_t;

typedef struct {
    int x;
    int y;
    int prev_x;
    int prev_y;
    int w;
    int h;
    obstacle_type_t type;
} obstacle_t;

static player_t player;
static obstacle_t obstacles[OBST_MAX];
static int obstacle_draw_x_prev[OBST_MAX];
static int obstacle_draw_y_prev[OBST_MAX];
static int player_draw_y_prev = 0;
static uint32_t scroll_acc_ms = 0;
static int next_spawn_x = 0;
static uint32_t score = 0;

static uint16_t lfsr = 0xA5A5;
static uint32_t ms_global = 0;

static uint8_t base_ready = 0;
static uint8_t draw_positions_valid = 0;

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

static void set_display_brightness(uint8_t level)
{
    uint8_t corrected_pwm = GAMMA_TABLE[level];
    IOWR_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_BRIGHTNESS_OFFSET, corrected_pwm);
}

/* =========================
 * Input handling
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

static volatile uint8_t kb_held     = 0;
static volatile uint8_t kb_pressed  = 0;
static volatile uint8_t ps2_held    = 0;
static volatile uint8_t launcher_exit_req = 0;

#define FLASHBOOT_START_ADDR 0x0180C040u

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

static void handle_input_merge(void) {
    handle_ps2();
    uint32_t raw = IORD_ALTERA_AVALON_PIO_DATA(PIO_0_BASE) & 0xF;
    uint8_t p = (uint8_t)((~raw) & 0xF);
    if (p & 0x4) launcher_exit_req = 1;
    kb_held = ps2_held;
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
 * Helpers
 * ========================= */
static void lfsr_step(void) {
    uint16_t bit = (uint16_t)(((lfsr >> 15) ^ (lfsr >> 13)) & 1);
    lfsr = (uint16_t)((lfsr << 1) | bit);
}

static int rand_range(int minv, int maxv)
{
    lfsr_step();
    return minv + (int)(lfsr % (uint16_t)(maxv - minv + 1));
}

static inline int logic_to_xv(int lx, int ly)
{
    return (LOGIC_H - 1) - ly;
}

static inline int logic_to_yv(int lx, int ly)
{
    return lx;
}

static void view_put_raw(int xv, int yv, uint8_t rgb)
{
    if ((unsigned)xv >= VIEW_W || (unsigned)yv >= VIEW_H) return;
    viewbuf[yv * VIEW_W + xv] = (uint8_t)(rgb & 7);
}

static void base_put_raw(int xv, int yv, uint8_t rgb)
{
    if ((unsigned)xv >= VIEW_W || (unsigned)yv >= VIEW_H) return;
    basebuf[yv * VIEW_W + xv] = (uint8_t)(rgb & 7);
}

static void logic_put(int lx, int ly, uint8_t rgb)
{
    if ((unsigned)lx >= LOGIC_W || (unsigned)ly >= LOGIC_H) return;
    int xv = logic_to_xv(lx, ly);
    int yv = logic_to_yv(lx, ly);
    view_put_raw(xv, yv, rgb);
}

static void logic_base_put(int lx, int ly, uint8_t rgb)
{
    if ((unsigned)lx >= LOGIC_W || (unsigned)ly >= LOGIC_H) return;
    int xv = logic_to_xv(lx, ly);
    int yv = logic_to_yv(lx, ly);
    base_put_raw(xv, yv, rgb);
}

static void draw_rect_logic(int x0, int y0, int w, int h, uint8_t color)
{
    for (int y = 0; y < h; y++) {
        int ly = y0 + y;
        if ((unsigned)ly >= LOGIC_H) continue;
        for (int x = 0; x < w; x++) {
            int lx = x0 + x;
            if ((unsigned)lx >= LOGIC_W) continue;
            logic_put(lx, ly, color);
        }
    }
}

static void draw_rect_base_logic(int x0, int y0, int w, int h, uint8_t color)
{
    for (int y = 0; y < h; y++) {
        int ly = y0 + y;
        if ((unsigned)ly >= LOGIC_H) continue;
        for (int x = 0; x < w; x++) {
            int lx = x0 + x;
            if ((unsigned)lx >= LOGIC_W) continue;
            logic_base_put(lx, ly, color);
        }
    }
}

static void restore_rect_from_base_logic(int x0, int y0, int w, int h)
{
    for (int y = 0; y < h; y++) {
        int ly = y0 + y;
        if ((unsigned)ly >= LOGIC_H) continue;
        for (int x = 0; x < w; x++) {
            int lx = x0 + x;
            if ((unsigned)lx >= LOGIC_W) continue;
            int xv = logic_to_xv(lx, ly);
            int yv = logic_to_yv(lx, ly);
            if ((unsigned)xv >= VIEW_W || (unsigned)yv >= VIEW_H) continue;
            viewbuf[yv * VIEW_W + xv] = basebuf[yv * VIEW_W + xv];
        }
    }
}

static void view_clear(void) {
    memset(viewbuf, 0, sizeof(viewbuf));
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

static void mark_full_redraw(void)
{
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

static int interp_pos(int prev, int cur, uint32_t acc_ms, uint32_t step_ms)
{
    if (prev == cur) return cur;
    if (step_ms == 0 || step_ms <= LOGIC_DT_MS) return cur;
    if (acc_ms > step_ms) acc_ms = step_ms;
    uint32_t t = (acc_ms * 256u) / step_ms;
    if (t > 255u) t = 255u;
    return prev + ((cur - prev) * (int)t) / 256;
}

static void draw_digit_3x7(int digit, int start_x, int start_y, uint8_t color)
{
    if (digit < 0 || digit > 9) return;
    for (int y = 0; y < SCORE_DIGIT_H; y++) {
        uint8_t row = DIGITS_3X7[digit][y];
        for (int x = 0; x < 3; x++) {
            if ((row >> (2 - x)) & 1) {
                logic_put(start_x + x, start_y + y, color);
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
                logic_put(px,   py,   color);
                logic_put(px+1, py,   color);
                logic_put(px,   py+1, color);
                logic_put(px+1, py+1, color);
            }
        }
    }
}

/* =========================
 * Base buffer (static background)
 * ========================= */
static void build_basebuf(void)
{
    memset(basebuf, COLOR_BLACK, sizeof(basebuf));

    for (int x = 0; x < LOGIC_W; x++) {
        logic_base_put(x, GROUND_Y - 1, COLOR_HORIZON);
    }

    for (int y = GROUND_Y; y < LOGIC_H; y++) {
        uint8_t c = ((y - GROUND_Y) & 1) ? COLOR_GROUND_DARK : COLOR_GROUND;
        draw_rect_base_logic(0, y, LOGIC_W, 1, c);
    }

    for (int y = 6; y < (GROUND_Y - 6); y += 10) {
        for (int x = 6; x < LOGIC_W; x += 18) {
            logic_base_put(x, y, COLOR_STAR);
        }
    }
}

/* =========================
 * Game logic
 * ========================= */
static void spawn_obstacle(obstacle_t *o, int x)
{
    o->type = (rand_range(0, 1) == 0) ? OBST_SPIKE : OBST_BLOCK;
    if (o->type == OBST_SPIKE) {
        o->w = SPIKE_W;
        o->h = SPIKE_H;
    } else {
        o->w = BLOCK_W;
        o->h = BLOCK_H;
    }
    o->x = x;
    o->y = GROUND_Y - o->h;
    o->prev_x = o->x;
    o->prev_y = o->y;
}

static void init_obstacles(void)
{
    next_spawn_x = LOGIC_W + 20;
    for (int i = 0; i < OBST_MAX; i++) {
        spawn_obstacle(&obstacles[i], next_spawn_x);
        obstacle_draw_x_prev[i] = obstacles[i].x;
        obstacle_draw_y_prev[i] = obstacles[i].y;
        next_spawn_x += rand_range(OBST_SPAWN_MIN_GAP, OBST_SPAWN_MAX_GAP);
    }
}

static void reset_game(void)
{
    player.y = GROUND_Y - PLAYER_SIZE;
    player.prev_y = player.y;
    player.acc_ms = 0;
    player.vel_y = 0;

    scroll_acc_ms = 0;
    score = 0;

    build_basebuf();
    base_ready = 0;
    draw_positions_valid = 0;

    init_obstacles();
}

static int rects_overlap(int ax0, int ay0, int aw, int ah, int bx0, int by0, int bw, int bh)
{
    int ax1 = ax0 + aw - 1;
    int ay1 = ay0 + ah - 1;
    int bx1 = bx0 + bw - 1;
    int by1 = by0 + bh - 1;
    if (ax1 < bx0 || ax0 > bx1 || ay1 < by0 || ay0 > by1) return 0;
    return 1;
}

static void update_player(uint8_t pressed_edge, uint8_t held)
{
    int on_ground = (player.y >= (GROUND_Y - PLAYER_SIZE));
    if (on_ground) player.y = GROUND_Y - PLAYER_SIZE;

    if (pressed_edge & (K_SPACE | K_ROT)) {
        if (on_ground) {
            player.vel_y = JUMP_VEL;
            player.jump_hold_ms = JUMP_HOLD_MS;
        }
    }

    player.acc_ms += LOGIC_DT_MS;
    while (player.acc_ms >= PLAYER_STEP_MS) {
        player.acc_ms -= PLAYER_STEP_MS;
        player.prev_y = player.y;
        if ((held & (K_SPACE | K_ROT)) && player.jump_hold_ms > 0) {
            player.vel_y += JUMP_HOLD_ACCEL;
            if (player.jump_hold_ms > LOGIC_DT_MS) player.jump_hold_ms -= LOGIC_DT_MS;
            else player.jump_hold_ms = 0;
        } else {
            player.jump_hold_ms = 0;
        }

        player.vel_y += GRAVITY_STEP;
        player.y += player.vel_y;
        if (player.y >= (GROUND_Y - PLAYER_SIZE)) {
            player.y = GROUND_Y - PLAYER_SIZE;
            player.vel_y = 0;
            player.jump_hold_ms = 0;
        }
    }
}

static void update_scroll(void)
{
    scroll_acc_ms += LOGIC_DT_MS;
    while (scroll_acc_ms >= SCROLL_STEP_MS) {
        scroll_acc_ms -= SCROLL_STEP_MS;
        score++;
        for (int i = 0; i < OBST_MAX; i++) {
            obstacles[i].prev_x = obstacles[i].x;
            obstacles[i].prev_y = obstacles[i].y;
            obstacles[i].x -= 1;
            if ((obstacles[i].x + obstacles[i].w) < 0) {
                spawn_obstacle(&obstacles[i], next_spawn_x);
                next_spawn_x += rand_range(OBST_SPAWN_MIN_GAP, OBST_SPAWN_MAX_GAP);
            }
        }
    }
}

static int check_collisions(void)
{
    int px = PLAYER_X;
    int py = player.y;
    for (int i = 0; i < OBST_MAX; i++) {
        obstacle_t *o = &obstacles[i];
        if (rects_overlap(px, py, PLAYER_SIZE, PLAYER_SIZE, o->x, o->y, o->w, o->h))
            return 1;
    }
    return 0;
}

static void update_play_state(uint8_t pressed_edge, uint8_t held)
{
    update_player(pressed_edge, held);
    update_scroll();
    if (check_collisions()) {
        current_state = STATE_GAME_OVER;
        base_ready = 0;
        draw_positions_valid = 0;
        mark_full_redraw();
    }
}

/* =========================
 * Rendering
 * ========================= */
static void draw_player(int x, int y)
{
    draw_rect_logic(x, y, PLAYER_SIZE, PLAYER_SIZE, COLOR_PLAYER);
}

static void draw_spike(int x, int y)
{
    for (int row = 0; row < SPIKE_H; row++) {
        int half = row / 2;
        int left = (SPIKE_W / 2) - half;
        int right = (SPIKE_W / 2) + half;
        for (int col = left; col <= right; col++) {
            logic_put(x + col, y + row, COLOR_SPIKE);
        }
    }
}

static void draw_block(int x, int y)
{
    draw_rect_logic(x, y, BLOCK_W, BLOCK_H, COLOR_BLOCK);
}

static void draw_score(void)
{
    uint32_t s = score;
    for (int i = 0; i < SCORE_DIGITS; i++) {
        int dig = (int)(s % 10);
        s /= 10;
        draw_digit_3x7(dig,
                       SCORE_X + ((SCORE_DIGITS - 1 - i) * (SCORE_DIGIT_W + SCORE_DIGIT_GAP)),
                       SCORE_Y,
                       COLOR_WHITE);
    }
}

static void render_play_screen(void)
{
    if (!base_ready) {
        memcpy(viewbuf, basebuf, sizeof(viewbuf));
        base_ready = 1;
        draw_positions_valid = 0;
    }

    if (draw_positions_valid) {
        restore_rect_from_base_logic(PLAYER_X, player_draw_y_prev, PLAYER_SIZE, PLAYER_SIZE);
        for (int i = 0; i < OBST_MAX; i++) {
            restore_rect_from_base_logic(obstacle_draw_x_prev[i], obstacle_draw_y_prev[i], OBST_W_MAX, OBST_H_MAX);
        }
    }

    restore_rect_from_base_logic(SCORE_X, SCORE_Y, SCORE_CLEAR_W, SCORE_CLEAR_H);

    int player_draw_y = interp_pos(player.prev_y, player.y, player.acc_ms, PLAYER_STEP_MS);
    draw_player(PLAYER_X, player_draw_y);
    player_draw_y_prev = player_draw_y;

    for (int i = 0; i < OBST_MAX; i++) {
        obstacle_t *o = &obstacles[i];
        int draw_x = interp_pos(o->prev_x, o->x, scroll_acc_ms, SCROLL_STEP_MS);
        int draw_y = o->y;
        if (o->type == OBST_SPIKE) draw_spike(draw_x, draw_y);
        else draw_block(draw_x, draw_y);
        obstacle_draw_x_prev[i] = draw_x;
        obstacle_draw_y_prev[i] = draw_y;
    }

    draw_score();
    draw_positions_valid = 1;
}

static void render_game_over(void)
{
    view_clear();
    int game_ids[] = {0, 1, 2, 3};
    int over_ids[] = {4, 5, 3, 6};
    for (int i = 0; i < 4; i++) {
        draw_vhdl_char(game_ids[i], 20 + (i * 20), 20, COLOR_WHITE);
        draw_vhdl_char(over_ids[i], 20 + (i * 20), 40, COLOR_WHITE);
    }
}

/* =========================
 * Main
 * ========================= */
static void gdash_run(void)
{
    uint32_t last_logic  = ms_global;
    uint32_t last_render = ms_global;
    uint32_t acc_logic   = 0;

    set_display_brightness(DEFAULT_BRIGHTNESS);

    init_panel_lut_small();
    reset_game();

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
                        break;
                    case STATE_GAME_OVER:
                        if (pressed_snapshot) {
                            reset_game();
                            current_state = STATE_PLAYING;
                        }
                        break;
                }
            }

            if (steps == MAX_STEPS)
                acc_logic = 0;
        }

        if ((ms_global - last_render) >= RENDER_DT_MS)
        {
            last_render = ms_global;

            if (current_state == STATE_PLAYING) {
                render_play_screen();
            } else {
                render_game_over();
            }

            wait_frame();
            blit_diff_to_panel_small_lut();
        }
    }
}

void gdash_module_entry(void)
{
    gdash_run();
}

const launcher_builtin_module_t gdash_module = {
    "builtin:gdash",
    "GDASH MOD",
    gdash_module_entry
};
