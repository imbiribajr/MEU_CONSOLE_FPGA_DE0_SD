#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#include "system.h"
#include "io.h"

static void flash_dump_raw(void) {
    int i;

    printf("flash base = 0x%08X\n", GENERIC_TRISTATE_CONTROLLER_0_BASE);
    for (i = 0; i < 16; ++i) {
        unsigned v = IORD_8DIRECT(GENERIC_TRISTATE_CONTROLLER_0_BASE, i);
        printf("raw[%02d] = 0x%02X\n", i, v & 0xFF);
    }
}

static void flash_cfi_query(void) {
    unsigned q, r, y;

    IOWR_8DIRECT(GENERIC_TRISTATE_CONTROLLER_0_BASE, 0x55, 0x98);
    usleep(1000);

    q = IORD_8DIRECT(GENERIC_TRISTATE_CONTROLLER_0_BASE, 0x10) & 0xFF;
    r = IORD_8DIRECT(GENERIC_TRISTATE_CONTROLLER_0_BASE, 0x11) & 0xFF;
    y = IORD_8DIRECT(GENERIC_TRISTATE_CONTROLLER_0_BASE, 0x12) & 0xFF;

    printf("CFI Q = 0x%02X\n", q);
    printf("CFI R = 0x%02X\n", r);
    printf("CFI Y = 0x%02X\n", y);

    IOWR_8DIRECT(GENERIC_TRISTATE_CONTROLLER_0_BASE, 0x00, 0xF0);
    usleep(1000);
}

int main(void) {
    printf("flash diag start\n");
    flash_dump_raw();
    flash_cfi_query();
    printf("flash diag done\n");

    while (1) {
        usleep(500000);
    }

    return 0;
}
