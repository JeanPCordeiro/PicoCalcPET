#include "platform.h"

#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>

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

int platform_list_disk_images(const char *root, char *paths, int path_stride, int max_paths)
{
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    const char *extensions[] = { ".dmk", ".dsk", ".jv1", ".jv3" };
    size_t i;
    int count = 0;
    size_t root_len;
    int has_trailing_slash;

    if (root == NULL || root[0] == '\0' || paths == NULL || path_stride <= 1 || max_paths <= 0) {
        return -1;
    }

    dir = opendir(root);
    if (dir == NULL) {
        return -1;
    }

    root_len = strlen(root);
    has_trailing_slash = (root_len > 0 && root[root_len - 1] == '/');

    while ((entry = readdir(dir)) != NULL) {
        const char *dot;
        int supported = 0;
        char full_path[512];

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(full_path, sizeof(full_path), "%s%s%s",
                 root, has_trailing_slash ? "" : "/", entry->d_name);
        full_path[sizeof(full_path) - 1] = '\0';
        if (stat(full_path, &st) != 0 || !S_ISREG(st.st_mode)) {
            continue;
        }

        dot = strrchr(entry->d_name, '.');
        if (dot == NULL) {
            continue;
        }
        for (i = 0; i < (sizeof(extensions) / sizeof(extensions[0])); ++i) {
            if (strcasecmp(dot, extensions[i]) == 0) {
                supported = 1;
                break;
            }
        }
        if (!supported) {
            continue;
        }

        if (count < max_paths) {
            char *slot = paths + (count * path_stride);
            snprintf(slot, (size_t)path_stride, "%s", full_path);
            slot[path_stride - 1] = '\0';
        }
        count++;
    }

    closedir(dir);
    return count;
}
