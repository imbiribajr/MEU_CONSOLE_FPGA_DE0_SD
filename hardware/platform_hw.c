#include "platform.h"
#include "system.h"
#include "io.h"
#include "altera_avalon_pio_regs.h"

void platform_wait_frame(void)
{
    while ((IORD_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_STATUS_ADDR) & LED_FRAME_DONE) == 0)
        ;
    IOWR_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_STATUS_ADDR, LED_FRAME_DONE);
}

void platform_write_pixel(uint16_t addr, uint8_t value)
{
    IOWR_8DIRECT(LED_MATRIX_AVALON_0_BASE, addr, value);
}

void platform_set_brightness(uint8_t level)
{
    IOWR_8DIRECT(LED_MATRIX_AVALON_0_BASE, LED_BRIGHTNESS_OFFSET, level);
}

uint32_t platform_read_ps2(void)
{
    return IORD_32DIRECT(PS2_INTERFACE_0_BASE, 0);
}

uint32_t platform_read_pio(void)
{
    return IORD_ALTERA_AVALON_PIO_DATA(PIO_0_BASE);
}
