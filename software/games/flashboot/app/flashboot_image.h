#ifndef FLASHBOOT_IMAGE_H
#define FLASHBOOT_IMAGE_H

#include <stdint.h>

#define FLASHBOOT_IMAGE_MAGIC 0x4C4E4348u /* LNCH */
#define FLASHBOOT_IMAGE_VERSION 1u

typedef struct flashboot_image_header {
    uint32_t magic;
    uint32_t version;
    uint32_t load_addr;
    uint32_t entry_addr;
    uint32_t gp_addr;
    uint32_t sp_addr;
    uint32_t payload_size;
    uint32_t payload_checksum;
} flashboot_image_header_t;

#endif
