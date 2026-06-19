#include "platform.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "pico/stdlib.h"
#include "pico/stdio/driver.h"

#include "assets/generated/pet2001_logo.h"
#include "drivers/display.h"
#include "drivers/fat32.h"
#include "drivers/font.h"
#include "drivers/keyboard.h"
#include "drivers/lcd.h"
#include "drivers/picocalc.h"
#include "drivers/sdcard.h"

volatile bool user_interrupt = false;

#define PICOCALC_STATUS_CYAN RGB(0, 255, 255)
#define PICOCALC_PET_GREEN RGB(0, 255, 128)
#define PICOCALC_PET_CELL_WIDTH 8
#define PICOCALC_PET_CELL_HEIGHT 8
#define PICOCALC_PET_FONT_HEIGHT 10

static int picocalc_cols = 40;
static int picocalc_rows = 25;
static int picocalc_status_row = 25;
static int picocalc_status_rows;
static int picocalc_status_next_line;
static fat32_error_t picocalc_last_file_error = FAT32_OK;
static bool picocalc_platform_initialised;

static int picocalc_pet_screen_pixel_height(void)
{
    return picocalc_rows * PICOCALC_PET_CELL_HEIGHT;
}

static void picocalc_draw_pet2001_logo(void)
{
    int y = picocalc_pet_screen_pixel_height();

    if (picocalc_cols != 40 || picocalc_rows != 25) {
        return;
    }
    if (y + PET2001_LOGO_HEIGHT > HEIGHT) {
        return;
    }

    lcd_blit(pet2001_logo_rgb565, 0, (uint16_t)y,
             PET2001_LOGO_WIDTH, PET2001_LOGO_HEIGHT);
}

static void picocalc_clear_line(int row)
{
    int max_col = lcd_get_columns();

    if (row < 0 || row >= ROWS || max_col <= 0) {
        return;
    }

    lcd_erase_line((uint8_t)row, 0, (uint8_t)(max_col - 1));
}

static void picocalc_write_line(int row, const char *text, uint16_t foreground)
{
    int max_col = lcd_get_columns();
    int col;

    if (text == NULL || row < 0 || row >= ROWS || max_col <= 0) {
        return;
    }

    lcd_set_foreground(foreground);
    lcd_set_background(BACKGROUND);
    picocalc_clear_line(row);
    for (col = 0; col < max_col && text[col] != '\0'; ++col) {
        lcd_putc((uint8_t)col, (uint8_t)row, (uint8_t)text[col]);
    }
}

static void picocalc_draw_font_cell8(int col, int row, uint8_t ch, uint8_t mode)
{
    static uint16_t pixels[PICOCALC_PET_CELL_WIDTH * PICOCALC_PET_CELL_HEIGHT]
        __attribute__((aligned(4)));
    bool reverse = (mode & PLATFORM_CELL_CURSOR) != 0;
    bool underscore = (mode & PLATFORM_CELL_UNDERSCORE) != 0;
    const uint8_t *glyph = &font_5x10.glyphs[ch * PICOCALC_PET_FONT_HEIGHT];
    int pixel_row;
    int pixel_col;

    for (pixel_row = 0; pixel_row < PICOCALC_PET_CELL_HEIGHT; ++pixel_row) {
        uint8_t bits = glyph[pixel_row];

        if (underscore && pixel_row == PICOCALC_PET_CELL_HEIGHT - 1) {
            bits = 0x1F;
        }
        for (pixel_col = 0; pixel_col < PICOCALC_PET_CELL_WIDTH; ++pixel_col) {
            bool on = pixel_col < 5 &&
                      (bits & (uint8_t)(0x10u >> pixel_col)) != 0;

            if (reverse) {
                on = !on;
            }
            pixels[pixel_row * PICOCALC_PET_CELL_WIDTH + pixel_col] =
                on ? PICOCALC_PET_GREEN : BACKGROUND;
        }
    }

    lcd_blit(pixels,
             (uint16_t)(col * PICOCALC_PET_CELL_WIDTH),
             (uint16_t)(row * PICOCALC_PET_CELL_HEIGHT),
             PICOCALC_PET_CELL_WIDTH,
             PICOCALC_PET_CELL_HEIGHT);
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
    lcd_set_foreground(PICOCALC_PET_GREEN);
    lcd_set_background(BACKGROUND);
    lcd_enable_cursor(false);
    lcd_set_underscore(false);
    lcd_define_scrolling(0, 0);
    lcd_scroll_reset();
    sleep_ms(250);
    lcd_clear_screen();
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
    case KEY_F5:
        mapped = PLATFORM_KEY_F5;
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
    picocalc_cols = cols;
    picocalc_rows = rows;
    picocalc_status_row = rows;
    if (cols == 40 && rows == 25) {
        picocalc_status_row = (picocalc_pet_screen_pixel_height() +
                               PET2001_LOGO_HEIGHT +
                               PICOCALC_PET_FONT_HEIGHT - 1) /
                              PICOCALC_PET_FONT_HEIGHT;
    }
    picocalc_status_rows = ROWS - picocalc_status_row;
    if (picocalc_status_rows < 0) {
        picocalc_status_rows = 0;
    }
    picocalc_status_next_line = 0;

    lcd_set_foreground(PICOCALC_PET_GREEN);
    lcd_set_background(BACKGROUND);
    lcd_clear_screen();
    picocalc_draw_pet2001_logo();
}

void platform_screen_write_cell(int col, int row, uint8_t ch, uint8_t mode)
{
    bool underscore;

    if (col < 0 || col >= picocalc_cols || row < 0 || row >= picocalc_rows) {
        return;
    }
    if (col >= lcd_get_columns() || row >= ROWS) {
        return;
    }

    if (picocalc_cols == 40) {
        picocalc_draw_font_cell8(col, row, ch ? ch : ' ', mode);
    } else {
        lcd_set_foreground(PICOCALC_PET_GREEN);
        lcd_set_background(BACKGROUND);
        underscore = (mode & PLATFORM_CELL_UNDERSCORE) != 0;
        lcd_set_underscore(underscore);
        lcd_putc((uint8_t)col, (uint8_t)row, ch ? ch : ' ');
        if (underscore) {
            lcd_set_underscore(false);
        }
    }
}

void platform_screen_write_glyph8(int col, int row, const uint8_t glyph[8], uint8_t mode)
{
    static uint16_t pixels[PICOCALC_PET_CELL_WIDTH * PICOCALC_PET_CELL_HEIGHT]
        __attribute__((aligned(4)));
    bool reverse = (mode & PLATFORM_CELL_CURSOR) != 0;
    bool underscore = (mode & PLATFORM_CELL_UNDERSCORE) != 0;
    uint16_t foreground = PICOCALC_PET_GREEN;
    uint16_t background = BACKGROUND;
    int pixel_row;
    int pixel_col;

    if (glyph == NULL || col < 0 || col >= picocalc_cols || row < 0 || row >= picocalc_rows) {
        return;
    }

    for (pixel_row = 0; pixel_row < PICOCALC_PET_CELL_HEIGHT; ++pixel_row) {
        uint8_t bits = pixel_row < 8 ? glyph[pixel_row] : 0x00;

        if (underscore && pixel_row == PICOCALC_PET_CELL_HEIGHT - 1) {
            bits = 0xFF;
        }
        for (pixel_col = 0; pixel_col < PICOCALC_PET_CELL_WIDTH; ++pixel_col) {
            bool on = (bits & (uint8_t)(0x80u >> pixel_col)) != 0;

            if (reverse) {
                on = !on;
            }
            pixels[pixel_row * PICOCALC_PET_CELL_WIDTH + pixel_col] =
                on ? foreground : background;
        }
    }

    lcd_blit(pixels,
             (uint16_t)(col * PICOCALC_PET_CELL_WIDTH),
             (uint16_t)(row * PICOCALC_PET_CELL_HEIGHT),
             PICOCALC_PET_CELL_WIDTH,
             PICOCALC_PET_CELL_HEIGHT);
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
}

void platform_status_puts(const char *text)
{
    int line;

    if (text == NULL || picocalc_status_rows <= 0) {
        return;
    }

    line = picocalc_status_row + picocalc_status_next_line;
    picocalc_write_line(line, text, PICOCALC_STATUS_CYAN);
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

    picocalc_write_line(picocalc_status_row + line, text, PICOCALC_STATUS_CYAN);
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

static bool picocalc_pet_file_extension_allowed(const char *name)
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

    return strcmp(ext, ".prg") == 0 || strcmp(ext, ".d64") == 0;
}

static int picocalc_file_compare(const void *left, const void *right)
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
            picocalc_pet_file_extension_allowed(entry.filename)) {
            if (count < max_images) {
                strncpy(images[count].name, entry.filename, PLATFORM_DISK_IMAGE_NAME_MAX - 1);
                images[count].name[PLATFORM_DISK_IMAGE_NAME_MAX - 1] = '\0';
                count++;
            }
        }
    }

    fat32_close(&dir);
    qsort(images, (size_t)count, sizeof(images[0]), picocalc_file_compare);
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
