#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#include "system.h"
#include "altera_avalon_spi_regs.h"

#include "launcher_storage.h"

#define SD_SPI_TIMEOUT 100000u
#define SD_CMD0    0u
#define SD_CMD8    8u
#define SD_CMD9    9u
#define SD_CMD17  17u
#define SD_CMD55  55u
#define SD_CMD58  58u
#define SD_ACMD41 41u

#define SECTOR_SIZE 512u
#define DIR_ATTR_LFN 0x0Fu
#define DIR_ATTR_DIR 0x10u
#define DIR_ATTR_VOL 0x08u
#define FAT32_EOC 0x0FFFFFF8u

typedef struct {
    uint8_t mounted;
    uint8_t block_addressed;
    uint32_t partition_lba;
    uint32_t fat_lba;
    uint32_t data_lba;
    uint32_t sectors_per_fat;
    uint32_t root_cluster;
    uint32_t sectors_per_cluster;
} fat32_state_t;

static fat32_state_t g_fs;
static launcher_storage_entry_t g_entries[12];
static int g_entry_count = 0;
static uint8_t g_sector[SECTOR_SIZE];
static uint8_t g_sector2[SECTOR_SIZE];
static uint8_t g_image_file_buf[256 * 1024];
static char g_debug_text[48] = "BOOT";
static char g_debug_lines[8][64];
static int g_debug_line_count = 0;

static uint32_t fat32_cluster_to_lba(uint32_t cluster);
static int fat32_read_fat_entry(uint32_t cluster, uint32_t *next_cluster);

static void set_debug_text(const char *text)
{
    strncpy(g_debug_text, text, sizeof(g_debug_text) - 1);
    g_debug_text[sizeof(g_debug_text) - 1] = 0;
}

static void debug_log_reset(void)
{
    g_debug_line_count = 0;
    memset(g_debug_lines, 0, sizeof(g_debug_lines));
}

static void debug_log_push(const char *text)
{
    if (g_debug_line_count < (int)(sizeof(g_debug_lines) / sizeof(g_debug_lines[0]))) {
        strncpy(g_debug_lines[g_debug_line_count], text, sizeof(g_debug_lines[0]) - 1);
        g_debug_lines[g_debug_line_count][sizeof(g_debug_lines[0]) - 1] = 0;
        g_debug_line_count++;
    } else {
        for (int i = 1; i < (int)(sizeof(g_debug_lines) / sizeof(g_debug_lines[0])); i++) {
            memcpy(g_debug_lines[i - 1], g_debug_lines[i], sizeof(g_debug_lines[0]));
        }
        strncpy(g_debug_lines[g_debug_line_count - 1], text, sizeof(g_debug_lines[0]) - 1);
        g_debug_lines[g_debug_line_count - 1][sizeof(g_debug_lines[0]) - 1] = 0;
    }
}

static void set_debug_text_count(const char *prefix, int value)
{
    char digits[12];
    int pos = 0;
    unsigned int n = (value < 0) ? (unsigned int)(-value) : (unsigned int)value;

    if (value < 0 && pos < (int)sizeof(digits) - 1) digits[pos++] = '-';

    {
        char rev[10];
        int rev_pos = 0;
        do {
            rev[rev_pos++] = (char)('0' + (n % 10u));
            n /= 10u;
        } while (n != 0 && rev_pos < (int)sizeof(rev));

        while (rev_pos > 0 && pos < (int)sizeof(digits) - 1) {
            digits[pos++] = rev[--rev_pos];
        }
    }
    digits[pos] = 0;

    set_debug_text(prefix);
    if (strlen(g_debug_text) + 1u + strlen(digits) < sizeof(g_debug_text)) {
        strcat(g_debug_text, " ");
        strcat(g_debug_text, digits);
    }
}

static void set_debug_text_name(const char *prefix, const char *name)
{
    set_debug_text(prefix);
    if (strlen(g_debug_text) + 1u + strlen(name) < sizeof(g_debug_text)) {
        strcat(g_debug_text, " ");
        strcat(g_debug_text, name);
    }
}

static void debug_log_pair(const char *prefix, const char *name)
{
    char line[64];
    size_t prefix_len = strlen(prefix);
    size_t name_len = strlen(name);
    if (prefix_len + 1u + name_len >= sizeof(line)) name_len = sizeof(line) - prefix_len - 2u;
    memcpy(line, prefix, prefix_len);
    line[prefix_len] = ' ';
    memcpy(line + prefix_len + 1u, name, name_len);
    line[prefix_len + 1u + name_len] = 0;
    debug_log_push(line);
}

static void debug_log_u32(const char *prefix, uint32_t value)
{
    char digits[16];
    char line[64];
    int pos = 0;
    do {
        digits[pos++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0 && pos < (int)sizeof(digits) - 1);

    strcpy(line, prefix);
    strcat(line, " ");
    while (pos > 0 && strlen(line) + 1u < sizeof(line)) {
        char c[2];
        c[0] = digits[--pos];
        c[1] = 0;
        strcat(line, c);
    }
    debug_log_push(line);
}

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int spi_wait_mask(uint32_t mask)
{
    uint32_t timeout = SD_SPI_TIMEOUT;
    while (timeout-- > 0) {
        if (IORD_ALTERA_AVALON_SPI_STATUS(SD_SPI_0_BASE) & mask) return 1;
    }
    return 0;
}

static uint8_t spi_transfer(uint8_t tx)
{
    if (!spi_wait_mask(ALTERA_AVALON_SPI_STATUS_TRDY_MSK)) return 0xFF;
    IOWR_ALTERA_AVALON_SPI_TXDATA(SD_SPI_0_BASE, tx);
    if (!spi_wait_mask(ALTERA_AVALON_SPI_STATUS_RRDY_MSK)) return 0xFF;
    return (uint8_t)IORD_ALTERA_AVALON_SPI_RXDATA(SD_SPI_0_BASE);
}

static void spi_select(int enable)
{
    if (enable) {
        IOWR_ALTERA_AVALON_SPI_SLAVE_SEL(SD_SPI_0_BASE, 0x1);
        IOWR_ALTERA_AVALON_SPI_CONTROL(SD_SPI_0_BASE, ALTERA_AVALON_SPI_CONTROL_SSO_MSK);
    } else {
        IOWR_ALTERA_AVALON_SPI_CONTROL(SD_SPI_0_BASE, 0);
        IOWR_ALTERA_AVALON_SPI_SLAVE_SEL(SD_SPI_0_BASE, 0);
    }
}

static void sd_send_idle_clocks(void)
{
    spi_select(0);
    for (int i = 0; i < 10; i++) {
        (void)spi_transfer(0xFF);
    }
}

static uint8_t sd_wait_r1(void)
{
    for (int i = 0; i < 16; i++) {
        uint8_t r = spi_transfer(0xFF);
        if ((r & 0x80u) == 0) return r;
    }
    return 0xFF;
}

static uint8_t sd_send_cmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    spi_transfer(0xFF);
    spi_transfer((uint8_t)(0x40u | cmd));
    spi_transfer((uint8_t)(arg >> 24));
    spi_transfer((uint8_t)(arg >> 16));
    spi_transfer((uint8_t)(arg >> 8));
    spi_transfer((uint8_t)arg);
    spi_transfer(crc);
    return sd_wait_r1();
}

static int sd_read_sector(uint32_t lba, uint8_t *buf)
{
    uint8_t r1;
    uint32_t arg = g_fs.block_addressed ? lba : (lba * SECTOR_SIZE);

    spi_select(1);
    r1 = sd_send_cmd(SD_CMD17, arg, 0x01);
    if (r1 != 0x00) {
        spi_select(0);
        spi_transfer(0xFF);
        return 0;
    }

    for (uint32_t i = 0; i < SD_SPI_TIMEOUT; i++) {
        uint8_t token = spi_transfer(0xFF);
        if (token == 0xFE) {
            for (uint32_t j = 0; j < SECTOR_SIZE; j++) buf[j] = spi_transfer(0xFF);
            (void)spi_transfer(0xFF);
            (void)spi_transfer(0xFF);
            spi_select(0);
            spi_transfer(0xFF);
            return 1;
        }
    }

    spi_select(0);
    spi_transfer(0xFF);
    return 0;
}

static launcher_storage_status_t load_raw_file(const launcher_storage_entry_t *entry, uint8_t *dst, uint32_t max_size)
{
    uint32_t remaining;
    uint32_t file_offset = 0;
    uint32_t cluster;

    if (entry == 0 || dst == 0) return LAUNCHER_STORAGE_IO_ERROR;
    if (entry->file_size == 0 || entry->file_size > max_size) return LAUNCHER_STORAGE_IO_ERROR;

    remaining = entry->file_size;
    cluster = entry->first_cluster;
    while (remaining > 0 && cluster >= 2u && cluster < FAT32_EOC) {
        for (uint32_t sec = 0; sec < g_fs.sectors_per_cluster && remaining > 0; sec++) {
            uint32_t lba = fat32_cluster_to_lba(cluster) + sec;
            uint32_t take;
            if (!sd_read_sector(lba, g_sector2)) return LAUNCHER_STORAGE_IO_ERROR;
            take = (remaining > SECTOR_SIZE) ? SECTOR_SIZE : remaining;
            memcpy(dst + file_offset, g_sector2, take);
            file_offset += take;
            remaining -= take;
        }
        if (remaining > 0) {
            uint32_t next;
            if (!fat32_read_fat_entry(cluster, &next)) return LAUNCHER_STORAGE_IO_ERROR;
            cluster = next;
        }
    }

    if (remaining != 0) return LAUNCHER_STORAGE_IO_ERROR;
    return LAUNCHER_STORAGE_OK;
}

static int sd_init_card(void)
{
    uint8_t r1;

    memset(&g_fs, 0, sizeof(g_fs));
    sd_send_idle_clocks();

    spi_select(1);
    r1 = sd_send_cmd(SD_CMD0, 0, 0x95);
    spi_select(0);
    spi_transfer(0xFF);
    if (r1 != 0x01) {
        set_debug_text("CMD0");
        return 0;
    }

    spi_select(1);
    r1 = sd_send_cmd(SD_CMD8, 0x000001AAu, 0x87);
    if (r1 == 0x01) {
        uint8_t r7[4];
        for (int i = 0; i < 4; i++) r7[i] = spi_transfer(0xFF);
        spi_select(0);
        spi_transfer(0xFF);
        if (r7[2] != 0x01 || r7[3] != 0xAA) {
            set_debug_text("CMD8SIG");
            return 0;
        }
    } else {
        spi_select(0);
        spi_transfer(0xFF);
        set_debug_text("CMD8");
        return 0;
    }

    for (int retry = 0; retry < 2000; retry++) {
        spi_select(1);
        r1 = sd_send_cmd(SD_CMD55, 0, 0x01);
        spi_select(0);
        spi_transfer(0xFF);
        if (r1 > 0x01) {
            set_debug_text("CMD55");
            return 0;
        }

        spi_select(1);
        r1 = sd_send_cmd(SD_ACMD41, 0x40000000u, 0x01);
        spi_select(0);
        spi_transfer(0xFF);
        if (r1 == 0x00) break;
        if (retry == 1999) {
            set_debug_text("ACMD41");
            return 0;
        }
    }

    spi_select(1);
    r1 = sd_send_cmd(SD_CMD58, 0, 0x01);
    if (r1 != 0x00) {
        spi_select(0);
        spi_transfer(0xFF);
        set_debug_text("CMD58");
        return 0;
    }
    {
        uint8_t ocr[4];
        for (int i = 0; i < 4; i++) ocr[i] = spi_transfer(0xFF);
        g_fs.block_addressed = (ocr[0] & 0x40u) ? 1u : 0u;
    }
    spi_select(0);
    spi_transfer(0xFF);
    set_debug_text("SD OK");
    return 1;
}

static uint32_t fat32_cluster_to_lba(uint32_t cluster)
{
    return g_fs.data_lba + (cluster - 2u) * g_fs.sectors_per_cluster;
}

static int fat32_read_fat_entry(uint32_t cluster, uint32_t *next_cluster)
{
    uint32_t fat_offset = cluster * 4u;
    uint32_t sector = g_fs.fat_lba + (fat_offset / SECTOR_SIZE);
    uint32_t offset = fat_offset % SECTOR_SIZE;

    if (!sd_read_sector(sector, g_sector)) return 0;
    if (offset > (SECTOR_SIZE - 4u)) return 0;
    *next_cluster = rd32(&g_sector[offset]) & 0x0FFFFFFFu;
    return 1;
}

static int name_equals_ci(const char *a, const char *b)
{
    while (*a && *b) {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) return 0;
        a++;
        b++;
    }
    return (*a == 0 && *b == 0);
}

static void format_short_name(const uint8_t *entry, char *out, int out_len)
{
    int pos = 0;
    for (int i = 0; i < 8 && pos + 1 < out_len; i++) {
        if (entry[i] == ' ') break;
        out[pos++] = (char)tolower(entry[i]);
    }
    if (entry[8] != ' ' && pos + 1 < out_len) {
        out[pos++] = '.';
        for (int i = 8; i < 11 && pos + 1 < out_len; i++) {
            if (entry[i] == ' ') break;
            out[pos++] = (char)tolower(entry[i]);
        }
    }
    out[pos] = 0;
}

static int entry_first_cluster(const uint8_t *entry)
{
    return ((int)rd16(&entry[20]) << 16) | rd16(&entry[26]);
}

static int lfn_extract_piece(const uint8_t *entry, char *dst, int max_chars)
{
    static const uint8_t offsets[] = {1,3,5,7,9,14,16,18,20,22,24,28,30};
    int count = 0;
    for (unsigned i = 0; i < sizeof(offsets); i++) {
        uint16_t ch = rd16(&entry[offsets[i]]);
        if (ch == 0x0000 || ch == 0xFFFF) break;
        if (count + 1 >= max_chars) break;
        dst[count++] = (char)(ch & 0xFFu);
    }
    dst[count] = 0;
    return count;
}

static int find_directory_named(uint32_t dir_cluster, const char *target_name, uint32_t *found_cluster)
{
    char lfn_stack[20][14];
    int lfn_count = 0;
    uint32_t cluster = dir_cluster;

    while (cluster >= 2u && cluster < FAT32_EOC) {
        debug_log_u32("SCAN CL", cluster);
        for (uint32_t sec = 0; sec < g_fs.sectors_per_cluster; sec++) {
            uint32_t lba = fat32_cluster_to_lba(cluster) + sec;
            debug_log_u32("SCAN LBA", lba);
            if (!sd_read_sector(lba, g_sector)) {
                set_debug_text("DIR READ");
                debug_log_push("DIR READ FAIL");
                return 0;
            }
            debug_log_u32("ROOT0", g_sector[0]);
            for (int off = 0; off < (int)SECTOR_SIZE; off += 32) {
                const uint8_t *entry = &g_sector[off];
                uint8_t first = entry[0];
                uint8_t attr = entry[11];
                if (first == 0x00) {
                    debug_log_push("ENTRY 00");
                    return 0;
                }
                if (first == 0xE5) { lfn_count = 0; continue; }
                if (attr == DIR_ATTR_LFN) {
                    if (lfn_count < 20) {
                        lfn_extract_piece(entry, lfn_stack[lfn_count], (int)sizeof(lfn_stack[0]));
                        lfn_count++;
                    }
                    continue;
                }
                if ((attr & DIR_ATTR_VOL) != 0) { lfn_count = 0; continue; }
                if ((attr & DIR_ATTR_DIR) != 0) {
                    char final_name[64];
                    char short_name[32];
                    format_short_name(entry, short_name, sizeof(short_name));
                    if (lfn_count > 0) {
                        final_name[0] = 0;
                        for (int i = lfn_count - 1; i >= 0; i--) strcat(final_name, lfn_stack[i]);
                    } else {
                        strncpy(final_name, short_name, sizeof(final_name) - 1);
                        final_name[sizeof(final_name) - 1] = 0;
                    }
                    lfn_count = 0;
                    set_debug_text_name("DIR?", final_name);
                    debug_log_pair("ROOT DIR", final_name);
                    if (name_equals_ci(final_name, target_name) ||
                        name_equals_ci(short_name, target_name)) {
                        *found_cluster = (uint32_t)entry_first_cluster(entry);
                        set_debug_text_name("DIR", final_name);
                        return 1;
                    }
                } else {
                    char final_name[64];
                    char short_name[32];
                    format_short_name(entry, short_name, sizeof(short_name));
                    if (lfn_count > 0) {
                        final_name[0] = 0;
                        for (int i = lfn_count - 1; i >= 0; i--) strcat(final_name, lfn_stack[i]);
                    } else {
                        strncpy(final_name, short_name, sizeof(final_name) - 1);
                        final_name[sizeof(final_name) - 1] = 0;
                    }
                    debug_log_pair("ROOT FILE", final_name);
                    lfn_count = 0;
                }
            }
        }
        if (!fat32_read_fat_entry(cluster, &cluster)) return 0;
    }
    return 0;
}

static int has_extension(const char *name, const char *ext)
{
    const char *dot = strrchr(name, '.');
    return (dot != 0 && name_equals_ci(dot + 1, ext));
}

static void trim_trailing_spaces(char *text)
{
    size_t len = strlen(text);
    while (len > 0) {
        char c = text[len - 1];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            text[--len] = 0;
        } else {
            break;
        }
    }
}

static void make_display_title(const char *name, char *title, int title_len)
{
    int pos = 0;
    while (*name && *name != '.' && pos + 1 < title_len) {
        title[pos++] = (char)toupper((unsigned char)*name++);
    }
    title[pos] = 0;
}

static void set_entry_title_from_image_header(launcher_storage_entry_t *entry)
{
    launcher_image_header_t hdr;
    uint32_t lba;

    if (entry == 0) return;
    if (entry->file_size < sizeof(hdr)) return;

    lba = fat32_cluster_to_lba(entry->first_cluster);
    if (!sd_read_sector(lba, g_sector2)) return;

    hdr.magic = rd32(&g_sector2[0]);
    hdr.version = rd32(&g_sector2[4]);
    if (hdr.magic != LAUNCHER_IMAGE_MAGIC || hdr.version != LAUNCHER_IMAGE_VERSION) return;

    memset(&hdr.title[0], 0, sizeof(hdr.title));
    memcpy(&hdr.title[0], &g_sector2[32], sizeof(hdr.title));
    hdr.title[sizeof(hdr.title) - 1] = 0;
    trim_trailing_spaces(hdr.title);
    if (hdr.title[0] == 0) return;

    strncpy(entry->title, hdr.title, LAUNCHER_MAX_TITLE - 1);
    entry->title[LAUNCHER_MAX_TITLE - 1] = 0;
}

static int list_games_in_dir(uint32_t dir_cluster)
{
    char lfn_stack[20][14];
    int lfn_count = 0;
    uint32_t cluster = dir_cluster;
    g_entry_count = 0;

    while (cluster >= 2u && cluster < FAT32_EOC) {
        for (uint32_t sec = 0; sec < g_fs.sectors_per_cluster; sec++) {
            if (!sd_read_sector(fat32_cluster_to_lba(cluster) + sec, g_sector)) return g_entry_count;
            for (int off = 0; off < (int)SECTOR_SIZE; off += 32) {
                const uint8_t *entry = &g_sector[off];
                uint8_t first = entry[0];
                uint8_t attr = entry[11];
                if (first == 0x00) return g_entry_count;
                if (first == 0xE5) { lfn_count = 0; continue; }
                if (attr == DIR_ATTR_LFN) {
                    if (lfn_count < 20) {
                        lfn_extract_piece(entry, lfn_stack[lfn_count], (int)sizeof(lfn_stack[0]));
                        lfn_count++;
                    }
                    continue;
                }
                if ((attr & DIR_ATTR_VOL) != 0 || (attr & DIR_ATTR_DIR) != 0) {
                    lfn_count = 0;
                    continue;
                }

                if (g_entry_count < (int)(sizeof(g_entries) / sizeof(g_entries[0]))) {
                    char final_name[64];
                    if (lfn_count > 0) {
                        final_name[0] = 0;
                        for (int i = lfn_count - 1; i >= 0; i--) strcat(final_name, lfn_stack[i]);
                    } else {
                        format_short_name(entry, final_name, sizeof(final_name));
                    }
                    if (has_extension(final_name, "gmod") || has_extension(final_name, "gimg")) {
                        launcher_storage_entry_t *dst = &g_entries[g_entry_count++];
                        memset(dst, 0, sizeof(*dst));
                        strncpy(dst->path, final_name, LAUNCHER_MAX_PATH - 1);
                        make_display_title(final_name, dst->title, LAUNCHER_MAX_TITLE);
                        dst->first_cluster = (uint32_t)entry_first_cluster(entry);
                        dst->file_size = rd32(&entry[28]);
                        set_entry_title_from_image_header(dst);
                        set_debug_text_name("FILE", dst->path);
                    }
                }
                lfn_count = 0;
            }
        }
        if (!fat32_read_fat_entry(cluster, &cluster)) break;
    }
    return g_entry_count;
}

static int list_games_from_root(void)
{
    set_debug_text("ROOT");
    return list_games_in_dir(g_fs.root_cluster);
}

static int mount_fat32(void)
{
    uint32_t boot_lba = 0;

    if (!sd_read_sector(0, g_sector)) {
        set_debug_text("MBR RD");
        return 0;
    }
    if (g_sector[510] != 0x55 || g_sector[511] != 0xAA) {
        set_debug_text("MBR SIG");
        return 0;
    }

    if (g_sector[82] == 'F' && g_sector[83] == 'A' && g_sector[84] == 'T') {
        boot_lba = 0;
    } else {
        boot_lba = rd32(&g_sector[454]);
        if (boot_lba == 0) {
            set_debug_text("PART");
            return 0;
        }
        if (!sd_read_sector(boot_lba, g_sector)) {
            set_debug_text("BOOT RD");
            return 0;
        }
        if (g_sector[510] != 0x55 || g_sector[511] != 0xAA) {
            set_debug_text("BOOTSIG");
            return 0;
        }
    }

    if (rd16(&g_sector[11]) != SECTOR_SIZE) {
        set_debug_text("BPS");
        return 0;
    }
    if (g_sector[16] == 0) {
        set_debug_text("FATS");
        return 0;
    }

    g_fs.partition_lba = boot_lba;
    g_fs.sectors_per_cluster = g_sector[13];
    g_fs.sectors_per_fat = rd32(&g_sector[36]);
    g_fs.root_cluster = rd32(&g_sector[44]);
    g_fs.fat_lba = boot_lba + rd16(&g_sector[14]);
    g_fs.data_lba = g_fs.fat_lba + (g_sector[16] * g_fs.sectors_per_fat);
    debug_log_u32("BOOT", g_fs.partition_lba);
    debug_log_u32("FAT", g_fs.fat_lba);
    debug_log_u32("DATA", g_fs.data_lba);
    debug_log_u32("ROOTCL", g_fs.root_cluster);
    debug_log_u32("SPC", g_fs.sectors_per_cluster);

    if (g_fs.sectors_per_cluster == 0 || g_fs.root_cluster < 2u) {
        set_debug_text("FAT32");
        return 0;
    }
    g_fs.mounted = 1;
    set_debug_text("FAT OK");
    return 1;
}

static launcher_storage_entry_t *find_entry_by_path(const char *path)
{
    for (int i = 0; i < g_entry_count; i++) {
        if (name_equals_ci(g_entries[i].path, path)) return &g_entries[i];
    }
    return 0;
}

static launcher_storage_status_t load_file_from_cluster(uint32_t cluster, uint32_t file_size, launcher_loaded_image_t *image)
{
    uint32_t remaining = file_size;
    uint32_t file_offset = 0;

    if (file_size > sizeof(g_image_file_buf)) return LAUNCHER_STORAGE_IO_ERROR;

    while (remaining > 0 && cluster >= 2u && cluster < FAT32_EOC) {
        for (uint32_t sec = 0; sec < g_fs.sectors_per_cluster && remaining > 0; sec++) {
            uint32_t lba = fat32_cluster_to_lba(cluster) + sec;
            if (!sd_read_sector(lba, g_sector2)) return LAUNCHER_STORAGE_IO_ERROR;
            {
                uint32_t take = (remaining > SECTOR_SIZE) ? SECTOR_SIZE : remaining;
                memcpy(g_image_file_buf + file_offset, g_sector2, take);
                file_offset += take;
                remaining -= take;
            }
        }
        if (remaining > 0) {
            uint32_t next;
            if (!fat32_read_fat_entry(cluster, &next)) return LAUNCHER_STORAGE_IO_ERROR;
            cluster = next;
        }
    }

    if (remaining != 0) return LAUNCHER_STORAGE_IO_ERROR;
    memcpy(&image->header, g_image_file_buf, sizeof(image->header));
    image->file_data = g_image_file_buf;
    image->file_size = file_size;
    if (launcher_image_validate(image) != LAUNCHER_IMAGE_OK) return LAUNCHER_STORAGE_BAD_IMAGE;
    return LAUNCHER_STORAGE_OK;
}

launcher_storage_status_t launcher_storage_init(void)
{
    uint32_t games_cluster;

    memset(&g_fs, 0, sizeof(g_fs));
    g_entry_count = 0;
    debug_log_reset();

    if (!sd_init_card()) return LAUNCHER_STORAGE_NO_MEDIA;
    if (!mount_fat32()) return LAUNCHER_STORAGE_IO_ERROR;
    if (!find_directory_named(g_fs.root_cluster, "games", &games_cluster)) {
        int root_count = list_games_from_root();
        if (root_count > 0) {
            set_debug_text_count("ROOTM", root_count);
            return LAUNCHER_STORAGE_OK;
        }
        set_debug_text("NO GAMES");
        return LAUNCHER_STORAGE_NOT_FOUND;
    }
    list_games_in_dir(games_cluster);
    set_debug_text_count("GMODS", g_entry_count);
    return LAUNCHER_STORAGE_OK;
}

int launcher_storage_list(launcher_storage_entry_t *entries, int max_entries)
{
    if (entries == 0 || max_entries <= 0) return g_entry_count;
    if (max_entries > g_entry_count) max_entries = g_entry_count;
    memcpy(entries, g_entries, (unsigned)max_entries * sizeof(g_entries[0]));
    return max_entries;
}

launcher_storage_status_t launcher_storage_load_image(const char *path, launcher_loaded_image_t *image)
{
    launcher_storage_entry_t *entry;
    if (!g_fs.mounted) return LAUNCHER_STORAGE_NO_MEDIA;
    entry = find_entry_by_path(path);
    if (entry == 0) {
        set_debug_text("NOFILE");
        return LAUNCHER_STORAGE_NOT_FOUND;
    }
    set_debug_text_name("LOAD", entry->path);
    return load_file_from_cluster(entry->first_cluster, entry->file_size, image);
}

static void trim_line(char *text)
{
    size_t len = strlen(text);
    if ((unsigned char)text[0] == 0xEF &&
        (unsigned char)text[1] == 0xBB &&
        (unsigned char)text[2] == 0xBF) {
        memmove(text, text + 3, strlen(text + 3) + 1);
        len = strlen(text);
    }
    while (len > 0) {
        char c = text[len - 1];
        if (c == '\r' || c == '\n' || c == ' ' || c == '\t') {
            text[--len] = 0;
        } else {
            break;
        }
    }
    while (*text == ' ' || *text == '\t') {
        memmove(text, text + 1, strlen(text));
    }
}

launcher_storage_status_t launcher_storage_load_module(const char *path, launcher_loaded_module_t *module)
{
    launcher_storage_entry_t *entry;
    char *line2;

    if (module == 0) return LAUNCHER_STORAGE_IO_ERROR;
    memset(module, 0, sizeof(*module));

    if (!g_fs.mounted) return LAUNCHER_STORAGE_NO_MEDIA;
    entry = find_entry_by_path(path);
    if (entry == 0) {
        set_debug_text("NOFILE");
        return LAUNCHER_STORAGE_NOT_FOUND;
    }
    if (entry->file_size == 0 || entry->file_size >= sizeof(g_image_file_buf)) {
        set_debug_text("GMODSZ");
        return LAUNCHER_STORAGE_IO_ERROR;
    }

    if (load_raw_file(entry, g_image_file_buf, sizeof(g_image_file_buf) - 1u) != LAUNCHER_STORAGE_OK) {
        return LAUNCHER_STORAGE_IO_ERROR;
    }

    g_image_file_buf[entry->file_size] = 0;
    strncpy(module->builtin_path, (const char *)g_image_file_buf, LAUNCHER_MAX_PATH - 1);
    trim_line(module->builtin_path);
    if (module->builtin_path[0] == 0) {
        set_debug_text("GMODFMT");
        return LAUNCHER_STORAGE_BAD_IMAGE;
    }

    line2 = strchr((char *)g_image_file_buf, '\n');
    if (line2 != 0) {
        line2++;
        strncpy(module->title, line2, LAUNCHER_MAX_TITLE - 1);
        trim_line(module->title);
    }
    if (module->title[0] == 0) {
        strncpy(module->title, entry->title, LAUNCHER_MAX_TITLE - 1);
    }
    if (strncmp(module->builtin_path, "external:", 9) == 0) {
        strncpy(module->external_path, module->builtin_path + 9, LAUNCHER_MAX_PATH - 1);
        trim_line(module->external_path);
    }

    set_debug_text_name("GMOD", module->builtin_path);
    return LAUNCHER_STORAGE_OK;
}

launcher_storage_status_t launcher_storage_load_module_file(const char *path, uint8_t *dst, uint32_t max_size)
{
    launcher_storage_entry_t *entry;

    if (!g_fs.mounted) return LAUNCHER_STORAGE_NO_MEDIA;
    entry = find_entry_by_path(path);
    if (entry == 0) {
        set_debug_text("NOFILE");
        return LAUNCHER_STORAGE_NOT_FOUND;
    }
    set_debug_text_name("FILE", entry->path);
    return load_raw_file(entry, dst, max_size);
}

const char *launcher_storage_debug_text(void)
{
    return g_debug_text;
}

int launcher_storage_debug_line_count(void)
{
    return g_debug_line_count;
}

const char *launcher_storage_debug_line(int index)
{
    if (index < 0 || index >= g_debug_line_count) return "";
    return g_debug_lines[index];
}
