#include "frontend/osd_menu.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "trs.h"
#include "trs_disk.h"
#include "platform/platform.h"
#include "frontend/status_runtime.h"
#include "emu/picocalc_reset_policy.h"
#ifdef PICOCALC_PLATFORM
#include "pico/time.h"
#include "drivers/fat32.h"
#endif

#define OSD_ROOT_DISKS "/TRS80/DISKS"
#define OSD_CONFIG_PATH "/config/picocalc_trs_osd.cfg"
#define OSD_MAX_IMAGES 64
#define OSD_ROWS 16
#define OSD_COLS 64
#define OSD_PICKER_PAGE_SIZE 8

typedef struct {
    char paths[OSD_MAX_IMAGES][FILENAME_MAX];
    int count;
} osd_media_catalog_t;

typedef struct {
    int drive_choice[2]; /* -1 means empty */
    int cursor;
} osd_menu_state_t;

typedef struct {
    int startup_timeout_seconds;
    char d0_path[FILENAME_MAX];
    char d1_path[FILENAME_MAX];
} osd_config_t;

static osd_config_t osd_config;
static int osd_config_loaded;

static uint64_t osd_now_ms(void)
{
#ifdef PICOCALC_PLATFORM
    return to_ms_since_boot(get_absolute_time());
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000u);
#endif
}

static const char *osd_leaf_name(const char *path)
{
    const char *leaf = path;
    const char *p;

    if (path == NULL || path[0] == '\0') {
        return "Empty";
    }

    for (p = path; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') {
            leaf = p + 1;
        }
    }
    return (leaf[0] != '\0') ? leaf : path;
}

static int osd_starts_with(const char *value, const char *prefix)
{
    size_t prefix_len;
    if (value == NULL || prefix == NULL) {
        return 0;
    }
    prefix_len = strlen(prefix);
    return strncmp(value, prefix, prefix_len) == 0;
}

static void osd_trim_line(char *line)
{
    size_t len;
    if (line == NULL) {
        return;
    }
    len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' ||
                       line[len - 1] == ' ' || line[len - 1] == '\t')) {
        line[len - 1] = '\0';
        len--;
    }
}

static void osd_config_set_defaults(void)
{
    memset(&osd_config, 0, sizeof(osd_config));
    osd_config.startup_timeout_seconds = 3;
}

#ifdef PICOCALC_PLATFORM
static int osd_config_read_text(char *buffer, size_t buffer_size)
{
    fat32_file_t file;
    fat32_error_t err;
    size_t total = 0;

    if (buffer == NULL || buffer_size < 2) {
        return -1;
    }

    err = fat32_open(&file, OSD_CONFIG_PATH);
    if (err != FAT32_OK) {
        return -1;
    }

    while (total < (buffer_size - 1)) {
        size_t chunk = 0;
        err = fat32_read(&file, buffer + total, (buffer_size - 1) - total, &chunk);
        if (err != FAT32_OK) {
            fat32_close(&file);
            return -1;
        }
        if (chunk == 0) {
            break;
        }
        total += chunk;
    }

    fat32_close(&file);
    buffer[total] = '\0';
    return (int)total;
}

static void osd_config_write_text(const char *text)
{
    fat32_file_t dir;
    fat32_file_t file;
    fat32_error_t err;
    size_t written = 0;
    size_t total_len;

    if (text == NULL) {
        return;
    }

    err = fat32_dir_create(&dir, "/config");
    if (err != FAT32_OK && err != FAT32_ERROR_FILE_EXISTS) {
        return;
    }
    if (err == FAT32_OK) {
        fat32_close(&dir);
    }

    fat32_delete(OSD_CONFIG_PATH);
    err = fat32_create(&file, OSD_CONFIG_PATH);
    if (err != FAT32_OK) {
        return;
    }

    total_len = strlen(text);
    err = fat32_write(&file, text, total_len, &written);
    fat32_close(&file);
    if (err != FAT32_OK || written != total_len) {
        return;
    }
}
#endif

static void osd_config_load(void)
{
    char text[768];
    char *line;
    char *next;

    if (osd_config_loaded) {
        return;
    }
    osd_config_loaded = 1;
    osd_config_set_defaults();

#ifdef PICOCALC_PLATFORM
    if (osd_config_read_text(text, sizeof(text)) < 0) {
        return;
    }
#else
    return;
#endif

    line = text;
    while (line != NULL && *line != '\0') {
        char *equals;

        next = strchr(line, '\n');
        if (next != NULL) {
            *next = '\0';
            next++;
        }

        osd_trim_line(line);
        equals = strchr(line, '=');
        if (equals != NULL) {
            *equals = '\0';
            equals++;

            if (strcmp(line, "STARTUP_TIMEOUT") == 0) {
                int value = atoi(equals);
                if (value >= 0 && value <= 30) {
                    osd_config.startup_timeout_seconds = value;
                }
            } else if (strcmp(line, "D0") == 0) {
                snprintf(osd_config.d0_path, sizeof(osd_config.d0_path), "%s", equals);
            } else if (strcmp(line, "D1") == 0) {
                snprintf(osd_config.d1_path, sizeof(osd_config.d1_path), "%s", equals);
            }
        }

        line = next;
    }
}

static void osd_config_save_from_state(const osd_media_catalog_t *catalog, const osd_menu_state_t *state)
{
    char text[1024];

    if (state->drive_choice[0] >= 0 && state->drive_choice[0] < catalog->count) {
        snprintf(osd_config.d0_path, sizeof(osd_config.d0_path), "%s",
                 catalog->paths[state->drive_choice[0]]);
    } else {
        osd_config.d0_path[0] = '\0';
    }

    if (state->drive_choice[1] >= 0 && state->drive_choice[1] < catalog->count) {
        snprintf(osd_config.d1_path, sizeof(osd_config.d1_path), "%s",
                 catalog->paths[state->drive_choice[1]]);
    } else {
        osd_config.d1_path[0] = '\0';
    }

    snprintf(text, sizeof(text),
             "STARTUP_TIMEOUT=%d\n"
             "D0=%s\n"
             "D1=%s\n",
             osd_config.startup_timeout_seconds,
             osd_config.d0_path,
             osd_config.d1_path);

#ifdef PICOCALC_PLATFORM
    osd_config_write_text(text);
#endif
}

static void osd_write_line(int row, const char *text)
{
    int col;

    for (col = 0; col < OSD_COLS; ++col) {
        uint8_t ch = ' ';
        if (text != NULL && text[col] != '\0') {
            ch = (uint8_t)text[col];
        }
        platform_screen_write_cell(col, row, ch, 0);
    }
}

static int osd_catalog_compare_path(const void *lhs, const void *rhs)
{
    const char *a = (const char *)lhs;
    const char *b = (const char *)rhs;
    return strcasecmp(osd_leaf_name(a), osd_leaf_name(b));
}

static int osd_is_filtered_meta_leaf(const char *leaf)
{
    if (leaf == NULL || leaf[0] == '\0') {
        return 1;
    }

    /* Hide common hidden/meta names often produced by host tooling. */
    if (leaf[0] == '.') {
        return 1;
    }
    if (strncmp(leaf, "._", 2) == 0) {
        return 1;
    }
    if (strcmp(leaf, "Thumbs.db") == 0 || strcmp(leaf, "Desktop.ini") == 0) {
        return 1;
    }

    return 0;
}

static void osd_filter_catalog(osd_media_catalog_t *catalog)
{
    int in_idx;
    int out_idx = 0;

    if (catalog == NULL || catalog->count <= 0) {
        return;
    }

    for (in_idx = 0; in_idx < catalog->count; ++in_idx) {
        const char *leaf = osd_leaf_name(catalog->paths[in_idx]);
        if (osd_is_filtered_meta_leaf(leaf)) {
            continue;
        }

        if (out_idx != in_idx) {
            snprintf(catalog->paths[out_idx], sizeof(catalog->paths[out_idx]), "%s",
                     catalog->paths[in_idx]);
        }
        out_idx++;
    }

    catalog->count = out_idx;
}

static void osd_sort_catalog(osd_media_catalog_t *catalog)
{
    if (catalog == NULL || catalog->count <= 1) {
        return;
    }

    qsort(catalog->paths, (size_t)catalog->count, sizeof(catalog->paths[0]), osd_catalog_compare_path);
}

static void osd_draw(const osd_media_catalog_t *catalog, const osd_menu_state_t *state,
                     int startup_mode, int countdown_seconds)
{
    char line[80];
    const char *d0 = "Empty";
    const char *d1 = "Empty";
    int row;

    if (state->drive_choice[0] >= 0 && state->drive_choice[0] < catalog->count) {
        d0 = osd_leaf_name(catalog->paths[state->drive_choice[0]]);
    }
    if (state->drive_choice[1] >= 0 && state->drive_choice[1] < catalog->count) {
        d1 = osd_leaf_name(catalog->paths[state->drive_choice[1]]);
    }

    for (row = 0; row < OSD_ROWS; ++row) {
        osd_write_line(row, "");
    }

    snprintf(line, sizeof(line), " PicoCalc TRS OSD");
    osd_write_line(0, line);
    snprintf(line, sizeof(line), " Root: %s", OSD_ROOT_DISKS);
    osd_write_line(1, line);
    snprintf(line, sizeof(line), " Found images: %d", catalog->count);
    osd_write_line(2, line);

    snprintf(line, sizeof(line), "%c D0: %s", state->cursor == 0 ? '>' : ' ', d0);
    osd_write_line(4, line);
    snprintf(line, sizeof(line), "%c D1: %s", state->cursor == 1 ? '>' : ' ', d1);
    osd_write_line(5, line);

    snprintf(line, sizeof(line), "%c Apply and Resume", state->cursor == 2 ? '>' : ' ');
    osd_write_line(7, line);
    snprintf(line, sizeof(line), "%c Apply and Reset", state->cursor == 3 ? '>' : ' ');
    osd_write_line(8, line);
    snprintf(line, sizeof(line), "%c Cancel", state->cursor == 4 ? '>' : ' ');
    osd_write_line(9, line);

    if (startup_mode) {
        snprintf(line, sizeof(line), " Auto-boot in %d sec (press any key)", countdown_seconds);
    } else {
        snprintf(line, sizeof(line), " F4/Esc close");
    }
    osd_write_line(12, line);
    osd_write_line(13, " Enter on D0/D1 opens disk picker");
    osd_write_line(15, " /TRS80/DISKS only");
    platform_screen_flush();
}

static void osd_draw_picker(const osd_media_catalog_t *catalog, int drive, int selected)
{
    char line[80];
    int row;
    int page;
    int page_start;
    int page_count;

    for (row = 0; row < OSD_ROWS; ++row) {
        osd_write_line(row, "");
    }

    page = (selected < 0) ? 0 : (selected / OSD_PICKER_PAGE_SIZE);
    page_start = page * OSD_PICKER_PAGE_SIZE;
    page_count = (catalog->count + OSD_PICKER_PAGE_SIZE - 1) / OSD_PICKER_PAGE_SIZE;
    if (page_count <= 0) {
        page_count = 1;
    }

    snprintf(line, sizeof(line), " Select image for D%d", drive);
    osd_write_line(0, line);
    snprintf(line, sizeof(line), " %d images sorted by filename", catalog->count);
    osd_write_line(1, line);
    snprintf(line, sizeof(line), " Page %d/%d", page + 1, page_count);
    osd_write_line(2, line);

    for (row = 0; row < OSD_PICKER_PAGE_SIZE; ++row) {
        int idx = page_start + row;
        if (idx >= catalog->count) {
            break;
        }
        snprintf(line, sizeof(line), "%c %s", idx == selected ? '>' : ' ',
                 osd_leaf_name(catalog->paths[idx]));
        osd_write_line(4 + row, line);
    }

    osd_write_line(13, " Up/Down=move PgUp/PgDn=page");
    osd_write_line(14, " Enter=select  Backspace=eject");
    osd_write_line(15, " Esc=cancel");
    platform_screen_flush();
}

static int osd_pick_image(const osd_media_catalog_t *catalog, int drive, int current_choice)
{
    int selected = current_choice;
    int keycode;
    int redraw = 1;

    if (catalog->count <= 0) {
        return -1;
    }
    if (selected < 0 || selected >= catalog->count) {
        selected = 0;
    }

    for (;;) {
        if (redraw) {
            osd_draw_picker(catalog, drive, selected);
            redraw = 0;
        }

        if (!platform_poll_key(&keycode, true)) {
            continue;
        }

        if (keycode == PLATFORM_KEY_UP) {
            selected = (selected + catalog->count - 1) % catalog->count;
            redraw = 1;
        } else if (keycode == PLATFORM_KEY_DOWN) {
            selected = (selected + 1) % catalog->count;
            redraw = 1;
        } else if (keycode == PLATFORM_KEY_PAGE_UP) {
            selected -= OSD_PICKER_PAGE_SIZE;
            if (selected < 0) {
                selected = 0;
            }
            redraw = 1;
        } else if (keycode == PLATFORM_KEY_PAGE_DOWN) {
            selected += OSD_PICKER_PAGE_SIZE;
            if (selected >= catalog->count) {
                selected = catalog->count - 1;
            }
            redraw = 1;
        } else if (keycode == PLATFORM_KEY_ENTER) {
            return selected;
        } else if (keycode == PLATFORM_KEY_BACKSPACE) {
            return -1;
        } else if (keycode == PLATFORM_KEY_ESC || keycode == PLATFORM_KEY_F4) {
            return current_choice;
        }
    }
}

static int osd_find_choice(const osd_media_catalog_t *catalog, const char *path)
{
    int i;

    if (path == NULL || path[0] == '\0') {
        return -1;
    }
    for (i = 0; i < catalog->count; ++i) {
        if (strcmp(path, catalog->paths[i]) == 0) {
            return i;
        }
    }
    return -1;
}

static void osd_cycle_choice(const osd_media_catalog_t *catalog, int *choice, int step)
{
    int span;
    int value;

    if (choice == NULL || catalog->count <= 0) {
        if (choice != NULL) {
            *choice = -1;
        }
        return;
    }

    span = catalog->count + 1; /* include Empty */
    value = (*choice < 0) ? 0 : (*choice + 1);
    value += step;
    while (value < 0) {
        value += span;
    }
    value %= span;
    *choice = value - 1;
}

static void osd_apply_selection(const osd_media_catalog_t *catalog, const osd_menu_state_t *state)
{
    int drive;
    int any_mounted = 0;

    for (drive = 0; drive < 2; ++drive) {
        if (state->drive_choice[drive] >= 0 && state->drive_choice[drive] < catalog->count) {
            trs_disk_insert(drive, catalog->paths[state->drive_choice[drive]]);
            any_mounted = 1;
        } else {
            trs_disk_remove(drive);
        }
    }

    trs_disk_controller = any_mounted ? 1 : 0;
}

static void osd_commit_startup_output(const osd_media_catalog_t *catalog, const osd_menu_state_t *state,
                                      char disk0_path[FILENAME_MAX], bool *disk0_enabled,
                                      char disk1_path[FILENAME_MAX], bool *disk1_enabled)
{
    if (state->drive_choice[0] >= 0 && state->drive_choice[0] < catalog->count) {
        snprintf(disk0_path, FILENAME_MAX, "%s", catalog->paths[state->drive_choice[0]]);
        *disk0_enabled = true;
    } else {
        disk0_path[0] = '\0';
        *disk0_enabled = false;
    }

    if (state->drive_choice[1] >= 0 && state->drive_choice[1] < catalog->count) {
        snprintf(disk1_path, FILENAME_MAX, "%s", catalog->paths[state->drive_choice[1]]);
        *disk1_enabled = true;
    } else {
        disk1_path[0] = '\0';
        *disk1_enabled = false;
    }
}

static void osd_load_catalog(osd_media_catalog_t *catalog)
{
    int listed;

    memset(catalog, 0, sizeof(*catalog));
    listed = platform_list_disk_images(OSD_ROOT_DISKS, (char *)catalog->paths,
                                       FILENAME_MAX, OSD_MAX_IMAGES);
    if (listed > 0) {
        catalog->count = listed;
        if (catalog->count > OSD_MAX_IMAGES) {
            catalog->count = OSD_MAX_IMAGES;
        }
        osd_filter_catalog(catalog);
        osd_sort_catalog(catalog);
    } else {
        catalog->count = 0;
    }
}

void osd_startup_menu(char disk0_path[FILENAME_MAX], bool *disk0_enabled,
                      char disk1_path[FILENAME_MAX], bool *disk1_enabled,
                      int timeout_seconds)
{
    osd_media_catalog_t catalog;
    osd_menu_state_t state;
    uint64_t start_ms;
    int timeout_ms;
    int interactive = 0;
    int keycode;
    int last_seconds = -1;
    int redraw = 1;

    if (disk0_path == NULL || disk1_path == NULL || disk0_enabled == NULL || disk1_enabled == NULL) {
        return;
    }

    osd_config_load();

    if (*disk0_enabled == false && osd_starts_with(osd_config.d0_path, OSD_ROOT_DISKS)) {
        snprintf(disk0_path, FILENAME_MAX, "%s", osd_config.d0_path);
        *disk0_enabled = true;
    }
    if (*disk1_enabled == false && osd_starts_with(osd_config.d1_path, OSD_ROOT_DISKS)) {
        snprintf(disk1_path, FILENAME_MAX, "%s", osd_config.d1_path);
        *disk1_enabled = true;
    }

    osd_load_catalog(&catalog);
    state.drive_choice[0] = *disk0_enabled ? osd_find_choice(&catalog, disk0_path) : -1;
    state.drive_choice[1] = *disk1_enabled ? osd_find_choice(&catalog, disk1_path) : -1;
    state.cursor = 2;

    if (timeout_seconds <= 0) {
        timeout_seconds = osd_config.startup_timeout_seconds;
    }
    timeout_ms = timeout_seconds > 0 ? timeout_seconds * 1000 : 0;
    start_ms = osd_now_ms();

    for (;;) {
        int remaining_ms = 0;
        int remaining_seconds = 0;

        if (!interactive && timeout_ms > 0) {
            uint64_t elapsed = osd_now_ms() - start_ms;
            if (elapsed >= (uint64_t)timeout_ms) {
                osd_commit_startup_output(&catalog, &state, disk0_path, disk0_enabled, disk1_path, disk1_enabled);
                osd_config_save_from_state(&catalog, &state);
                return;
            }
            remaining_ms = timeout_ms - (int)elapsed;
            remaining_seconds = (remaining_ms + 999) / 1000;
        }

        if (redraw || remaining_seconds != last_seconds) {
            osd_draw(&catalog, &state, 1, remaining_seconds);
            last_seconds = remaining_seconds;
            redraw = 0;
        }

        if (!platform_poll_key(&keycode, false)) {
#ifdef PICOCALC_PLATFORM
            sleep_ms(10);
#endif
            continue;
        }

        interactive = 1;
        redraw = 1;
        if (keycode == PLATFORM_KEY_UP) {
            state.cursor = (state.cursor + 4) % 5;
        } else if (keycode == PLATFORM_KEY_DOWN) {
            state.cursor = (state.cursor + 1) % 5;
        } else if (keycode == PLATFORM_KEY_LEFT) {
            if (state.cursor == 0 || state.cursor == 1) {
                osd_cycle_choice(&catalog, &state.drive_choice[state.cursor], -1);
            }
        } else if (keycode == PLATFORM_KEY_RIGHT) {
            if (state.cursor == 0 || state.cursor == 1) {
                osd_cycle_choice(&catalog, &state.drive_choice[state.cursor], 1);
            }
        } else if (keycode == PLATFORM_KEY_BACKSPACE) {
            if (state.cursor == 0 || state.cursor == 1) {
                state.drive_choice[state.cursor] = -1;
            }
        } else if (keycode == PLATFORM_KEY_ESC) {
            return;
        } else if (keycode == PLATFORM_KEY_ENTER) {
            if (state.cursor == 0 || state.cursor == 1) {
                state.drive_choice[state.cursor] = osd_pick_image(
                    &catalog, state.cursor, state.drive_choice[state.cursor]);
            } else if (state.cursor == 2 || state.cursor == 3) {
                osd_commit_startup_output(&catalog, &state, disk0_path, disk0_enabled, disk1_path, disk1_enabled);
                osd_config_save_from_state(&catalog, &state);
                return;
            } else if (state.cursor == 4) {
                return;
            }
        }
    }
}

void osd_runtime_menu(void)
{
    osd_media_catalog_t catalog;
    osd_menu_state_t state;
    int keycode;

    osd_config_load();
    osd_load_catalog(&catalog);
    state.drive_choice[0] = osd_find_choice(&catalog, trs_disk_getfilename(0));
    state.drive_choice[1] = osd_find_choice(&catalog, trs_disk_getfilename(1));
    state.cursor = 2;

    for (;;) {
        osd_draw(&catalog, &state, 0, 0);
        if (!platform_poll_key(&keycode, true)) {
            continue;
        }

        if (keycode == PLATFORM_KEY_UP) {
            state.cursor = (state.cursor + 4) % 5;
        } else if (keycode == PLATFORM_KEY_DOWN) {
            state.cursor = (state.cursor + 1) % 5;
        } else if (keycode == PLATFORM_KEY_LEFT) {
            if (state.cursor == 0 || state.cursor == 1) {
                osd_cycle_choice(&catalog, &state.drive_choice[state.cursor], -1);
            }
        } else if (keycode == PLATFORM_KEY_RIGHT) {
            if (state.cursor == 0 || state.cursor == 1) {
                osd_cycle_choice(&catalog, &state.drive_choice[state.cursor], 1);
            }
        } else if (keycode == PLATFORM_KEY_BACKSPACE) {
            if (state.cursor == 0 || state.cursor == 1) {
                state.drive_choice[state.cursor] = -1;
            }
        } else if (keycode == PLATFORM_KEY_ESC || keycode == PLATFORM_KEY_F4) {
            break;
        } else if (keycode == PLATFORM_KEY_ENTER) {
            if (state.cursor == 0 || state.cursor == 1) {
                state.drive_choice[state.cursor] = osd_pick_image(
                    &catalog, state.cursor, state.drive_choice[state.cursor]);
            } else if (state.cursor == 2) {
                osd_apply_selection(&catalog, &state);
                osd_config_save_from_state(&catalog, &state);
                picocalc_status_set_message("OSD: media applied");
                break;
            } else if (state.cursor == 3) {
                osd_apply_selection(&catalog, &state);
                osd_config_save_from_state(&catalog, &state);
                trs_reset(1);
                picocalc_apply_post_reset_policy();
                picocalc_status_set_message("OSD: media applied + reset");
                break;
            } else if (state.cursor == 4) {
                break;
            }
        }
    }

    trs_screen_refresh();
}
