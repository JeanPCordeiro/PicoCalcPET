#include "pet2001.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emu/vice_6502_cpu.h"
#include "platform/platform.h"
#include "platform/platform_file.h"

enum {
    PIA_PORT_A = 0,
    PIA_CTRL_A = 1,
    PIA_PORT_B = 2,
    PIA_CTRL_B = 3
};

enum {
    PET_KEY_LATCH_FRAMES = 8,
    PET_PRG_GAME_KEY_LATCH_FRAMES = 30,
    PET_KEY_LEFT_SHIFT_COL = 0,
    PET_KEY_RIGHT_SHIFT_COL = 5
};

enum {
    PET_KERNAL_SETLFS = 0xFFBA,
    PET_KERNAL_SETNAM = 0xFFBD,
    PET_KERNAL_LOAD = 0xFFD5,
    PET_KERNAL_SAVE = 0xFFD8,
    PET_BASIC4_LOAD_DEVICE = 0xF356,
    PET_BASIC4_FILE_OPEN = 0xF4A5,
    PET_BASIC4_SAVE_DEVICE = 0xF6FD,
    PET_CBM_DEVICE = 8,
    PET_CBM_DISK_TRACKS = 35,
    PET_CBM_DISK_SIZE = 174848
};

#define PET_CBM_D64_PATH "/PET2001/DISK/pet.d64"

typedef struct {
    int key;
    uint8_t row;
    uint8_t col;
    bool shift;
} pet_key_map_t;

static const pet_key_map_t pet_graphics_key_map[] = {
    { 'a', 4, 0, false }, { 'b', 6, 2, false }, { 'c', 6, 1, false },
    { 'd', 4, 1, false }, { 'e', 2, 1, false }, { 'f', 5, 1, false },
    { 'g', 4, 2, false }, { 'h', 5, 2, false }, { 'i', 3, 3, false },
    { 'j', 4, 3, false }, { 'k', 5, 3, false }, { 'l', 4, 4, false },
    { 'm', 6, 3, false }, { 'n', 7, 2, false }, { 'o', 2, 4, false },
    { 'p', 3, 4, false }, { 'q', 2, 0, false }, { 'r', 3, 1, false },
    { 's', 5, 0, false }, { 't', 2, 2, false }, { 'u', 2, 3, false },
    { 'v', 7, 1, false }, { 'w', 3, 0, false }, { 'x', 7, 0, false },
    { 'y', 3, 2, false }, { 'z', 6, 0, false },
    { 'A', 4, 0, true }, { 'B', 6, 2, true }, { 'C', 6, 1, true },
    { 'D', 4, 1, true }, { 'E', 2, 1, true }, { 'F', 5, 1, true },
    { 'G', 4, 2, true }, { 'H', 5, 2, true }, { 'I', 3, 3, true },
    { 'J', 4, 3, true }, { 'K', 5, 3, true }, { 'L', 4, 4, true },
    { 'M', 6, 3, true }, { 'N', 7, 2, true }, { 'O', 2, 4, true },
    { 'P', 3, 4, true }, { 'Q', 2, 0, true }, { 'R', 3, 1, true },
    { 'S', 5, 0, true }, { 'T', 2, 2, true }, { 'U', 2, 3, true },
    { 'V', 7, 1, true }, { 'W', 3, 0, true }, { 'X', 7, 0, true },
    { 'Y', 3, 2, true }, { 'Z', 6, 0, true },
    { '0', 8, 6, false }, { '1', 6, 6, false }, { '2', 7, 6, false },
    { '3', 6, 7, false }, { '4', 4, 6, false }, { '5', 5, 6, false },
    { '6', 4, 7, false }, { '7', 2, 6, false }, { '8', 3, 6, false },
    { '9', 2, 7, false },
    { '!', 0, 0, false }, { '"', 1, 0, false }, { '#', 0, 1, false },
    { '$', 1, 1, false }, { '%', 0, 2, false }, { '&', 0, 3, false },
    { '\'', 1, 2, false }, { '(', 0, 4, false }, { ')', 1, 4, false },
    { '*', 5, 7, false }, { '+', 7, 7, false }, { ',', 7, 3, false },
    { '-', 8, 7, false }, { '.', 9, 6, false }, { '/', 3, 7, false },
    { ':', 5, 4, false }, { ';', 6, 4, false }, { '<', 9, 3, false },
    { '=', 9, 7, false }, { '>', 8, 4, false }, { '?', 7, 4, false },
    { '@', 8, 1, false }, { '[', 9, 1, false }, { '\\', 1, 3, false },
    { ']', 8, 2, false }, { '^', 2, 5, false }, { ' ', 9, 2, false },
    { PLATFORM_KEY_ENTER, 6, 5, false },
    { PLATFORM_KEY_BACKSPACE, 1, 7, false },
    { PLATFORM_KEY_ESC, 9, 4, false },
    { PLATFORM_KEY_TAB, 9, 0, false },
    { PLATFORM_KEY_HOME, 0, 6, false },
    { PLATFORM_KEY_UP, 1, 6, true },
    { PLATFORM_KEY_DOWN, 1, 6, false },
    { PLATFORM_KEY_LEFT, 0, 7, true },
    { PLATFORM_KEY_RIGHT, 0, 7, false },
    { PLATFORM_KEY_END, 0, 5, false },
    { PLATFORM_KEY_PAGE_DOWN, 2, 5, false }
};

static const pet_key_map_t pet_business_key_map[] = {
    { 'a', 3, 0, false }, { 'b', 6, 2, false }, { 'c', 6, 1, false },
    { 'd', 3, 1, false }, { 'e', 5, 1, false }, { 'f', 2, 2, false },
    { 'g', 3, 2, false }, { 'h', 2, 3, false }, { 'i', 4, 5, false },
    { 'j', 3, 3, false }, { 'k', 2, 5, false }, { 'l', 3, 5, false },
    { 'm', 8, 3, false }, { 'n', 7, 2, false }, { 'o', 5, 5, false },
    { 'p', 4, 6, false }, { 'q', 5, 0, false }, { 'r', 4, 2, false },
    { 's', 2, 1, false }, { 't', 5, 2, false }, { 'u', 5, 3, false },
    { 'v', 7, 1, false }, { 'w', 4, 1, false }, { 'x', 8, 1, false },
    { 'y', 4, 3, false }, { 'z', 7, 0, false },
    { 'A', 3, 0, true }, { 'B', 6, 2, true }, { 'C', 6, 1, true },
    { 'D', 3, 1, true }, { 'E', 5, 1, true }, { 'F', 2, 2, true },
    { 'G', 3, 2, true }, { 'H', 2, 3, true }, { 'I', 4, 5, true },
    { 'J', 3, 3, true }, { 'K', 2, 5, true }, { 'L', 3, 5, true },
    { 'M', 8, 3, true }, { 'N', 7, 2, true }, { 'O', 5, 5, true },
    { 'P', 4, 6, true }, { 'Q', 5, 0, true }, { 'R', 4, 2, true },
    { 'S', 2, 1, true }, { 'T', 5, 2, true }, { 'U', 5, 3, true },
    { 'V', 7, 1, true }, { 'W', 4, 1, true }, { 'X', 8, 1, true },
    { 'Y', 4, 3, true }, { 'Z', 7, 0, true },
    { '0', 1, 3, false }, { '1', 1, 0, false }, { '2', 0, 0, false },
    { '3', 9, 1, false }, { '4', 1, 1, false }, { '5', 0, 1, false },
    { '6', 9, 2, false }, { '7', 1, 2, false }, { '8', 0, 2, false },
    { '9', 9, 3, false },
    { '!', 1, 0, true }, { '"', 0, 0, true }, { '#', 9, 1, true },
    { '$', 1, 1, true }, { '%', 0, 1, true }, { '&', 9, 2, true },
    { '\'', 1, 2, true }, { '(', 0, 2, true }, { ')', 9, 3, true },
    { '*', 9, 5, true }, { '+', 2, 6, true }, { ',', 7, 3, false },
    { '-', 0, 3, false }, { '.', 6, 3, false }, { '/', 8, 6, false },
    { ':', 9, 5, false }, { ';', 2, 6, false }, { '<', 7, 3, true },
    { '=', 0, 3, true }, { '>', 6, 3, true }, { '?', 8, 6, true },
    { '@', 3, 6, false }, { '[', 5, 6, false }, { '\\', 4, 4, false },
    { ']', 2, 4, false }, { '^', 1, 5, false }, { ' ', 8, 2, false },
    { PLATFORM_KEY_ENTER, 3, 4, false },
    { PLATFORM_KEY_BACKSPACE, 4, 7, false },
    { PLATFORM_KEY_ESC, 9, 4, false },
    { PLATFORM_KEY_TAB, 4, 0, false },
    { PLATFORM_KEY_HOME, 8, 4, false },
    { PLATFORM_KEY_DOWN, 5, 4, false },
    { PLATFORM_KEY_RIGHT, 0, 5, false }
};

enum {
    VIA_ORB = 0,
    VIA_ORA = 1,
    VIA_DDRB = 2,
    VIA_DDRA = 3,
    VIA_T1CL = 4,
    VIA_T1CH = 5,
    VIA_T1LL = 6,
    VIA_T1LH = 7,
    VIA_T2CL = 8,
    VIA_T2CH = 9,
    VIA_SR = 10,
    VIA_ACR = 11,
    VIA_PCR = 12,
    VIA_IFR = 13,
    VIA_IER = 14,
    VIA_ORA_NO_HANDSHAKE = 15
};

static bool read_prefix_range(const char *path, uint8_t *buffer, size_t buffer_size,
                              size_t minimum_size, size_t *bytes_read);
static bool load_rom_part(pet2001_t *pet, const char *label, const char *path,
                          uint8_t *buffer, size_t buffer_size,
                          size_t minimum_size, size_t *bytes_read);
static void pet2001_set_matrix_key(pet2001_t *pet, uint8_t row, uint8_t col,
                                   bool pressed, uint8_t latch_frames);
static uint16_t pet2001_load_word(const pet2001_t *pet, uint16_t address);
static bool pet2001_cbm_capture_rom_file_state(pet2001_t *pet);
static bool pet2001_cbm_load(pet2001_t *pet, uint16_t load_address, bool use_header);
static bool pet2001_cbm_save(pet2001_t *pet, uint16_t start, uint16_t end);

static bool read_exact_prefix(const char *path, uint8_t *buffer, size_t buffer_size,
                              size_t required_size)
{
    size_t bytes_read = 0;

    return read_prefix_range(path, buffer, buffer_size, required_size, &bytes_read);
}

static bool read_prefix_range(const char *path, uint8_t *buffer, size_t buffer_size,
                              size_t minimum_size, size_t *bytes_read)
{
    platform_file_t *file;
    size_t offset = 0;

    if (path == NULL || path[0] == '\0' || buffer == NULL ||
        minimum_size > buffer_size) {
        return false;
    }

    file = platform_fopen(path, "rb");
    if (file == NULL) {
        return false;
    }

    memset(buffer, 0xFF, buffer_size);
    while (offset < buffer_size) {
        int ch = platform_getc(file);
        if (ch == EOF) {
            break;
        }
        buffer[offset++] = (uint8_t)ch;
    }

    platform_fclose(file);
    if (bytes_read != NULL) {
        *bytes_read = offset;
    }
    return offset >= minimum_size;
}

static bool load_rom_part(pet2001_t *pet, const char *label, const char *path,
                          uint8_t *buffer, size_t buffer_size,
                          size_t minimum_size, size_t *bytes_read)
{
    size_t actual = 0;
    bool loaded = read_prefix_range(path, buffer, buffer_size, minimum_size, &actual);

    if (bytes_read != NULL) {
        *bytes_read = actual;
    }
    if (!loaded && pet != NULL) {
        snprintf(pet->last_error, sizeof(pet->last_error),
                 "%s %lu/%lu bytes",
                 label,
                 (unsigned long)actual,
                 (unsigned long)minimum_size);
    }
    return loaded;
}

static bool read_combined_rom(pet2001_t *pet, const char *path)
{
    platform_file_t *file;
    size_t offset = 0;
    uint8_t combined[0x6000];

    file = platform_fopen(path, "rb");
    if (file == NULL) {
        return false;
    }

    memset(combined, 0xFF, sizeof(combined));
    while (offset < sizeof(combined)) {
        int ch = platform_getc(file);
        if (ch == EOF) {
            break;
        }
        combined[offset++] = (uint8_t)ch;
    }
    platform_fclose(file);

    if (offset < 0x3000) {
        return false;
    }

    if (offset >= 0x5000) {
        memcpy(pet->basic_rom, combined, 0x3000);
        memcpy(pet->editor_rom, combined + 0x3000, sizeof(pet->editor_rom));
        memcpy(pet->kernal_rom, combined + 0x4000, sizeof(pet->kernal_rom));
        pet->basic_rom_base = 0xB000;
        pet->basic_rom_size = 0x3000;
        if (offset >= 0x5800) {
            memcpy(pet->character_rom, combined + 0x5000, 0x0800);
        }
    } else {
        memcpy(pet->basic_rom, combined, 0x2000);
        memcpy(pet->editor_rom, combined + 0x2000, sizeof(pet->editor_rom));
        memcpy(pet->kernal_rom, combined + 0x3000, sizeof(pet->kernal_rom));
        pet->basic_rom_base = 0xC000;
        pet->basic_rom_size = 0x2000;
        if (offset >= 0x4800) {
            memcpy(pet->character_rom, combined + 0x4000, 0x0800);
        }
    }

    return true;
}

static void pet2001_reset_io(pet2001_t *pet)
{
    memset(pet->io, 0xFF, sizeof(pet->io));
    memset(pet->key_matrix, 0x00, sizeof(pet->key_matrix));
    memset(pet->key_latch_frames, 0, sizeof(pet->key_latch_frames));
    memset(&pet->pia1, 0, sizeof(pet->pia1));
    memset(&pet->pia2, 0, sizeof(pet->pia2));
    memset(&pet->via, 0, sizeof(pet->via));

    pet->pia1.port_a = 0xFF;
    pet->pia1.port_b = 0xFF;
    pet->pia2.port_a = 0xFF;
    pet->pia2.port_b = 0xFF;
    pet->via.reg[VIA_ORA] = 0xFF;
    pet->via.reg[VIA_ORA_NO_HANDSHAKE] = 0xFF;
    pet->via.reg[VIA_ORB] = 0xFF;
    pet->selected_key_row = 0x0F;
    pet->key_row_scan_mask = 0;
    pet->key_row_changes = 0;
    pet->last_io_read_addr = 0;
    pet->last_io_write_addr = 0;
    pet->last_io_read_value = 0;
    pet->last_io_write_value = 0;
    pet->retrace_signal = false;
    pet->retrace_poll_counter = 0;
}

static void pet2001_set_retrace(pet2001_t *pet, bool active)
{
    bool changed = pet->retrace_signal != active;

    pet->retrace_signal = active;
    if (changed) {
        pet->pia1.ctrl_b |= 0x80;
    }
}

static bool pet2001_poll_retrace(pet2001_t *pet)
{
    bool active;

    pet->retrace_poll_counter++;
    active = (pet->retrace_poll_counter & 0x08u) != 0;
    pet2001_set_retrace(pet, active);
    return active;
}

static void pet2001_set_matrix_key(pet2001_t *pet, uint8_t row, uint8_t col,
                                   bool pressed, uint8_t latch_frames)
{
    uint8_t mask;

    if (row >= 10 || col >= 8) {
        return;
    }

    mask = (uint8_t)(1u << col);
    if (pressed) {
        pet->key_matrix[row] |= mask;
        pet->key_latch_frames[row][col] = latch_frames;
    } else {
        pet->key_matrix[row] &= (uint8_t)~mask;
        pet->key_latch_frames[row][col] = 0;
    }
}

static void pet2001_release_expired_keys(pet2001_t *pet)
{
    uint8_t row;
    uint8_t col;

    for (row = 0; row < 10; ++row) {
        for (col = 0; col < 8; ++col) {
            if (pet->key_latch_frames[row][col] == 0) {
                continue;
            }
            pet->key_latch_frames[row][col]--;
            if (pet->key_latch_frames[row][col] == 0) {
                pet->key_matrix[row] &= (uint8_t)~(1u << col);
            }
        }
    }
}

static const pet_key_map_t *pet2001_active_key_map(const pet2001_t *pet, size_t *count)
{
    if (pet != NULL && pet->keyboard_layout == PET2001_KEYBOARD_BUSINESS) {
        *count = sizeof(pet_business_key_map) / sizeof(pet_business_key_map[0]);
        return pet_business_key_map;
    }

    *count = sizeof(pet_graphics_key_map) / sizeof(pet_graphics_key_map[0]);
    return pet_graphics_key_map;
}

static uint8_t pet2001_shift_row(const pet2001_t *pet)
{
    return pet != NULL && pet->keyboard_layout == PET2001_KEYBOARD_BUSINESS ? 6 : 8;
}

static bool pet2001_uses_basic1_pointers(const pet2001_t *pet)
{
    if (pet == NULL) {
        return false;
    }

    return pet->basic_rom_size == 0x2000 &&
           pet->keyboard_layout == PET2001_KEYBOARD_GRAPHICS;
}

static bool pet2001_use_graphics_game_aliases(const pet2001_t *pet)
{
    return pet != NULL &&
           pet->keyboard_layout == PET2001_KEYBOARD_BUSINESS &&
           pet->last_prg_size != 0;
}

static void pet2001_set_graphics_game_alias(pet2001_t *pet, int platform_key,
                                            bool pressed)
{
    uint8_t latch_frames = pressed ? PET_PRG_GAME_KEY_LATCH_FRAMES : 0;

    if (!pet2001_use_graphics_game_aliases(pet)) {
        return;
    }

    switch (platform_key) {
    case 'a':
    case 'A':
        pet2001_set_matrix_key(pet, 4, 0, pressed, latch_frames);
        break;
    case '4':
        pet2001_set_matrix_key(pet, 4, 6, pressed, latch_frames);
        break;
    case '6':
        pet2001_set_matrix_key(pet, 4, 7, pressed, latch_frames);
        break;
    default:
        break;
    }
}

static void pet2001_store_word(pet2001_t *pet, uint16_t address, uint16_t value)
{
    if (address + 1u >= sizeof(pet->ram)) {
        return;
    }

    pet->ram[address] = (uint8_t)(value & 0xFF);
    pet->ram[address + 1u] = (uint8_t)(value >> 8);
}

static uint16_t pet2001_load_word(const pet2001_t *pet, uint16_t address)
{
    if (pet == NULL || address + 1u >= sizeof(pet->ram)) {
        return 0;
    }

    return (uint16_t)pet->ram[address] |
           (uint16_t)((uint16_t)pet->ram[address + 1u] << 8);
}

static int d64_track_sector_count(int track)
{
    if (track >= 1 && track <= 17) {
        return 21;
    }
    if (track >= 18 && track <= 24) {
        return 19;
    }
    if (track >= 25 && track <= 30) {
        return 18;
    }
    if (track >= 31 && track <= 35) {
        return 17;
    }
    return 0;
}

static long d64_sector_offset(int track, int sector)
{
    long offset = 0;
    int current_track;
    int sector_count = d64_track_sector_count(track);

    if (sector_count == 0 || sector < 0 || sector >= sector_count) {
        return -1;
    }

    for (current_track = 1; current_track < track; ++current_track) {
        offset += (long)d64_track_sector_count(current_track) * 256L;
    }
    return offset + (long)sector * 256L;
}

static bool d64_seek_sector(platform_file_t *file, int track, int sector)
{
    long offset = d64_sector_offset(track, sector);

    return offset >= 0 && platform_fseek(file, offset, SEEK_SET) == 0;
}

static bool d64_read_sector(platform_file_t *file, int track, int sector, uint8_t buffer[256])
{
    int i;

    if (!d64_seek_sector(file, track, sector)) {
        return false;
    }

    for (i = 0; i < 256; ++i) {
        int ch = platform_getc(file);

        if (ch == EOF) {
            return false;
        }
        buffer[i] = (uint8_t)ch;
    }
    return true;
}

static bool d64_write_sector(platform_file_t *file, int track, int sector,
                             const uint8_t buffer[256])
{
    int i;

    if (!d64_seek_sector(file, track, sector)) {
        return false;
    }

    for (i = 0; i < 256; ++i) {
        if (platform_putc(buffer[i], file) == EOF) {
            return false;
        }
    }
    return true;
}

static bool d64_create_blank_image(const char *path)
{
    platform_file_t *file;
    uint8_t sector[256];
    int track;
    int i;

    file = platform_fopen(path, "wb");
    if (file == NULL) {
        return false;
    }

    for (i = 0; i < PET_CBM_DISK_SIZE; ++i) {
        if (platform_putc(0, file) == EOF) {
            platform_fclose(file);
            return false;
        }
    }
    platform_fclose(file);

    file = platform_fopen(path, "r+b");
    if (file == NULL) {
        return false;
    }

    memset(sector, 0, sizeof(sector));
    sector[0] = 18;
    sector[1] = 1;
    sector[2] = 'A';
    for (track = 1; track <= PET_CBM_DISK_TRACKS; ++track) {
        int sectors = d64_track_sector_count(track);
        int entry = 4 + (track - 1) * 4;
        uint32_t bitmap = sectors >= 24 ? 0xFFFFFFu : ((1u << sectors) - 1u);

        if (track == 18) {
            bitmap &= ~(1u << 0);
            bitmap &= ~(1u << 1);
            sectors -= 2;
        }
        sector[entry] = (uint8_t)sectors;
        sector[entry + 1] = (uint8_t)(bitmap & 0xFF);
        sector[entry + 2] = (uint8_t)((bitmap >> 8) & 0xFF);
        sector[entry + 3] = (uint8_t)((bitmap >> 16) & 0xFF);
    }
    memset(sector + 144, 0xA0, 16);
    memcpy(sector + 144, "PICOCALC PET", 12);
    sector[162] = 'P';
    sector[163] = 'T';
    sector[165] = '2';
    sector[166] = 'A';
    if (!d64_write_sector(file, 18, 0, sector)) {
        platform_fclose(file);
        return false;
    }

    memset(sector, 0, sizeof(sector));
    sector[0] = 0;
    sector[1] = 255;
    if (!d64_write_sector(file, 18, 1, sector)) {
        platform_fclose(file);
        return false;
    }

    platform_fclose(file);
    return true;
}

static void pet2001_cbm_normalize_name(const char *source, uint8_t *target)
{
    size_t i;

    memset(target, 0xA0, 16);
    for (i = 0; i < 16 && source[i] != '\0'; ++i) {
        uint8_t ch = (uint8_t)source[i];

        if (ch >= 'a' && ch <= 'z') {
            ch = (uint8_t)(ch - ('a' - 'A'));
        }
        if (ch == ',') {
            break;
        }
        target[i] = ch;
    }
}

static void pet2001_cbm_build_path(char *path, size_t path_size)
{
    const char *env_path = getenv("PICOCALC_PET_D64");

    snprintf(path, path_size, "%s",
             env_path != NULL && env_path[0] != '\0' ? env_path : PET_CBM_D64_PATH);
}

static platform_file_t *pet2001_cbm_open_d64(pet2001_t *pet, bool writable)
{
    char path[FILENAME_MAX];
    platform_file_t *file;

    pet2001_cbm_build_path(path, sizeof(path));
    file = platform_fopen(path, writable ? "r+b" : "rb");
    if (file == NULL && writable) {
        if (!d64_create_blank_image(path)) {
            snprintf(pet->last_error, sizeof(pet->last_error), "D64 create failed");
            return NULL;
        }
        file = platform_fopen(path, "r+b");
    }
    if (file == NULL) {
        snprintf(pet->last_error, sizeof(pet->last_error), "D64 open failed");
    }
    return file;
}

static bool d64_find_file(platform_file_t *file, const uint8_t name[16],
                          uint8_t *start_track, uint8_t *start_sector,
                          uint16_t *sector_count)
{
    uint8_t sector[256];
    int track = 18;
    int sector_num = 1;

    while (track != 0) {
        int entry;
        int next_track;
        int next_sector;

        if (!d64_read_sector(file, track, sector_num, sector)) {
            return false;
        }
        next_track = sector[0];
        next_sector = sector[1];
        for (entry = 0; entry < 8; ++entry) {
            int offset = 2 + entry * 32;

            if ((sector[offset + 2] & 0x07) == 0x02 &&
                memcmp(sector + offset + 5, name, 16) == 0) {
                if (start_track != NULL) {
                    *start_track = sector[offset + 3];
                }
                if (start_sector != NULL) {
                    *start_sector = sector[offset + 4];
                }
                if (sector_count != NULL) {
                    *sector_count = (uint16_t)sector[offset + 30] |
                                    (uint16_t)((uint16_t)sector[offset + 31] << 8);
                }
                return true;
            }
        }
        track = next_track;
        sector_num = next_sector;
    }

    return false;
}

static bool d64_bam_mark(platform_file_t *file, int track, int sector_num, bool free_sector)
{
    uint8_t bam[256];
    int entry = 4 + (track - 1) * 4;
    uint8_t mask;

    if (!d64_read_sector(file, 18, 0, bam) || track < 1 || track > PET_CBM_DISK_TRACKS ||
        sector_num < 0 || sector_num >= d64_track_sector_count(track)) {
        return false;
    }

    mask = (uint8_t)(1u << (sector_num & 7));
    if (free_sector) {
        if ((bam[entry + 1 + (sector_num / 8)] & mask) == 0) {
            bam[entry]++;
        }
        bam[entry + 1 + (sector_num / 8)] |= mask;
    } else {
        if ((bam[entry + 1 + (sector_num / 8)] & mask) != 0 && bam[entry] > 0) {
            bam[entry]--;
        }
        bam[entry + 1 + (sector_num / 8)] &= (uint8_t)~mask;
    }
    return d64_write_sector(file, 18, 0, bam);
}

static bool d64_allocate_sector(platform_file_t *file, uint8_t *track, uint8_t *sector_num)
{
    uint8_t bam[256];
    int candidate_track;

    if (!d64_read_sector(file, 18, 0, bam)) {
        return false;
    }

    for (candidate_track = 1; candidate_track <= PET_CBM_DISK_TRACKS; ++candidate_track) {
        int entry = 4 + (candidate_track - 1) * 4;
        int candidate_sector;

        if (candidate_track == 18 || bam[entry] == 0) {
            continue;
        }
        for (candidate_sector = 0;
             candidate_sector < d64_track_sector_count(candidate_track);
             ++candidate_sector) {
            uint8_t mask = (uint8_t)(1u << (candidate_sector & 7));

            if ((bam[entry + 1 + (candidate_sector / 8)] & mask) != 0) {
                *track = (uint8_t)candidate_track;
                *sector_num = (uint8_t)candidate_sector;
                return d64_bam_mark(file, candidate_track, candidate_sector, false);
            }
        }
    }

    return false;
}

static bool d64_find_free_directory_entry(platform_file_t *file, int *dir_track,
                                          int *dir_sector, int *entry_offset)
{
    uint8_t sector[256];
    int track = 18;
    int sector_num = 1;

    while (track != 0) {
        int entry;

        if (!d64_read_sector(file, track, sector_num, sector)) {
            return false;
        }
        for (entry = 0; entry < 8; ++entry) {
            int offset = 2 + entry * 32;

            if (sector[offset + 2] == 0) {
                *dir_track = track;
                *dir_sector = sector_num;
                *entry_offset = offset;
                return true;
            }
        }

        if (sector[0] == 0) {
            uint8_t new_track;
            uint8_t new_sector;
            uint8_t new_dir[256];

            if (!d64_allocate_sector(file, &new_track, &new_sector)) {
                return false;
            }
            sector[0] = new_track;
            sector[1] = new_sector;
            if (!d64_write_sector(file, track, sector_num, sector)) {
                return false;
            }
            memset(new_dir, 0, sizeof(new_dir));
            new_dir[0] = 0;
            new_dir[1] = 255;
            if (!d64_write_sector(file, new_track, new_sector, new_dir)) {
                return false;
            }
            *dir_track = new_track;
            *dir_sector = new_sector;
            *entry_offset = 2;
            return true;
        }

        track = sector[0];
        sector_num = sector[1];
    }
    return false;
}

static uint16_t pet2001_basic_start_pointer(const pet2001_t *pet)
{
    return pet2001_uses_basic1_pointers(pet) ? 0x007A : 0x0028;
}

static void pet2001_set_basic_program_bounds(pet2001_t *pet, uint16_t start, uint16_t end)
{
    uint16_t basicstart;
    uint16_t loadadr;

    basicstart = pet2001_basic_start_pointer(pet);
    if (pet2001_uses_basic1_pointers(pet)) {
        loadadr = 0x00E3;
    } else {
        loadadr = 0x00C7;
    }

    pet2001_store_word(pet, basicstart, start);
    pet2001_store_word(pet, loadadr, start);
    pet2001_store_word(pet, (uint16_t)(basicstart + 2u), end);
    pet2001_store_word(pet, (uint16_t)(basicstart + 4u), end);
    pet2001_store_word(pet, (uint16_t)(basicstart + 6u), end);
    pet2001_store_word(pet, (uint16_t)(loadadr + 2u), end);
}

static uint8_t pia_read_port(const pet2001_pia_t *pia, uint8_t external, bool port_b)
{
    uint8_t ddr = port_b ? pia->ddr_b : pia->ddr_a;
    uint8_t output = port_b ? pia->port_b : pia->port_a;

    return (uint8_t)((external & (uint8_t)~ddr) | (output & ddr));
}

static uint8_t pet2001_read_pia1(pet2001_t *pet, uint16_t address)
{
    uint8_t reg = (uint8_t)(address & 0x03);

    switch (reg) {
    case PIA_PORT_A:
        if ((pet->pia1.ctrl_a & 0x04) == 0) {
            return pet->pia1.ddr_a;
        }
        pet->pia1.ctrl_a &= 0x3F;
        return pia_read_port(&pet->pia1, 0xFF, false);
    case PIA_CTRL_A:
        return pet->pia1.ctrl_a;
    case PIA_PORT_B:
        if ((pet->pia1.ctrl_b & 0x04) == 0) {
            return pet->pia1.ddr_b;
        }
        pet->pia1.ctrl_b &= 0x3F;
        if (pet->selected_key_row < sizeof(pet->key_matrix)) {
            pet->key_row_scan_mask |= (uint16_t)(1u << pet->selected_key_row);
            return pia_read_port(&pet->pia1,
                                 (uint8_t)~pet->key_matrix[pet->selected_key_row],
                                 true);
        }
        return pia_read_port(&pet->pia1, 0xFF, true);
    case PIA_CTRL_B:
        return pet->pia1.ctrl_b;
    default:
        return 0xFF;
    }
}

static uint8_t pet2001_read_pia2(pet2001_t *pet, uint16_t address)
{
    uint8_t reg = (uint8_t)(address & 0x03);

    switch (reg) {
    case PIA_PORT_A:
        if ((pet->pia2.ctrl_a & 0x04) == 0) {
            return pet->pia2.ddr_a;
        }
        pet->pia2.ctrl_a &= 0x3F;
        return pia_read_port(&pet->pia2, 0xFF, false);
    case PIA_CTRL_A:
        return pet->pia2.ctrl_a;
    case PIA_PORT_B:
        if ((pet->pia2.ctrl_b & 0x04) == 0) {
            return pet->pia2.ddr_b;
        }
        pet->pia2.ctrl_b &= 0x3F;
        return pia_read_port(&pet->pia2, 0xFF, true);
    case PIA_CTRL_B:
        return pet->pia2.ctrl_b;
    default:
        return 0xFF;
    }
}

static void pet2001_write_pia(pet2001_pia_t *pia, uint16_t address, uint8_t value)
{
    uint8_t reg = (uint8_t)(address & 0x03);

    switch (reg) {
    case PIA_PORT_A:
        if (pia->ctrl_a & 0x04) {
            pia->port_a = value;
        } else {
            pia->ddr_a = value;
        }
        break;
    case PIA_CTRL_A:
        pia->ctrl_a = (uint8_t)((pia->ctrl_a & 0xC0) | (value & 0x3F));
        break;
    case PIA_PORT_B:
        if (pia->ctrl_b & 0x04) {
            pia->port_b = value;
        } else {
            pia->ddr_b = value;
        }
        break;
    case PIA_CTRL_B:
        pia->ctrl_b = (uint8_t)((pia->ctrl_b & 0xC0) | (value & 0x3F));
        break;
    default:
        break;
    }
}

static uint8_t pet2001_read_via(pet2001_t *pet, uint16_t address)
{
    uint8_t reg = (uint8_t)(address & 0x0F);

    switch (reg) {
    case VIA_ORB:
    {
        uint8_t external = pet2001_poll_retrace(pet) ? 0xDF : 0xFF;
        return (uint8_t)((external & (uint8_t)~pet->via.reg[VIA_DDRB]) |
                         (pet->via.reg[VIA_ORB] & pet->via.reg[VIA_DDRB]));
    }
    case VIA_ORA:
    case VIA_ORA_NO_HANDSHAKE:
        return (uint8_t)((0xFF & (uint8_t)~pet->via.reg[VIA_DDRA]) |
                         (pet->via.reg[VIA_ORA] & pet->via.reg[VIA_DDRA]));
    case VIA_IFR:
        return pet->via.reg[VIA_IFR];
    case VIA_IER:
        return (uint8_t)(pet->via.reg[VIA_IER] | 0x80);
    default:
        return pet->via.reg[reg];
    }
}

static void pet2001_write_via(pet2001_t *pet, uint16_t address, uint8_t value)
{
    uint8_t reg = (uint8_t)(address & 0x0F);

    switch (reg) {
    case VIA_ORA_NO_HANDSHAKE:
        pet->via.reg[VIA_ORA] = value;
        pet->via.reg[VIA_ORA_NO_HANDSHAKE] = value;
        break;
    case VIA_IFR:
        pet->via.reg[VIA_IFR] &= (uint8_t)~value;
        break;
    case VIA_IER:
        if (value & 0x80) {
            pet->via.reg[VIA_IER] |= (uint8_t)(value & 0x7F);
        } else {
            pet->via.reg[VIA_IER] &= (uint8_t)~(value & 0x7F);
        }
        break;
    default:
        pet->via.reg[reg] = value;
        break;
    }
}

bool pet2001_init(pet2001_t *pet)
{
    if (pet == NULL) {
        return false;
    }

    memset(pet, 0, sizeof(*pet));
    memset(pet->video, ' ', sizeof(pet->video));
    memset(pet->video_dirty, true, sizeof(pet->video_dirty));
    pet->basic_rom_base = 0xC000;
    pet->basic_rom_size = 0x2000;
    pet->keyboard_layout = PET2001_KEYBOARD_GRAPHICS;
    pet->last_error[0] = '\0';
    pet2001_reset_io(pet);
    return true;
}

bool pet2001_load_roms(pet2001_t *pet, const pet2001_rom_paths_t *paths)
{
    bool loaded;
    size_t basic_size = 0;

    if (pet == NULL || paths == NULL) {
        return false;
    }

    pet->last_error[0] = '\0';
    if (paths->combined[0] != '\0') {
        loaded = read_combined_rom(pet, paths->combined);
        if (!loaded) {
            snprintf(pet->last_error, sizeof(pet->last_error),
                     "combined ROM invalid");
        }
    } else {
        loaded = load_rom_part(pet, "basic", paths->basic, pet->basic_rom,
                               sizeof(pet->basic_rom), 0x2000, &basic_size) &&
                 load_rom_part(pet, "editor", paths->editor, pet->editor_rom,
                               sizeof(pet->editor_rom), sizeof(pet->editor_rom), NULL) &&
                 load_rom_part(pet, "kernal", paths->kernal, pet->kernal_rom,
                               sizeof(pet->kernal_rom), sizeof(pet->kernal_rom), NULL) &&
                 load_rom_part(pet, "chars", paths->characters, pet->character_rom,
                               sizeof(pet->character_rom), 0x0800, NULL);
        if (loaded) {
            pet->basic_rom_base = basic_size > 0x2000 ? 0xB000 : 0xC000;
            pet->basic_rom_size = basic_size > 0x2000 ? 0x3000 : 0x2000;
        }
    }

    if (!loaded) {
        return false;
    }

    pet->rom_paths = *paths;
    pet->keyboard_layout = paths->keyboard_layout;
    pet->roms_loaded = true;
    return true;
}

bool pet2001_load_prg(pet2001_t *pet, const char *path)
{
    platform_file_t *file;
    int low;
    int high;
    uint16_t address;
    uint16_t start;
    uint32_t loaded = 0;

    if (pet == NULL || path == NULL || path[0] == '\0') {
        return false;
    }

    pet->last_error[0] = '\0';
    file = platform_fopen(path, "rb");
    if (file == NULL) {
        snprintf(pet->last_error, sizeof(pet->last_error), "PRG open failed");
        return false;
    }

    low = platform_getc(file);
    high = platform_getc(file);
    if (low == EOF || high == EOF) {
        platform_fclose(file);
        snprintf(pet->last_error, sizeof(pet->last_error), "PRG header missing");
        return false;
    }

    start = (uint16_t)((uint8_t)low | ((uint16_t)(uint8_t)high << 8));
    address = start;
    while (address < sizeof(pet->ram)) {
        int ch = platform_getc(file);
        if (ch == EOF) {
            break;
        }
        pet->ram[address++] = (uint8_t)ch;
        loaded++;
    }
    platform_fclose(file);

    if (loaded == 0) {
        snprintf(pet->last_error, sizeof(pet->last_error), "PRG has no body");
        return false;
    }
    if (address == sizeof(pet->ram)) {
        snprintf(pet->last_error, sizeof(pet->last_error), "PRG truncated at RAM end");
    }

    pet->last_prg_start = start;
    pet->last_prg_end = address;
    pet->last_prg_size = loaded;
    pet2001_set_basic_program_bounds(pet, start, address);
    return true;
}

bool pet2001_save_prg(pet2001_t *pet, const char *path)
{
    platform_file_t *file;
    uint16_t basicstart;
    uint16_t start;
    uint16_t end;
    uint16_t address;

    if (pet == NULL || path == NULL || path[0] == '\0') {
        return false;
    }

    pet->last_error[0] = '\0';
    basicstart = pet2001_basic_start_pointer(pet);
    start = pet2001_load_word(pet, basicstart);
    end = pet2001_load_word(pet, (uint16_t)(basicstart + 2u));
    if (start < 0x0400 || start >= sizeof(pet->ram) || end <= start ||
        end > sizeof(pet->ram)) {
        snprintf(pet->last_error, sizeof(pet->last_error), "no BASIC program to save");
        return false;
    }

    file = platform_fopen(path, "wb");
    if (file == NULL) {
        snprintf(pet->last_error, sizeof(pet->last_error), "PRG save open failed");
        return false;
    }

    if (platform_putc(start & 0xFF, file) == EOF ||
        platform_putc((start >> 8) & 0xFF, file) == EOF) {
        platform_fclose(file);
        snprintf(pet->last_error, sizeof(pet->last_error), "PRG save header failed");
        return false;
    }

    for (address = start; address < end; ++address) {
        if (platform_putc(pet->ram[address], file) == EOF) {
            platform_fclose(file);
            snprintf(pet->last_error, sizeof(pet->last_error), "PRG save write failed");
            return false;
        }
    }

    if (platform_fclose(file) == EOF) {
        snprintf(pet->last_error, sizeof(pet->last_error), "PRG save close failed");
        return false;
    }

    pet->last_prg_start = start;
    pet->last_prg_end = end;
    pet->last_prg_size = (uint32_t)(end - start);
    return true;
}

static void pet2001_set_kernal_status(pet2001_t *pet, bool error)
{
    if (error) {
        pet->cpu.p |= P_CARRY;
    } else {
        pet->cpu.p &= (uint8_t)~P_CARRY;
    }
    MOS6510_REGS_SET_ZERO(&pet->cpu, error ? 1 : 0);
    MOS6510_REGS_SET_SIGN(&pet->cpu, 0);
}

static void pet2001_kernal_rts(pet2001_t *pet)
{
    uint8_t sp;
    uint16_t ret;

    sp = pet->cpu.sp;
    sp++;
    ret = pet->ram[0x0100u + sp];
    sp++;
    ret |= (uint16_t)((uint16_t)pet->ram[0x0100u + sp] << 8);
    pet->cpu.sp = sp;
    MOS6510_REGS_SET_PC(&pet->cpu, (uint16_t)(ret + 1u));
    pet->pc = (uint16_t)MOS6510_REGS_GET_PC(&pet->cpu);
}

static uint16_t pet2001_stack_return_address(const pet2001_t *pet)
{
    uint8_t sp;

    if (pet == NULL) {
        return 0;
    }

    sp = (uint8_t)(pet->cpu.sp + 1u);
    return (uint16_t)pet->ram[0x0100u + sp] |
           (uint16_t)((uint16_t)pet->ram[0x0100u + (uint8_t)(sp + 1u)] << 8);
}

static bool pet2001_cbm_capture_rom_file_state(pet2001_t *pet)
{
    uint16_t name_addr;
    uint8_t length;
    uint8_t i;

    if (pet == NULL) {
        return false;
    }

    length = pet->ram[0x00D1];
    if (length >= sizeof(pet->cbm_filename)) {
        length = sizeof(pet->cbm_filename) - 1;
    }
    name_addr = pet2001_load_word(pet, 0x00DA);
    for (i = 0; i < length; ++i) {
        pet->cbm_filename[i] = (char)pet2001_read(pet, (uint16_t)(name_addr + i));
    }
    pet->cbm_filename[length] = '\0';
    pet->cbm_filename_len = length;
    pet->cbm_device = pet->ram[0x00D4];
    pet->cbm_secondary = pet->ram[0x00D3];
    return pet->cbm_device == PET_CBM_DEVICE && pet->cbm_filename_len != 0;
}

static bool pet2001_cbm_load(pet2001_t *pet, uint16_t load_address, bool use_header)
{
    platform_file_t *file;
    uint8_t wanted_name[16];
    uint8_t sector[256];
    uint8_t track;
    uint8_t sector_num;
    uint16_t address = load_address;
    uint16_t start_address = load_address;
    bool have_address = !use_header;
    uint32_t loaded = 0;

    if (pet == NULL || pet->cbm_filename_len == 0) {
        return false;
    }

    file = pet2001_cbm_open_d64(pet, false);
    if (file == NULL) {
        return false;
    }

    pet2001_cbm_normalize_name(pet->cbm_filename, wanted_name);
    if (!d64_find_file(file, wanted_name, &track, &sector_num, NULL)) {
        platform_fclose(file);
        snprintf(pet->last_error, sizeof(pet->last_error), "D64 file not found");
        return false;
    }

    while (track != 0) {
        int end = 256;
        int i;
        uint8_t next_track;
        uint8_t next_sector;

        if (!d64_read_sector(file, track, sector_num, sector)) {
            platform_fclose(file);
            snprintf(pet->last_error, sizeof(pet->last_error), "D64 read failed");
            return false;
        }
        next_track = sector[0];
        next_sector = sector[1];
        if (next_track == 0) {
            end = sector[1] + 1;
            if (end < 2) {
                end = 2;
            }
        }

        for (i = 2; i < end; ++i) {
            if (!have_address) {
                address = sector[i++];
                if (i >= end) {
                    platform_fclose(file);
                    snprintf(pet->last_error, sizeof(pet->last_error), "D64 load header bad");
                    return false;
                }
                address |= (uint16_t)((uint16_t)sector[i] << 8);
                start_address = address;
                have_address = true;
                continue;
            }
            if (address >= sizeof(pet->ram)) {
                platform_fclose(file);
                snprintf(pet->last_error, sizeof(pet->last_error), "D64 load over RAM");
                return false;
            }
            pet->ram[address++] = sector[i];
            loaded++;
        }

        track = next_track;
        sector_num = next_sector;
    }

    platform_fclose(file);
    if (!have_address || loaded == 0) {
        snprintf(pet->last_error, sizeof(pet->last_error), "D64 load empty");
        return false;
    }

    pet->last_prg_start = start_address;
    pet->last_prg_end = address;
    pet->last_prg_size = loaded;
    pet2001_set_basic_program_bounds(pet, start_address, address);
    pet->cpu.x = (uint8_t)(address & 0xFF);
    pet->cpu.y = (uint8_t)(address >> 8);
    return true;
}

static bool pet2001_cbm_save(pet2001_t *pet, uint16_t start, uint16_t end)
{
    platform_file_t *file;
    uint8_t wanted_name[16];
    uint8_t sector[256];
    uint8_t first_track = 0;
    uint8_t first_sector = 0;
    uint8_t prev_track = 0;
    uint8_t prev_sector = 0;
    uint16_t address = start;
    uint16_t sector_count = 0;
    int dir_track;
    int dir_sector;
    int entry_offset;

    if (pet == NULL || pet->cbm_filename_len == 0 || start >= end ||
        start >= sizeof(pet->ram) || end > sizeof(pet->ram)) {
        return false;
    }

    file = pet2001_cbm_open_d64(pet, true);
    if (file == NULL) {
        return false;
    }

    pet2001_cbm_normalize_name(pet->cbm_filename, wanted_name);
    if (d64_find_file(file, wanted_name, NULL, NULL, NULL)) {
        platform_fclose(file);
        snprintf(pet->last_error, sizeof(pet->last_error), "D64 file exists");
        return false;
    }

    while (address < end || first_track == 0) {
        uint8_t track;
        uint8_t sector_num;
        int data_pos = 2;

        if (!d64_allocate_sector(file, &track, &sector_num)) {
            platform_fclose(file);
            snprintf(pet->last_error, sizeof(pet->last_error), "D64 disk full");
            return false;
        }
        if (first_track == 0) {
            first_track = track;
            first_sector = sector_num;
        }
        if (prev_track != 0) {
            uint8_t prev[256];

            if (!d64_read_sector(file, prev_track, prev_sector, prev)) {
                platform_fclose(file);
                return false;
            }
            prev[0] = track;
            prev[1] = sector_num;
            if (!d64_write_sector(file, prev_track, prev_sector, prev)) {
                platform_fclose(file);
                return false;
            }
        }

        memset(sector, 0, sizeof(sector));
        sector[0] = 0;
        sector[1] = 1;
        if (sector_count == 0) {
            sector[data_pos++] = (uint8_t)(start & 0xFF);
            sector[data_pos++] = (uint8_t)(start >> 8);
        }
        while (address < end && data_pos < 256) {
            sector[data_pos++] = pet->ram[address++];
        }
        if (address >= end) {
            sector[0] = 0;
            sector[1] = (uint8_t)(data_pos - 1);
        }
        if (!d64_write_sector(file, track, sector_num, sector)) {
            platform_fclose(file);
            snprintf(pet->last_error, sizeof(pet->last_error), "D64 write failed");
            return false;
        }
        prev_track = track;
        prev_sector = sector_num;
        sector_count++;
    }

    if (!d64_find_free_directory_entry(file, &dir_track, &dir_sector, &entry_offset) ||
        !d64_read_sector(file, dir_track, dir_sector, sector)) {
        platform_fclose(file);
        snprintf(pet->last_error, sizeof(pet->last_error), "D64 directory full");
        return false;
    }
    sector[entry_offset + 2] = 0x82;
    sector[entry_offset + 3] = first_track;
    sector[entry_offset + 4] = first_sector;
    memcpy(sector + entry_offset + 5, wanted_name, 16);
    sector[entry_offset + 30] = (uint8_t)(sector_count & 0xFF);
    sector[entry_offset + 31] = (uint8_t)(sector_count >> 8);
    if (!d64_write_sector(file, dir_track, dir_sector, sector)) {
        platform_fclose(file);
        return false;
    }

    platform_fclose(file);
    pet->last_prg_start = start;
    pet->last_prg_end = end;
    pet->last_prg_size = (uint32_t)(end - start);
    return true;
}

bool pet2001_kernal_trap(pet2001_t *pet)
{
    uint16_t pc;

    if (pet == NULL) {
        return false;
    }

    pc = (uint16_t)MOS6510_REGS_GET_PC(&pet->cpu);
    switch (pc) {
    case PET_KERNAL_SETLFS:
        pet->cbm_logical_file = pet->cpu.a;
        pet->cbm_device = pet->cpu.x;
        pet->cbm_secondary = pet->cpu.y;
        pet2001_set_kernal_status(pet, false);
        pet2001_kernal_rts(pet);
        return true;
    case PET_KERNAL_SETNAM:
    {
        uint16_t name_addr = (uint16_t)pet->cpu.x | (uint16_t)((uint16_t)pet->cpu.y << 8);
        uint8_t length = pet->cpu.a;
        uint8_t i;

        if (length >= sizeof(pet->cbm_filename)) {
            length = sizeof(pet->cbm_filename) - 1;
        }
        for (i = 0; i < length; ++i) {
            pet->cbm_filename[i] = (char)pet2001_read(pet, (uint16_t)(name_addr + i));
        }
        pet->cbm_filename[length] = '\0';
        pet->cbm_filename_len = length;
        pet2001_set_kernal_status(pet, false);
        pet2001_kernal_rts(pet);
        return true;
    }
    case PET_KERNAL_LOAD:
        if (pet->cbm_device != PET_CBM_DEVICE) {
            return false;
        }
        pet2001_set_kernal_status(
            pet,
            !pet2001_cbm_load(pet,
                              (uint16_t)pet->cpu.x |
                                  (uint16_t)((uint16_t)pet->cpu.y << 8),
                              pet->cbm_secondary == 0));
        pet2001_kernal_rts(pet);
        return true;
    case PET_KERNAL_SAVE:
        if (pet->cbm_device != PET_CBM_DEVICE) {
            return false;
        }
    {
        uint16_t start_pointer = pet->cpu.a;
        uint16_t start = pet2001_load_word(pet, start_pointer);
        uint16_t end = (uint16_t)pet->cpu.x | (uint16_t)((uint16_t)pet->cpu.y << 8);

        pet2001_set_kernal_status(pet, !pet2001_cbm_save(pet, start, end));
        pet2001_kernal_rts(pet);
        return true;
    }
    case PET_BASIC4_FILE_OPEN:
        if (pet2001_stack_return_address(pet) != 0xF373 ||
            !pet2001_cbm_capture_rom_file_state(pet)) {
            return false;
        }
        pet2001_set_kernal_status(pet, !pet2001_cbm_load(pet, 0, true));
        pet->ram[0x0096] = (pet->cpu.p & P_CARRY) != 0 ? 0x04 : 0x00;
        pet->ram[0x009D] = 0x00;
        pet2001_kernal_rts(pet);
        pet2001_kernal_rts(pet);
        return true;
    case PET_BASIC4_SAVE_DEVICE:
        if (!pet2001_cbm_capture_rom_file_state(pet)) {
            return false;
        }
    {
        uint16_t basicstart = pet2001_basic_start_pointer(pet);
        uint16_t start = pet2001_load_word(pet, basicstart);
        uint16_t end = pet2001_load_word(pet, (uint16_t)(basicstart + 2u));

        pet2001_set_kernal_status(pet, !pet2001_cbm_save(pet, start, end));
        pet->ram[0x0096] = (pet->cpu.p & P_CARRY) != 0 ? 0x04 : 0x00;
        pet2001_kernal_rts(pet);
        return true;
    }
    default:
        return false;
    }
}

void pet2001_reset(pet2001_t *pet)
{
    if (pet == NULL) {
        return;
    }

    memset(pet->ram, 0, sizeof(pet->ram));
    memset(pet->video, ' ', sizeof(pet->video));
    memset(pet->video_dirty, true, sizeof(pet->video_dirty));
    pet2001_reset_io(pet);
    memset(&pet->cpu, 0, sizeof(pet->cpu));
    pet->cycles_executed = 0;
    pet->last_prg_start = 0;
    pet->last_prg_end = 0;
    pet->last_prg_size = 0;
    pet->typeahead_head = 0;
    pet->typeahead_tail = 0;
    pet->typeahead_wait_frames = 0;
    pet->io_reads = 0;
    pet->io_writes = 0;
    pet->video_writes = 0;
    pet->frames_executed = 0;
    pet->cpu.sp = 0xFF;
    pet->cpu.p = P_INTERRUPT | P_UNUSED;
    pet->pc = (uint16_t)pet->kernal_rom[0x0FFC] |
              (uint16_t)((uint16_t)pet->kernal_rom[0x0FFD] << 8);
    if (pet->pc == 0xFFFF || pet->pc == 0x0000) {
        pet->pc = 0xF000;
    }
    MOS6510_REGS_SET_PC(&pet->cpu, pet->pc);
}

void pet2001_run_frame(pet2001_t *pet)
{
    if (pet == NULL) {
        return;
    }

    pet->frames_executed++;
    pet2001_set_retrace(pet, (pet->frames_executed & 1u) != 0);
    if (pet->typeahead_wait_frames > 0) {
        pet->typeahead_wait_frames--;
    } else if (pet->typeahead_head != pet->typeahead_tail) {
        int key = pet->typeahead[pet->typeahead_head];
        pet->typeahead_head =
            (uint8_t)((pet->typeahead_head + 1u) %
                      (sizeof(pet->typeahead) / sizeof(pet->typeahead[0])));
        pet2001_key_event(pet, key, true);
        pet->typeahead_wait_frames = PET_KEY_LATCH_FRAMES + 2u;
    }
    pet2001_step_cycles(pet, 16667);
    pet2001_release_expired_keys(pet);
}

void pet2001_step_cycles(pet2001_t *pet, uint32_t cycles)
{
    if (pet == NULL) {
        return;
    }

    if (pet->roms_loaded) {
        pet->cycles_executed += vice_6502_step(pet, cycles);
    }
    pet->pc = (uint16_t)MOS6510_REGS_GET_PC(&pet->cpu);
}

void pet2001_key_event(pet2001_t *pet, int platform_key, bool pressed)
{
    size_t i;
    size_t key_map_count;
    const pet_key_map_t *key_map;

    if (platform_key == '\r' || platform_key == '\n') {
        platform_key = PLATFORM_KEY_ENTER;
    }

    if (pet == NULL || platform_key == PLATFORM_KEY_NONE) {
        return;
    }

    key_map = pet2001_active_key_map(pet, &key_map_count);
    for (i = 0; i < key_map_count; ++i) {
        const pet_key_map_t *mapped = &key_map[i];

        if (mapped->key != platform_key) {
            continue;
        }

        pet2001_set_matrix_key(pet, mapped->row, mapped->col, pressed,
                               pressed ? PET_KEY_LATCH_FRAMES : 0);
        if (mapped->shift) {
            pet2001_set_matrix_key(pet, pet2001_shift_row(pet), PET_KEY_LEFT_SHIFT_COL,
                                   pressed, pressed ? PET_KEY_LATCH_FRAMES : 0);
        }
        pet2001_set_graphics_game_alias(pet, platform_key, pressed);
        return;
    }
}

bool pet2001_queue_text(pet2001_t *pet, const char *text)
{
    if (pet == NULL || text == NULL) {
        return false;
    }

    while (*text != '\0') {
        uint8_t next_tail =
            (uint8_t)((pet->typeahead_tail + 1u) %
                      (sizeof(pet->typeahead) / sizeof(pet->typeahead[0])));
        if (next_tail == pet->typeahead_head) {
            snprintf(pet->last_error, sizeof(pet->last_error), "typeahead full");
            return false;
        }

        pet->typeahead[pet->typeahead_tail] = (uint16_t)(uint8_t)*text++;
        pet->typeahead_tail = next_tail;
    }

    return true;
}

void pet2001_toggle_keyboard_layout(pet2001_t *pet)
{
    if (pet == NULL) {
        return;
    }

    memset(pet->key_matrix, 0, sizeof(pet->key_matrix));
    memset(pet->key_latch_frames, 0, sizeof(pet->key_latch_frames));
    pet->keyboard_layout = pet->keyboard_layout == PET2001_KEYBOARD_BUSINESS
                               ? PET2001_KEYBOARD_GRAPHICS
                               : PET2001_KEYBOARD_BUSINESS;
}

const char *pet2001_keyboard_layout_name(const pet2001_t *pet)
{
    return pet != NULL && pet->keyboard_layout == PET2001_KEYBOARD_BUSINESS
               ? "business"
               : "graphics";
}

uint8_t pet2001_read(pet2001_t *pet, uint16_t address)
{
    if (pet == NULL) {
        return 0xFF;
    }

    if (address < sizeof(pet->ram)) {
        return pet->ram[address];
    }

    if (address >= 0x8000 && address < 0x8400) {
        size_t offset = (size_t)(address - 0x8000) % sizeof(pet->video);
        return pet->video[offset];
    }

    if (address >= 0x8400 && address < 0x9000) {
        size_t offset = (size_t)(address - 0x8000) % sizeof(pet->video);
        return pet->video[offset];
    }

    if (address >= 0xE800 && address < 0xF000) {
        size_t offset = (size_t)(address - 0xE800);
        uint8_t value;
        pet->io_reads++;

        if (address >= 0xE810 && address <= 0xE81F) {
            value = pet2001_read_pia1(pet, address);
        } else if (address >= 0xE820 && address <= 0xE82F) {
            value = pet2001_read_pia2(pet, address);
        } else if (address >= 0xE840 && address <= 0xE84F) {
            value = pet2001_read_via(pet, address);
        } else {
            value = pet->io[offset];
        }
        pet->last_io_read_addr = address;
        pet->last_io_read_value = value;
        return value;
    }

    if (address >= pet->basic_rom_base &&
        address < (uint16_t)(pet->basic_rom_base + pet->basic_rom_size)) {
        return pet->basic_rom[address - pet->basic_rom_base];
    }

    if (address >= 0xE000 && address < 0xE800) {
        return pet->editor_rom[address - 0xE000];
    }

    if (address >= 0xF000) {
        return pet->kernal_rom[address - 0xF000];
    }

    return 0xFF;
}

void pet2001_write(pet2001_t *pet, uint16_t address, uint8_t value)
{
    if (pet == NULL) {
        return;
    }

    if (address < sizeof(pet->ram)) {
        pet->ram[address] = value;
        return;
    }

    if (address >= 0x8000 && address < 0x8400) {
        size_t offset = (size_t)(address - 0x8000) % sizeof(pet->video);
        pet->video[offset] = value;
        pet->video_dirty[offset] = true;
        pet->video_writes++;
        return;
    }

    if (address >= 0x8400 && address < 0x9000) {
        size_t offset = (size_t)(address - 0x8000) % sizeof(pet->video);
        pet->video[offset] = value;
        pet->video_dirty[offset] = true;
        pet->video_writes++;
        return;
    }

    if (address >= 0xE800 && address < 0xF000) {
        pet->io[address - 0xE800] = value;
        pet->io_writes++;
        pet->last_io_write_addr = address;
        pet->last_io_write_value = value;
        if (address >= 0xE810 && address <= 0xE81F) {
            uint8_t old_row = pet->selected_key_row;
            pet2001_write_pia(&pet->pia1, address, value);
            pet->selected_key_row = (uint8_t)(pet->pia1.port_a & 0x0F);
            if (pet->selected_key_row != old_row) {
                pet->key_row_changes++;
            }
            if (pet->selected_key_row < sizeof(pet->key_matrix)) {
                pet->key_row_scan_mask |= (uint16_t)(1u << pet->selected_key_row);
            }
        } else if (address >= 0xE820 && address <= 0xE82F) {
            pet2001_write_pia(&pet->pia2, address, value);
        } else if (address >= 0xE840 && address <= 0xE84F) {
            pet2001_write_via(pet, address, value);
        }
    }
}
