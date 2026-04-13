#include <stdint.h>
#include <string.h>
#include "system.h"
#include "nios2.h"
#include "sys/alt_cache.h"

#include "launcher_image.h"

#define SDRAM_BASE_ADDR   ((uint32_t)NEW_SDRAM_CONTROLLER_0_BASE)
#define SDRAM_END_ADDR    (SDRAM_BASE_ADDR + NEW_SDRAM_CONTROLLER_0_SPAN)
#define ONCHIP_BASE_ADDR  ((uint32_t)ONCHIP_MEMORY2_0_BASE)
#define ONCHIP_END_ADDR   (ONCHIP_BASE_ADDR + ONCHIP_MEMORY2_0_SPAN)

static void launcher_jump(uint32_t entry, uint32_t gp, uint32_t sp)
    __attribute__((noreturn));

static void launcher_jump(uint32_t entry, uint32_t gp, uint32_t sp)
{
    __asm__ volatile (
        "mov gp, %0\n"
        "mov sp, %1\n"
        "jmp %2\n"
        :
        : "r"(gp), "r"(sp), "r"(entry)
    );

    for (;;) {
    }
}

static int range_is_inside_region(uint32_t addr, uint32_t size, uint32_t base, uint32_t end)
{
    if (size == 0) return 1;
    if (addr < base || addr >= end) return 0;
    if (size > (end - addr)) return 0;
    return 1;
}

static int range_is_allowed(uint32_t addr, uint32_t size)
{
    return range_is_inside_region(addr, size, SDRAM_BASE_ADDR, SDRAM_END_ADDR) ||
           range_is_inside_region(addr, size, ONCHIP_BASE_ADDR, ONCHIP_END_ADDR);
}

static const launcher_image_segment_t *launcher_image_segments(const launcher_loaded_image_t *image)
{
    return (const launcher_image_segment_t *)(image->file_data + sizeof(launcher_image_header_t));
}

launcher_image_status_t launcher_image_validate(const launcher_loaded_image_t *image)
{
    uint32_t min_addr = 0xffffffffu;
    uint32_t max_addr = 0;
    uint32_t expected_header_size;

    if (image == 0 || image->file_data == 0) return LAUNCHER_IMAGE_BAD_MAGIC;
    if (image->header.magic != LAUNCHER_IMAGE_MAGIC) return LAUNCHER_IMAGE_BAD_MAGIC;
    if (image->header.version != LAUNCHER_IMAGE_VERSION) return LAUNCHER_IMAGE_BAD_VERSION;
    if (image->header.segment_count == 0 || image->header.segment_count > LAUNCHER_IMAGE_MAX_SEGMENTS) {
        return LAUNCHER_IMAGE_BAD_LAYOUT;
    }

    expected_header_size = (uint32_t)(sizeof(launcher_image_header_t) +
                                      image->header.segment_count * sizeof(launcher_image_segment_t));
    if (image->header.header_size != expected_header_size) {
        return LAUNCHER_IMAGE_BAD_LAYOUT;
    }
    if (image->file_size < expected_header_size) {
        return LAUNCHER_IMAGE_BAD_LAYOUT;
    }

    {
        const launcher_image_segment_t *segments = launcher_image_segments(image);
        for (uint32_t i = 0; i < image->header.segment_count; i++) {
            const launcher_image_segment_t *seg = &segments[i];
            if (seg->file_size > seg->mem_size) return LAUNCHER_IMAGE_BAD_LAYOUT;
            if (!range_is_allowed(seg->dest_addr, seg->mem_size)) return LAUNCHER_IMAGE_BAD_RANGE;
            if (seg->data_offset < expected_header_size) return LAUNCHER_IMAGE_BAD_LAYOUT;
            if (seg->file_size > (image->file_size - seg->data_offset)) return LAUNCHER_IMAGE_BAD_LAYOUT;
            if (seg->dest_addr < min_addr) min_addr = seg->dest_addr;
            if (seg->dest_addr + seg->mem_size > max_addr) max_addr = seg->dest_addr + seg->mem_size;
        }
    }

    if (!range_is_allowed(image->header.entry_addr, 4)) return LAUNCHER_IMAGE_BAD_ENTRY;
    if (image->header.entry_addr < min_addr || image->header.entry_addr >= max_addr) {
        return LAUNCHER_IMAGE_BAD_ENTRY;
    }
    return LAUNCHER_IMAGE_OK;
}

launcher_image_status_t launcher_image_boot(const launcher_loaded_image_t *image)
{
    launcher_image_status_t st = launcher_image_validate(image);
    if (st != LAUNCHER_IMAGE_OK) return st;

    {
        const launcher_image_segment_t *segments = launcher_image_segments(image);
        for (uint32_t i = 0; i < image->header.segment_count; i++) {
            const launcher_image_segment_t *seg = &segments[i];
            const uint8_t *src = image->file_data + seg->data_offset;
            if (seg->file_size != 0) {
                memcpy((void *)seg->dest_addr, src, seg->file_size);
            }
            if (seg->mem_size > seg->file_size) {
                memset((void *)(seg->dest_addr + seg->file_size), 0, seg->mem_size - seg->file_size);
            }
        }
    }

    alt_dcache_flush_all();
    alt_icache_flush_all();

    NIOS2_WRITE_IENABLE(0);
    NIOS2_WRITE_STATUS(0);

    launcher_jump(image->header.entry_addr, image->header.gp_addr, image->header.stack_addr);
}
