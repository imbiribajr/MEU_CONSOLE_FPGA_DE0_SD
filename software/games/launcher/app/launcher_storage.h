#ifndef LAUNCHER_STORAGE_H
#define LAUNCHER_STORAGE_H

#include "launcher_image.h"

#define LAUNCHER_MAX_TITLE 16
#define LAUNCHER_MAX_PATH  48

typedef struct {
    char title[LAUNCHER_MAX_TITLE];
    char path[LAUNCHER_MAX_PATH];
    uint32_t first_cluster;
    uint32_t file_size;
} launcher_storage_entry_t;

typedef struct {
    char builtin_path[LAUNCHER_MAX_PATH];
    char external_path[LAUNCHER_MAX_PATH];
    char title[LAUNCHER_MAX_TITLE];
} launcher_loaded_module_t;

typedef enum {
    LAUNCHER_STORAGE_OK = 0,
    LAUNCHER_STORAGE_NO_MEDIA = -10,
    LAUNCHER_STORAGE_NOT_FOUND = -11,
    LAUNCHER_STORAGE_BAD_IMAGE = -12,
    LAUNCHER_STORAGE_IO_ERROR = -13
} launcher_storage_status_t;

launcher_storage_status_t launcher_storage_init(void);
int launcher_storage_list(launcher_storage_entry_t *entries, int max_entries);
launcher_storage_status_t launcher_storage_load_image(const char *path, launcher_loaded_image_t *image);
launcher_storage_status_t launcher_storage_load_module(const char *path, launcher_loaded_module_t *module);
launcher_storage_status_t launcher_storage_load_module_file(const char *path, uint8_t *dst, uint32_t max_size);
const char *launcher_storage_debug_text(void);
int launcher_storage_debug_line_count(void);
const char *launcher_storage_debug_line(int index);

#endif
