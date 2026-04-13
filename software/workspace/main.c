#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "system.h"
#include "io.h"
#include "altera_avalon_pio_regs.h"
#include "sprites.h"

/* main.c version: 1.0.7 */

/* =========================
 * Display setup
 * ========================= */
#define PHY_W 128        // largura fisica (painel)
#define PHY_H 64         // altura fisica (painel)
#define VIEW_W 64        // largura virtual usada no jogo
#define VIEW_H 128       // altura virtual usada no jogo

#define HUD_LINE_Y 14    // linha divisoria do HUD
#define GAME_TOP_Y 15    // Y do topo da area de jogo
#define GAME_RENDER_Y_OFFSET -1

/* =========================
 * Timing
 * ========================= */
#define MS_TICK_US      1000 // tick base (us)
#define LOGIC_DT_MS     3    // passo fixo da logica (ms)
#define RENDER_DT_MS    6    // intervalo de render (ms)

/* =========================
 * Colors
 * ========================= */
#define RGB(r,g,b)  (uint8_t)(((r)?1:0) | ((g)?2:0) | ((b)?4:0))
#define COLOR_BLACK 0
#define COLOR_WHITE 7
#define COLOR_RED 1

/* =========================
 * River Raid tuning
 * ========================= */
#define RIVER_MIN_W 36          // largura minima do rio (px)
#define RIVER_MAX_W 58          // largura maxima do rio (px)
#define RIVER_STATIC 0          // 1 = margens retas, 0 = rio variavel
#define RIVER_INIT_W ((RIVER_MIN_W + RIVER_MAX_W) / 2)
#define RIVER_STEP_MS 5       // tempo base para descer 1 linha (ms) - velocidade do game = 5 valor ideal sem tremer
#define RIVER_STEP_DELTA_MS 1  // ajuste relativo ao passo atual (ms) 
#define RIVER_SCROLL_PX_BASE 1   // pixels por passo (fixo em 1) - não alterar - 
#define RIVER_STEP_SLEW_MS  1    // quanto ajusta o passo por tick (ms) - 
#define RIVER_DRAW_ENABLE 1     // 1 = desenha rio, 0 = desativa para teste
#define OBJECTS_DRAW_ENABLE 1   // 1 = desenha objetos, 0 = desativa para teste

/* =========================
 * Opcoes de teste
 * ========================= */
/* Config estavel sugerida: FORCE_FULL_BLIT=0, RENDER_ON_VSYNC_ONLY=0, SCROLL_SYNC_RENDER=0. */
#define COLLISION_ENABLE 1       // 1 = colisao ativa, 0 = desativa para teste
#define FORCE_FULL_BLIT 0        // 1 = ignora diff e escreve tudo (teste de ghost)
#define RENDER_ON_VSYNC_ONLY 0   // 1 = renderiza somente quando o frame terminou
#define SCROLL_SYNC_RENDER 0     // 1 = aplica scroll no render quando velocidade maxima


#define RIVER_MIN_BASE_MS 1     // limite minimo do passo base (ms) -----------------------------------------------testando
#define DIFFICULTY_STEP_MS 5000 // a cada X ms reduz a velocidade base
#define SPAWN_RAMP_MS 300000     // tempo para aumentar spawn (ms)     (tempo para chegar no máximo)
#define ENEMY_SPAWN_MIN 2      // limite inicial do roll de inimigos -  frequência base do HELICOPTER_SPRITE (OBJ_ENEMY).
#define ENEMY_SPAWN_MAX 24      // limite final do roll de inimigos  - frequência base do HELICOPTER_SPRITE (OBJ_ENEMY).
#define ENEMY_ATARI_MIN 1      // limite inicial do roll de heli ATARI - frequência base do HELICOPTER_ATARI (OBJ_HELI_ATARI).
#define ENEMY_ATARI_MAX 10     // limite final do roll de heli ATARI - frequência base do HELICOPTER_ATARI (OBJ_HELI_ATARI).
#define PLANE_SPAWN_MIN 2     // limite inicial do roll de avioes - Quanto menor esse número, menos aviões aparecem.
#define PLANE_SPAWN_MAX 20   // 176     // limite final do roll de avioes
#define BOAT_MIN_Y_GAP 10       // espaco minimo vertical entre barcos (px)
#define FLYING_SPAWN_Y_MARGIN 8 // margem acima do jogador para spawn de objetos voadores (px)

#define PLAYER_W 10             // largura maxima do jogador (px)
#define PLAYER_H 14             // altura maxima do jogador (px)
#define PLAYER_HIT_W 8          // largura do hitbox do jogador (px)
#define PLAYER_HIT_H 12         // altura do hitbox do jogador (px)
#define PLAYER_HIT_X_OFF 1      // deslocamento X do hitbox do jogador (px)
#define PLAYER_HIT_Y_OFF 1      // deslocamento Y do hitbox do jogador (px)
#define PLAYER_STRAIGHT_W 10    // largura da nave reta (px)
#define PLAYER_STRAIGHT_H 14    // altura da nave reta (px)
#define PLAYER_TURN_W 10        // largura da nave virando (px)
#define PLAYER_TURN_H 14        // altura da nave virando (px)
#define PLAYER_Y (VIEW_H - PLAYER_H - 2) // Y da base do jogador
#define PLAYER_MOVE_MS 1        // tempo de interpolacao do movimento (ms) - valor maior deixa mais arrastado
#define PLAYER_SHOT_MS 10      // periodo base do tiro continuo (ms)
#define PLAYER_SHOT_MIN_MS 5   // periodo minimo do tiro continuo (ms)
#define PLAYER_SHOT_ACCEL_MS 10 // tempo para acelerar o tiro (ms)
#define PLAYER_SHOT_STEP_MS 5  // reducao do periodo a cada acel (ms)

#define NUM_LIVES 5

#define DAS_DELAY_MS 12        // atraso inicial do DAS (ms)
#define DAS_SPEED_MS 5         // repeticao do DAS (ms)
#define DIAG_STEP_EVERY 40 //10       // sobe 1px a cada N passos laterais

#define BULLET_SPEED_PX 3        // velocidade do tiro do jogador (px/tick)
#define PLAYER_BULLETS 8         // maximo de tiros do jogador simultaneos

#define ENEMY_MAX 5              // maximo de inimigos simultaneos
#define ENEMY_W 14               // largura do inimigo (px)
#define ENEMY_H 7                // altura do inimigo (px)
#define HELI_HIT_W 10            // largura do hitbox do helicoptero (px)
#define HELI_HIT_H 6             // altura do hitbox do helicoptero (px)
#define HELI_HIT_X_OFF 2         // deslocamento X do hitbox do helicoptero (px)
#define HELI_HIT_Y_OFF 0         // deslocamento Y do hitbox do helicoptero (px)
#define PLANE_HIT_W 10           // largura do hitbox do aviao (px)
#define PLANE_HIT_H 6            // altura do hitbox do aviao (px)
#define PLANE_HIT_X_OFF 2        // deslocamento X do hitbox do aviao (px)
#define PLANE_HIT_Y_OFF 0        // deslocamento Y do hitbox do aviao (px)
#define HELI_ATARI_W 16          // largura do helicoptero ATARI (px)
#define HELI_ATARI_H 10          // altura do helicoptero ATARI (px)

#define BOAT_SMALL_W 14          // largura do porta-avioes (px)
#define BOAT_SMALL_H 6           // altura do porta-avioes (px)
#define DESTROYER_W 32           // largura do destroyer (px)
#define DESTROYER_H 6            // altura do destroyer (px)
#define PLANE_W 14               // largura do aviao (px)
#define PLANE_H 6                // altura do aviao (px)
#define EXPLOSION_W 10           // largura da explosao (px)
#define EXPLOSION_H 8            // altura da explosao (px)
#define EXPLOSION2_W 15          // largura da explosao 2 (px)
#define EXPLOSION2_H 11          // altura da explosao 2 (px)

#define BRIDGE_SIDE_W 32         // largura do sprite lateral da ponte (px)
#define BRIDGE_SIDE_H 12         // altura do sprite lateral da ponte (px)
#define BRIDGE_H BRIDGE_SIDE_H   // altura da ponte (px)
#define BRIDGE_OPEN_W 33         // largura da abertura da ponte (px)
#define BRIDGE_EFFECT_H 4        // alcance vertical do afunilamento (px)
#define BRIDGE_COLLISION_H 1     // alcance vertical do afunilamento para colisao (px)
#define BRIDGE_VEG_INSET 20       // quanto a vegetacao invade o rio (px)
#define BRIDGE_VEG_DEPTH 20       // profundidade visivel da vegetacao no rio (px)
#define BRIDGE_VEG_UP 25          // altura acima da ponte com vegetacao (px)
#define BRIDGE_VEG_DOWN 30        // altura abaixo da ponte com vegetacao (px)

#define RESPAWN_PORTAL_H 18       // altura do portal de respawn (px)
#define RESPAWN_PORTAL_NARROW_H 6 // altura da garganta do portal (px)
#define RESPAWN_PORTAL_FLARE_H 8  // altura do alargamento do portal (px)
#define RESPAWN_PORTAL_NARROW_W 30 // largura estreita do portal (px)
#define RESPAWN_PORTAL_BELOW_H 64  // altura do portal abaixo do jogador (px)

#define FUEL_MAX 100             // combustivel maximo
#define FUEL_DEC_MS 90           // tempo para consumir 1 unidade de combustivel (ms)
#define FUEL_PICKUP 25           // combustivel ganho ao coletar
#define FUEL_W 10                // largura do combustivel (px)
#define FUEL_H 24                // altura do combustivel (px)
#define HUD_FUEL_W 27            // largura do HUD fuel (px)
#define HUD_FUEL_H 10            // altura do HUD fuel (px)

#define SCORE_ENEMY 10
#define SCORE_BOAT  20
#define SCORE_PLANE 30
#define SCORE_BRIDGE 50

#define EXPLOSION_MS 50
#define PLAYER_EXPLOSION_MS 80
#define DESTROYER_EXPLOSION_MS 90

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

typedef enum { OBJ_ENEMY, OBJ_HELI_ATARI, OBJ_FUEL, OBJ_BOAT_SMALL, OBJ_DESTROYER, OBJ_PLANE, OBJ_BRIDGE } obj_type_t;

typedef struct {
    int x;
    int y;
    int vx;
    int w;
    int h;
    obj_type_t type;
    int destroyed;
    int explosion_ms;
    uint8_t explosion_kind;
    int active;
} obj_t;

static void update_explosions(void);
static void draw_explosion_clipped(int x0, int y0);
static void draw_explosion2_clipped(int x0, int y0);

static int river_left[VIEW_H];
static int river_right[VIEW_H];
static int river_head = 0;
static int river_center = VIEW_W / 2;
static int river_width = 30;

static obj_t objects[ENEMY_MAX];
static bullet_t player_bullets[PLAYER_BULLETS];
static uint8_t enemy_turn_cd[ENEMY_MAX];

static int player_x = 0;
static int player_y = 0;
static int player_prev_x = 0;
static int player_prev_y = 0;
static int player_dir = 0;
static uint8_t heli_frame = 0;
static uint32_t heli_anim_acc_ms = 0;
static uint32_t player_move_acc_ms = 0;
static uint32_t player_shot_acc_ms = 0;
static uint32_t player_shot_hold_ms = 0;
static int timer_das_l = 0;
static int timer_das_r = 0;
static int timer_das_u = 0;
static int timer_das_d = 0;
static uint8_t diag_step = 0;

static uint32_t scroll_acc_ms = 0;
static uint32_t river_scroll_limit_ms = RIVER_STEP_MS;
static uint32_t river_scroll_target_ms = RIVER_STEP_MS;
static uint32_t river_scroll_base_ms = RIVER_STEP_MS;
static uint32_t fuel_acc_ms = 0;
static uint8_t fuel = FUEL_MAX;
static uint32_t score = 0;
static uint8_t lives = NUM_LIVES;

static uint8_t spawn_cd_enemy = 0;
static uint8_t spawn_cd_fuel = 0;
static uint8_t spawn_cd_boat = 0;
static uint8_t spawn_cd_plane = 0;
static uint8_t spawn_cd_bridge = 0;
static uint8_t enemy_atari_toggle = 0;
static uint32_t respawn_delay_ms = 0;
static uint8_t respawn_pending = 0;
static uint8_t player_hidden = 0;
static uint32_t player_explosion_ms = 0;

static uint16_t lfsr = 0xA5A5;
static uint32_t ms_global = 0;
static uint32_t game_time_ms = 0;

static uint8_t input_locked = 0;
static uint8_t game_started = 0;
static uint8_t prev_any_key = 0;

static int river_speed_dir = 0;
static uint8_t scroll_step_pending = 0;

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

static inline int frame_done_pending(void) {
    if ((IORD_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_STATUS_ADDR) & LED_FRAME_DONE) == 0)
        return 0;
    IOWR_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_STATUS_ADDR, LED_FRAME_DONE);
    return 1;
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
static uint8_t ps2_pending_release = 0;
static uint8_t ps2_pending_extended = 0;

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

static void view_put(int xv, int yv, uint8_t rgb) {
    if ((unsigned)xv >= VIEW_W || (unsigned)yv >= VIEW_H) return;
    viewbuf[yv * VIEW_W + xv] = (uint8_t)(rgb & 7);
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

static void view_clear(void) {
    memset(viewbuf, 0, sizeof(viewbuf));
}

static int get_scroll_offset_px(void)
{
    if (river_scroll_limit_ms == 0) return 0;
    int offset_y_px = (int)((scroll_acc_ms * RIVER_SCROLL_PX_BASE) / river_scroll_limit_ms);
    if (offset_y_px >= RIVER_SCROLL_PX_BASE) offset_y_px = RIVER_SCROLL_PX_BASE - 1;
    if (offset_y_px < 0) offset_y_px = 0;
    return offset_y_px;
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
            if (yv < GAME_TOP_Y) {
                prevbuf[i] = v;
                uint16_t addr = (uint16_t)(base_addr_xv[xv] + xp);
                IOWR_8DIRECT(LED_MATRIX_AVALON_0_BASE, addr, v);
                continue;
            }
            if (!FORCE_FULL_BLIT && v == prevbuf[i]) continue;
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
        if (code == 0xE0) {
            ps2_pending_extended = 1;
            continue;
        }
        if (code == 0xF0) {
            ps2_pending_release = 1;
            continue;
        }
        if (ps2_pending_extended) {
            extended = 1;
            ps2_pending_extended = 0;
        }
        if (ps2_pending_release) {
            released = 1;
            ps2_pending_release = 0;
        }
        if (released) ps2_break_code(code, extended);
        else ps2_make_code(code, extended);
    }
}

static void handle_input_merge(void) {
    handle_ps2();
    uint32_t raw = IORD_ALTERA_AVALON_PIO_DATA(PIO_0_BASE) & 0xF;
    uint8_t p = (uint8_t)((~raw) & 0xF);
    uint8_t pio_held = 0;
    if (p & 0x8) pio_held |= K_LEFT;
    if (p & 0x1) pio_held |= K_RIGHT;
    if (p & 0x4) pio_held |= K_ROT;
    if (p & 0x2) pio_held |= K_DOWN;
    uint8_t pio_pressed = pio_held & ~kb_held;
    if (pio_pressed) kb_pressed |= pio_pressed;
    kb_held = ps2_held | pio_held;
}

/* =========================
 * River generation
 * ========================= */
static void river_init(void)
{
    river_center = VIEW_W / 2;
    river_width = RIVER_INIT_W;
    for (int y = 0; y < VIEW_H; y++) {
        int left = river_center - (river_width / 2);
        int right = left + river_width - 1;
        if (left < 1) left = 1;
        if (right > VIEW_W - 2) right = VIEW_W - 2;
        river_left[y] = left;
        river_right[y] = right;
    }
    river_head = 0;
}

static int river_idx(int y)
{
    int idx = river_head + y;
    if (idx >= VIEW_H) idx -= VIEW_H;
    return idx;
}

static void river_generate_top(void)
{
    int left;
    int right;

#if RIVER_STATIC
    left = river_center - (river_width / 2);
    right = left + river_width - 1;
    if (left < 1) { left = 1; right = left + river_width - 1; }
    if (right > VIEW_W - 2) { right = VIEW_W - 2; left = right - river_width + 1; }
    river_left[river_head] = left;
    river_right[river_head] = right;
    river_center = left + (river_width / 2);
    return;
#endif

    int delta = (int)(lfsr & 3) - 1;      // -1..2
    int wdelta = (int)((lfsr >> 2) & 3) - 1; // -1..2
    river_center += delta;
    river_width += wdelta;

    if (river_width < RIVER_MIN_W) river_width = RIVER_MIN_W;
    if (river_width > RIVER_MAX_W) river_width = RIVER_MAX_W;

    left = river_center - (river_width / 2);
    right = left + river_width - 1;

    if (left < 1) { left = 1; right = left + river_width - 1; }
    if (right > VIEW_W - 2) { right = VIEW_W - 2; left = right - river_width + 1; }

    river_left[river_head] = left;
    river_right[river_head] = right;
    river_center = left + (river_width / 2);
}

static void river_scroll_step(void)
{
    river_head--;
    if (river_head < 0) river_head = VIEW_H - 1;
    lfsr_step();
    river_generate_top();
}

static void apply_bridge_narrowing(int world_y, int base_left, int base_right, int *out_left, int *out_right)
{
    *out_left = base_left;
    *out_right = base_right;
    for (int i = 0; i < ENEMY_MAX; i++) {
        if (!objects[i].active || objects[i].type != OBJ_BRIDGE || objects[i].destroyed) continue;
        int dy = world_y - objects[i].y;
        if (dy < 0) dy = -dy;
        if (dy > BRIDGE_EFFECT_H) continue;
        int bridge_left = objects[i].x;
        int bridge_right = objects[i].x + objects[i].w - 1;
        int t = BRIDGE_EFFECT_H - dy;
        int left = base_left + ((bridge_left - base_left) * t) / BRIDGE_EFFECT_H;
        int right = base_right + ((bridge_right - base_right) * t) / BRIDGE_EFFECT_H;
        if (left < 1) left = 1;
        if (right > VIEW_W - 2) right = VIEW_W - 2;
        if (right <= left) {
            left = base_left;
            right = base_right;
        }
        *out_left = left;
        *out_right = right;
        return;
    }
}

static void apply_bridge_narrowing_collision(int world_y, int base_left, int base_right,
    int *out_left, int *out_right)
{
    if (BRIDGE_COLLISION_H <= 0) {
        *out_left = base_left;
        *out_right = base_right;
        return;
    }
    *out_left = base_left;
    *out_right = base_right;
    for (int i = 0; i < ENEMY_MAX; i++) {
        if (!objects[i].active || objects[i].type != OBJ_BRIDGE || objects[i].destroyed) continue;
        int dy = world_y - objects[i].y;
        if (dy < 0) dy = -dy;
        if (dy > BRIDGE_COLLISION_H) continue;
        int bridge_left = objects[i].x;
        int bridge_right = objects[i].x + objects[i].w - 1;
        int t = BRIDGE_COLLISION_H - dy;
        int left = base_left + ((bridge_left - base_left) * t) / BRIDGE_COLLISION_H;
        int right = base_right + ((bridge_right - base_right) * t) / BRIDGE_COLLISION_H;
        if (left < 1) left = 1;
        if (right > VIEW_W - 2) right = VIEW_W - 2;
        if (right <= left) {
            left = base_left;
            right = base_right;
        }
        *out_left = left;
        *out_right = right;
        return;
    }
}

/* =========================
 * Object handling
 * ========================= */
static void objects_reset(void)
{
    for (int i = 0; i < ENEMY_MAX; i++) {
        objects[i].active = 0;
        enemy_turn_cd[i] = 0;
        objects[i].destroyed = 0;
        objects[i].explosion_ms = 0;
        objects[i].explosion_kind = 0;
    }
    enemy_atari_toggle = 0;
}

static int alloc_object_slot(void)
{
    for (int i = 0; i < ENEMY_MAX; i++) {
        if (!objects[i].active) return i;
    }
    return -1;
}

static int clamp_i32(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int has_active_type(obj_type_t type)
{
    for (int i = 0; i < ENEMY_MAX; i++) {
        if (objects[i].active && objects[i].type == type) return 1;
    }
    return 0;
}

static uint8_t object_color(const obj_t *o)
{
    switch (o->type) {
        case OBJ_FUEL:  return RGB(1,0,0);
        case OBJ_BOAT_SMALL: return RGB(1,1,1);
        case OBJ_DESTROYER: return RGB(1,1,1);
        case OBJ_PLANE: return RGB(0,1,1);
        case OBJ_BRIDGE:return RGB(1,1,1);
        default:        return RGB(1,0,0);
    }
}

static void player_hitbox(int *x, int *y, int *w, int *h)
{
    *x = player_x + PLAYER_HIT_X_OFF;
    *y = player_y + PLAYER_HIT_Y_OFF;
    *w = PLAYER_HIT_W;
    *h = PLAYER_HIT_H;
}

static void create_respawn_portal(void)
{
    int base_y = clamp_i32(player_y, 0, VIEW_H - 1);
    int center = player_x + (PLAYER_W / 2);
    for (int i = 0; i < RESPAWN_PORTAL_H; i++) {
        int y = base_y - (RESPAWN_PORTAL_H - 1 - i);
        if (y < GAME_TOP_Y || y >= VIEW_H) continue;
        int from_bottom = base_y - y;
        int width;
        if (from_bottom < RESPAWN_PORTAL_NARROW_H) {
            width = RESPAWN_PORTAL_NARROW_W;
        } else if (from_bottom < (RESPAWN_PORTAL_NARROW_H + RESPAWN_PORTAL_FLARE_H)) {
            int t = from_bottom - RESPAWN_PORTAL_NARROW_H;
            int span = RIVER_INIT_W - RESPAWN_PORTAL_NARROW_W;
            width = RESPAWN_PORTAL_NARROW_W + ((span * t) / RESPAWN_PORTAL_FLARE_H);
        } else {
            width = RIVER_INIT_W;
        }
        int idx = river_idx(y);
        int left = center - (width / 2);
        int right = left + width - 1;
        if (left < 1) { left = 1; right = left + width - 1; }
        if (right > VIEW_W - 2) { right = VIEW_W - 2; left = right - width + 1; }
        if (right > left) {
            river_left[idx] = left;
            river_right[idx] = right;
        }
    }

    for (int i = 1; i <= RESPAWN_PORTAL_BELOW_H; i++) {
        int y = base_y + i;
        if (y < GAME_TOP_Y || y >= VIEW_H) break;
        int idx = river_idx(y);
        int left = center - (RESPAWN_PORTAL_NARROW_W / 2);
        int right = left + RESPAWN_PORTAL_NARROW_W - 1;
        if (left < 1) { left = 1; right = left + RESPAWN_PORTAL_NARROW_W - 1; }
        if (right > VIEW_W - 2) { right = VIEW_W - 2; left = right - RESPAWN_PORTAL_NARROW_W + 1; }
        if (right > left) {
            river_left[idx] = left;
            river_right[idx] = right;
        }
    }
}

static void object_hitbox(const obj_t *o, int base_y, int *x, int *y, int *w, int *h)
{
    *x = o->x;
    *y = base_y;
    *w = o->w;
    *h = o->h;
    if (o->type == OBJ_ENEMY) {
        *x += HELI_HIT_X_OFF;
        *y += HELI_HIT_Y_OFF;
        *w = HELI_HIT_W;
        *h = HELI_HIT_H;
    } else if (o->type == OBJ_PLANE) {
        *x += PLANE_HIT_X_OFF;
        *y += PLANE_HIT_Y_OFF;
        *w = PLANE_HIT_W;
        *h = PLANE_HIT_H;
    }
}

static void draw_sprite_mask32_clipped(int x0, int y0, int w, int h, const uint32_t *rows, uint8_t color,
    int flip_x, int y_start, int y_end)
{
    if (y_start < 0) y_start = 0;
    if (y_end > h) y_end = h;
    for (int y = y_start; y < y_end; y++) {
        int yy = y0 + y;
        if (yy < 0 || yy >= VIEW_H) continue;
        uint32_t row = rows[y];
        for (int x = 0; x < w; x++) {
            int xx = x0 + x;
            if (xx < 0 || xx >= VIEW_W) continue;
            int bit = flip_x ? x : (w - 1 - x);
            if ((row >> bit) & 1) {
                view_put(xx, yy, color);
            }
        }
    }
}

static void draw_sprite_mask16(int x0, int y0, int w, int h, const uint16_t *rows, uint8_t color, int flip_x)
{
    for (int y = 0; y < h; y++) {
        int yy = y0 + y;
        if (yy < 0 || yy >= VIEW_H) continue;
        uint16_t row = rows[y];
        for (int x = 0; x < w; x++) {
            int xx = x0 + x;
            if (xx < 0 || xx >= VIEW_W) continue;
            int bit = flip_x ? x : (w - 1 - x);
            if ((row >> bit) & 1) {
                view_put(xx, yy, color);
            }
        }
    }
}

typedef struct {
    int x;
    int y;
    int w;
    int h;
} rect_t;

static int rect_overlap(rect_t a, rect_t b)
{
    if (a.x + a.w <= b.x) return 0;
    if (b.x + b.w <= a.x) return 0;
    if (a.y + a.h <= b.y) return 0;
    if (b.y + b.h <= a.y) return 0;
    return 1;
}

static void avoid_bridge_overlap(int *x, int *y, int w, int h)
{
    rect_t r = { *x, *y, w, h };
    for (int i = 0; i < ENEMY_MAX; i++) {
        if (!objects[i].active || objects[i].type != OBJ_BRIDGE) continue;
        rect_t left = { objects[i].x - BRIDGE_SIDE_W, objects[i].y, BRIDGE_SIDE_W, BRIDGE_SIDE_H };
        rect_t right = { objects[i].x + objects[i].w, objects[i].y, BRIDGE_SIDE_W, BRIDGE_SIDE_H };
        rect_t center = { objects[i].x, objects[i].y, objects[i].w, BRIDGE_SIDE_H };

        if (rect_overlap(r, left) || rect_overlap(r, right) ||
            (!objects[i].destroyed && rect_overlap(r, center))) {
            *y = objects[i].y - h - 2;
            return;
        }
    }
}

static void draw_sprite_mask16_clipped(int x0, int y0, int w, int h, const uint16_t *rows, uint8_t color,
    int flip_x, int y_start, int y_end)
{
    if (y_start < 0) y_start = 0;
    if (y_end > h) y_end = h;
    for (int y = y_start; y < y_end; y++) {
        int yy = y0 + y;
        if (yy < 0 || yy >= VIEW_H) continue;
        uint16_t row = rows[y];
        for (int x = 0; x < w; x++) {
            int xx = x0 + x;
            if (xx < 0 || xx >= VIEW_W) continue;
            int bit = flip_x ? x : (w - 1 - x);
            if ((row >> bit) & 1) {
                view_put(xx, yy, color);
            }
        }
    }
}

static void draw_row_mask16(int x0, int y, int w, uint16_t row, uint8_t color)
{
    if (y < 0 || y >= VIEW_H) return;
    for (int x = 0; x < w; x++) {
        int xx = x0 + x;
        if (xx < 0 || xx >= VIEW_W) continue;
        int bit = (w - 1 - x);
        if ((row >> bit) & 1) {
            view_put(xx, y, color);
        }
    }
}

static void draw_fuel_atari_clipped(int x0, int y0, int y_start, int y_end)
{
    if (y_start < 0) y_start = 0;
    if (y_end > FUEL_H) y_end = FUEL_H;
    for (int r = y_start; r < y_end; r++) {
        int yy = y0 + r;
        int block = r / 6;
        int row = r % 6;
        uint8_t bg = (block == 0 || block == 2) ? RGB(1,0,0) : RGB(1,1,1);
        draw_rect(x0, yy, FUEL_W, 1, bg);
        if (block == 0) draw_row_mask16(x0, yy, FUEL_W, F_SPRITE[row], RGB(0,0,1));
        else if (block == 1) draw_row_mask16(x0, yy, FUEL_W, U_SPRITE[row], RGB(0,0,1));
        else if (block == 2) draw_row_mask16(x0, yy, FUEL_W, E_SPRITE[row], RGB(0,0,1));
        else if (block == 3) draw_row_mask16(x0, yy, FUEL_W, L_SPRITE[row], RGB(0,0,1));
    }
}

static void draw_hud_fuel(int x0, int y0)
{
    int inner_left = 1;
    int inner_right = HUD_FUEL_W - 2;
    int inner_range = inner_right - inner_left;
    int pos = inner_left;
    if (inner_range > 0) {
        pos = inner_left + (int)((inner_range * (uint32_t)fuel) / FUEL_MAX);
    }
    int bar_x = x0 + pos;
    for (int y = 0; y < HUD_FUEL_H; y++) {
        uint32_t row = HUD_FUEL_SPRITE[y];
        int yy = y0 + y;
        if (yy < 0 || yy >= VIEW_H) continue;
        for (int x = 0; x < HUD_FUEL_W; x++) {
            int xx = x0 + x;
            if (xx < 0 || xx >= VIEW_W) continue;
            int bit = (HUD_FUEL_W - 1 - x);
            if ((row >> bit) & 1) {
                view_put(xx, yy, COLOR_WHITE);
            }
        }
    }
    if (bar_x < x0 + inner_left) bar_x = x0 + inner_left;
    if (bar_x > x0 + inner_right - 1) bar_x = x0 + inner_right - 1;
    draw_rect(bar_x, y0 + 2, 2, HUD_FUEL_H - 4, RGB(1,1,0));
}
static int object_damageable(const obj_t *o)
{
    return (o->type != OBJ_FUEL && o->explosion_ms == 0);
}

static int object_is_boat(const obj_t *o)
{
    return (o->type == OBJ_BOAT_SMALL || o->type == OBJ_DESTROYER);
}

static int object_explodable(const obj_t *o)
{
    return (o->type == OBJ_BOAT_SMALL ||
            o->type == OBJ_DESTROYER ||
            o->type == OBJ_PLANE ||
            o->type == OBJ_ENEMY ||
            o->type == OBJ_HELI_ATARI);
}

static void start_explosion(obj_t *o)
{
    if (o->type == OBJ_DESTROYER) {
        o->explosion_ms = DESTROYER_EXPLOSION_MS;
        o->explosion_kind = 2;
    } else {
        o->explosion_ms = EXPLOSION_MS;
        o->explosion_kind = 1;
    }
    o->vx = 0;
}

static int boat_y_conflict(const obj_t *skip, int y, int h)
{
    for (int i = 0; i < ENEMY_MAX; i++) {
        if (!objects[i].active || !object_is_boat(&objects[i])) continue;
        if (skip && &objects[i] == skip) continue;
        int oy = objects[i].y;
        int oh = objects[i].h;
        if (y < oy + oh + BOAT_MIN_Y_GAP && y + h + BOAT_MIN_Y_GAP > oy) {
            return 1;
        }
    }
    return 0;
}

static int fuel_overlap_any(int x, int y, int w, int h)
{
    rect_t r = { x, y, w, h };
    for (int i = 0; i < ENEMY_MAX; i++) {
        if (!objects[i].active) continue;
        if (objects[i].type == OBJ_FUEL) continue;
        rect_t o = { objects[i].x, objects[i].y, objects[i].w, objects[i].h };
        if (rect_overlap(r, o)) return 1;
    }
    return 0;
}

static int lerp_int(int a, int b, uint32_t t, uint32_t tmax)
{
    if (t >= tmax) return b;
    return a + (int)(((int64_t)(b - a) * (int64_t)t) / (int64_t)tmax);
}

static void setup_fuel_object(obj_t *o, int y)
{
    int idx = river_idx(clamp_i32(y, 0, VIEW_H - 1));
    int left = river_left[idx] + 1;
    int right = river_right[idx] - FUEL_W - 1;
    if (right < left) return;

    lfsr_step();
    int jitter = ((int)(lfsr & 0x1F)) - 16;
    int center = (left + right) / 2;
    int x = clamp_i32(center + jitter, left, right);

    for (int attempt = 0; attempt < 6; attempt++) {
        int tx = x;
        int ty = y;
        avoid_bridge_overlap(&tx, &ty, FUEL_W, FUEL_H);
        if (!fuel_overlap_any(tx, ty, FUEL_W, FUEL_H)) {
            x = tx;
            y = ty;
            break;
        }
        y -= (FUEL_H + BOAT_MIN_Y_GAP);
    }
    o->x = x;
    o->y = y;
    o->vx = 0;
    o->w = FUEL_W;
    o->h = FUEL_H;
    o->type = OBJ_FUEL;
    o->destroyed = 0;
    o->explosion_ms = 0;
    o->explosion_kind = 0;
    o->active = 1;
}

static void object_spawn(obj_type_t type)
{
    int slot = alloc_object_slot();
    if (slot < 0) return;

    int left = river_left[river_head] + 2;
    int right = river_right[river_head] - 2;
    int w = ENEMY_W;
    int h = ENEMY_H;
    int vx = 0;

    if (type == OBJ_FUEL) { w = FUEL_W; h = FUEL_H; }
    if (type == OBJ_PLANE) { w = PLANE_W; h = PLANE_H; }
    if (type == OBJ_ENEMY) { w = ENEMY_W; h = ENEMY_H; }
    if (type == OBJ_HELI_ATARI) { w = HELI_ATARI_W; h = HELI_ATARI_H; }
    if (type == OBJ_BRIDGE) {
        int span = right - left + 1;
        w = BRIDGE_OPEN_W;
        if (w < PLAYER_W + 2) w = PLAYER_W + 2;
        if (w > span) w = span;
        h = BRIDGE_H;
    }

    lfsr_step();
    int dir = (lfsr & 1) ? 1 : -1;
    if (type == OBJ_ENEMY) {
        enemy_atari_toggle ^= 1;
        if (enemy_atari_toggle) {
        type = OBJ_HELI_ATARI;
        w = HELI_ATARI_W;
        h = HELI_ATARI_H;
        }
    }
    int span = (right - left - w + 1);
    if (type == OBJ_BRIDGE) span = 1;
    if (span < 1) return;
    int x = left + (lfsr % span);
    int y = 0;

    int target_x = player_x + (PLAYER_W / 2);
    int center_bias = (VIEW_W / 2);
    target_x = (target_x + center_bias) / 2;
    int speed = 1;

    if (type == OBJ_PLANE || type == OBJ_ENEMY || type == OBJ_HELI_ATARI) {
        int min_y = GAME_TOP_Y;
        int max_y = player_y - h - FLYING_SPAWN_Y_MARGIN;
        if (max_y < min_y) max_y = min_y;
        y = min_y + (int)(lfsr % (uint32_t)(max_y - min_y + 1));

        lfsr_step();
        dir = (lfsr & 1) ? 1 : -1;
        if (dir > 0) x = -w;
        else x = VIEW_W;

        if (type == OBJ_PLANE) speed = 1;
        else if (type == OBJ_ENEMY || type == OBJ_HELI_ATARI) speed = 1;
        vx = dir * speed;
    } else if (type == OBJ_BRIDGE) {
        int span = right - left + 1;
        x = left + (span - w) / 2;
        y = -(BRIDGE_H + BRIDGE_VEG_UP);
    } else if (type == OBJ_FUEL) {
        x = left + (lfsr % span);
        y = -FUEL_H;
    }

    objects[slot].x = x;
    objects[slot].y = y;
    objects[slot].vx = vx;
    objects[slot].w = w;
    objects[slot].h = h;
    objects[slot].type = type;
    objects[slot].destroyed = 0;
    objects[slot].explosion_ms = 0;
    objects[slot].explosion_kind = 0;
    objects[slot].active = 1;
    enemy_turn_cd[slot] = 0;
}

static void setup_boat_object(obj_t *o, int y, int vx, int pos, obj_type_t type)
{
    int yy = y;
    int idx = river_idx(clamp_i32(y, 0, VIEW_H - 1));
    int w = (type == OBJ_DESTROYER) ? DESTROYER_W : BOAT_SMALL_W;
    int h = (type == OBJ_DESTROYER) ? DESTROYER_H : BOAT_SMALL_H;
    int left = river_left[idx] + 1;
    int right = river_right[idx] - w - 1;
    if (right < left && type == OBJ_DESTROYER) {
        type = OBJ_BOAT_SMALL;
        w = BOAT_SMALL_W;
        h = BOAT_SMALL_H;
        right = river_right[idx] - w - 1;
    }
    if (right < left) return;

    int x = left;
    if (pos == 1) x = right;
    else if (pos == 2) x = (left + right) / 2;

    for (int attempt = 0; attempt < 6; attempt++) {
        int tx = x;
        int ty = yy;
        avoid_bridge_overlap(&tx, &ty, w, h);
        if (!boat_y_conflict(o, ty, h)) {
            x = tx;
            yy = ty;
            break;
        }
        yy -= (h + BOAT_MIN_Y_GAP);
    }
    o->x = x;
    o->y = yy;
    o->vx = vx;
    o->w = w;
    o->h = h;
    o->type = type;
    o->destroyed = 0;
    o->explosion_ms = 0;
    o->explosion_kind = 0;
    o->active = 1;
}

static void spawn_boat_at(int y, int vx, int pos, obj_type_t type)
{
    int slot = alloc_object_slot();
    if (slot < 0) return;

    setup_boat_object(&objects[slot], y, vx, pos, type);
}

static void spawn_initial_boats(void)
{
    for (int i = 0; i < 4; i++) {
        lfsr_step();
        if (i == 3 && (lfsr & 1)) continue;
        int pos = (int)(lfsr & 3);
        obj_type_t type = ((lfsr >> 2) & 3) ? OBJ_BOAT_SMALL : OBJ_DESTROYER;
        int h = (type == OBJ_DESTROYER) ? DESTROYER_H : BOAT_SMALL_H;
        int y = -((h + BOAT_MIN_Y_GAP) * (i + 1));
        spawn_boat_at(y, 0, pos, type);
    }
}

static void spawn_initial_fuel(void)
{
    const int ys[] = { -48, -12 };
    for (int i = 0; i < 2; i++) {
        int slot = alloc_object_slot();
        if (slot < 0) return;
        setup_fuel_object(&objects[slot], ys[i]);
    }
}

static void spawn_bridge_and_boats(void)
{
    int slot = alloc_object_slot();
    if (slot < 0) return;

    int left = river_left[river_head] + 2;
    int right = river_right[river_head] - 2;
    int span = right - left + 1;
    if (span < PLAYER_W + 2) return;

    int w = BRIDGE_OPEN_W;
    if (w < PLAYER_W + 2) w = PLAYER_W + 2;
    if (w > span) w = span;

    int x = left + (span - w) / 2 + 3;
    objects[slot].x = x;
    objects[slot].y = -(BRIDGE_H + BRIDGE_VEG_UP);
    objects[slot].vx = 0;
    objects[slot].w = w;
    objects[slot].h = BRIDGE_H;
    objects[slot].type = OBJ_BRIDGE;
    objects[slot].explosion_ms = 0;
    objects[slot].explosion_kind = 0;
    objects[slot].active = 1;

    lfsr_step();
    int boat_count = (lfsr & 1) ? 1 : 2;
    for (int i = 0; i < boat_count; i++) {
        lfsr_step();
        int pos = (int)(lfsr & 1);
        obj_type_t type = ((lfsr >> 2) & 3) ? OBJ_BOAT_SMALL : OBJ_DESTROYER;
        int h = (type == OBJ_DESTROYER) ? DESTROYER_H : BOAT_SMALL_H;
        int y = -(BRIDGE_H + (i + 1) * h);
        spawn_boat_at(y, 0, pos, type);
    }
}

static void objects_scroll_step(void)
{
    for (int i = 0; i < ENEMY_MAX; i++) {
        if (!objects[i].active) continue;
        objects[i].y += 1;
        if (objects[i].explosion_ms > 0) {
            if (objects[i].y >= VIEW_H) {
                objects[i].active = 0;
                objects[i].explosion_ms = 0;
            }
            continue;
        }
        if (objects[i].type == OBJ_PLANE || objects[i].type == OBJ_ENEMY || objects[i].type == OBJ_HELI_ATARI) {
            int speed = objects[i].vx < 0 ? -objects[i].vx : objects[i].vx;
            if (speed < 1) speed = 1;
            objects[i].vx = (objects[i].vx >= 0) ? speed : -speed;
        }
        if (object_is_boat(&objects[i]) && objects[i].vx == 0) {
            int dy = player_y - objects[i].y;
            if (dy < 0) dy = -dy;
            if (dy < 20) {
                int idx = river_idx(objects[i].y);
                int left = river_left[idx] + 1;
                int right = river_right[idx] - objects[i].w - 1;
                if (player_x < (left + right) / 2) objects[i].vx = 1;
                else objects[i].vx = -1;
            }
        }
        if (objects[i].vx != 0) objects[i].x += objects[i].vx;
        if (object_is_boat(&objects[i]) && objects[i].vx != 0) {
            int idx = river_idx(objects[i].y);
            int left = river_left[idx] + 1;
            int right = river_right[idx] - objects[i].w - 1;
            if (objects[i].x <= left) {
                objects[i].x = left;
                objects[i].vx = 0;
            } else if (objects[i].x >= right) {
                objects[i].x = right;
                objects[i].vx = 0;
            }
        }
        if (objects[i].y >= VIEW_H) {
            if (objects[i].type == OBJ_BRIDGE) {
                int offset_y_px = get_scroll_offset_px();
                int draw_y = objects[i].y + offset_y_px + GAME_RENDER_Y_OFFSET;
                if (draw_y - BRIDGE_VEG_UP < VIEW_H) continue;
            }
            if (object_is_boat(&objects[i])) {
                lfsr_step();
                int pos = (int)(lfsr & 3);
                int yy = - (16 + (int)(lfsr & 31));
                setup_boat_object(&objects[i], yy, 0, pos, objects[i].type);
            } else if (objects[i].type == OBJ_FUEL) {
                setup_fuel_object(&objects[i], - (24 + (int)(lfsr & 63)));
            } else {
                objects[i].active = 0;
                enemy_turn_cd[i] = 0;
            }
        }
        if (objects[i].x + objects[i].w < 0) objects[i].active = 0;
        if (objects[i].x >= VIEW_W) objects[i].active = 0;
    }

    if (spawn_cd_enemy) spawn_cd_enemy--;
    if (spawn_cd_fuel) spawn_cd_fuel--;
    if (spawn_cd_boat) spawn_cd_boat--;
    if (spawn_cd_plane) spawn_cd_plane--;
    if (spawn_cd_bridge) spawn_cd_bridge--;

    lfsr_step();
    if ((lfsr & 15) == 0) {
        uint8_t roll = (uint8_t)(lfsr & 0xFF);
        int enemy_thresh = lerp_int(ENEMY_SPAWN_MIN, ENEMY_SPAWN_MAX, game_time_ms, SPAWN_RAMP_MS);
        int atari_thresh = lerp_int(ENEMY_ATARI_MIN, ENEMY_ATARI_MAX, game_time_ms, SPAWN_RAMP_MS);
        int plane_thresh = lerp_int(PLANE_SPAWN_MIN, PLANE_SPAWN_MAX, game_time_ms, SPAWN_RAMP_MS);
        if (plane_thresh < enemy_thresh + atari_thresh + 1) plane_thresh = enemy_thresh + atari_thresh + 1;
        if (roll < enemy_thresh && spawn_cd_enemy == 0) {
            object_spawn(OBJ_ENEMY);
            spawn_cd_enemy = 16;
        } else if (roll < (enemy_thresh + atari_thresh) && spawn_cd_enemy == 0) {
            object_spawn(OBJ_HELI_ATARI);
            spawn_cd_enemy = 20;
        } else if (roll < plane_thresh && spawn_cd_plane == 0) {
            object_spawn(OBJ_PLANE);
            spawn_cd_plane = 24;
        } else if (spawn_cd_fuel == 0 && (lfsr & 63) == 11 && !has_active_type(OBJ_FUEL)) {
            object_spawn(OBJ_FUEL);
            spawn_cd_fuel = 60;
        }
    }

    lfsr_step();
    if (spawn_cd_bridge == 0 && (lfsr & 31) == 7 && !has_active_type(OBJ_BRIDGE)) {
        spawn_bridge_and_boats();
        spawn_cd_bridge = 48;
    }
}

/* =========================
 * Game state
 * ========================= */
static void reset_player(void)
{
    player_y = PLAYER_Y;
    int idx = river_idx(clamp_i32(player_y, 0, VIEW_H - 1));
    int left = river_left[idx] + 1;
    int right = river_right[idx] - PLAYER_W - 1;
    if (right < left) {
        player_x = (VIEW_W - PLAYER_W) / 2;
    } else {
        player_x = clamp_i32((left + right) / 2, left, right);
    }
    player_prev_x = player_x;
    player_prev_y = player_y;
    player_dir = 0;
    player_move_acc_ms = 0;
    player_shot_acc_ms = 0;
    player_shot_hold_ms = 0;
    timer_das_l = 0;
    timer_das_r = 0;
    timer_das_u = 0;
    timer_das_d = 0;
    diag_step = 0;
    heli_frame = 0;
    heli_anim_acc_ms = 0;
    for (int i = 0; i < PLAYER_BULLETS; i++)
        player_bullets[i].active = 0;
}

static void respawn_after_death(void)
{
    objects_reset();
    lfsr_step();
    spawn_initial_boats();
    spawn_initial_fuel();
    if (lfsr & 1) spawn_bridge_and_boats();
    reset_player();
    create_respawn_portal();
    input_locked = 1;
    player_hidden = 0;
}

static void start_respawn_delay(void)
{
    respawn_delay_ms = 200;
    respawn_pending = 1;
    player_hidden = 1;
    input_locked = 1;
    player_explosion_ms = PLAYER_EXPLOSION_MS;
}

static void reset_full_game(void)
{
    score = 0;
    lives = NUM_LIVES;
    fuel = FUEL_MAX;
    scroll_acc_ms = 0;
    fuel_acc_ms = 0;
    river_init();
    objects_reset();
    spawn_initial_boats();
    spawn_initial_fuel();
    reset_player();
    create_respawn_portal();
    river_scroll_limit_ms = RIVER_STEP_MS;
    river_scroll_target_ms = RIVER_STEP_MS;
    river_scroll_base_ms = RIVER_STEP_MS;
    spawn_cd_enemy = 0;
    spawn_cd_fuel = 0;
    spawn_cd_boat = 0;
    spawn_cd_plane = 0;
    spawn_cd_bridge = 0;
    game_time_ms = 0;
    game_started = 0;
    input_locked = 1;
    respawn_delay_ms = 0;
    respawn_pending = 0;
    player_hidden = 0;
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

static int player_hit_bank(void)
{
    int offset_y_px = get_scroll_offset_px();
    if (offset_y_px > 0) offset_y_px -= 1;
    int hx, hy, hw, hh;
    player_hitbox(&hx, &hy, &hw, &hh);
    for (int y = 0; y < hh; y++) {
        int yy = hy + y;
        if (yy < 0 || yy >= VIEW_H) continue;
        int src_y = yy - offset_y_px;
        if (src_y < 0) src_y = 0;
        if (src_y >= VIEW_H) src_y = VIEW_H - 1;
        int idx = river_idx(src_y);
        int left = river_left[idx];
        int right = river_right[idx];
        apply_bridge_narrowing_collision(src_y, left, right, &left, &right);
        if (hx < left - 1) return 1;
        if (hx + hw - 1 > right + 1) return 1;
    }
    return 0;
}

static int player_hit_object(const obj_t *o)
{
    int offset_y_px = get_scroll_offset_px();
    if (offset_y_px > 0) offset_y_px -= 1;
    int obj_y = o->y + offset_y_px;
    int obj_x = o->x;
    int w = o->w;
    int h = o->h;
    int hx, hy, hw, hh;
    player_hitbox(&hx, &hy, &hw, &hh);
    if (o->explosion_ms > 0) return 0;
    if (o->type == OBJ_BRIDGE && o->destroyed) {
        return 0;
    }

    object_hitbox(o, obj_y, &obj_x, &obj_y, &w, &h);
    if (hx + hw - 1 < obj_x) return 0;
    if (hx > obj_x + w - 1) return 0;
    if (hy + hh - 1 < obj_y) return 0;
    if (hy > obj_y + h - 1) return 0;
    return 1;
}

static void handle_objects_collisions(void)
{
    for (int i = 0; i < ENEMY_MAX; i++) {
        if (!objects[i].active) continue;
        if (player_hit_object(&objects[i])) {
            if (objects[i].type == OBJ_FUEL) {
                int f = (int)fuel + FUEL_PICKUP;
                fuel = (uint8_t)((f > FUEL_MAX) ? FUEL_MAX : f);
                return;
            } else {
                if (objects[i].type == OBJ_BRIDGE) {
                    objects[i].destroyed = 1;
                } else if (object_explodable(&objects[i])) {
                    start_explosion(&objects[i]);
                } else {
                    objects[i].active = 0;
                }
                if (lives > 1) {
                    lives--;
                    start_respawn_delay();
                } else {
                    lives = 0;
                    player_explosion_ms = PLAYER_EXPLOSION_MS;
                    enter_game_over();
                }
                return;
            }
        }
    }
}

static void handle_bullets(void)
{
    int offset_y_px = get_scroll_offset_px();
    if (offset_y_px > 0) offset_y_px -= 1;
    int order[ENEMY_MAX];
    int order_count = 0;
    for (int i = 0; i < ENEMY_MAX; i++) {
        if (!objects[i].active) continue;
        order[order_count++] = i;
    }
    for (int i = 0; i < order_count; i++) {
        for (int j = i + 1; j < order_count; j++) {
            if (objects[order[j]].y > objects[order[i]].y) {
                int tmp = order[i];
                order[i] = order[j];
                order[j] = tmp;
            }
        }
    }
    for (int b = 0; b < PLAYER_BULLETS; b++) {
        if (!player_bullets[b].active) continue;
        player_bullets[b].y -= BULLET_SPEED_PX;
        if (player_bullets[b].y < GAME_TOP_Y) {
            player_bullets[b].active = 0;
            continue;
        }

        for (int oi = 0; oi < order_count; oi++) {
            int i = order[oi];
            if (!objects[i].active) continue;
            if (!object_damageable(&objects[i])) continue;
            int obj_y = objects[i].y + offset_y_px;
            int hit = 0;
            if (objects[i].type == OBJ_BRIDGE && objects[i].destroyed) {
                int left_x = objects[i].x - BRIDGE_SIDE_W;
                int right_x = objects[i].x + objects[i].w;
                int y_min = obj_y;
                int y_max = obj_y + BRIDGE_SIDE_H - 1;
                int hit_left = (player_bullets[b].x >= left_x &&
                                player_bullets[b].x < left_x + BRIDGE_SIDE_W &&
                                player_bullets[b].y >= y_min &&
                                player_bullets[b].y <= y_max);
                int hit_right = (player_bullets[b].x >= right_x &&
                                 player_bullets[b].x < right_x + BRIDGE_SIDE_W &&
                                 player_bullets[b].y >= y_min &&
                                 player_bullets[b].y <= y_max);
                hit = hit_left || hit_right;
            } else {
                int obj_x = objects[i].x;
                int obj_hit_y = obj_y;
                int obj_w = objects[i].w;
                int obj_h = objects[i].h;
                object_hitbox(&objects[i], obj_y, &obj_x, &obj_hit_y, &obj_w, &obj_h);
                hit = (player_bullets[b].x >= obj_x &&
                       player_bullets[b].x < obj_x + obj_w &&
                       player_bullets[b].y >= obj_hit_y &&
                       player_bullets[b].y < obj_hit_y + obj_h);
            }

            if (hit) {
                if (objects[i].type == OBJ_BRIDGE) {
                    player_bullets[b].active = 0;
                    if (!objects[i].destroyed) {
                        objects[i].destroyed = 1;
                        score += SCORE_BRIDGE;
                    }
                } else {
                    if (object_explodable(&objects[i])) {
                        start_explosion(&objects[i]);
                    } else {
                        objects[i].active = 0;
                    }
                    player_bullets[b].active = 0;
                    if (object_is_boat(&objects[i])) score += SCORE_BOAT;
                    else if (objects[i].type == OBJ_PLANE) score += SCORE_PLANE;
                    else score += SCORE_ENEMY;
                }
                break;
            }
        }
    }
}

static void fire_player_bullet(void)
{
    for (int b = 0; b < PLAYER_BULLETS; b++) {
        if (player_bullets[b].active) return;
    }

    int idx0 = -1;
    for (int b = 0; b < PLAYER_BULLETS; b++) {
        if (player_bullets[b].active) continue;
        idx0 = b;
        break;
    }
    if (idx0 < 0) return;

    uint16_t player_interp = 0;
    if (PLAYER_MOVE_MS > 0) {
        uint32_t acc = player_move_acc_ms;
        if (acc > PLAYER_MOVE_MS) acc = PLAYER_MOVE_MS;
        player_interp = (uint16_t)((acc * 256u) / PLAYER_MOVE_MS);
        if (player_interp > 255) player_interp = 255;
    }
    int player_x_draw = player_prev_x + ((player_x - player_prev_x) * (int)player_interp) / 256;
    int player_y_draw = player_prev_y + ((player_y - player_prev_y) * (int)player_interp) / 256;

    int shot_x = player_x_draw + (PLAYER_W / 2);
    player_bullets[idx0].x = shot_x;
    player_bullets[idx0].y = player_y_draw - 1;
    player_bullets[idx0].active = 1;
}

static void update_play_state(uint8_t pressed_edge, uint8_t held)
{
    update_explosions();
    if (player_explosion_ms > 0) {
        if (player_explosion_ms > LOGIC_DT_MS)
            player_explosion_ms -= LOGIC_DT_MS;
        else
            player_explosion_ms = 0;
    }
    if (respawn_pending) {
        if (respawn_delay_ms > LOGIC_DT_MS) respawn_delay_ms -= LOGIC_DT_MS;
        else respawn_delay_ms = 0;
        if (respawn_delay_ms == 0) {
            respawn_pending = 0;
            respawn_after_death();
        }
        return;
    }
    if (input_locked) {
        if (held == 0) {
            pressed_edge = 0;
            held = 0;
            return;
        }
        input_locked = 0;
    }

    if (!game_started) {
        if (pressed_edge) {
            game_started = 1;
        } else {
            return;
        }
    }

    game_time_ms += LOGIC_DT_MS;


    if (pressed_edge & (K_SPACE | K_DOWN)) {
        fire_player_bullet();
        player_shot_acc_ms = 0;
        player_shot_hold_ms = 0;
    }

    if (held & (K_SPACE | K_DOWN)) {
        player_shot_hold_ms += LOGIC_DT_MS;
        player_shot_acc_ms += LOGIC_DT_MS;
        uint32_t dec = (player_shot_hold_ms / PLAYER_SHOT_ACCEL_MS) * PLAYER_SHOT_STEP_MS;
        uint32_t limit = (PLAYER_SHOT_MS > dec) ? (PLAYER_SHOT_MS - dec) : PLAYER_SHOT_MIN_MS;
        if (limit < PLAYER_SHOT_MIN_MS) limit = PLAYER_SHOT_MIN_MS;
        if (player_shot_acc_ms >= limit) {
            fire_player_bullet();
            player_shot_acc_ms = 0;
        }
    } else {
        player_shot_acc_ms = 0;
        player_shot_hold_ms = 0;
    }

    player_move_acc_ms += LOGIC_DT_MS;
    if (player_move_acc_ms > PLAYER_MOVE_MS) player_move_acc_ms = PLAYER_MOVE_MS;

    if (held & K_LEFT) {
        player_dir = -1;
        if (timer_das_l == 0) {
            player_prev_x = player_x;
            player_prev_y = player_y;
            player_x--;
            diag_step++;
            if (diag_step >= DIAG_STEP_EVERY) {
                player_y--;
                diag_step = 0;
            }
            timer_das_l = DAS_DELAY_MS;
            player_move_acc_ms = 0;
        } else {
            timer_das_l -= LOGIC_DT_MS;
            if (timer_das_l <= 0) {
                player_prev_x = player_x;
                player_prev_y = player_y;
                player_x--;
                diag_step++;
                if (diag_step >= DIAG_STEP_EVERY) {
                    player_y--;
                    diag_step = 0;
                }
                timer_das_l = DAS_SPEED_MS;
                player_move_acc_ms = 0;
            }
        }
    } else {
        timer_das_l = 0;
    }

    if (held & K_RIGHT) {
        player_dir = 1;
        if (timer_das_r == 0) {
            player_prev_x = player_x;
            player_prev_y = player_y;
            player_x++;
            diag_step++;
            if (diag_step >= DIAG_STEP_EVERY) {
                player_y--;
                diag_step = 0;
            }
            timer_das_r = DAS_DELAY_MS;
            player_move_acc_ms = 0;
        } else {
            timer_das_r -= LOGIC_DT_MS;
            if (timer_das_r <= 0) {
                player_prev_x = player_x;
                player_prev_y = player_y;
                player_x++;
                diag_step++;
                if (diag_step >= DIAG_STEP_EVERY) {
                    player_y--;
                    diag_step = 0;
                }
                timer_das_r = DAS_SPEED_MS;
                player_move_acc_ms = 0;
            }
        }
    } else {
        timer_das_r = 0;
    }

    if (held & K_ROT) {
        if (timer_das_u == 0) {
            player_prev_y = player_y;
            player_y--;
            timer_das_u = DAS_DELAY_MS;
            player_move_acc_ms = 0;
        } else {
            timer_das_u -= LOGIC_DT_MS;
            if (timer_das_u <= 0) {
                player_prev_y = player_y;
                player_y--;
                timer_das_u = DAS_SPEED_MS;
                player_move_acc_ms = 0;
            }
        }
    } else {
        timer_das_u = 0;
    }

    if (held & K_DOWN) {
        if (timer_das_d == 0) {
            player_prev_y = player_y;
            player_y++;
            timer_das_d = DAS_DELAY_MS;
            player_move_acc_ms = 0;
        } else {
            timer_das_d -= LOGIC_DT_MS;
            if (timer_das_d <= 0) {
                player_prev_y = player_y;
                player_y++;
                timer_das_d = DAS_SPEED_MS;
                player_move_acc_ms = 0;
            }
        }
    } else {
        timer_das_d = 0;
    }

    if (player_x < 1) player_x = 1;
    if (player_x > VIEW_W - PLAYER_W - 1) player_x = VIEW_W - PLAYER_W - 1;
    if (player_y < GAME_TOP_Y + 2) player_y = GAME_TOP_Y + 2;
    if (player_y > VIEW_H - PLAYER_H - 1) player_y = VIEW_H - PLAYER_H - 1;

    if ((held & (K_LEFT | K_RIGHT)) == 0) {
        player_dir = 0;
    }

    if (held & K_ROT) {
        if (river_scroll_base_ms > RIVER_STEP_DELTA_MS)
            river_scroll_target_ms = river_scroll_base_ms - RIVER_STEP_DELTA_MS;
        else
            river_scroll_target_ms = 1;
        river_speed_dir = 1;
    } else if (held & K_DOWN) {
        river_scroll_target_ms = river_scroll_base_ms + RIVER_STEP_DELTA_MS;
        river_speed_dir = 2;
    } else {
        river_scroll_target_ms = river_scroll_base_ms;
        river_speed_dir = 0;
    }

    if (river_scroll_target_ms < RIVER_MIN_BASE_MS)
        river_scroll_target_ms = RIVER_MIN_BASE_MS;

    if (river_scroll_limit_ms != river_scroll_target_ms && scroll_acc_ms == 0) {
        if (river_scroll_limit_ms < river_scroll_target_ms)
            river_scroll_limit_ms += RIVER_STEP_SLEW_MS;
        else
            river_scroll_limit_ms -= RIVER_STEP_SLEW_MS;

        if (river_scroll_limit_ms < RIVER_MIN_BASE_MS)
            river_scroll_limit_ms = RIVER_MIN_BASE_MS;
    }


    scroll_acc_ms += LOGIC_DT_MS;
    if (scroll_acc_ms >= river_scroll_limit_ms) {
        scroll_acc_ms = 0;
        if (SCROLL_SYNC_RENDER && river_scroll_limit_ms <= 1) {
            scroll_step_pending = 1;
        } else {
            river_scroll_step();
            objects_scroll_step();
        }
    }

    heli_anim_acc_ms += LOGIC_DT_MS;
    if (heli_anim_acc_ms >= 15) {
        heli_anim_acc_ms = 0;
        heli_frame ^= 1;
    }

    fuel_acc_ms += LOGIC_DT_MS;
    if (fuel_acc_ms >= FUEL_DEC_MS) {
        fuel_acc_ms = 0;
        if (fuel > 0) fuel--;
        if (fuel == 0) {
            enter_game_over();
            return;
        }
    }

    if (COLLISION_ENABLE && player_hit_bank()) {
        if (lives > 1) {
            lives--;
            start_respawn_delay();
        } else {
            lives = 0;
            player_explosion_ms = PLAYER_EXPLOSION_MS;
            enter_game_over();
            return;
        }
    }

    handle_bullets();
    if (COLLISION_ENABLE) handle_objects_collisions();
}

/* =========================
 * Rendering
 * ========================= */
static void draw_player(int x0, int y0)
{
    if (player_dir < 0) {
        int x = x0 + ((PLAYER_W - PLAYER_TURN_W) / 2);
        draw_sprite_mask16(x, y0, PLAYER_TURN_W, PLAYER_TURN_H, PLAYER_LEFT_SPRITE, RGB(1,1,0), 0);
    } else if (player_dir > 0) {
        int x = x0 + ((PLAYER_W - PLAYER_TURN_W) / 2);
        draw_sprite_mask16(x, y0, PLAYER_TURN_W, PLAYER_TURN_H, PLAYER_RIGHT_SPRITE, RGB(1,1,0), 0);
    } else {
        draw_sprite_mask16(x0, y0, PLAYER_STRAIGHT_W, PLAYER_STRAIGHT_H, PLAYER_SPRITE, RGB(1,1,0), 0);
    }
}

static void render_hud(void)
{
    uint32_t s = score;
    for (int i = 0; i < 6; i++) {
        int dig = (int)(s % 10);
        s /= 10;
        draw_digit_tall(dig, 2 + ((5 - i) * 4), 1, RGB(1,1,0));
    }

    for (int xv = 0; xv < VIEW_W; xv++) {
        view_put(xv, HUD_LINE_Y, RGB(0,0,1));
    }

    int bar_x = VIEW_W - HUD_FUEL_W - 10;
    int bar_y = 1;
    draw_hud_fuel(bar_x, bar_y);

    int lives_draw = (lives > 99) ? 99 : lives;
    int lives_tens = lives_draw / 10;
    int lives_ones = lives_draw % 10;
    draw_digit_tall(lives_tens, 55, 1, RGB(1,0,0));
    draw_digit_tall(lives_ones, 59, 1, RGB(1,0,0));
}

static void render_river(void)
{
    int offset_y_px = get_scroll_offset_px();
    for (int y = GAME_TOP_Y; y < VIEW_H; y++) {
        int draw_y = y + GAME_RENDER_Y_OFFSET;
        if (draw_y < GAME_TOP_Y || draw_y >= VIEW_H) continue;
        int src_y = y - offset_y_px;
        if (src_y < 0) src_y = 0;
        int idx = river_idx(src_y);
        int left = river_left[idx];
        int right = river_right[idx];
        apply_bridge_narrowing(src_y, left, right, &left, &right);
        draw_rect(left, draw_y, right - left + 1, 1, COLOR_BLACK);
        if (left > 0) {
            draw_rect(0, draw_y, left, 1, RGB(0,1,0));
        }
        if (right < VIEW_W - 1) {
            draw_rect(right + 1, draw_y, VIEW_W - right - 1, 1, RGB(0,1,0));
        }
    }
}

static void render_objects(void)
{
    int offset_y_px = get_scroll_offset_px();
    int order[ENEMY_MAX];
    int order_count = 0;
    rect_t bridge_rects[ENEMY_MAX * 3];
    int bridge_count = 0;

    for (int i = 0; i < ENEMY_MAX; i++) {
        if (!objects[i].active) continue;
        order[order_count++] = i;
    }

    for (int i = 0; i < order_count; i++) {
        for (int j = i + 1; j < order_count; j++) {
            if (objects[order[j]].y < objects[order[i]].y) {
                int tmp = order[i];
                order[i] = order[j];
                order[j] = tmp;
            }
        }
    }

    for (int i = 0; i < order_count; i++) {
        int idx = order[i];
        if (objects[idx].type != OBJ_BRIDGE) continue;
        int draw_y = objects[idx].y + offset_y_px + GAME_RENDER_Y_OFFSET;
        if (draw_y >= VIEW_H) continue;
        for (int r = 0; r < BRIDGE_SIDE_H; r++) {
            int yy = draw_y + r;
            int src_y = objects[idx].y + r;
            if (yy < GAME_TOP_Y || yy >= VIEW_H) continue;
            if (src_y < 0) src_y = 0;
            if (src_y >= VIEW_H) src_y = VIEW_H - 1;
            int ridx = river_idx(src_y);
            int left = river_left[ridx];
            int right = river_right[ridx];
            apply_bridge_narrowing(src_y, left, right, &left, &right);

            int cx0 = objects[idx].x;
            int cx1 = objects[idx].x + objects[idx].w - 1;
            int clip_l = (cx0 < left) ? left : cx0;
            int clip_r = (cx1 > right) ? right : cx1;
            if (!objects[idx].destroyed && clip_l <= clip_r) {
                draw_rect(clip_l, yy, clip_r - clip_l + 1, 1, COLOR_WHITE);
            }
        }

        int ridx = river_idx(objects[idx].y);
        int base_left = river_left[ridx];
        int base_right = river_right[ridx];
        int veg_left = base_left + BRIDGE_VEG_DEPTH;
        int veg_right = base_right - BRIDGE_VEG_DEPTH;
        if (veg_left < 0) veg_left = 0;
        if (veg_left > VIEW_W) veg_left = VIEW_W;
        if (veg_right < -1) veg_right = -1;
        if (veg_right > VIEW_W - 1) veg_right = VIEW_W - 1;
        if (veg_left > 0)
            bridge_rects[bridge_count++] = (rect_t){ 0, draw_y - BRIDGE_VEG_UP, veg_left, BRIDGE_SIDE_H + BRIDGE_VEG_UP + BRIDGE_VEG_DOWN };
        if (veg_right < VIEW_W - 1)
            bridge_rects[bridge_count++] = (rect_t){ veg_right + 1, draw_y - BRIDGE_VEG_UP, VIEW_W - veg_right - 1, BRIDGE_SIDE_H + BRIDGE_VEG_UP + BRIDGE_VEG_DOWN };
        if (!objects[idx].destroyed)
            bridge_rects[bridge_count++] = (rect_t){ objects[idx].x, draw_y, objects[idx].w, BRIDGE_SIDE_H };
    }

    rect_t drawn_rects[ENEMY_MAX];
    int drawn_count = 0;

    for (int i = 0; i < order_count; i++) {
        int idx = order[i];
        if (objects[idx].type == OBJ_BRIDGE) continue;
        int draw_y = objects[idx].y + offset_y_px + GAME_RENDER_Y_OFFSET;
        int h = objects[idx].h;
        if (draw_y >= VIEW_H) continue;
        if (draw_y + h <= GAME_TOP_Y) continue;
        int y_start = 0;
        int y_end = h;
        if (draw_y < GAME_TOP_Y) y_start = GAME_TOP_Y - draw_y;
        if (draw_y + h > VIEW_H) y_end = VIEW_H - draw_y;

        if (objects[idx].explosion_ms > 0) {
            if (objects[idx].explosion_kind == 2) {
                int ex = objects[idx].x + ((objects[idx].w - EXPLOSION2_W) / 2);
                int ey = draw_y + ((objects[idx].h - EXPLOSION2_H) / 2);
                draw_explosion2_clipped(ex, ey);
            } else {
                int ex = objects[idx].x + ((objects[idx].w - EXPLOSION_W) / 2);
                int ey = draw_y + ((objects[idx].h - EXPLOSION_H) / 2);
                draw_explosion_clipped(ex, ey);
            }
            continue;
        }

        rect_t rect = { objects[idx].x, draw_y, objects[idx].w, objects[idx].h };
        int is_flying = (objects[idx].type == OBJ_PLANE ||
                         objects[idx].type == OBJ_ENEMY ||
                         objects[idx].type == OBJ_HELI_ATARI);
        int is_fuel = (objects[idx].type == OBJ_FUEL);
        int is_boat = object_is_boat(&objects[idx]);
        if (is_flying) continue;
        int blocked = 0;
        if (!is_flying && !is_fuel && !is_boat) {
            for (int b = 0; b < bridge_count; b++) {
                if (rect_overlap(rect, bridge_rects[b])) { blocked = 1; break; }
            }
            if (blocked) continue;
            for (int d = 0; d < drawn_count; d++) {
                if (rect_overlap(rect, drawn_rects[d])) { blocked = 1; break; }
            }
            if (blocked) continue;
        }

        if (objects[idx].type == OBJ_FUEL) {
            draw_fuel_atari_clipped(objects[idx].x, draw_y, y_start, y_end);
        } else if (objects[idx].type == OBJ_BOAT_SMALL) {
            draw_sprite_mask16_clipped(objects[idx].x, draw_y, objects[idx].w, objects[idx].h, CARRIER_SPRITE,
                object_color(&objects[idx]), objects[idx].vx < 0, y_start, y_end);
        } else if (objects[idx].type == OBJ_DESTROYER) {
            draw_sprite_mask32_clipped(objects[idx].x, draw_y, objects[idx].w, objects[idx].h, DESTROYER_ATARI_SPRITE,
                object_color(&objects[idx]), objects[idx].vx < 0, y_start, y_end);
        } else if (objects[idx].type == OBJ_PLANE) {
            draw_sprite_mask16_clipped(objects[idx].x, draw_y, objects[idx].w, objects[idx].h, PLANE_SPRITE,
                object_color(&objects[idx]), objects[idx].vx < 0, y_start, y_end);
        } else if (objects[idx].type == OBJ_HELI_ATARI) {
            const uint16_t *sprite = heli_frame ? HELICOPTER_ATARI2_SPRITE : HELICOPTER_ATARI1_SPRITE;
            draw_sprite_mask16_clipped(objects[idx].x, draw_y, objects[idx].w, objects[idx].h, sprite,
                object_color(&objects[idx]), objects[idx].vx < 0, y_start, y_end);
        } else {
            draw_sprite_mask16_clipped(objects[idx].x, draw_y, objects[idx].w, objects[idx].h, HELICOPTER_SPRITE,
                object_color(&objects[idx]), objects[idx].vx > 0, y_start, y_end);
        }

        if (drawn_count < (int)(sizeof(drawn_rects) / sizeof(drawn_rects[0])))
            drawn_rects[drawn_count++] = rect;
    }

    for (int i = 0; i < order_count; i++) {
        int idx = order[i];
        if (objects[idx].type != OBJ_BRIDGE) continue;
        int draw_y = objects[idx].y + offset_y_px + GAME_RENDER_Y_OFFSET;
        if (draw_y - BRIDGE_VEG_UP >= VIEW_H) continue;
        int veg_top = draw_y - BRIDGE_VEG_UP;
        int veg_bottom = draw_y + BRIDGE_SIDE_H + BRIDGE_VEG_DOWN - 1;
        int center_y = draw_y + (BRIDGE_SIDE_H / 2);
        for (int yy = veg_top; yy <= veg_bottom; yy++) {
            if (yy < GAME_TOP_Y || yy >= VIEW_H) continue;
            int src_y = objects[idx].y + (yy - draw_y);
            if (src_y < 0) src_y = 0;
            if (src_y >= VIEW_H) src_y = VIEW_H - 1;
            int ridx = river_idx(src_y);
            int base_left = river_left[ridx];
            int base_right = river_right[ridx];
            int dist = yy - center_y;
            if (dist < 0) dist = -dist;
            int depth = BRIDGE_VEG_DEPTH - (dist / 2);
            int span = base_right - base_left + 1;
            int max_depth = (span - (PLAYER_W + 2)) / 2;
            if (max_depth < 0) max_depth = 0;
            if (depth > max_depth) depth = max_depth;
            if (depth < 2) depth = 2;
            int veg_left = base_left + depth;
            int veg_right = base_right - depth;
            if (veg_left < 0) veg_left = 0;
            if (veg_left > VIEW_W) veg_left = VIEW_W;
            if (veg_right < -1) veg_right = -1;
            if (veg_right > VIEW_W - 1) veg_right = VIEW_W - 1;
            if (veg_left > veg_right) continue;
            if (veg_left > 0) {
                draw_rect(0, yy, veg_left, 1, RGB(0,1,0));
            }
            if (veg_right < VIEW_W - 1) {
                draw_rect(veg_right + 1, yy, VIEW_W - veg_right - 1, 1, RGB(0,1,0));
            }
        }
    }

    for (int i = 0; i < order_count; i++) {
        int idx = order[i];
        if (objects[idx].type == OBJ_BRIDGE) continue;
        int draw_y = objects[idx].y + offset_y_px + GAME_RENDER_Y_OFFSET;
        int h = objects[idx].h;
        if (draw_y >= VIEW_H) continue;
        if (draw_y + h <= GAME_TOP_Y) continue;
        int y_start = 0;
        int y_end = h;
        if (draw_y < GAME_TOP_Y) y_start = GAME_TOP_Y - draw_y;
        if (draw_y + h > VIEW_H) y_end = VIEW_H - draw_y;

        int is_flying = (objects[idx].type == OBJ_PLANE ||
                         objects[idx].type == OBJ_ENEMY ||
                         objects[idx].type == OBJ_HELI_ATARI);
        if (objects[idx].explosion_ms > 0) continue;
        if (!is_flying) continue;

        if (objects[idx].type == OBJ_PLANE) {
            draw_sprite_mask16_clipped(objects[idx].x, draw_y, objects[idx].w, objects[idx].h, PLANE_SPRITE,
                object_color(&objects[idx]), objects[idx].vx < 0, y_start, y_end);
        } else if (objects[idx].type == OBJ_HELI_ATARI) {
            const uint16_t *sprite = heli_frame ? HELICOPTER_ATARI2_SPRITE : HELICOPTER_ATARI1_SPRITE;
            draw_sprite_mask16_clipped(objects[idx].x, draw_y, objects[idx].w, objects[idx].h, sprite,
                object_color(&objects[idx]), objects[idx].vx < 0, y_start, y_end);
        } else {
            draw_sprite_mask16_clipped(objects[idx].x, draw_y, objects[idx].w, objects[idx].h, HELICOPTER_SPRITE,
                object_color(&objects[idx]), objects[idx].vx > 0, y_start, y_end);
        }
    }
}

static void draw_explosion_clipped(int x0, int y0)
{
    int y_start = 0;
    int y_end = EXPLOSION_H;
    if (y0 < GAME_TOP_Y) y_start = GAME_TOP_Y - y0;
    if (y0 + EXPLOSION_H > VIEW_H) y_end = VIEW_H - y0;
    if (y_start >= y_end) return;
    draw_sprite_mask16_clipped(x0, y0, EXPLOSION_W, EXPLOSION_H, EXPLOSION1_SPRITE,
        COLOR_WHITE, 0, y_start, y_end);
}

static void draw_explosion2_clipped(int x0, int y0)
{
    int y_start = 0;
    int y_end = EXPLOSION2_H;
    if (y0 < GAME_TOP_Y) y_start = GAME_TOP_Y - y0;
    if (y0 + EXPLOSION2_H > VIEW_H) y_end = VIEW_H - y0;
    if (y_start >= y_end) return;
    draw_sprite_mask16_clipped(x0, y0, EXPLOSION2_W, EXPLOSION2_H, EXPLOSION2_SPRITE,
        COLOR_WHITE, 0, y_start, y_end);
}

static void update_explosions(void)
{
    for (int i = 0; i < ENEMY_MAX; i++) {
        if (!objects[i].active) continue;
        if (objects[i].explosion_ms == 0) continue;
        if (objects[i].explosion_ms > LOGIC_DT_MS)
            objects[i].explosion_ms -= LOGIC_DT_MS;
        else {
            objects[i].explosion_ms = 0;
            objects[i].active = 0;
        }
    }
}

static void render_play_screen(void)
{
#if RIVER_DRAW_ENABLE
    render_river();
#endif
#if OBJECTS_DRAW_ENABLE
    render_objects();
#endif
    draw_rect(0, VIEW_H - 1, VIEW_W, 1, COLOR_BLACK);
    draw_rect(0, 0, VIEW_W, HUD_LINE_Y, COLOR_BLACK);
    render_hud();

    if (player_explosion_ms > 0) {
        int draw_y = player_y + GAME_RENDER_Y_OFFSET;
        int ex = player_x + ((PLAYER_W - EXPLOSION_W) / 2);
        int ey = draw_y + ((PLAYER_H - EXPLOSION_H) / 2);
        draw_explosion_clipped(ex, ey);
    }

    if (!player_hidden) {
        uint16_t player_interp = 0;
        if (PLAYER_MOVE_MS > 0) {
            uint32_t acc = player_move_acc_ms;
            if (acc > PLAYER_MOVE_MS) acc = PLAYER_MOVE_MS;
            player_interp = (uint16_t)((acc * 256u) / PLAYER_MOVE_MS);
            if (player_interp > 255) player_interp = 255;
        }
        int player_x_draw = player_prev_x + ((player_x - player_prev_x) * (int)player_interp) / 256;
        int player_y_draw = player_prev_y + ((player_y - player_prev_y) * (int)player_interp) / 256;
        player_y_draw += GAME_RENDER_Y_OFFSET;
        draw_player(player_x_draw, player_y_draw);

        for (int b = 0; b < PLAYER_BULLETS; b++) {
            if (!player_bullets[b].active) continue;
            int bullet_y = player_bullets[b].y + GAME_RENDER_Y_OFFSET;
            if (bullet_y < GAME_TOP_Y || bullet_y >= VIEW_H) continue;
            if (bullet_y + 3 > (VIEW_H - 1)) continue;
            int bx = player_bullets[b].x;
            if (bx > 0) draw_rect(bx - 1, bullet_y, 1, 3, RGB(1,1,1));
            if (bx < VIEW_W) draw_rect(bx, bullet_y, 1, 3, RGB(1,1,1));
        }
    }
}

int main(void)
{
    uint32_t last_logic  = ms_global;
    uint32_t last_render = ms_global;
    uint32_t acc_logic   = 0;

    set_display_brightness(DEFAULT_BRIGHTNESS);
    printf("River Raid - PS2 + PIO\n");

    init_panel_lut_small();
    reset_full_game();

    static uint8_t game_over_armed = 0;

    while (1)
    {
        usleep(MS_TICK_US);
        ms_global++;
        lfsr_step();
        handle_input_merge();

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

        int render_due = 0;
#if RENDER_ON_VSYNC_ONLY
        render_due = frame_done_pending();
#else
        render_due = ((ms_global - last_render) >= RENDER_DT_MS);
#endif

        if (render_due)
        {
            last_render = ms_global;

            if (current_state == STATE_GAME_OVER_FADE)
                continue;

            uint32_t render_start = ms_global;
            (void)render_start;

            if (SCROLL_SYNC_RENDER && river_scroll_limit_ms <= 1 && scroll_step_pending) {
                scroll_step_pending = 0;
                river_scroll_step();
                objects_scroll_step();
            }

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

            if (!RENDER_ON_VSYNC_ONLY) {
                wait_frame();
            }
            blit_diff_to_panel_small_lut();
        }
    }
}
