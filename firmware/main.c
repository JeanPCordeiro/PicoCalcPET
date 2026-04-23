#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "trs.h"
#include "trs_disk.h"
#include "z80.h"
#include "platform/platform.h"
#include "platform/platform_file.h"
#include "emu/picocalc_reset_policy.h"
#ifndef PICOCALC_PATH_MAX
#define PICOCALC_PATH_MAX 260
#endif
#if defined(PICOCALC_PLATFORM)
#include "drivers/fat32.h"
#define DISK_PICKER_MAX_IMAGES 64
#undef PICOCALC_PATH_MAX
#define PICOCALC_PATH_MAX FAT32_MAX_PATH_LEN

typedef struct {
    char filename[PICOCALC_PATH_MAX];
} disk_picker_entry_t;
#endif
#include "frontend/osd_menu.h"

extern const char *program_name;

static void write_line_centered(int row, const char *text)
{
    size_t len;
    int col;
    size_t i;

    if (text == NULL) {
        return;
    }

    len = strlen(text);
    if (len > 64) {
        len = 64;
    }

    col = (64 - (int)len) / 2;
    if (col < 0) {
        col = 0;
    }

    for (i = 0; i < len; ++i) {
        platform_screen_write_cell(col + (int)i, row, (uint8_t)text[i], 0);
    }
}

static void write_line(int row, const char *text)
{
    size_t i;

    if (text == NULL) {
        return;
    }

    for (i = 0; text[i] != '\0' && i < 64; ++i) {
        platform_screen_write_cell((int)i, row, (uint8_t)text[i], 0);
    }
}

static void clear_line_cells(int row)
{
    int col;

    for (col = 0; col < 64; ++col) {
        platform_screen_write_cell(col, row, ' ', 0);
    }
}

static void show_boot_banner(void)
{
    clear_line_cells(2);
    write_line_centered(2, "PicoCalc TRS-80 Model III");
}

static void show_boot_stage(const char *text)
{
    clear_line_cells(4);
    write_line_centered(4, text);
    platform_screen_flush();
}

static void status_printf(const char *format, ...)
{
    char buffer[65];
    va_list args;

    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    platform_status_puts(buffer);
}

static const char *status_leaf_name(const char *path)
{
    const char *cursor;
    const char *leaf;

    if (path == NULL || path[0] == '\0') {
        return "none";
    }

    leaf = path;
    for (cursor = path; *cursor != '\0'; ++cursor) {
        if (*cursor == '/' || *cursor == '\\') {
            leaf = cursor + 1;
        }
    }

    return (leaf[0] != '\0') ? leaf : path;
}

static bool is_allowed_disk_image_name(const char *filename)
{
    const char *extension;
    char extension_lower[5];
    size_t i;

    if (filename == NULL || filename[0] == '\0' || filename[0] == '.') {
        return false;
    }

    extension = strrchr(filename, '.');
    if (extension == NULL || strlen(extension) != 4) {
        return false;
    }

    for (i = 0; i < 4; ++i) {
        extension_lower[i] = (char)tolower((unsigned char)extension[i]);
    }
    extension_lower[4] = '\0';

    return strcmp(extension_lower, ".dsk") == 0 ||
           strcmp(extension_lower, ".dmk") == 0 ||
           strcmp(extension_lower, ".jv1") == 0 ||
           strcmp(extension_lower, ".jv3") == 0;
}

static void set_selected_disk_path(char *buffer, size_t buffer_size,
                                   const char *dir_path, const char *filename)
{
    char candidate[PICOCALC_PATH_MAX];

    if (buffer == NULL || buffer_size == 0 || dir_path == NULL || filename == NULL) {
        return;
    }

    snprintf(candidate, sizeof(candidate), "TRS80/DISKS/%s", filename);
    if (platform_file_exists(candidate)) {
        strncpy(buffer, candidate, buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
        return;
    }

    snprintf(candidate, sizeof(candidate), "/TRS80/DISKS/%s", filename);
    if (platform_file_exists(candidate)) {
        strncpy(buffer, candidate, buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
        return;
    }

    snprintf(buffer, buffer_size, "%s/%s", dir_path, filename);
    buffer[buffer_size - 1] = '\0';
}

static void show_runtime_status(bool disk0_found, const char *disk0_path,
                                bool disk1_found, const char *disk1_path)
{
    const char *rom_label;

    rom_label = (strncmp(romfile3, "embedded:", 9) == 0) ? "embedded" : "file";

    platform_status_clear();
    status_printf("ROM:%s %dB PC:%04X D0:%c D1:%c",
                  rom_label,
                  trs_rom_size,
                  Z80_PC,
                  disk0_found ? 'Y' : 'N',
                  disk1_found ? 'Y' : 'N');
    if (disk0_found) {
        status_printf("Disk0:%s (%s)", status_leaf_name(disk0_path),
                      trs_disk_getwriteprotect(0) ? "ro" : "rw");
    } else {
        platform_status_puts("Disk0:none");
    }
    if (disk1_found) {
        status_printf("Disk1:%s (%s)", status_leaf_name(disk1_path),
                      trs_disk_getwriteprotect(1) ? "ro" : "rw");
    } else {
        platform_status_puts("Disk1:none");
    }
}

static void show_missing_rom_screen(void)
{
    char detail[65];
    char detect[65];

    platform_screen_configure(64, 16);
    write_line_centered(2, "TRS-80 Model III ROM Missing");
    write_line_centered(5, "Put the ROM at:");
    write_line_centered(7, "/TRS80/ROMS/model3.rom");
    write_line_centered(10, "Also accepted:");
    write_line_centered(11, "TRS80/ROMS/trs80m3.rom");
    snprintf(detect, sizeof(detect), "SD detect: gpio=%d present=%d",
             platform_sd_detect_state(),
             platform_sd_card_present() ? 1 : 0);
    snprintf(detail, sizeof(detail), "Probe: %s (%d)",
             platform_last_file_error(),
             platform_last_file_error_code());
    write_line(12, detect);
    write_line(13, detail);
    write_line_centered(15, "Power cycle after copying.");
    platform_screen_flush();
}

static void show_disk_directory_screen_and_select(char *disk0_path, size_t disk0_path_size,
                                                  bool *disk0_selected, char *disk1_path,
                                                  size_t disk1_path_size, bool *disk1_selected)
{
    int row;

    platform_screen_configure(64, 16);
    for (row = 0; row < 16; ++row) {
        clear_line_cells(row);
    }

    write_line_centered(0, "SD Disk Picker");
    write_line_centered(1, "/TRS80/DISKS");

#if defined(PICOCALC_PLATFORM)
    {
        static const char *candidate_dirs[] = {
            "TRS80/DISKS",
            "/TRS80/DISKS"
        };
        const int max_visible_entries = 8;
        const int list_first_row = 4;
        const int d0_row = 13;
        const int d1_row = 14;
        const int hint_row = 15;
        static disk_picker_entry_t entries[DISK_PICKER_MAX_IMAGES];
        fat32_file_t dir;
        fat32_entry_t entry;
        fat32_error_t result;
        const char *opened_dir;
        char line[65];
        size_t dir_index;
        unsigned int total_entries;
        int shown_entries;
        int selected_index;
        int top_index;
        int keycode;
        bool read_error;

        memset(&dir, 0, sizeof(dir));
        memset(entries, 0, sizeof(entries));
        opened_dir = NULL;
        result = FAT32_ERROR_DIR_NOT_FOUND;
        total_entries = 0;
        shown_entries = 0;
        selected_index = 0;
        top_index = 0;
        read_error = false;

        (void)fat32_set_current_dir("/");
        for (dir_index = 0; dir_index < (sizeof(candidate_dirs) / sizeof(candidate_dirs[0])); ++dir_index) {
            result = fat32_open(&dir, candidate_dirs[dir_index]);
            if (result == FAT32_OK) {
                opened_dir = candidate_dirs[dir_index];
                break;
            }
        }

        if (opened_dir == NULL) {
            snprintf(line, sizeof(line), "Open failed: %s", fat32_error_string(result));
            write_line_centered(6, "Unable to open disk folder");
            write_line(8, line);
            write_line_centered(hint_row, "Press any key to continue");
            platform_screen_flush();
            platform_poll_key(NULL, true);
        } else {
            for (;;) {
                memset(&entry, 0, sizeof(entry));
                result = fat32_dir_read(&dir, &entry);
                if (result != FAT32_OK || entry.filename[0] == '\0') {
                    break;
                }
                if (entry.attr & FAT32_ATTR_DIRECTORY) {
                    continue;
                }
                if (!is_allowed_disk_image_name(entry.filename)) {
                    continue;
                }

                total_entries++;
                if (shown_entries < DISK_PICKER_MAX_IMAGES) {
                    strncpy(entries[shown_entries].filename, entry.filename, FAT32_MAX_FILENAME_LEN);
                    entries[shown_entries].filename[FAT32_MAX_FILENAME_LEN] = '\0';
                    shown_entries++;
                }
            }

            fat32_close(&dir);

            if (result != FAT32_OK) {
                read_error = true;
            }

            if (shown_entries == 0) {
                write_line_centered(6, "(No disk images found)");
                if (read_error) {
                    snprintf(line, sizeof(line), "Read error: %s", fat32_error_string(result));
                    write_line(8, line);
                }
                write_line_centered(hint_row, "Press any key to continue");
                platform_screen_flush();
                platform_poll_key(NULL, true);
            } else {
                for (;;) {
                    int i;
                    int list_row;
                    int entry_index;

                    for (row = 2; row < 16; ++row) {
                        clear_line_cells(row);
                    }

                    write_line(2, "UP/DN:move  0:D0  1:D1  Enter:boot");
                    if (total_entries > (unsigned int)shown_entries) {
                        snprintf(line, sizeof(line), "Showing %d of %u valid images",
                                 shown_entries, total_entries);
                    } else {
                        snprintf(line, sizeof(line), "Valid images: %u", total_entries);
                    }
                    write_line(3, line);

                    list_row = list_first_row;
                    for (i = 0; i < max_visible_entries; ++i) {
                        entry_index = top_index + i;
                        if (entry_index >= shown_entries) {
                            break;
                        }
                        snprintf(line, sizeof(line), "%c %s",
                                 (entry_index == selected_index) ? '>' : ' ',
                                 entries[entry_index].filename);
                        write_line(list_row, line);
                        list_row++;
                    }

                    if (disk0_selected != NULL && *disk0_selected) {
                        snprintf(line, sizeof(line), "D0: %s", status_leaf_name(disk0_path));
                    } else {
                        snprintf(line, sizeof(line), "D0: (auto)");
                    }
                    write_line(d0_row, line);

                    if (disk1_selected != NULL && *disk1_selected) {
                        snprintf(line, sizeof(line), "D1: %s", status_leaf_name(disk1_path));
                    } else {
                        snprintf(line, sizeof(line), "D1: (auto)");
                    }
                    write_line(d1_row, line);

                    if (read_error) {
                        snprintf(line, sizeof(line), "Read warning: %s", fat32_error_string(result));
                        write_line(hint_row, line);
                    } else {
                        write_line_centered(hint_row, "Select D0/D1, then press Enter");
                    }

                    platform_screen_flush();
                    platform_poll_key(&keycode, true);

                    if (keycode == PLATFORM_KEY_UP) {
                        if (selected_index > 0) {
                            selected_index--;
                        }
                    } else if (keycode == PLATFORM_KEY_DOWN) {
                        if (selected_index + 1 < shown_entries) {
                            selected_index++;
                        }
                    } else if (keycode == '0') {
                        set_selected_disk_path(disk0_path, disk0_path_size, opened_dir,
                                               entries[selected_index].filename);
                        if (disk0_selected != NULL) {
                            *disk0_selected = true;
                        }
                    } else if (keycode == '1') {
                        set_selected_disk_path(disk1_path, disk1_path_size, opened_dir,
                                               entries[selected_index].filename);
                        if (disk1_selected != NULL) {
                            *disk1_selected = true;
                        }
                    } else if (keycode == PLATFORM_KEY_ENTER || keycode == PLATFORM_KEY_ESC) {
                        break;
                    }

                    if (selected_index < top_index) {
                        top_index = selected_index;
                    }
                    if (selected_index >= top_index + max_visible_entries) {
                        top_index = selected_index - max_visible_entries + 1;
                    }
                }
            }
        }
    }
#else
    write_line_centered(7, "Disk listing requires PICOCALC build");
    write_line_centered(15, "Press any key to continue");
    platform_screen_flush();
    platform_poll_key(NULL, true);
#endif

    for (row = 0; row < 16; ++row) {
        clear_line_cells(row);
    }
    platform_screen_flush();
}

static bool select_model3_rom_path(int argc, char **argv)
{
    static const char *candidate_paths[] = {
        "TRS80/ROMS/model3.rom",
        "TRS80/ROMS/trs80m3.rom",
        "/TRS80/ROMS/model3.rom",
        "/TRS80/ROMS/trs80m3.rom"
    };
    const char *env_path;
    size_t i;

    romfile3[0] = '\0';

    if (argc > 1) {
        strncpy(romfile3, argv[1], FILENAME_MAX - 1);
        romfile3[FILENAME_MAX - 1] = '\0';
        return platform_file_exists(romfile3);
    }

    env_path = getenv("PICOCALC_TRS_ROM");
    if (env_path != NULL && env_path[0] != '\0') {
        strncpy(romfile3, env_path, FILENAME_MAX - 1);
        romfile3[FILENAME_MAX - 1] = '\0';
        return platform_file_exists(romfile3);
    }

    for (i = 0; i < (sizeof(candidate_paths) / sizeof(candidate_paths[0])); ++i) {
        if (platform_file_exists(candidate_paths[i])) {
            strncpy(romfile3, candidate_paths[i], FILENAME_MAX - 1);
            romfile3[FILENAME_MAX - 1] = '\0';
            return true;
        }
    }

    if (platform_embedded_model3_rom_available()) {
        strncpy(romfile3, "embedded:model3.rom", FILENAME_MAX - 1);
        romfile3[FILENAME_MAX - 1] = '\0';
        return true;
    }

    strncpy(romfile3, candidate_paths[0], FILENAME_MAX - 1);
    romfile3[FILENAME_MAX - 1] = '\0';
    return false;
}

static bool select_disk_path(int argc, char **argv, int drive,
                             char *buffer, size_t buffer_size)
{
    static const char *extensions[] = {
        ".dmk",
        ".dsk",
        ".jv3",
        ".jv1"
    };
    static const char *disk_dirs[] = {
        "TRS80/DISKS",
        "/TRS80/DISKS"
    };
    const char *env_name;
    const char *env_path;
    int arg_index;
    char candidate[FILENAME_MAX];
    size_t ext_index;
    size_t dir_index;
    if (buffer == NULL || buffer_size == 0) {
        return false;
    }

    buffer[0] = '\0';

    arg_index = 2 + drive;
    if (argc > arg_index && argv[arg_index][0] != '\0') {
        strncpy(buffer, argv[arg_index], buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
        return true;
    }

    env_name = (drive == 0) ? "PICOCALC_TRS_DISK0" : "PICOCALC_TRS_DISK1";
    env_path = getenv(env_name);
    if (env_path != NULL && env_path[0] != '\0') {
        strncpy(buffer, env_path, buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
        return true;
    }

    for (ext_index = 0; ext_index < (sizeof(extensions) / sizeof(extensions[0])); ++ext_index) {
        for (dir_index = 0; dir_index < (sizeof(disk_dirs) / sizeof(disk_dirs[0])); ++dir_index) {
            snprintf(candidate, sizeof(candidate), "%s/disk%d%s", disk_dirs[dir_index], drive,
                     extensions[ext_index]);
            if (platform_file_exists(candidate)) {
                strncpy(buffer, candidate, buffer_size - 1);
                buffer[buffer_size - 1] = '\0';
                return true;
            }
        snprintf(candidate, sizeof(candidate), "/TRS80/DISKS/disk%d%s", drive, extensions[ext_index]);
        if (platform_file_exists(candidate)) {
            strncpy(buffer, candidate, buffer_size - 1);
            buffer[buffer_size - 1] = '\0';
            return true;
        }
    }

    return false;
}

int main(int argc, char **argv)
{
    char disk0_path[FILENAME_MAX];
    char disk1_path[FILENAME_MAX];
    bool disk0_selected;
    bool disk1_selected;
    bool sd_present;
    bool rom_found;
    bool disk0_found;
    bool disk1_found;

    program_name = "PicoCalcTRS";

    trs_model = 3;
    trs_sdl_init();
    platform_status_clear();
    show_boot_banner();
    show_boot_stage("Initializing firmware...");
    platform_status_puts("Boot: display+input ready");
    disk0_path[0] = '\0';
    disk1_path[0] = '\0';
    disk0_selected = false;
    disk1_selected = false;

    show_boot_stage("Checking SD card...");
    sd_present = platform_sd_card_present();
    status_printf("Boot: SD card %s", sd_present ? "present" : "absent");

    if (!sd_present) {
        show_boot_stage("No SD card: embedded boot");
        disk0_found = false;
        disk1_found = false;
        if (platform_embedded_model3_rom_available()) {
            strncpy(romfile3, "embedded:model3.rom", FILENAME_MAX - 1);
            romfile3[FILENAME_MAX - 1] = '\0';
            rom_found = true;
            platform_status_puts("Boot: using embedded ROM");
        } else {
            romfile3[0] = '\0';
            rom_found = false;
            platform_status_puts("Boot: embedded ROM missing");
        }
    } else {
        show_boot_stage("Listing disk folder...");
        show_disk_directory_screen_and_select(
            disk0_path, sizeof(disk0_path), &disk0_selected,
            disk1_path, sizeof(disk1_path), &disk1_selected
        );
        show_boot_banner();
        show_boot_stage("Probing ROM...");
        platform_status_puts("Boot: probing ROM");
        rom_found = select_model3_rom_path(argc, argv);
        if (rom_found) {
            status_printf("Boot: ROM %s", status_leaf_name(romfile3));
        } else {
            platform_status_puts("Boot: ROM not found");
        }

        show_boot_stage("Probing disks...");
        if (disk0_selected || disk1_selected) {
            disk0_found = disk0_selected;
            disk1_found = disk1_selected;
        } else {
            disk0_found = select_disk_path(argc, argv, 0, disk0_path, sizeof(disk0_path));
            disk1_found = select_disk_path(argc, argv, 1, disk1_path, sizeof(disk1_path));
        }
        status_printf("Boot: D0:%c D1:%c", disk0_found ? 'Y' : 'N', disk1_found ? 'Y' : 'N');

    }

    show_boot_stage("Startup OSD...");
    osd_startup_menu(disk0_path, &disk0_found, disk1_path, &disk1_found, 0);
    status_printf("Boot: OSD D0:%c D1:%c", disk0_found ? 'Y' : 'N', disk1_found ? 'Y' : 'N');

    if (!rom_found) {
        show_missing_rom_screen();
        for (;;) {
            platform_poll_key(NULL, true);
        }
    }

    show_boot_stage("Attaching disk images...");
    trs_disk_controller = (disk0_found || disk1_found) ? 1 : 0;
    if (disk0_found) {
        status_printf("Boot: mount D0 %s", status_leaf_name(disk0_path));
        trs_disk_insert(0, disk0_path);
    }
    if (disk1_found) {
        status_printf("Boot: mount D1 %s", status_leaf_name(disk1_path));
        trs_disk_insert(1, disk1_path);
    }

    show_boot_stage("Resetting Model III...");
    platform_status_puts("Boot: resetting CPU/FDC");
    trs_reset(1);
    picocalc_apply_post_reset_policy();
    show_boot_stage("Starting ROM...");
    show_runtime_status(disk0_found, disk0_path, disk1_found, disk1_path);
    trs_screen_caption();
    z80_run(1);

    return 0;
}
