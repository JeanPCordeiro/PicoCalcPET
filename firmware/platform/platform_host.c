#include "platform.h"

#include <stdbool.h>
#include <dirent.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

typedef struct {
    uint8_t ch;
    uint8_t mode;
} host_cell_t;

static host_cell_t host_screen[80 * 24];
static int host_cols = 64;
static int host_rows = 16;
static bool host_dirty;
static int host_last_file_error;
static const char *host_last_file_error_text = "not checked";

void platform_init(void)
{
    memset(host_screen, 0, sizeof(host_screen));
    host_dirty = false;
}

bool platform_poll_key(int *keycode, bool wait)
{
    (void)keycode;
    (void)wait;
    return false;
}

void platform_screen_configure(int cols, int rows)
{
    host_cols = cols;
    host_rows = rows;
    memset(host_screen, 0, sizeof(host_screen));
    host_dirty = true;
}

void platform_screen_write_cell(int col, int row, uint8_t ch, uint8_t mode)
{
    size_t index;

    if (col < 0 || col >= host_cols || row < 0 || row >= host_rows) {
        return;
    }

    index = (size_t)row * (size_t)host_cols + (size_t)col;
    if (index >= (sizeof(host_screen) / sizeof(host_screen[0]))) {
        return;
    }

    host_screen[index].ch = ch;
    host_screen[index].mode = mode;
    host_dirty = true;
}

void platform_screen_write_glyph8(int col, int row, const uint8_t glyph[8], uint8_t mode)
{
    (void)glyph;
    platform_screen_write_cell(col, row, '#', mode);
}

void platform_screen_flush(void)
{
    host_dirty = false;
}

void platform_status_clear(void)
{
}

void platform_status_puts(const char *text)
{
    if (text == NULL) {
        return;
    }

    fprintf(stderr, "%s\n", text);
}

void platform_status_write_line(int line, const char *text)
{
    (void)line;
    platform_status_puts(text);
}

void platform_status_set_operator_context(const char *rom_source, int rom_size,
                                          const char *disk0_path, bool disk0_present, bool disk0_write_protected,
                                          const char *disk1_path, bool disk1_present, bool disk1_write_protected)
{
    (void)rom_source;
    (void)rom_size;
    (void)disk0_path;
    (void)disk0_present;
    (void)disk0_write_protected;
    (void)disk1_path;
    (void)disk1_present;
    (void)disk1_write_protected;
}

void platform_status_refresh_operator(uint16_t pc, float clock_mhz, bool audio_enabled,
                                      bool disk_sfx_enabled)
{
    (void)pc;
    (void)clock_mhz;
    (void)audio_enabled;
    (void)disk_sfx_enabled;
}

void platform_set_disk_led(int drive, int on_off)
{
    (void)drive;
    (void)on_off;
}

void platform_set_hard_led(int drive, int on_off)
{
    (void)drive;
    (void)on_off;
}

void platform_set_turbo_led(bool enabled)
{
    (void)enabled;
}

bool platform_file_exists(const char *path)
{
    FILE *fp;

    if (path == NULL || path[0] == '\0') {
        host_last_file_error = -1;
        host_last_file_error_text = "invalid path";
        return false;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        host_last_file_error = -2;
        host_last_file_error_text = "host fopen failed";
        return false;
    }

    fclose(fp);
    host_last_file_error = 0;
    host_last_file_error_text = "ok";
    return true;
}

int platform_list_disk_images(const char *dir_path, platform_disk_image_t *images, int max_images)
{
    DIR *dir;
    struct dirent *entry;
    int count = 0;

    if (dir_path == NULL || images == NULL || max_images <= 0) {
        host_last_file_error = -1;
        host_last_file_error_text = "invalid parameter";
        return -1;
    }

    dir = opendir(dir_path);
    if (dir == NULL) {
        host_last_file_error = -3;
        host_last_file_error_text = "host opendir failed";
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        const char *dot = strrchr(entry->d_name, '.');

        if (dot == NULL ||
            (strcasecmp(dot, ".prg") != 0 && strcasecmp(dot, ".d64") != 0)) {
            continue;
        }
        if (count < max_images) {
            strncpy(images[count].name, entry->d_name, PLATFORM_DISK_IMAGE_NAME_MAX - 1);
            images[count].name[PLATFORM_DISK_IMAGE_NAME_MAX - 1] = '\0';
            count++;
        }
    }

    closedir(dir);
    host_last_file_error = 0;
    host_last_file_error_text = "ok";
    return count;
}

int platform_last_file_error_code(void)
{
    return host_last_file_error;
}

const char *platform_last_file_error(void)
{
    return host_last_file_error_text;
}

int platform_sd_detect_state(void)
{
    return -1;
}

bool platform_sd_card_present(void)
{
    return false;
}
