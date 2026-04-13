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
#include "spaceinvaders_module.h"

/* =========================
 * Display setup
 * ========================= */
#define PHY_W 128        // largura fisica (painel)
#define PHY_H 64         // altura fisica (painel)
#define VIEW_W 64        // largura virtual usada no jogo
#define VIEW_H 128       // altura virtual usada no jogo

#define HUD_LINE_Y 14    // linha divisoria do HUD
#define GAME_TOP_Y 15    // Y do topo da area de jogo

/* =========================
 * Timing
 * ========================= */
#define MS_TICK_US      1000 // tick base (us)
#define LOGIC_DT_MS     3    // passo fixo da logica (ms)
#define RENDER_DT_MS    8    // intervalo de render (ms)

/* =========================
 * Colors
 * ========================= */
#define RGB(r,g,b)  (uint8_t)(((r)?1:0) | ((g)?2:0) | ((b)?4:0))
#define COLOR_BLACK 0
#define COLOR_WHITE 7

/* =========================
 * Space Invaders tuning
 * ========================= */
#define INV_ROWS 3            // numero de fileiras de aliens
#define INV_COLS 5            // numero de aliens por fileira
#define INV_W    8            // largura do sprite do alien (px)
#define INV_H    8            // altura do sprite do alien (px)
#define INV_X_GAP 4            // espaco horizontal entre aliens (px)
#define INV_Y_GAP 4            // espaco vertical entre aliens (px)
#define INV_STEP_Y 2           // descida ao bater na borda (px)
#define INV_BASE_SPEED_MS 40  // tempo base do passo lateral (ms)
#define INV_MIN_SPEED_MS  20   // tempo minimo do passo lateral (ms)

#define PLAYER_W 8             // largura do sprite do jogador (px)
#define PLAYER_H 8             // altura do sprite do jogador (px)
#define PLAYER_MOVE_MS 3       // cadencia do movimento (ms)
#define PLAYER_Y (VIEW_H - PLAYER_H - 2) // Posição Y da base do jogador (altura da nave)
#define PLAYER_SHOT_MS 50     // periodo de tiro continuo (ms)

#define PLAYER_BULLET_SPEED 3  // velocidade do tiro do jogador (px/tick)
#define ENEMY_BULLET_SPEED  1  // velocidade do tiro inimigo (px/tick)
#define ENEMY_BULLETS 2         // maximo de tiros inimigos simultaneos
#define ENEMY_SHOT_MS 180       // intervalo de tiro inimigo (ms)

#define SHIELD_COUNT 3          // numero de escudos
#define SHIELD_W 12             // largura do escudo (px)
#define SHIELD_H 6              // altura do escudo (px)
#define SHIELD_Y (VIEW_H - 40)  // Y da base do escudo

#define SCORE_BONUS_WAVE 200    // bonus ao limpar uma onda
#define MAX_LEVEL 9             // nivel maximo de dificuldade

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

static uint8_t viewbuf[VIEW_W * VIEW_H];
static uint8_t prevbuf[VIEW_W * VIEW_H];
static uint16_t base_addr_xv[VIEW_W];

typedef enum { STATE_PLAYING, STATE_GAME_OVER_FADE, STATE_GAME_OVER } game_state_t;
static game_state_t current_state = STATE_PLAYING;

typedef struct { int x; int y; int active; } bullet_t;

static uint8_t inv_alive[INV_ROWS][INV_COLS];
static int inv_x_off = 0;
static int inv_prev_x_off = 0;
static int inv_y_off = 0;
static int inv_dir = 1;
static uint32_t inv_move_acc_ms = 0;
static uint32_t inv_move_limit_ms = INV_BASE_SPEED_MS;

static uint32_t enemy_shot_acc_ms = 0;
static bullet_t player_bullet = {0, 0, 0};
static bullet_t enemy_bullets[ENEMY_BULLETS];

static int player_x = 0;
static int player_prev_x = 0;
static uint32_t player_move_acc_ms = 0;
static uint32_t player_shot_acc_ms = 0;

static uint8_t shield[SHIELD_COUNT][SHIELD_H][SHIELD_W];

static uint32_t score = 0;
static uint8_t lives = 3;
static uint8_t level = 1;

static uint8_t inv_anim = 0;

static uint16_t lfsr = 0xA5A5;
static uint32_t ms_global = 0;

static uint8_t input_locked = 0;
static uint8_t game_started = 0;
static uint8_t prev_any_key = 0;

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
static volatile uint8_t uart_held   = 0;
static volatile uint8_t launcher_exit_req = 0;

#define FLASHBOOT_START_ADDR 0x0180C040u

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
                break;
            case 'R':
                uart_held |= K_RIGHT;
                kb_pressed |= K_RIGHT;
                break;
            case 'r':
                uart_held &= (uint8_t)~K_RIGHT;
                break;
            case 'T':
                uart_held |= K_SPACE;
                kb_pressed |= K_SPACE;
                break;
            case 't':
                uart_held &= (uint8_t)~K_SPACE;
                break;
            default:
                break;
        }
    }
}

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
 * Game helpers
 * ========================= */
static const uint8_t ALIEN1_SPRITE_A[INV_H] = {
    0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x24, 0x5A, 0xA5
};
static const uint8_t ALIEN1_SPRITE_B[INV_H] = {
    0x18, 0x3C, 0x7E, 0xDB, 0xFF, 0x24, 0xA5, 0x5A
};
static const uint8_t ALIEN2_SPRITE_A[INV_H] = {
    0x24, 0x7E, 0xDB, 0xFF, 0xFF, 0x5A, 0xA5, 0x24
};
static const uint8_t ALIEN2_SPRITE_B[INV_H] = {
    0x24, 0x7E, 0xDB, 0xFF, 0xFF, 0x24, 0x5A, 0xA5
};
static const uint8_t ALIEN3_SPRITE_A[INV_H] = {
    0x3C, 0x7E, 0xDB, 0xFF, 0xFF, 0x24, 0x24, 0x42
};
static const uint8_t ALIEN3_SPRITE_B[INV_H] = {
    0x3C, 0x7E, 0xDB, 0xFF, 0xFF, 0x42, 0x24, 0x24
};

static const uint8_t PLAYER_SPRITE[PLAYER_H] = {
    0x18, 0x3C, 0x3C, 0x7E, 0xFF, 0xFF, 0xBD, 0x81
};

static const uint8_t SHIELD_PATTERN[SHIELD_H][SHIELD_W] = {
    {0,1,1,1,1,1,1,1,1,1,1,0},
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,0,0,0,0,1,1,1,1},
    {1,1,1,0,0,0,0,0,0,1,1,1}
};

static int invader_count(void)
{
    int count = 0;
    for (int r = 0; r < INV_ROWS; r++)
        for (int c = 0; c < INV_COLS; c++)
            if (inv_alive[r][c]) count++;
    return count;
}

static int invader_speed_ms(int count)
{
    int max_inv = INV_ROWS * INV_COLS;
    int dec = (max_inv - count) * 3;
    int base = (int)INV_BASE_SPEED_MS - ((int)(level - 1) * 8);
    int speed = base - dec;
    if (speed < INV_MIN_SPEED_MS) speed = INV_MIN_SPEED_MS;
    return speed;
}

static void reset_shields(void)
{
    for (int s = 0; s < SHIELD_COUNT; s++)
        for (int y = 0; y < SHIELD_H; y++)
            for (int x = 0; x < SHIELD_W; x++)
                shield[s][y][x] = SHIELD_PATTERN[y][x];
}


static void reset_invaders(void)
{
    for (int r = 0; r < INV_ROWS; r++)
        for (int c = 0; c < INV_COLS; c++)
            inv_alive[r][c] = 1;

    inv_x_off = 0;
    inv_prev_x_off = 0;
    inv_y_off = 0;
    inv_dir = 1;
    inv_move_acc_ms = 0;
    inv_move_limit_ms = invader_speed_ms(INV_ROWS * INV_COLS);
    inv_anim = 0;
}

static void reset_player(void)
{
    player_x = (VIEW_W - PLAYER_W) / 2;
    player_prev_x = player_x;
    player_move_acc_ms = 0;
    player_bullet.active = 0;
    for (int i = 0; i < ENEMY_BULLETS; i++)
        enemy_bullets[i].active = 0;
}

static void reset_full_game(void)
{
    score = 0;
    lives = 3;
    level = 1;
    reset_invaders();
    reset_shields();
    reset_player();
    game_started = 0;
    input_locked = 1;
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

static void draw_invader_sprite(int x0, int y0, const uint8_t *sprite, uint8_t color)
{
    for (int y = 0; y < INV_H; y++) {
        uint8_t row = sprite[y];
        for (int x = 0; x < INV_W; x++) {
            if ((row >> (INV_W - 1 - x)) & 1)
                view_put(x0 + x, y0 + y, color);
        }
    }
}

static void draw_player_sprite(int x0, int y0, uint8_t color)
{
    for (int y = 0; y < PLAYER_H; y++) {
        uint8_t row = PLAYER_SPRITE[y];
        for (int x = 0; x < PLAYER_W; x++) {
            if ((row >> (PLAYER_W - 1 - x)) & 1)
                view_put(x0 + x, y0 + y, color);
        }
    }
}

static int shield_hit(int x, int y)
{
    for (int s = 0; s < SHIELD_COUNT; s++) {
        int sx = 6 + s * (SHIELD_W + 8);
        if (x < sx || x >= sx + SHIELD_W) continue;
        if (y < SHIELD_Y || y >= SHIELD_Y + SHIELD_H) continue;
        int lx = x - sx;
        int ly = y - SHIELD_Y;
        if (shield[s][ly][lx]) {
            shield[s][ly][lx] = 0;
            return 1;
        }
    }
    return 0;
}

static void fire_player_bullet(void)
{
    if (player_bullet.active) return;
    player_bullet.x = player_x + (PLAYER_W / 2);
    player_bullet.y = PLAYER_Y - 1;
    player_bullet.active = 1;
}

static void maybe_fire_enemy(void)
{
    enemy_shot_acc_ms += LOGIC_DT_MS;
    if (enemy_shot_acc_ms < ENEMY_SHOT_MS) return;
    enemy_shot_acc_ms = 0;

    int slot = -1;
    for (int i = 0; i < ENEMY_BULLETS; i++) {
        if (!enemy_bullets[i].active) { slot = i; break; }
    }
    if (slot < 0) return;

    lfsr_step();
    int col = lfsr % INV_COLS;

    for (int r = INV_ROWS - 1; r >= 0; r--) {
        if (!inv_alive[r][col]) continue;
        int start_x = (VIEW_W - (INV_COLS * INV_W + (INV_COLS - 1) * INV_X_GAP)) / 2;
        int ix = start_x + inv_x_off + col * (INV_W + INV_X_GAP);
        int iy = GAME_TOP_Y + 4 + inv_y_off + r * (INV_H + INV_Y_GAP);
        enemy_bullets[slot].x = ix + (INV_W / 2);
        enemy_bullets[slot].y = iy + INV_H;
        enemy_bullets[slot].active = 1;
        break;
    }
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
            game_started = 1;
        } else {
            return;
        }
    }

    if (pressed_edge & (K_ROT | K_SPACE)) {
        fire_player_bullet();
        player_shot_acc_ms = 0;
    }

    if (held & (K_ROT | K_SPACE)) {
        player_shot_acc_ms += LOGIC_DT_MS;
        if (player_shot_acc_ms >= PLAYER_SHOT_MS) {
            fire_player_bullet();
            player_shot_acc_ms = 0;
        }
    } else {
        player_shot_acc_ms = 0;
    }

    player_move_acc_ms += LOGIC_DT_MS;
    if (player_move_acc_ms >= PLAYER_MOVE_MS) {
        player_move_acc_ms = 0;
        player_prev_x = player_x;
        if (held & K_LEFT) player_x--;
        if (held & K_RIGHT) player_x++;
        if (player_x < 1) player_x = 1;
        if (player_x > VIEW_W - PLAYER_W - 1) player_x = VIEW_W - PLAYER_W - 1;
    }

    if (player_bullet.active) {
        player_bullet.y -= PLAYER_BULLET_SPEED;
        if (player_bullet.y < GAME_TOP_Y) player_bullet.active = 0;
    }

    for (int i = 0; i < ENEMY_BULLETS; i++) {
        if (!enemy_bullets[i].active) continue;
        enemy_bullets[i].y += ENEMY_BULLET_SPEED;
        if (enemy_bullets[i].y >= VIEW_H) enemy_bullets[i].active = 0;
    }

    if (player_bullet.active) {
        if (shield_hit(player_bullet.x, player_bullet.y)) {
            player_bullet.active = 0;
        }
    }
    for (int i = 0; i < ENEMY_BULLETS; i++) {
        if (!enemy_bullets[i].active) continue;
        if (shield_hit(enemy_bullets[i].x, enemy_bullets[i].y)) {
            enemy_bullets[i].active = 0;
        }
    }

    if (player_bullet.active) {
        int start_x = (VIEW_W - (INV_COLS * INV_W + (INV_COLS - 1) * INV_X_GAP)) / 2;
        int hit = 0;
        for (int r = 0; r < INV_ROWS; r++) {
            for (int c = 0; c < INV_COLS; c++) {
                if (!inv_alive[r][c]) continue;
                int ix = start_x + inv_x_off + c * (INV_W + INV_X_GAP);
                int iy = GAME_TOP_Y + 4 + inv_y_off + r * (INV_H + INV_Y_GAP);
                if (player_bullet.x >= ix && player_bullet.x < ix + INV_W &&
                    player_bullet.y >= iy && player_bullet.y < iy + INV_H) {
                    inv_alive[r][c] = 0;
                    player_bullet.active = 0;
                    score += 10 + (r * 5);
                    inv_move_limit_ms = invader_speed_ms(invader_count());
                    hit = 1;
                    break;
                }
            }
            if (hit) break;
        }
    }

    for (int i = 0; i < ENEMY_BULLETS; i++) {
        if (!enemy_bullets[i].active) continue;
        if (enemy_bullets[i].x >= player_x &&
            enemy_bullets[i].x < player_x + PLAYER_W &&
            enemy_bullets[i].y >= PLAYER_Y &&
            enemy_bullets[i].y < PLAYER_Y + PLAYER_H) {
            enemy_bullets[i].active = 0;
            if (lives > 1) {
                lives--;
                reset_player();
                reset_shields();
                input_locked = 1;
            } else {
                lives = 0;
                enter_game_over();
            }
            return;
        }
    }

    inv_move_acc_ms += LOGIC_DT_MS;
    if (inv_move_acc_ms >= inv_move_limit_ms) {
        inv_move_acc_ms = 0;
        inv_anim ^= 1u;
        inv_prev_x_off = inv_x_off;

        int leftmost = VIEW_W;
        int rightmost = 0;
        int start_x = (VIEW_W - (INV_COLS * INV_W + (INV_COLS - 1) * INV_X_GAP)) / 2;
        for (int r = 0; r < INV_ROWS; r++) {
            for (int c = 0; c < INV_COLS; c++) {
                if (!inv_alive[r][c]) continue;
                int ix = start_x + inv_x_off + c * (INV_W + INV_X_GAP);
                if (ix < leftmost) leftmost = ix;
                if (ix + INV_W > rightmost) rightmost = ix + INV_W;
            }
        }

        int next_x = inv_x_off + inv_dir;
        int next_left = leftmost + inv_dir;
        int next_right = rightmost + inv_dir;

        if (next_left <= 1 || next_right >= VIEW_W - 1) {
            inv_dir = -inv_dir;
            inv_y_off += INV_STEP_Y;
        } else {
            inv_x_off = next_x;
        }
    }

    int inv_bottom = 0;
    int start_x = (VIEW_W - (INV_COLS * INV_W + (INV_COLS - 1) * INV_X_GAP)) / 2;
    for (int r = 0; r < INV_ROWS; r++) {
        for (int c = 0; c < INV_COLS; c++) {
            if (!inv_alive[r][c]) continue;
            int iy = GAME_TOP_Y + 4 + inv_y_off + r * (INV_H + INV_Y_GAP);
            int bottom = iy + INV_H;
            if (bottom > inv_bottom) inv_bottom = bottom;
        }
    }
    if (inv_bottom >= PLAYER_Y - 1) {
        lives = 0;
        enter_game_over();
        return;
    }

    if (invader_count() == 0) {
        score += SCORE_BONUS_WAVE;
        if (level < MAX_LEVEL) level++;
        reset_invaders();
        reset_shields();
        reset_player();
    }

    maybe_fire_enemy();
}

/* =========================
 * Rendering
 * ========================= */
static void render_play_screen(void)
{
    uint32_t s = score;
    for (int i = 0; i < 6; i++) {
        int dig = (int)(s % 10);
        s /= 10;
        draw_digit_tall(dig, 2 + ((5 - i) * 4), 1, RGB(1,1,0));
    }
    draw_digit_tall((lives > 9) ? 9 : lives, 56, 1, RGB(1,0,0));

    for (int xv = 0; xv < VIEW_W; xv++) {
        view_put(xv, HUD_LINE_Y, RGB(0,0,1));
    }

    for (int sidx = 0; sidx < SHIELD_COUNT; sidx++) {
        int sx = 6 + sidx * (SHIELD_W + 8);
        for (int y = 0; y < SHIELD_H; y++) {
            for (int x = 0; x < SHIELD_W; x++) {
                if (shield[sidx][y][x])
                    view_put(sx + x, SHIELD_Y + y, RGB(0,1,0));
            }
        }
    }

    int start_x = (VIEW_W - (INV_COLS * INV_W + (INV_COLS - 1) * INV_X_GAP)) / 2;
    uint16_t inv_interp = 0;
    if (inv_move_limit_ms > 0) {
        uint32_t acc = inv_move_acc_ms;
        if (acc > inv_move_limit_ms) acc = inv_move_limit_ms;
        inv_interp = (uint16_t)((acc * 256u) / inv_move_limit_ms);
        if (inv_interp > 255) inv_interp = 255;
    }
    int inv_x_draw = inv_prev_x_off + ((inv_x_off - inv_prev_x_off) * (int)inv_interp) / 256;
    for (int r = 0; r < INV_ROWS; r++) {
        for (int c = 0; c < INV_COLS; c++) {
            if (!inv_alive[r][c]) continue;
            int ix = start_x + inv_x_draw + c * (INV_W + INV_X_GAP);
            int iy = GAME_TOP_Y + 4 + inv_y_off + r * (INV_H + INV_Y_GAP);
            const uint8_t *spr = NULL;
            if (r == 0) {
                spr = inv_anim ? ALIEN3_SPRITE_B : ALIEN3_SPRITE_A;
            } else if (r == 1 || r == 2) {
                spr = inv_anim ? ALIEN2_SPRITE_B : ALIEN2_SPRITE_A;
            } else {
                spr = inv_anim ? ALIEN1_SPRITE_B : ALIEN1_SPRITE_A;
            }
            draw_invader_sprite(ix, iy, spr, RGB(1,0,1));
        }
    }

    uint16_t player_interp = 0;
    if (PLAYER_MOVE_MS > 0) {
        uint32_t acc = player_move_acc_ms;
        if (acc > PLAYER_MOVE_MS) acc = PLAYER_MOVE_MS;
        player_interp = (uint16_t)((acc * 256u) / PLAYER_MOVE_MS);
        if (player_interp > 255) player_interp = 255;
    }
    int player_x_draw = player_prev_x + ((player_x - player_prev_x) * (int)player_interp) / 256;
    draw_player_sprite(player_x_draw, PLAYER_Y, RGB(0,0,1));

    if (player_bullet.active) {
        draw_rect(player_bullet.x, player_bullet.y, 1, 2, RGB(1,1,0));
    }
    for (int i = 0; i < ENEMY_BULLETS; i++) {
        if (!enemy_bullets[i].active) continue;
        draw_rect(enemy_bullets[i].x, enemy_bullets[i].y, 1, 2, RGB(1,0,0));
    }
}

static void spaceinvaders_run(void)
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

void spaceinvaders_module_entry(void)
{
    spaceinvaders_run();
}

const launcher_builtin_module_t spaceinvaders_module = {
    "builtin:spaceinvaders",
    "SPACE MOD",
    spaceinvaders_module_entry
};
