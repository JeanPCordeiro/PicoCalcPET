#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "trs.h"
#include "trs_disk.h"
#include "z80.h"
#include "platform/platform.h"

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

static void status_printf(const char *format, ...)
{
    char buffer[65];
    va_list args;

    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    platform_status_puts(buffer);
}

static void show_missing_rom_screen(void)
{
    char detail[65];
    char detect[65];

    platform_screen_configure(64, 16);
    write_line_centered(2, "TRS-80 Model III ROM Missing");
    write_line_centered(5, "Put the ROM at:");
    write_line_centered(7, "/roms/model3.rom");
    write_line_centered(10, "Also accepted:");
    write_line_centered(11, "roms/trs80m3.rom");
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

static void select_model3_rom_path(int argc, char **argv)
{
    static const char *candidate_paths[] = {
        "roms/model3.rom",
        "roms/trs80m3.rom",
        "sdcard/roms/model3.rom",
        "/roms/model3.rom"
    };
    const char *env_path;
    size_t i;

    romfile3[0] = '\0';

    if (argc > 1) {
        strncpy(romfile3, argv[1], FILENAME_MAX - 1);
        romfile3[FILENAME_MAX - 1] = '\0';
        return;
    }

    env_path = getenv("PICOCALC_TRS_ROM");
    if (env_path != NULL && env_path[0] != '\0') {
        strncpy(romfile3, env_path, FILENAME_MAX - 1);
        romfile3[FILENAME_MAX - 1] = '\0';
        return;
    }

    for (i = 0; i < (sizeof(candidate_paths) / sizeof(candidate_paths[0])); ++i) {
        if (platform_file_exists(candidate_paths[i])) {
            strncpy(romfile3, candidate_paths[i], FILENAME_MAX - 1);
            romfile3[FILENAME_MAX - 1] = '\0';
            return;
        }
    }

    strncpy(romfile3, candidate_paths[0], FILENAME_MAX - 1);
    romfile3[FILENAME_MAX - 1] = '\0';
}

static bool select_disk0_path(int argc, char **argv, char *buffer, size_t buffer_size)
{
    static const char *candidate_paths[] = {
        "disks/disk0.dmk",
        "disks/disk0.jv3",
        "disks/disk0.jv1",
        "/disks/disk0.dmk",
        "/disks/disk0.jv3",
        "/disks/disk0.jv1"
    };
    const char *env_path;
    size_t i;

    if (buffer == NULL || buffer_size == 0) {
        return false;
    }

    buffer[0] = '\0';

    if (argc > 2 && argv[2][0] != '\0') {
        strncpy(buffer, argv[2], buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
        return true;
    }

    env_path = getenv("PICOCALC_TRS_DISK0");
    if (env_path != NULL && env_path[0] != '\0') {
        strncpy(buffer, env_path, buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
        return true;
    }

    for (i = 0; i < (sizeof(candidate_paths) / sizeof(candidate_paths[0])); ++i) {
        if (platform_file_exists(candidate_paths[i])) {
            strncpy(buffer, candidate_paths[i], buffer_size - 1);
            buffer[buffer_size - 1] = '\0';
            return true;
        }
    }

    return false;
}

int main(int argc, char **argv)
{
    char disk0_path[FILENAME_MAX];
    bool rom_found;
    bool disk_found;

    program_name = "picocalc_trs_scaffold";

    trs_model = 3;
    trs_sdl_init();
    platform_status_clear();
    select_model3_rom_path(argc, argv);
    disk_found = select_disk0_path(argc, argv, disk0_path, sizeof(disk0_path));
    rom_found = platform_file_exists(romfile3);

    platform_status_puts("Initializing PicoCalc TRS scaffold");
    platform_status_puts("Target machine: TRS-80 Model III");
    status_printf("ROM path: %s", romfile3);
    if (!rom_found) {
        platform_status_puts("ROM file not found");
        show_missing_rom_screen();
        for (;;) {
            platform_poll_key(NULL, true);
        }
    }

    if (disk_found) {
        trs_disk_insert(0, disk0_path);
        status_printf("Disk 0: %s (%s)",
                      disk0_path,
                      trs_disk_getwriteprotect(0) ? "ro" : "rw");
    } else {
        platform_status_puts("Disk 0: none");
    }

    trs_reset(1);
    trs_screen_caption();
    platform_status_puts("Model III reset path completed");
    platform_status_puts("Starting emulator run loop");
    z80_run(1);

    return 0;
}
