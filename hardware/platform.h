#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

#define LED_STATUS_ADDR 8193
#define LED_FRAME_DONE  0x01
#define LED_BRIGHTNESS_OFFSET 8192

void platform_wait_frame(void);
void platform_write_pixel(uint16_t addr, uint8_t value);
void platform_set_brightness(uint8_t level);
uint32_t platform_read_ps2(void);
uint32_t platform_read_pio(void);

#endif
