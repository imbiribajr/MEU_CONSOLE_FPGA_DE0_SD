#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "system.h"
#include "io.h"
#include "altera_avalon_pio_regs.h"
#include "altera_avalon_uart_regs.h"
#include "altera_avalon_timer_regs.h"
#include "sys/alt_irq.h"
#include "arkanoid_module.h"


/* =========================
 * Display setup
 * ========================= */
#define PHY_W 128
#define PHY_H 64
#define VIEW_W 64
#define VIEW_H 128

#define HUD_LINE_Y 12
#define GAME_TOP_Y (HUD_LINE_Y + 1)

/* =========================
 * Timing
 * ========================= */
#define MS_TICK_US 1000
#define LOGIC_DT_MS 3
#define RENDER_DT_MS 2

/* =========================
 * Audio
 * ========================= */
#define AUDIO_ENABLED 0
#define AUDIO_USE_TIMER 0
#if AUDIO_ENABLED
#include "terasic_lib/AUDIO.h"
#include "terasic_lib/I2C.h"
#include "audio/effects.h"
#define AUDIO_SAMPLES_PER_TICK (EFFECTS_SAMPLE_RATE / 1000u)
#define AUDIO_ENABLE_DEFAULT 1
#endif

#if !AUDIO_ENABLED
#define AUDIO_SAMPLES_PER_TICK 0u
#define AUDIO_ENABLE_DEFAULT 0
#define AUDIO_Init()                 0
#define AUDIO_SetSampleRate(...)     ((void)0)
#define AUDIO_DacEnableSoftMute(...) ((void)0)
#define AUDIO_EnableByPass(...)      ((void)0)
#define AUDIO_EnableSiteTone(...)    ((void)0)
#define AUDIO_MicMute(...)           ((void)0)
#define AUDIO_LineInMute(...)        ((void)0)
#define AUDIO_SetLineOutVol(...)     ((void)0)
#define AUDIO_FifoClear()            ((void)0)
#define AUDIO_DacFifoNotFull()       0
#define AUDIO_DacFifoSetData(l, r)   ((void)(l), (void)(r))
#define effects_init()               ((void)0)
#define effects_next_sample()        ((int16_t)0)
#define effects_trigger_game_over()  ((void)0)
#define effects_trigger_click()      ((void)0)
#define effects_trigger_line_clear() ((void)0)
#define timer0_start()               ((void)0)
#define enable_interrupts()          ((void)0)
#endif

/* =========================
 * Colors
 * ========================= */
#define RGB(r,g,b)  (uint8_t)(((r)?1:0) | ((g)?2:0) | ((b)?4:0))
#define COLOR_BLACK 0
#define COLOR_WHITE 7

/* =========================
 * Arkanoid tuning
 * ========================= */
#define BRICK_COLS 8
#define BRICK_ROWS 6
#define BRICK_W (VIEW_W / BRICK_COLS)
#define BRICK_H 4
#define BRICK_START_Y (GAME_TOP_Y + 2)

#define PADDLE_W 12
#define PADDLE_H 2
#define PADDLE_Y (VIEW_H - 6)
#define PADDLE_SPEED 4

#define BALL_RADIUS 1
#define BALL_SPEED_Q8 500/* 0.94 px/tick */
#define BALL_SPEED_BOOST_Q8 800

#define NUM_LIVES 3

/* =========================
 * Fonts
 * ========================= */
static const uint16_t FONT_NUM[10] = {
    0x7B6F, 0x2C97, 0x73E7, 0x73CF, 0x5BC9,
    0x79CF, 0x79EF, 0x7249, 0x7BEF, 0x7BCF
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

static const uint8_t FONT_VHDL[7][7] = {
    {0x0E, 0x11, 0x10, 0x13, 0x11, 0x11, 0x0E},
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    {0x11, 0x1B, 0x15, 0x11, 0x11, 0x11, 0x11},
    {0x1F, 0x10, 0x10, 0x1F, 0x10, 0x10, 0x1F},
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    {0x11, 0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04},
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}
};

static uint8_t viewbuf[VIEW_W * VIEW_H];
static uint8_t prevbuf[VIEW_W * VIEW_H];
static uint8_t basebuf[VIEW_W * VIEW_H];
static uint16_t base_addr_xv[VIEW_W];

typedef enum { STATE_PLAYING, STATE_GAME_OVER } game_state_t;
static game_state_t current_state = STATE_PLAYING;

static uint8_t bricks[BRICK_ROWS][BRICK_COLS];
static uint32_t score = 0;
static uint8_t lives = NUM_LIVES;

static int paddle_x = 0;
static int paddle_dx = 0;
static int paddle_prev_x = 0;

static int ball_x_q8 = 0;
static int ball_y_q8 = 0;
static int ball_vx_q8 = 0;
static int ball_vy_q8 = 0;
static uint8_t ball_attached = 1;
static int ball_prev_x_q8 = 0;
static int ball_prev_y_q8 = 0;

static int prev_draw_paddle_x = -1;
static int prev_draw_ball_x = -1;
static int prev_draw_ball_y = -1;
static int brick_dirty = 0;
static int brick_dirty_x0 = 0;
static int brick_dirty_y0 = 0;
static int brick_dirty_w = 0;
static int brick_dirty_h = 0;

static uint32_t ms_global = 0;

/* =========================
 * Video sync
 * ========================= */
#define LED_STATUS_ADDR 8193
#define LED_FRAME_DONE  0x01

static inline void wait_frame(void) {
    while ((IORD_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_STATUS_ADDR) & LED_FRAME_DONE) == 0)
        ;
    IOWR_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_STATUS_ADDR, LED_FRAME_DONE);
}

/* =========================
 * Brightness control
 * ========================= */
#define LED_BRIGHTNESS_OFFSET 8192
#define DEFAULT_BRIGHTNESS 100

/* =========================
 * Audio state
 * ========================= */
static uint8_t audio_timer_ok = 0;
static volatile uint32_t audio_pending_ticks = 0;
#define AUDIO_PENDING_MAX 16u
static uint8_t audio_enabled = AUDIO_ENABLE_DEFAULT;
static uint8_t audio_started = 0;

/* =========================
 * Input
 * ========================= */
#define PS2_BASE PS2_INTERFACE_0_BASE
#define PS2_VALID_MASK    (1u << 31)
#define PS2_RELEASED_MASK (1u << 30)
#define PS2_EXTENDED_MASK (1u << 29)
#define PS2_CODE_MASK     0xFF

#define K_RIGHT   0x01
#define K_LEFT    0x02
#define K_LAUNCH  0x04
#define K_AUDIO_TOGGLE 0x08
#define UART_RX_BUDGET_PER_TICK 16

#define PIO_ENABLE 0

static volatile uint8_t kb_held = 0;
static volatile uint8_t kb_pressed = 0;
static volatile uint8_t ps2_held = 0;
static volatile uint8_t uart_held = 0;
static volatile uint8_t launcher_exit_req = 0;

#define FLASHBOOT_START_ADDR 0x0180C040u

/* =========================
 * Audio helpers
 * ========================= */
static void audio_init(void)
{
    if (AUDIO_Init()) {
        AUDIO_SetSampleRate(RATE_ADC48K_DAC48K);
        AUDIO_DacEnableSoftMute(false);
        AUDIO_EnableByPass(false);
        AUDIO_EnableSiteTone(false);
        AUDIO_MicMute(true);
        AUDIO_LineInMute(true);
        AUDIO_SetLineOutVol(0xFF, 0xFF);
    }
    AUDIO_FifoClear();
    effects_init();
}

static void audio_tick(void)
{
    for (uint32_t i = 0; i < AUDIO_SAMPLES_PER_TICK; i++) {
        if (!AUDIO_DacFifoNotFull()) {
            break;
        }
        int16_t s = effects_next_sample();
        AUDIO_DacFifoSetData(s, s);
    }
}

static void audio_process_pending(void)
{
    while (audio_pending_ticks > 0) {
        audio_pending_ticks--;
        audio_tick();
    }
}

static void audio_set_enabled(uint8_t enabled)
{
    audio_enabled = enabled ? 1 : 0;
    if (!audio_enabled) {
        audio_pending_ticks = 0;
        AUDIO_FifoClear();
        return;
    }
    audio_started = 1;
}

/* =========================
 * Helpers
 * ========================= */
static void set_display_brightness(uint8_t level)
{
    uint8_t corrected_pwm = GAMMA_TABLE[level];
    IOWR_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_BRIGHTNESS_OFFSET, corrected_pwm);
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

static void restore_rect_from_base(int x0, int y0, int w, int h)
{
    if (w <= 0 || h <= 0) return;
    if (x0 < 0) { w += x0; x0 = 0; }
    if (y0 < 0) { h += y0; y0 = 0; }
    if (x0 + w > VIEW_W) w = VIEW_W - x0;
    if (y0 + h > VIEW_H) h = VIEW_H - y0;
    if (w <= 0 || h <= 0) return;
    for (int y = 0; y < h; y++) {
        int row = (y0 + y) * VIEW_W + x0;
        for (int x = 0; x < w; x++) {
            viewbuf[row + x] = basebuf[row + x];
        }
    }
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

static void view_clear(void) {
    memset(viewbuf, 0, sizeof(viewbuf));
}

static void view_put(int xv, int yv, uint8_t rgb) {
    if ((unsigned)xv >= VIEW_W || (unsigned)yv >= VIEW_H) return;
    viewbuf[yv * VIEW_W + xv] = (uint8_t)(rgb & 7);
}

static void draw_vhdl_char(int char_idx, int start_x, int start_y, uint8_t color) {
    for (int row = 0; row < 7; row++) {
        uint8_t line = FONT_VHDL[char_idx][row];
        for (int col = 0; col < 5; col++) {
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

static void draw_rect_buf(uint8_t *buf, int x0, int y0, int w, int h, uint8_t color)
{
    for (int y = 0; y < h; y++) {
        int yy = y0 + y;
        if (yy < 0 || yy >= VIEW_H) continue;
        int row = yy * VIEW_W;
        for (int x = 0; x < w; x++) {
            int xx = x0 + x;
            if (xx < 0 || xx >= VIEW_W) continue;
            buf[row + xx] = color;
        }
    }
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
        if (code == 0x29) ps2_update_held(K_LAUNCH, 0);
        if (code == 0x2C) ps2_update_held(K_AUDIO_TOGGLE, 0); /* T */
    } else {
        if (code == 0x6B) ps2_update_held(K_LEFT, 0);
        if (code == 0x74) ps2_update_held(K_RIGHT, 0);
    }
}

static void ps2_break_code(uint8_t code, int extended) {
    if (!extended) {
        if (code == 0x1C) ps2_update_held(K_LEFT, 1);
        if (code == 0x23) ps2_update_held(K_RIGHT, 1);
        if (code == 0x29) ps2_update_held(K_LAUNCH, 1);
        if (code == 0x2C) ps2_update_held(K_AUDIO_TOGGLE, 1); /* T */
    } else {
        if (code == 0x6B) ps2_update_held(K_LEFT, 1);
        if (code == 0x74) ps2_update_held(K_RIGHT, 1);
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

        switch ((uint8_t)IORD_ALTERA_AVALON_UART_RXDATA(UART_0_BASE)) {
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
            case 'F':
                uart_held |= K_LAUNCH;
                kb_pressed |= K_LAUNCH;
                break;
            case 'f':
                uart_held &= (uint8_t)~K_LAUNCH;
                break;
            case 'T':
                kb_pressed |= K_AUDIO_TOGGLE;
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
    {
        uint32_t raw = IORD_ALTERA_AVALON_PIO_DATA(PIO_0_BASE) & 0xF;
        uint8_t p = (uint8_t)((~raw) & 0xF);
        if (p & 0x4) launcher_exit_req = 1;
    }
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
 * Game logic
 * ========================= */
static void reset_bricks(void)
{
    for (int r = 0; r < BRICK_ROWS; r++) {
        for (int c = 0; c < BRICK_COLS; c++) {
            bricks[r][c] = 1;
        }
    }
}

static void build_basebuf(void)
{
    memset(basebuf, 0, sizeof(basebuf));
    for (int xv = 0; xv < VIEW_W; xv++) {
        basebuf[HUD_LINE_Y * VIEW_W + xv] = RGB(0,0,1);
    }
    for (int r = 0; r < BRICK_ROWS; r++) {
        int y0 = BRICK_START_Y + r * BRICK_H;
        uint8_t color = (r & 1) ? RGB(1,0,0) : RGB(0,1,1);
        for (int c = 0; c < BRICK_COLS; c++) {
            if (!bricks[r][c]) continue;
            int x0 = c * BRICK_W;
            draw_rect_buf(basebuf, x0, y0, BRICK_W - 1, BRICK_H - 1, color);
        }
    }
    brick_dirty = 0;
}

static void reset_ball_and_paddle(void)
{
    paddle_x = (VIEW_W - PADDLE_W) / 2;
    paddle_dx = 0;
    ball_attached = 1;
    ball_x_q8 = (paddle_x + (PADDLE_W / 2)) << 8;
    ball_y_q8 = (PADDLE_Y - 2) << 8;
    ball_vx_q8 = BALL_SPEED_Q8;
    ball_vy_q8 = -BALL_SPEED_Q8;
    paddle_prev_x = paddle_x;
    ball_prev_x_q8 = ball_x_q8;
    ball_prev_y_q8 = ball_y_q8;
    prev_draw_paddle_x = -1;
    prev_draw_ball_x = -1;
    prev_draw_ball_y = -1;
}

static void force_full_redraw(void)
{
    memcpy(viewbuf, basebuf, sizeof(viewbuf));
    memset(prevbuf, 0xFF, sizeof(prevbuf));
    prev_draw_paddle_x = -1;
    prev_draw_ball_x = -1;
    prev_draw_ball_y = -1;
    brick_dirty = 0;
}

static void reset_game(void)
{
    score = 0;
    lives = NUM_LIVES;
    reset_bricks();
    reset_ball_and_paddle();
    current_state = STATE_PLAYING;
    build_basebuf();
    force_full_redraw();
}

static void apply_paddle_input(uint8_t held)
{
    paddle_dx = 0;
    if (held & K_LEFT) paddle_dx = -PADDLE_SPEED;
    if (held & K_RIGHT) paddle_dx = PADDLE_SPEED;
    paddle_x += paddle_dx;
    if (paddle_x < 0) paddle_x = 0;
    if (paddle_x > VIEW_W - PADDLE_W) paddle_x = VIEW_W - PADDLE_W;
}

static void handle_ball_collisions(void)
{
    int ball_x = ball_x_q8 >> 8;
    int ball_y = ball_y_q8 >> 8;

    if (ball_x <= 0) {
        ball_x = 0;
        ball_vx_q8 = -ball_vx_q8;
    } else if (ball_x >= VIEW_W - 1) {
        ball_x = VIEW_W - 1;
        ball_vx_q8 = -ball_vx_q8;
    }

    if (ball_y <= GAME_TOP_Y) {
        ball_y = GAME_TOP_Y;
        ball_vy_q8 = -ball_vy_q8;
    }

    if (ball_y >= VIEW_H - 1) {
        if (lives > 0) lives--;
        if (lives == 0) {
            effects_trigger_game_over();
            current_state = STATE_GAME_OVER;
        } else {
            reset_ball_and_paddle();
            force_full_redraw();
        }
        return;
    }

    int paddle_top = PADDLE_Y;
    int paddle_bottom = PADDLE_Y + PADDLE_H;
    if (ball_y >= paddle_top - BALL_RADIUS && ball_y <= paddle_bottom) {
        if (ball_x >= paddle_x - 1 && ball_x <= paddle_x + PADDLE_W + 1) {
            int rel = (ball_x - paddle_x) - (PADDLE_W / 2);
            int max_rel = PADDLE_W / 2;
            int vx = (rel * BALL_SPEED_BOOST_Q8) / (max_rel ? max_rel : 1);
            ball_vx_q8 = vx;
            ball_vy_q8 = -BALL_SPEED_Q8;
            ball_y_q8 = (paddle_top - 2) << 8;
            effects_trigger_click();
        }
    }

    for (int r = 0; r < BRICK_ROWS; r++) {
        int by = BRICK_START_Y + r * BRICK_H;
        if (ball_y < by - 1 || ball_y > by + BRICK_H) continue;
        for (int c = 0; c < BRICK_COLS; c++) {
            if (!bricks[r][c]) continue;
            int bx = c * BRICK_W;
            if (ball_x >= bx && ball_x < bx + BRICK_W) {
                bricks[r][c] = 0;
                draw_rect_buf(basebuf, bx, by, BRICK_W - 1, BRICK_H - 1, COLOR_BLACK);
                brick_dirty = 1;
                brick_dirty_x0 = bx;
                brick_dirty_y0 = by;
                brick_dirty_w = BRICK_W;
                brick_dirty_h = BRICK_H;
                score += 10;
                ball_vy_q8 = -ball_vy_q8;
                effects_trigger_line_clear();
                return;
            }
        }
    }
}

static void update_game(uint8_t held, uint8_t pressed)
{
    apply_paddle_input(held);
    if (ball_attached) {
        ball_x_q8 = (paddle_x + (PADDLE_W / 2)) << 8;
        ball_y_q8 = (PADDLE_Y - 2) << 8;
        if (pressed & K_LAUNCH) {
            ball_attached = 0;
        }
        return;
    }

    ball_x_q8 += ball_vx_q8;
    ball_y_q8 += ball_vy_q8;
    handle_ball_collisions();
}

/* =========================
 * Rendering
 * ========================= */
static void render_hud(void)
{
    uint32_t s = score;
    for (int i = 0; i < 6; i++) {
        int dig = (int)(s % 10);
        s /= 10;
        draw_digit_tall(dig, 2 + ((5 - i) * 4), 1, RGB(1,1,0));
    }
    draw_digit_tall(lives, 56, 1, RGB(1,0,0));
    for (int xv = 0; xv < VIEW_W; xv++) {
        view_put(xv, HUD_LINE_Y, RGB(0,0,1));
    }
}

static void render_paddle(int x)
{
    draw_rect(x, PADDLE_Y, PADDLE_W, PADDLE_H, RGB(1,1,1));
}

static void render_ball(int x, int y)
{
    view_put(x, y, RGB(1,1,0));
    view_put(x - 1, y, RGB(1,1,0));
    view_put(x + 1, y, RGB(1,1,0));
    view_put(x, y - 1, RGB(1,1,0));
    view_put(x, y + 1, RGB(1,1,0));
}

static void render_game_over(void)
{
    int game_ids[] = {0, 1, 2, 3};
    for (int i = 0; i < 4; i++) {
        draw_vhdl_char(game_ids[i], 8 + (i * 12), 40, COLOR_WHITE);
    }

    int over_ids[] = {4, 5, 3, 6};
    for (int i = 0; i < 4; i++) {
        draw_vhdl_char(over_ids[i], 8 + (i * 12), 58, COLOR_WHITE);
    }
}

static void arkanoid_run(void)
{
    uint32_t last_logic = ms_global;
    uint32_t last_render = ms_global;
    uint32_t acc_logic = 0;

    audio_init();
    if (AUDIO_USE_TIMER && audio_enabled) {
        timer0_start();
        enable_interrupts();
        audio_started = 1;
    }

    set_display_brightness(DEFAULT_BRIGHTNESS);
    init_panel_lut_small();
    reset_game();

    while (1) {
        usleep(MS_TICK_US);
        ms_global++;
        handle_input_merge();
        if (launcher_exit_req) {
            launcher_soft_reset();
        }

        if (audio_enabled) {
            if (audio_timer_ok) {
                audio_process_pending();
            } else {
                audio_tick();
            }
        }

        uint32_t dt = ms_global - last_logic;
        if (dt > 0) {
            if (dt > 40) dt = 40;
            last_logic = ms_global;
            acc_logic += dt;
            while (acc_logic >= LOGIC_DT_MS) {
                acc_logic -= LOGIC_DT_MS;
                uint8_t held = kb_held;
                uint8_t pressed = kb_pressed;
                kb_pressed = 0;
                if (pressed & K_AUDIO_TOGGLE) {
                    audio_set_enabled(!audio_enabled);
                }
                paddle_prev_x = paddle_x;
                ball_prev_x_q8 = ball_x_q8;
                ball_prev_y_q8 = ball_y_q8;
                if (current_state == STATE_PLAYING) {
                    update_game(held, pressed);
                } else if (current_state == STATE_GAME_OVER) {
                    if (pressed & K_LAUNCH) {
                        reset_game();
                    }
                }
            }
        }

        if ((ms_global - last_render) >= RENDER_DT_MS) {
            last_render = ms_global;
            if (current_state == STATE_GAME_OVER) {
                restore_rect_from_base(0, 0, VIEW_W, VIEW_H);
            }
            if (prev_draw_paddle_x >= 0) {
                restore_rect_from_base(prev_draw_paddle_x, PADDLE_Y, PADDLE_W, PADDLE_H);
            }
            if (prev_draw_ball_x >= 0) {
                restore_rect_from_base(prev_draw_ball_x - 1, prev_draw_ball_y - 1, 3, 3);
            }
            if (brick_dirty) {
                restore_rect_from_base(brick_dirty_x0, brick_dirty_y0, brick_dirty_w, brick_dirty_h);
                brick_dirty = 0;
            }
            restore_rect_from_base(0, 0, VIEW_W, HUD_LINE_Y + 1);
            render_hud();
            {
                uint32_t interp_ms = acc_logic;
                int paddle_draw_x = paddle_prev_x;
                int ball_draw_x = ball_x_q8 >> 8;
                int ball_draw_y = ball_y_q8 >> 8;
                if (interp_ms > LOGIC_DT_MS) interp_ms = LOGIC_DT_MS;
                if (LOGIC_DT_MS > 0) {
                    int delta_p = paddle_x - paddle_prev_x;
                    paddle_draw_x = paddle_prev_x + (delta_p * (int)interp_ms) / (int)LOGIC_DT_MS;
                    int dx_q8 = ball_x_q8 - ball_prev_x_q8;
                    int dy_q8 = ball_y_q8 - ball_prev_y_q8;
                    int ball_draw_x_q8 = ball_prev_x_q8 + (int)((dx_q8 * (int)interp_ms) / (int)LOGIC_DT_MS);
                    int ball_draw_y_q8 = ball_prev_y_q8 + (int)((dy_q8 * (int)interp_ms) / (int)LOGIC_DT_MS);
                    ball_draw_x = ball_draw_x_q8 >> 8;
                    ball_draw_y = ball_draw_y_q8 >> 8;
                }
                render_paddle(paddle_draw_x);
                render_ball(ball_draw_x, ball_draw_y);
                prev_draw_paddle_x = paddle_draw_x;
                prev_draw_ball_x = ball_draw_x;
                prev_draw_ball_y = ball_draw_y;
            }
            if (current_state == STATE_GAME_OVER) {
                render_game_over();
            }
            wait_frame();
            blit_diff_to_panel_small_lut();
        }
    }
}

void arkanoid_module_entry(void)
{
    arkanoid_run();
}

const launcher_builtin_module_t arkanoid_module = {
    "builtin:arkanoid",
    "ARKANOID MOD",
    arkanoid_module_entry
};
