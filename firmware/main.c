#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "trs.h"
#include "trs_disk.h"
#include "z80.h"
#include "platform/platform.h"
#include "platform/platform_file.h"
#include "emu/picocalc_reset_policy.h"

extern const char *program_name;

#define PICOCALC_TRS_ROM_DIR "/TRS80/ROMS"
#define PICOCALC_TRS_DISK_DIR "/TRS80/DISKS"
#define PICOCALC_DISK_PICKER_MAX_IMAGES 64
#define PICOCALC_DISK_PICKER_VISIBLE_ROWS 8

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

static void clear_screen_cells(void)
{
    int row;

    for (row = 0; row < 16; ++row) {
        clear_line_cells(row);
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

static void show_runtime_status(bool disk0_found, const char *disk0_path,
                                bool disk1_found, const char *disk1_path)
{
    const char *rom_label;

    rom_label = (strncmp(romfile3, "embedded:", 9) == 0) ? "EMB" : "SD";

    platform_status_clear();
    platform_status_set_operator_context(rom_label, trs_rom_size,
                                         disk0_path, disk0_found,
                                         disk0_found && trs_disk_getwriteprotect(0),
                                         disk1_path, disk1_found,
                                         disk1_found && trs_disk_getwriteprotect(1));
    platform_status_refresh_operator((uint16_t)Z80_PC, z80_state.clockMHz, trs_sound != 0);
}

static void show_missing_rom_screen(void)
{
    char detail[65];
    char detect[65];
    bool sd_present;

    sd_present = platform_sd_card_present();
    platform_screen_configure(64, 16);
    write_line_centered(1, "TRS-80 Model III ROM Missing");
    if (sd_present) {
        write_line_centered(4, "Copy one ROM file to SD:");
        write_line_centered(6, PICOCALC_TRS_ROM_DIR "/model3.rom");
        write_line_centered(8, "Also accepted:");
        write_line_centered(9, PICOCALC_TRS_ROM_DIR "/trs80m3.rom");
    } else {
        write_line_centered(4, "Insert an SD card with:");
        write_line_centered(6, PICOCALC_TRS_ROM_DIR "/model3.rom");
        write_line_centered(8, "or rebuild with embedded ROM support.");
    }
    snprintf(detect, sizeof(detect), "SD detect: gpio=%d present=%d",
             platform_sd_detect_state(),
             sd_present ? 1 : 0);
    snprintf(detail, sizeof(detail), "Last probe: %s (%d)",
             platform_last_file_error(),
             platform_last_file_error_code());
    write_line(12, detect);
    write_line(13, detail);
    write_line_centered(15, "Power cycle after fixing ROM.");
    platform_screen_flush();
}

static bool select_model3_rom_path(int argc, char **argv)
{
    static const char *candidate_paths[] = {
        PICOCALC_TRS_ROM_DIR "/model3.rom",
        PICOCALC_TRS_ROM_DIR "/trs80m3.rom"
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
    const char *env_name;
    const char *env_path;
    int arg_index;

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

    return false;
}

static void build_disk_image_path(const char *name, char *buffer, size_t buffer_size)
{
    if (buffer == NULL || buffer_size == 0) {
        return;
    }

    if (name == NULL || name[0] == '\0') {
        buffer[0] = '\0';
        return;
    }

    snprintf(buffer, buffer_size, PICOCALC_TRS_DISK_DIR "/%s", name);
}

static void draw_disk_picker(int drive, const platform_disk_image_t *images,
                             int image_count, int selected, int top,
                             int list_error)
{
    char line[65];
    int row;
    int index;

    clear_screen_cells();
    snprintf(line, sizeof(line), "D%d Disk Image", drive);
    write_line_centered(1, line);
    write_line_centered(2, PICOCALC_TRS_DISK_DIR);
    write_line_centered(3, "Sorted .DSK .DMK .JV1 .JV3");

    if (list_error) {
        snprintf(line, sizeof(line), "Directory unavailable: %.40s",
                 platform_last_file_error());
        write_line(4, line);
    } else if (image_count == 0) {
        write_line_centered(4, "No disk images found; Enter selects none");
    } else {
        snprintf(line, sizeof(line), "%d image%s available",
                 image_count, image_count == 1 ? "" : "s");
        write_line_centered(4, line);
    }

    for (row = 0; row < PICOCALC_DISK_PICKER_VISIBLE_ROWS; ++row) {
        index = top + row;
        if (index > image_count) {
            break;
        }

        if (index == 0) {
            snprintf(line, sizeof(line), "%c [none] leave D%d empty",
                     selected == index ? '>' : ' ', drive);
        } else {
            snprintf(line, sizeof(line), "%c %-3d %s",
                     selected == index ? '>' : ' ',
                     index,
                     images[index - 1].name);
        }
        write_line(5 + row, line);
    }

    write_line_centered(14, "Up/Down choose  Enter attach  N none  Esc skip");
    write_line_centered(15, "Home/End and PgUp/PgDn move faster");
    platform_screen_flush();
}

static bool run_disk_picker_for_drive(int drive, const platform_disk_image_t *images,
                                      int image_count, int list_error,
                                      char *buffer, size_t buffer_size)
{
    int selected;
    int top = 0;
    int key;
    int max_selection = image_count;

    if (buffer == NULL || buffer_size == 0) {
        return false;
    }

    selected = (image_count > 0) ? 1 : 0;

    for (;;) {
        if (selected < top) {
            top = selected;
        } else if (selected >= top + PICOCALC_DISK_PICKER_VISIBLE_ROWS) {
            top = selected - PICOCALC_DISK_PICKER_VISIBLE_ROWS + 1;
        }

        draw_disk_picker(drive, images, image_count, selected, top, list_error);
        if (!platform_poll_key(&key, true)) {
            continue;
        }

        switch (key) {
        case PLATFORM_KEY_UP:
            if (selected > 0) {
                selected--;
            }
            break;
        case PLATFORM_KEY_DOWN:
            if (selected < max_selection) {
                selected++;
            }
            break;
        case PLATFORM_KEY_PAGE_UP:
            selected -= PICOCALC_DISK_PICKER_VISIBLE_ROWS;
            if (selected < 0) {
                selected = 0;
            }
            break;
        case PLATFORM_KEY_PAGE_DOWN:
            selected += PICOCALC_DISK_PICKER_VISIBLE_ROWS;
            if (selected > max_selection) {
                selected = max_selection;
            }
            break;
        case PLATFORM_KEY_HOME:
            selected = 0;
            break;
        case PLATFORM_KEY_END:
            selected = max_selection;
            break;
        case PLATFORM_KEY_ENTER:
            if (selected == 0) {
                buffer[0] = '\0';
                return false;
            }
            build_disk_image_path(images[selected - 1].name, buffer, buffer_size);
            return true;
        case 'n':
        case 'N':
        case '0':
        case PLATFORM_KEY_BACKSPACE:
        case PLATFORM_KEY_ESC:
            buffer[0] = '\0';
            return false;
        default:
            break;
        }
    }
}

static void run_disk_picker(char *disk0_path, size_t disk0_path_size,
                            char *disk1_path, size_t disk1_path_size,
                            bool *disk0_found, bool *disk1_found)
{
    platform_disk_image_t images[PICOCALC_DISK_PICKER_MAX_IMAGES];
    int image_count;
    int list_error = 0;

    show_boot_stage("Reading disk directory...");
    image_count = platform_list_disk_images(PICOCALC_TRS_DISK_DIR, images,
                                            PICOCALC_DISK_PICKER_MAX_IMAGES);
    if (image_count < 0) {
        image_count = 0;
        list_error = 1;
    }

    *disk0_found = run_disk_picker_for_drive(0, images, image_count, list_error,
                                             disk0_path, disk0_path_size);
    if (*disk0_found) {
        status_printf("Boot: D0 selected %s", status_leaf_name(disk0_path));
    } else {
        platform_status_puts("Boot: D0 none");
    }
    *disk1_found = run_disk_picker_for_drive(1, images, image_count, list_error,
                                             disk1_path, disk1_path_size);
    if (*disk1_found) {
        status_printf("Boot: D1 selected %s", status_leaf_name(disk1_path));
    } else {
        platform_status_puts("Boot: D1 none");
    }
}

int main(int argc, char **argv)
{
    char disk0_path[FILENAME_MAX];
    char disk1_path[FILENAME_MAX];
    bool sd_present;
    bool rom_found;
    bool disk0_found;
    bool disk1_found;
    bool disk_args_used = false;

    program_name = "PicoCalcTRS";

    trs_model = 3;
    trs_sdl_init();
    platform_status_clear();
    show_boot_banner();
    show_boot_stage("Initializing firmware...");
    platform_status_puts("Boot: display+input ready");
    disk0_path[0] = '\0';
    disk1_path[0] = '\0';

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
        show_boot_stage("Probing ROM...");
        platform_status_puts("Boot: probing ROM");
        rom_found = select_model3_rom_path(argc, argv);
        if (rom_found) {
            status_printf("Boot: ROM %s", status_leaf_name(romfile3));
        } else {
            platform_status_puts("Boot: ROM not found");
        }

        show_boot_stage("Probing disks...");
        if (argc > 2 || getenv("PICOCALC_TRS_DISK0") != NULL ||
            getenv("PICOCALC_TRS_DISK1") != NULL) {
            disk0_found = select_disk_path(argc, argv, 0, disk0_path, sizeof(disk0_path));
            disk1_found = select_disk_path(argc, argv, 1, disk1_path, sizeof(disk1_path));
            disk_args_used = true;
        } else {
            run_disk_picker(disk0_path, sizeof(disk0_path),
                            disk1_path, sizeof(disk1_path),
                            &disk0_found, &disk1_found);
        }
        if (disk_args_used) {
            platform_status_puts("Boot: disks from args/env");
        }
        status_printf("Boot: D0:%c D1:%c", disk0_found ? 'Y' : 'N', disk1_found ? 'Y' : 'N');
    }
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
