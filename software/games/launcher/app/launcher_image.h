#ifndef LAUNCHER_IMAGE_H
#define LAUNCHER_IMAGE_H

#include <stdint.h>

#define LAUNCHER_IMAGE_MAGIC 0x47494D47u
#define LAUNCHER_IMAGE_VERSION 2u
#define LAUNCHER_IMAGE_MAX_SEGMENTS 8u

typedef struct {
    uint32_t dest_addr;
    uint32_t data_offset;
    uint32_t file_size;
    uint32_t mem_size;
} launcher_image_segment_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t segment_count;
    uint32_t entry_addr;
    uint32_t stack_addr;
    uint32_t gp_addr;
    uint32_t header_size;
    uint32_t crc32;
    char title[32];
} launcher_image_header_t;

typedef enum {
    LAUNCHER_IMAGE_OK = 0,
    LAUNCHER_IMAGE_BAD_MAGIC = -1,
    LAUNCHER_IMAGE_BAD_VERSION = -2,
    LAUNCHER_IMAGE_BAD_RANGE = -3,
    LAUNCHER_IMAGE_BAD_ENTRY = -4,
    LAUNCHER_IMAGE_BAD_LAYOUT = -5
} launcher_image_status_t;

typedef struct {
    launcher_image_header_t header;
    const uint8_t *file_data;
    uint32_t file_size;
} launcher_loaded_image_t;

launcher_image_status_t launcher_image_validate(const launcher_loaded_image_t *image);
launcher_image_status_t launcher_image_boot(const launcher_loaded_image_t *image);

#endif
