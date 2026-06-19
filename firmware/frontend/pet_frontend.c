#include "pet_frontend.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "platform/platform.h"

static uint8_t pet_screen_code_to_ascii(uint8_t code)
{
    code &= 0x7F;

    static const uint8_t low_symbols[32] = {
        '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
        'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
        'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
        'X', 'Y', 'Z', '[', '\\', ']', '^', '<'
    };

    static const uint8_t high_symbols[32] = {
        '-', '|', '-', '|', '+', '+', '+', '+',
        '+', '+', '+', '+', '+', '+', '+', '+',
        '*', '*', '*', '*', '*', '*', '*', '*',
        '*', '*', '*', '*', '*', '*', '*', '*'
    };

    if (code < 0x20) {
        return low_symbols[code];
    }
    if (code >= 1 && code <= 26) {
        return (uint8_t)('A' + code - 1);
    }
    if (code >= 0x20 && code <= 0x5F) {
        return code;
    }
    if (code >= 0x60) {
        return high_symbols[code - 0x60];
    }
    return '.';
}

static const uint8_t *pet_frontend_character_glyph(const pet2001_t *pet, uint8_t code)
{
    size_t offset = (size_t)(code & 0x7F) * 8u;

    if (pet == NULL || offset + 7u >= sizeof(pet->character_rom)) {
        return NULL;
    }
    return &pet->character_rom[offset];
}

static void clear_row(int row)
{
    int col;

    for (col = 0; col < PET_FRONTEND_COLS; ++col) {
        platform_screen_write_cell(col, row, ' ', 0);
    }
}

static void pet_frontend_function_help(void)
{
    pet_frontend_status_line(0, "F1 DBG  F2 KBD  F3 PRG  F4 SAVE  F5 D64");
}

static const char *pet_frontend_leaf_name(const char *path)
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
    return leaf[0] != '\0' ? leaf : path;
}

static void pet_frontend_disk_status_line(const pet2001_t *pet)
{
    char line[PET_FRONTEND_COLS + 1];
    const char *d0 = "none";
    const char *d1 = "none";

    if (pet != NULL) {
        if (pet->cbm_disk_mounted[0]) {
            d0 = pet_frontend_leaf_name(pet->cbm_disk_path[0]);
        }
        if (pet->cbm_disk_mounted[1]) {
            d1 = pet_frontend_leaf_name(pet->cbm_disk_path[1]);
        }
    }

    snprintf(line, sizeof(line), "D0:%.16s D1:%.16s", d0, d1);
    pet_frontend_status_line(1, line);
}

void pet_frontend_init(void)
{
    platform_screen_configure(PET_FRONTEND_COLS, PET_FRONTEND_ROWS);
    pet_frontend_clear();
}

void pet_frontend_clear(void)
{
    int row;

    for (row = 0; row < PET_FRONTEND_ROWS; ++row) {
        clear_row(row);
    }
}

void pet_frontend_write_line(int row, const char *text)
{
    int col;

    if (row < 0 || row >= PET_FRONTEND_ROWS || text == NULL) {
        return;
    }

    clear_row(row);
    for (col = 0; col < PET_FRONTEND_COLS && text[col] != '\0'; ++col) {
        platform_screen_write_cell(col, row, (uint8_t)text[col], 0);
    }
}

void pet_frontend_write_centered(int row, const char *text)
{
    size_t len;
    int col;

    if (row < 0 || row >= PET_FRONTEND_ROWS || text == NULL) {
        return;
    }

    len = strlen(text);
    if (len > PET_FRONTEND_COLS) {
        len = PET_FRONTEND_COLS;
    }

    col = (PET_FRONTEND_COLS - (int)len) / 2;
    if (col < 0) {
        col = 0;
    }

    clear_row(row);
    for (size_t i = 0; i < len; ++i) {
        platform_screen_write_cell(col + (int)i, row, (uint8_t)text[i], 0);
    }
}

void pet_frontend_status_line(int line, const char *text)
{
    if (line < 0 || line >= PET_FRONTEND_STATUS_LINES) {
        return;
    }

    platform_status_write_line(line, text);
}

void pet_frontend_flush(void)
{
    platform_screen_flush();
}

void pet_frontend_show_boot_banner(void)
{
    pet_frontend_clear();
    pet_frontend_write_centered(1, "PicoCalc PET 2001");
    pet_frontend_write_centered(3, "Commodore PET emulator");
    pet_frontend_function_help();
    pet_frontend_disk_status_line(NULL);
    pet_frontend_status_line(2, "PET2001 1.00MHz ROM:probe RAM:32K");
    pet_frontend_status_line(3, "KBD:ready TAPE:none PRG:none");
    pet_frontend_flush();
}

void pet_frontend_show_boot_stage(const char *text)
{
    pet_frontend_write_centered(5, text);
    pet_frontend_flush();
}

void pet_frontend_show_stub_screen(const pet2001_t *pet)
{
    pet_frontend_clear();
    pet_frontend_write_centered(0, "*** COMMODORE BASIC 1 ***");
    pet_frontend_write_centered(2, "31743 BYTES FREE");
    pet_frontend_write_line(4, "READY.");
    pet_frontend_write_line(5, "_");

    pet_frontend_refresh_status(pet, "ROM running; no video yet");
}

void pet_frontend_render_video(pet2001_t *pet)
{
    int row;
    int col;

    if (pet == NULL) {
        return;
    }

    for (row = 0; row < PET_FRONTEND_ROWS; ++row) {
        for (col = 0; col < PET_FRONTEND_COLS; ++col) {
            size_t offset = (size_t)row * PET_FRONTEND_COLS + (size_t)col;
            uint8_t code;
            uint8_t mode;

            if (offset >= sizeof(pet->video) || !pet->video_dirty[offset]) {
                continue;
            }

            code = pet->video[offset];
            mode = (code & 0x80) ? PLATFORM_CELL_CURSOR : 0;
            platform_screen_write_glyph8(col, row,
                                         pet_frontend_character_glyph(pet, code),
                                         mode);
            pet->video_dirty[offset] = false;
        }
    }
}

void pet_frontend_refresh_status(const pet2001_t *pet, const char *message)
{
    char line[PET_FRONTEND_COLS + 1];
    const char *detail = message;

    pet_frontend_function_help();

    snprintf(line, sizeof(line), "PET2001 ROM:%s RAM:32K",
             pet != NULL && pet->roms_loaded ? "running" : "probe");
    pet_frontend_disk_status_line(pet);
    pet_frontend_status_line(2, line);

    if (pet != NULL && pet->last_error[0] != '\0' &&
        strncmp(pet->last_error, "D64", 3) == 0) {
        detail = pet->last_error;
    }

    pet_frontend_status_line(3, detail != NULL ? detail : "KBD:ready TAPE:none");
}

void pet_frontend_refresh_debug_status(const pet2001_t *pet)
{
    char line[PET_FRONTEND_COLS + 1];

    pet_frontend_function_help();

    snprintf(line, sizeof(line), "PC:%04X L:%lu D:%lu ST:%02X",
             pet != NULL ? pet->pc : 0,
             pet != NULL ? (unsigned long)pet->cbm_load_attempts : 0UL,
             pet != NULL ? (unsigned long)pet->cbm_directory_loads : 0UL,
             pet != NULL ? (unsigned int)pet->ram[0x0096] : 0U);
    pet_frontend_disk_status_line(pet);
    pet_frontend_status_line(2, line);

    snprintf(line, sizeof(line), "D1:%02X D3:%02X D4:%02X B:%02X%02X E:%02X%02X",
             pet != NULL ? (unsigned int)pet->ram[0x00D1] : 0U,
             pet != NULL ? (unsigned int)pet->ram[0x00D3] : 0U,
             pet != NULL ? (unsigned int)pet->ram[0x00D4] : 0U,
             pet != NULL ? (unsigned int)pet->ram[0x0029] : 0U,
             pet != NULL ? (unsigned int)pet->ram[0x0028] : 0U,
             pet != NULL ? (unsigned int)pet->ram[0x002B] : 0U,
             pet != NULL ? (unsigned int)pet->ram[0x002A] : 0U);
    pet_frontend_status_line(3, line);
}
