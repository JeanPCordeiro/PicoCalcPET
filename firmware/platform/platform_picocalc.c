#include "platform.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

#include "pico/stdlib.h"
#include "pico/stdio/driver.h"

#include "trs.h"

#include "drivers/picocalc.h"
#include "drivers/keyboard.h"
#include "drivers/lcd.h"
#include "drivers/display.h"
#include "drivers/fat32.h"
#include "drivers/font.h"
#include "drivers/sdcard.h"

extern volatile bool user_interrupt;

typedef struct {
    uint8_t width;
    uint8_t height;
    uint8_t glyphs[];
} trs_font_t;

extern const trs_font_t trs80_model3_font_5x18;

enum {
    PICOCALC_TRS_FONT_WIDTH = 5,
    PICOCALC_TRS_FONT_HEIGHT = 18,
    PICOCALC_TRS_FONT_STATUS_GAP = 0
};

#define PICOCALC_STATUS_CYAN RGB(0, 255, 255)
#define PICOCALC_TRS_AMBER RGB(255, 191, 0)

static int picocalc_cols = 64;
static int picocalc_rows = 16;
static int picocalc_status_row;
static int picocalc_status_rows;
static int picocalc_status_next_line;
static fat32_error_t picocalc_last_file_error = FAT32_OK;
static bool picocalc_platform_initialised;
static bool picocalc_use_trs_font;
static uint16_t picocalc_trs_cell_buffer[PICOCALC_TRS_FONT_WIDTH * 2 * PICOCALC_TRS_FONT_HEIGHT];

typedef struct {
    char name[28];
    bool present;
    bool write_protected;
    uint32_t active_until_ms;
} picocalc_operator_disk_t;

static char picocalc_operator_rom_source[8] = "--";
static int picocalc_operator_rom_size;
static picocalc_operator_disk_t picocalc_operator_disks[2];
static bool picocalc_operator_enabled;
static uint16_t picocalc_operator_last_pc;
static float picocalc_operator_last_clock_mhz;
static bool picocalc_operator_last_audio_enabled;

#define PICOCALC_DISK_ACTIVITY_MS 250u

static uint16_t picocalc_trs_foreground(void)
{
    return PICOCALC_TRS_AMBER;
}

static uint16_t picocalc_trs_background(void)
{
    return BACKGROUND;
}

static int picocalc_trs_pixel_height(void)
{
    return picocalc_rows * PICOCALC_TRS_FONT_HEIGHT;
}

static void picocalc_draw_status_divider(void)
{
    int gap_top;
    int gap_height;
    int line_y;

    if (!picocalc_use_trs_font || picocalc_status_rows <= 0) {
        return;
    }

    gap_top = picocalc_trs_pixel_height();
    gap_height = (picocalc_status_row * GLYPH_HEIGHT) - gap_top;
    if (gap_height <= 0) {
        return;
    }

    line_y = gap_top + ((gap_height - 1) / 2);
    lcd_solid_rectangle(PICOCALC_STATUS_CYAN, 0, (uint16_t)line_y, WIDTH, 1);
}

static void picocalc_render_trs_cell(int col, int row, uint8_t ch, uint8_t mode)
{
    const uint8_t *glyph;
    uint8_t glyph_code = ch;
    uint16_t *pixel = picocalc_trs_cell_buffer;
    uint16_t fg = picocalc_trs_foreground();
    uint16_t bg = picocalc_trs_background();
    bool invert = false;
    bool expanded = (mode & EXPANDED) != 0;
    bool cursor = (mode & PLATFORM_CELL_CURSOR) != 0;
    bool underscore = (mode & PLATFORM_CELL_UNDERSCORE) != 0;
    int cell_width = PICOCALC_TRS_FONT_WIDTH;
    int x;
    int y;
    int src_row;

    if (expanded && (col & 1)) {
        return;
    }

    if ((mode & REVERSE) && !(mode & INVERSE)) {
        invert = true;
    }

    if (trs_model > 1 && glyph_code >= 0xC0 &&
        (mode & (ALTERNATE | INVERSE)) == 0) {
        glyph_code = (uint8_t)(glyph_code - 0x40);
    }

    if ((mode & INVERSE) && (glyph_code & 0x80)) {
        invert = true;
        glyph_code = (uint8_t)(glyph_code & 0x7F);
    }

    if (invert) {
        uint16_t tmp = fg;
        fg = bg;
        bg = tmp;
    }

    if (expanded) {
        cell_width *= 2;
    }

    glyph = &trs80_model3_font_5x18.glyphs[glyph_code * trs80_model3_font_5x18.height];

    for (src_row = 0; src_row < trs80_model3_font_5x18.height; ++src_row) {
        uint8_t bits = glyph[src_row];
        int bit;

        for (bit = 0; bit < trs80_model3_font_5x18.width; ++bit) {
            uint16_t colour = (bits & (1u << (trs80_model3_font_5x18.width - 1 - bit))) ? fg : bg;

            if (underscore && src_row == (trs80_model3_font_5x18.height - 1)) {
                colour = fg;
            }

            if (cursor) {
                colour = (colour == fg) ? bg : fg;
            }

            if (expanded) {
                *(pixel++) = colour;
                *(pixel++) = colour;
            } else {
                *(pixel++) = colour;
            }
        }
    }

    x = col * PICOCALC_TRS_FONT_WIDTH;
    y = row * PICOCALC_TRS_FONT_HEIGHT;
    lcd_blit(picocalc_trs_cell_buffer, (uint16_t)x, (uint16_t)y, (uint16_t)cell_width,
             trs80_model3_font_5x18.height);
}

static void picocalc_clear_line(int row)
{
    int max_col = lcd_get_columns();

    if (row < 0 || row >= ROWS || max_col <= 0) {
        return;
    }

    lcd_erase_line((uint8_t)row, 0, (uint8_t)(max_col - 1));
}

static void picocalc_write_line(int row, const char *text)
{
    int max_col = lcd_get_columns();
    int col;

    if (text == NULL || row < 0 || row >= ROWS || max_col <= 0) {
        return;
    }

    lcd_set_foreground(PICOCALC_STATUS_CYAN);
    lcd_set_background(BACKGROUND);
    picocalc_clear_line(row);
    for (col = 0; col < max_col && text[col] != '\0'; ++col) {
        lcd_putc((uint8_t)col, (uint8_t)row, (uint8_t)text[col]);
    }
}

static const char *picocalc_leaf_name(const char *path)
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

static void picocalc_copy_disk_label(char *target, size_t target_size, const char *path)
{
    const char *leaf;
    size_t len;

    if (target == NULL || target_size == 0) {
        return;
    }

    leaf = picocalc_leaf_name(path);
    len = strlen(leaf);
    if (len >= target_size) {
        size_t keep = target_size - 2;
        memcpy(target, leaf, keep);
        target[keep] = '~';
        target[keep + 1] = '\0';
        return;
    }

    memcpy(target, leaf, len + 1);
}

static void picocalc_write_operator_line(int line, const char *text)
{
    char buffer[65];

    if (text == NULL || line < 0 || line >= picocalc_status_rows) {
        return;
    }

    snprintf(buffer, sizeof(buffer), "%-64.64s", text);
    picocalc_write_line(picocalc_status_row + line, buffer);
}

static void picocalc_render_operator_panel(void)
{
    char line0[80];
    char line1[96];
    const char *d0_name;
    const char *d1_name;
    char d0_active;
    char d1_active;
    const char *d0_mode;
    const char *d1_mode;
    uint32_t now_ms;

    if (!picocalc_operator_enabled || picocalc_status_rows <= 0) {
        return;
    }

    now_ms = to_ms_since_boot(get_absolute_time());
    d0_name = picocalc_operator_disks[0].present ? picocalc_operator_disks[0].name : "none";
    d1_name = picocalc_operator_disks[1].present ? picocalc_operator_disks[1].name : "none";
    d0_active = (picocalc_operator_disks[0].present &&
                 now_ms < picocalc_operator_disks[0].active_until_ms) ? '*' : '.';
    d1_active = (picocalc_operator_disks[1].present &&
                 now_ms < picocalc_operator_disks[1].active_until_ms) ? '*' : '.';
    d0_mode = picocalc_operator_disks[0].present
        ? (picocalc_operator_disks[0].write_protected ? "ro" : "rw")
        : "--";
    d1_mode = picocalc_operator_disks[1].present
        ? (picocalc_operator_disks[1].write_protected ? "ro" : "rw")
        : "--";

    snprintf(line0, sizeof(line0), "M3 %.2fMHz ROM:%s PC:%04X AUD:%s",
             (double)picocalc_operator_last_clock_mhz,
             picocalc_operator_rom_source,
             (unsigned int)picocalc_operator_last_pc,
             picocalc_operator_last_audio_enabled ? "on" : "off");
    snprintf(line1, sizeof(line1), "D0:%c %-18.18s %s   D1:%c %-18.18s %s",
             d0_active, d0_name, d0_mode,
             d1_active, d1_name, d1_mode);

    picocalc_write_operator_line(0, line0);
    if (picocalc_status_rows > 1) {
        picocalc_write_operator_line(1, line1);
    }
    if (picocalc_status_rows > 2) {
        picocalc_write_operator_line(2, "ESC=BREAK  BRK=RESET");
    }
}

void platform_init(void)
{
    if (picocalc_platform_initialised) {
        return;
    }

    stdio_init_all();
    picocalc_init();
    stdio_set_driver_enabled(&picocalc_stdio_driver, false);
    lcd_set_font(&font_5x10);
    lcd_set_foreground(PICOCALC_STATUS_CYAN);
    lcd_set_background(BACKGROUND);
    lcd_enable_cursor(false);
    lcd_set_underscore(false);
    lcd_define_scrolling(0, 0);
    lcd_scroll_reset();
    sleep_ms(250);
    lcd_clear_screen();
    picocalc_status_row = picocalc_rows;
    picocalc_status_rows = ROWS - picocalc_status_row;
    picocalc_status_next_line = 0;
    picocalc_platform_initialised = true;
}

bool platform_poll_key(int *keycode, bool wait)
{
    int ch;
    int mapped = PLATFORM_KEY_NONE;

    if (user_interrupt) {
        user_interrupt = false;
        if (keycode != NULL) {
            *keycode = PLATFORM_KEY_BREAK;
        }
        return true;
    }

    if (wait) {
        while (!keyboard_key_available()) {
            if (user_interrupt) {
                user_interrupt = false;
                if (keycode != NULL) {
                    *keycode = PLATFORM_KEY_BREAK;
                }
                return true;
            }
            tight_loop_contents();
        }
    } else if (!keyboard_key_available()) {
        return false;
    }

    ch = keyboard_get_key();

    switch (ch) {
    case KEY_UP:
        mapped = PLATFORM_KEY_UP;
        break;
    case KEY_DOWN:
        mapped = PLATFORM_KEY_DOWN;
        break;
    case KEY_LEFT:
        mapped = PLATFORM_KEY_LEFT;
        break;
    case KEY_RIGHT:
        mapped = PLATFORM_KEY_RIGHT;
        break;
    case KEY_TAB:
        mapped = PLATFORM_KEY_TAB;
        break;
    case KEY_ESC:
        mapped = PLATFORM_KEY_ESC;
        break;
    case KEY_BREAK:
        mapped = PLATFORM_KEY_BREAK;
        break;
    case KEY_HOME:
        mapped = PLATFORM_KEY_HOME;
        break;
    case KEY_END:
        mapped = PLATFORM_KEY_END;
        break;
    case KEY_PAGE_UP:
        mapped = PLATFORM_KEY_PAGE_UP;
        break;
    case KEY_PAGE_DOWN:
        mapped = PLATFORM_KEY_PAGE_DOWN;
        break;
    case KEY_BACKSPACE:
    case KEY_DEL:
        mapped = PLATFORM_KEY_BACKSPACE;
        break;
    case KEY_ENTER:
    case KEY_RETURN:
        mapped = PLATFORM_KEY_ENTER;
        break;
    case KEY_F1:
        mapped = PLATFORM_KEY_F1;
        break;
    case KEY_F2:
        mapped = PLATFORM_KEY_F2;
        break;
    case KEY_F3:
        mapped = PLATFORM_KEY_F3;
        break;
    case KEY_F4:
        mapped = PLATFORM_KEY_F4;
        break;
    default:
        mapped = ch;
        break;
    }

    if (keycode != NULL) {
        *keycode = mapped;
    }
    return true;
}

void platform_screen_configure(int cols, int rows)
{
    int row;
    int cleared_height;

    picocalc_cols = cols;
    picocalc_rows = rows;
    picocalc_use_trs_font = (cols == 64 && rows == 16);

    if (picocalc_use_trs_font) {
        lcd_define_scrolling(0, 0);
        lcd_scroll_reset();
        picocalc_status_rows = (HEIGHT - picocalc_trs_pixel_height()) / GLYPH_HEIGHT;
        if (picocalc_status_rows < 0) {
            picocalc_status_rows = 0;
        }
        picocalc_status_row = ROWS - picocalc_status_rows;
        cleared_height = picocalc_status_row * GLYPH_HEIGHT + PICOCALC_TRS_FONT_STATUS_GAP;
        lcd_solid_rectangle(picocalc_trs_background(), 0, 0, WIDTH, (uint16_t)cleared_height);
        picocalc_draw_status_divider();
    } else {
        picocalc_status_row = rows;
        picocalc_status_rows = ROWS - picocalc_status_row;

        for (row = 0; row < picocalc_rows && row < ROWS; ++row) {
            picocalc_clear_line(row);
        }
    }
}

void platform_screen_write_cell(int col, int row, uint8_t ch, uint8_t mode)
{
    bool underscore;

    (void)mode;
    if (col < 0 || col >= picocalc_cols || row < 0 || row >= picocalc_rows) {
        return;
    }

    if (picocalc_use_trs_font) {
        /* TRS rendering assumes absolute LCD coordinates; force a neutral scroll offset. */
        lcd_scroll_reset();
        picocalc_render_trs_cell(col, row, ch ? ch : ' ', mode);
        return;
    }

    if (col >= lcd_get_columns() || row >= ROWS) {
        return;
    }

    underscore = (mode & PLATFORM_CELL_UNDERSCORE) != 0;
    lcd_set_underscore(underscore);
    lcd_putc((uint8_t)col, (uint8_t)row, ch ? ch : ' ');
    if (underscore) {
        lcd_set_underscore(false);
    }
}

void platform_screen_flush(void)
{
}

void platform_status_clear(void)
{
    int line;

    if (picocalc_status_rows <= 0) {
        return;
    }

    for (line = 0; line < picocalc_status_rows; ++line) {
        picocalc_clear_line(picocalc_status_row + line);
    }
    picocalc_status_next_line = 0;
    picocalc_operator_enabled = false;
}

void platform_status_puts(const char *text)
{
    int line;

    if (text == NULL || picocalc_status_rows <= 0) {
        return;
    }

    if (picocalc_operator_enabled) {
        picocalc_write_operator_line((picocalc_status_rows > 2) ? 2 : 0, text);
        return;
    }

    line = picocalc_status_row + picocalc_status_next_line;
    picocalc_write_line(line, text);
    picocalc_status_next_line = (picocalc_status_next_line + 1) % picocalc_status_rows;
}

void platform_status_write_line(int line, const char *text)
{
    if (text == NULL || picocalc_status_rows <= 0) {
        return;
    }

    if (line < 0 || line >= picocalc_status_rows) {
        return;
    }

    picocalc_write_line(picocalc_status_row + line, text);
}

void platform_status_set_operator_context(const char *rom_source, int rom_size,
                                          const char *disk0_path, bool disk0_present, bool disk0_write_protected,
                                          const char *disk1_path, bool disk1_present, bool disk1_write_protected)
{
    snprintf(picocalc_operator_rom_source, sizeof(picocalc_operator_rom_source),
             "%s", (rom_source != NULL && rom_source[0] != '\0') ? rom_source : "--");
    picocalc_operator_rom_size = rom_size;
    (void)picocalc_operator_rom_size;

    picocalc_operator_disks[0].present = disk0_present;
    picocalc_operator_disks[0].write_protected = disk0_write_protected;
    picocalc_operator_disks[0].active_until_ms = 0;
    picocalc_copy_disk_label(picocalc_operator_disks[0].name,
                             sizeof(picocalc_operator_disks[0].name), disk0_path);

    picocalc_operator_disks[1].present = disk1_present;
    picocalc_operator_disks[1].write_protected = disk1_write_protected;
    picocalc_operator_disks[1].active_until_ms = 0;
    picocalc_copy_disk_label(picocalc_operator_disks[1].name,
                             sizeof(picocalc_operator_disks[1].name), disk1_path);

    picocalc_operator_enabled = true;
    picocalc_render_operator_panel();
}

void platform_status_refresh_operator(uint16_t pc, float clock_mhz, bool audio_enabled)
{
    if (!picocalc_operator_enabled) {
        return;
    }

    picocalc_operator_last_pc = pc;
    picocalc_operator_last_clock_mhz = clock_mhz;
    picocalc_operator_last_audio_enabled = audio_enabled;
    picocalc_render_operator_panel();
}

void platform_set_disk_led(int drive, int on_off)
{
    if (drive < 0) {
        int i;
        for (i = 0; i < 2; ++i) {
            picocalc_operator_disks[i].active_until_ms = 0;
        }
        picocalc_render_operator_panel();
        return;
    }

    if (drive < 0 || drive >= 2) {
        return;
    }

    if (on_off) {
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        bool was_active = now_ms < picocalc_operator_disks[drive].active_until_ms;
        picocalc_operator_disks[drive].active_until_ms =
            now_ms + PICOCALC_DISK_ACTIVITY_MS;
        if (was_active) {
            return;
        }
    } else {
        picocalc_operator_disks[drive].active_until_ms = 0;
    }
    picocalc_render_operator_panel();
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
    fat32_file_t file;
    fat32_error_t result = FAT32_ERROR_INIT_FAILED;
    int attempt;

    if (path == NULL || path[0] == '\0') {
        picocalc_last_file_error = FAT32_ERROR_INVALID_PATH;
        return false;
    }

    for (attempt = 0; attempt < 8; ++attempt) {
        result = fat32_open(&file, path);
        if (result == FAT32_OK) {
            fat32_close(&file);
            picocalc_last_file_error = FAT32_OK;
            return true;
        }

        picocalc_last_file_error = result;
        if (result == FAT32_ERROR_FILE_NOT_FOUND ||
            result == FAT32_ERROR_DIR_NOT_FOUND ||
            result == FAT32_ERROR_INVALID_PATH ||
            result == FAT32_ERROR_NOT_A_FILE ||
            result == FAT32_ERROR_NOT_A_DIRECTORY) {
            break;
        }
        sleep_ms(250);
    }

    return false;
}

static int picocalc_ascii_tolower(int ch)
{
    if (ch >= 'A' && ch <= 'Z') {
        return ch + ('a' - 'A');
    }

    return ch;
}

static bool picocalc_disk_image_extension_allowed(const char *name)
{
    const char *dot;
    char ext[5];
    size_t i;

    if (name == NULL || name[0] == '\0' || name[0] == '.') {
        return false;
    }

    dot = strrchr(name, '.');
    if (dot == NULL || strlen(dot) != 4) {
        return false;
    }

    for (i = 0; i < 4; ++i) {
        ext[i] = (char)picocalc_ascii_tolower((unsigned char)dot[i]);
    }
    ext[4] = '\0';

    return strcmp(ext, ".dsk") == 0 ||
           strcmp(ext, ".dmk") == 0 ||
           strcmp(ext, ".jv1") == 0 ||
           strcmp(ext, ".jv3") == 0;
}

static int picocalc_disk_image_compare(const void *left, const void *right)
{
    const platform_disk_image_t *left_image = left;
    const platform_disk_image_t *right_image = right;

    return strcasecmp(left_image->name, right_image->name);
}

int platform_list_disk_images(const char *dir_path, platform_disk_image_t *images, int max_images)
{
    fat32_file_t dir;
    fat32_entry_t entry;
    fat32_error_t result;
    int count = 0;
    int attempt;

    if (dir_path == NULL || images == NULL || max_images <= 0) {
        picocalc_last_file_error = FAT32_ERROR_INVALID_PARAMETER;
        return -1;
    }

    for (attempt = 0; attempt < 8; ++attempt) {
        result = fat32_open(&dir, dir_path);
        if (result == FAT32_OK) {
            break;
        }

        picocalc_last_file_error = result;
        if (result == FAT32_ERROR_FILE_NOT_FOUND ||
            result == FAT32_ERROR_DIR_NOT_FOUND ||
            result == FAT32_ERROR_INVALID_PATH ||
            result == FAT32_ERROR_NOT_A_FILE ||
            result == FAT32_ERROR_NOT_A_DIRECTORY) {
            return -1;
        }
        sleep_ms(250);
    }

    if (result != FAT32_OK) {
        picocalc_last_file_error = result;
        return -1;
    }

    if ((dir.attributes & FAT32_ATTR_DIRECTORY) == 0) {
        fat32_close(&dir);
        picocalc_last_file_error = FAT32_ERROR_NOT_A_DIRECTORY;
        return -1;
    }

    while (fat32_dir_read(&dir, &entry) == FAT32_OK && entry.filename[0] != '\0') {
        if ((entry.attr & FAT32_ATTR_DIRECTORY) == 0 &&
            picocalc_disk_image_extension_allowed(entry.filename)) {
            if (count < max_images) {
                strncpy(images[count].name, entry.filename, PLATFORM_DISK_IMAGE_NAME_MAX - 1);
                images[count].name[PLATFORM_DISK_IMAGE_NAME_MAX - 1] = '\0';
                count++;
            }
        }
    }

    fat32_close(&dir);
    qsort(images, (size_t)count, sizeof(images[0]), picocalc_disk_image_compare);
    picocalc_last_file_error = FAT32_OK;
    return count;
}

int platform_last_file_error_code(void)
{
    return (int)picocalc_last_file_error;
}

const char *platform_last_file_error(void)
{
    return fat32_error_string(picocalc_last_file_error);
}

int platform_sd_detect_state(void)
{
    return gpio_get(SD_DETECT) ? 1 : 0;
}

bool platform_sd_card_present(void)
{
    return sd_card_present();
}
