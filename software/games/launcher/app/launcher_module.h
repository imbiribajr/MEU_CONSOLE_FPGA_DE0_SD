#ifndef LAUNCHER_MODULE_H
#define LAUNCHER_MODULE_H

#include <stdint.h>

typedef struct {
    const char *path;
    const char *title;
    void (*entry)(void);
} launcher_builtin_module_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t image_size;
    uint32_t entry_offset;
} launcher_external_module_header_t;

#define LAUNCHER_EXTERNAL_MODULE_MAGIC 0x474D4F44u /* GMOD */
#define LAUNCHER_EXTERNAL_MODULE_VERSION 1u
#define LAUNCHER_EXTERNAL_MODULE_LOAD_ADDR 0x00980000u
#define LAUNCHER_EXTERNAL_MODULE_MAX_SIZE (256u * 1024u)

#endif
