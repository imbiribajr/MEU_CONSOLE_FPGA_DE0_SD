#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "system.h"
#include "io.h"
#include "altera_avalon_pio_regs.h"
#include "pong_module.h"

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
#define CELL_PX 4   // - tamanho do tabuleiro

#define GAME_H_PX (GRID_H * CELL_PX)
#define GAME_BOTTOM_Y (GAME_TOP_Y + GAME_H_PX - 1)

/* =========================
 * Timing
 * ========================= */
#define MS_TICK_US      1000
#define LOGIC_DT_MS     3
#define RENDER_DT_MS    8 // 125 Hz, set to 16 for ~60 Hz

/* =========================
 * Pong tuning
 * ========================= */
#define PADDLE_W_PX     16
#define PADDLE_H_PX     2
#define BALL_SIZE_PX    2

#define PADDLE_SPEED_FP 200  /* 0.78 px per logic tick */
#define AI_SPEED_FP     160
#define BALL_SPEED_FP_X 100 //120
#define BALL_SPEED_FP_Y 120 //140
#define BALL_SPEEDUP_FP 10
#define BALL_SPEED_MAX_FP 200
#define BALL_SPEED_MIN_X_FP 40

#define SCORE_TO_WIN    9

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

static uint16_t lfsr = 0xA5A5;

static uint32_t ms_global = 0;
static uint32_t logic_acc_ms = 0;

static int32_t paddle_top_x_fp = 0;
static int32_t paddle_bottom_x_fp = 0;
static int32_t ball_x_fp = 0;
static int32_t ball_y_fp = 0;
static int32_t ball_vx_fp = 0;
static int32_t ball_vy_fp = 0;
static int32_t ball_prev_x_fp = 0;
static int32_t ball_prev_y_fp = 0;

static uint8_t score_top = 0;
static uint8_t score_bottom = 0;
static int8_t serve_dir = 1;
static uint8_t game_started = 0;
static uint8_t input_locked = 0;

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

static volatile uint8_t kb_held     = 0;
static volatile uint8_t kb_pressed  = 0;
static volatile uint8_t ps2_held    = 0;
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

static void draw_ball_round(int x0, int y0, uint8_t color)
{
    if (BALL_SIZE_PX == 3) {
        view_put(x0 + 1, y0 + 0, color);
        view_put(x0 + 0, y0 + 1, color);
        view_put(x0 + 1, y0 + 1, color);
        view_put(x0 + 2, y0 + 1, color);
        view_put(x0 + 1, y0 + 2, color);
        return;
    }
    draw_rect(x0, y0, BALL_SIZE_PX, BALL_SIZE_PX, color);
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
 * Pong logic
 * ========================= */
static int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void reset_ball_wait(int dir)
{
    int32_t cx = (VIEW_W / 2) - (BALL_SIZE_PX / 2);
    int32_t cy = GAME_TOP_Y + (GAME_H_PX / 2) - (BALL_SIZE_PX / 2);
    ball_x_fp = cx << 8;
    ball_y_fp = cy << 8;
    ball_prev_x_fp = ball_x_fp;
    ball_prev_y_fp = ball_y_fp;
    ball_vx_fp = 0;
    ball_vy_fp = 0;
    serve_dir = (dir >= 0) ? 1 : -1;
    game_started = 0;
    input_locked = 1;
}

static void serve_ball(void)
{
    int sign = (lfsr & 1) ? 1 : -1;
    ball_vx_fp = sign * BALL_SPEED_FP_X;
    ball_vy_fp = serve_dir * BALL_SPEED_FP_Y;
    game_started = 1;
}

static void reset_full_game(void)
{
    score_top = 0;
    score_bottom = 0;

    int32_t center_x_fp = ((VIEW_W - PADDLE_W_PX) / 2) << 8;
    paddle_top_x_fp = center_x_fp;
    paddle_bottom_x_fp = center_x_fp;

    reset_ball_wait(1);
}

static void enter_game_over(void)
{
    current_state = STATE_GAME_OVER_FADE;
    fade_brightness = DEFAULT_BRIGHTNESS;
    fade_acc_ms = 0;
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
        if (pressed_edge) {
            serve_ball();
        } else {
            return;
        }
    }

    ball_prev_x_fp = ball_x_fp;
    ball_prev_y_fp = ball_y_fp;

    if ((held & K_LEFT) || (held & K_ROT)) {
        paddle_bottom_x_fp -= PADDLE_SPEED_FP;
    } else if ((held & K_RIGHT) || (held & K_DOWN)) {
        paddle_bottom_x_fp += PADDLE_SPEED_FP;
    }

    int32_t min_x_fp = (1 << 8);
    int32_t max_x_fp = ((VIEW_W - 1 - PADDLE_W_PX) << 8);
    paddle_bottom_x_fp = clamp_i32(paddle_bottom_x_fp, min_x_fp, max_x_fp);

    int32_t ball_x_px = ball_x_fp >> 8;
    int32_t paddle_top_center = (paddle_top_x_fp >> 8) + (PADDLE_W_PX / 2);
    if (ball_x_px > paddle_top_center + 3) paddle_top_x_fp += AI_SPEED_FP;
    else if (ball_x_px < paddle_top_center - 3) paddle_top_x_fp -= AI_SPEED_FP;
    paddle_top_x_fp = clamp_i32(paddle_top_x_fp, min_x_fp, max_x_fp);

    int32_t max_abs = ball_vx_fp >= 0 ? ball_vx_fp : -ball_vx_fp;
    int32_t abs_vy = ball_vy_fp >= 0 ? ball_vy_fp : -ball_vy_fp;
    if (abs_vy > max_abs) max_abs = abs_vy;
    int steps = 1 + (max_abs >> 8);
    if (steps > 6) steps = 6;

    int top_y = GAME_TOP_Y;
    int bottom_y = GAME_BOTTOM_Y - PADDLE_H_PX + 1;
    int top_x = paddle_top_x_fp >> 8;
    int bottom_x = paddle_bottom_x_fp >> 8;

    for (int s = 0; s < steps; s++) {
        int32_t step_vx = ball_vx_fp / steps;
        int32_t step_vy = ball_vy_fp / steps;
        ball_x_fp += step_vx;
        ball_y_fp += step_vy;

        int32_t ball_x = ball_x_fp >> 8;
        int32_t ball_y = ball_y_fp >> 8;

        if (ball_x_fp <= (1 << 8) && ball_vx_fp < 0) {
            ball_x_fp = (1 << 8);
            if (ball_vx_fp > -BALL_SPEED_MIN_X_FP) ball_vx_fp = -BALL_SPEED_MIN_X_FP;
            ball_vx_fp = -ball_vx_fp;
        } else if (ball_x_fp >= ((VIEW_W - 1 - BALL_SIZE_PX) << 8) && ball_vx_fp > 0) {
            ball_x_fp = ((VIEW_W - 1 - BALL_SIZE_PX) << 8);
            if (ball_vx_fp < BALL_SPEED_MIN_X_FP) ball_vx_fp = BALL_SPEED_MIN_X_FP;
            ball_vx_fp = -ball_vx_fp;
        }

        if (ball_vy_fp < 0 && ball_y <= top_y + PADDLE_H_PX) {
            if (ball_x + BALL_SIZE_PX >= top_x &&
                ball_x <= top_x + PADDLE_W_PX) {
                ball_y = top_y + PADDLE_H_PX;
                ball_y_fp = ball_y << 8;
                ball_vy_fp = -ball_vy_fp;

                int32_t center = top_x + (PADDLE_W_PX / 2);
                int32_t offset = (ball_x + (BALL_SIZE_PX / 2)) - center;
                ball_vx_fp += offset * 8;
                if (ball_vx_fp > BALL_SPEED_MAX_FP) ball_vx_fp = BALL_SPEED_MAX_FP;
                if (ball_vx_fp < -BALL_SPEED_MAX_FP) ball_vx_fp = -BALL_SPEED_MAX_FP;
                if (ball_vx_fp > 0 && ball_vx_fp < BALL_SPEED_MIN_X_FP) ball_vx_fp = BALL_SPEED_MIN_X_FP;
                if (ball_vx_fp < 0 && ball_vx_fp > -BALL_SPEED_MIN_X_FP) ball_vx_fp = -BALL_SPEED_MIN_X_FP;

                if (ball_vy_fp > 0 && ball_vy_fp < BALL_SPEED_MAX_FP) ball_vy_fp += BALL_SPEEDUP_FP;
            }
        }

        if (ball_vy_fp > 0 && ball_y + BALL_SIZE_PX >= bottom_y) {
            if (ball_x + BALL_SIZE_PX >= bottom_x &&
                ball_x <= bottom_x + PADDLE_W_PX) {
                ball_y = bottom_y - BALL_SIZE_PX;
                ball_y_fp = ball_y << 8;
                ball_vy_fp = -ball_vy_fp;

                int32_t center = bottom_x + (PADDLE_W_PX / 2);
                int32_t offset = (ball_x + (BALL_SIZE_PX / 2)) - center;
                ball_vx_fp += offset * 8;
                if (ball_vx_fp > BALL_SPEED_MAX_FP) ball_vx_fp = BALL_SPEED_MAX_FP;
                if (ball_vx_fp < -BALL_SPEED_MAX_FP) ball_vx_fp = -BALL_SPEED_MAX_FP;
                if (ball_vx_fp > 0 && ball_vx_fp < BALL_SPEED_MIN_X_FP) ball_vx_fp = BALL_SPEED_MIN_X_FP;
                if (ball_vx_fp < 0 && ball_vx_fp > -BALL_SPEED_MIN_X_FP) ball_vx_fp = -BALL_SPEED_MIN_X_FP;

                if (ball_vy_fp < 0 && -ball_vy_fp < BALL_SPEED_MAX_FP) ball_vy_fp -= BALL_SPEEDUP_FP;
            }
        }

        ball_y = ball_y_fp >> 8;
        if (ball_y < (GAME_TOP_Y - BALL_SIZE_PX)) {
            score_bottom++;
            if (score_bottom >= SCORE_TO_WIN) {
                enter_game_over();
                return;
            }
            reset_ball_wait(-1);
            return;
        } else if (ball_y > GAME_BOTTOM_Y) {
            score_top++;
            if (score_top >= SCORE_TO_WIN) {
                enter_game_over();
                return;
            }
            reset_ball_wait(1);
            return;
        }
    }
}

/* =========================
 * Rendering
 * ========================= */
static void render_play_screen(void)
{
    draw_digit_tall(score_top % 10, 8, 1, RGB(0,1,0));
    draw_digit_tall(score_bottom % 10, 52, 1, RGB(0,1,1));


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

    int top_y = GAME_TOP_Y;
    int bottom_y = GAME_BOTTOM_Y - PADDLE_H_PX;
    int top_x = paddle_top_x_fp >> 8;
    int bottom_x = paddle_bottom_x_fp >> 8;

    draw_rect(top_x, top_y, PADDLE_W_PX, PADDLE_H_PX, RGB(0,1,0));
    draw_rect(bottom_x, bottom_y, PADDLE_W_PX, PADDLE_H_PX, RGB(0,1,1));

    uint16_t interp = 0;
    if (LOGIC_DT_MS > 0) {
        uint32_t acc = logic_acc_ms;
        if (acc > LOGIC_DT_MS) acc = LOGIC_DT_MS;
        interp = (uint16_t)((acc * 256u) / LOGIC_DT_MS);
        if (interp > 255) interp = 255;
    }

    int32_t bx_fp = ball_prev_x_fp + ((ball_x_fp - ball_prev_x_fp) * (int32_t)interp) / 256;
    int32_t by_fp = ball_prev_y_fp + ((ball_y_fp - ball_prev_y_fp) * (int32_t)interp) / 256;
    int ball_x = bx_fp >> 8;
    int ball_y = by_fp >> 8;
    draw_ball_round(ball_x, ball_y, RGB(1,1,1));
}

static void pong_run(void)
{
    uint32_t last_logic  = ms_global;
    uint32_t last_render = ms_global;
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
            logic_acc_ms += dt;

            uint32_t steps = 0;
            while (logic_acc_ms >= LOGIC_DT_MS && steps < MAX_STEPS)
            {
                logic_acc_ms -= LOGIC_DT_MS;
                steps++;

                uint8_t pressed_snapshot = kb_pressed;
                kb_pressed = 0;

                switch (current_state)
                {
                    case STATE_PLAYING:
                        update_play_state(pressed_snapshot, kb_held);
                        break;

                    case STATE_GAME_OVER_FADE:
                        if (update_game_over_fade()) {
                            input_locked = 1;
                        }
                        break;

                    case STATE_GAME_OVER:
                        if (input_locked && kb_held == 0) input_locked = 0;
                        if (!input_locked && kb_held != 0) {
                            reset_full_game();
                            current_state = STATE_PLAYING;
                            game_over_armed = 0;
                        }
                        break;
                }
            }

            if (steps == MAX_STEPS)
                logic_acc_ms = 0;
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

void pong_module_entry(void)
{
    pong_run();
}

const launcher_builtin_module_t pong_module = {
    "builtin:pong",
    "PONG MOD",
    pong_module_entry
};
