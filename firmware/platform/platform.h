#ifndef PICOCALC_TRS_PLATFORM_H
#define PICOCALC_TRS_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

enum {
    PLATFORM_KEY_NONE = 0,
    PLATFORM_KEY_UP = 0x100,
    PLATFORM_KEY_DOWN,
    PLATFORM_KEY_LEFT,
    PLATFORM_KEY_RIGHT,
    PLATFORM_KEY_TAB,
    PLATFORM_KEY_ESC,
    PLATFORM_KEY_BREAK,
    PLATFORM_KEY_CLEAR,
    PLATFORM_KEY_BACKSPACE,
    PLATFORM_KEY_ENTER,
    PLATFORM_KEY_HOME,
    PLATFORM_KEY_END,
    PLATFORM_KEY_PAGE_UP,
    PLATFORM_KEY_PAGE_DOWN,
    PLATFORM_KEY_F1,
    PLATFORM_KEY_F2,
    PLATFORM_KEY_F3,
    PLATFORM_KEY_F4
};

enum {
    PLATFORM_CELL_CURSOR = 0x40,
    PLATFORM_CELL_UNDERSCORE = 0x80
};

void platform_init(void);
bool platform_poll_key(int *keycode, bool wait);
void platform_screen_configure(int cols, int rows);
void platform_screen_write_cell(int col, int row, uint8_t ch, uint8_t mode);
void platform_screen_flush(void);
void platform_status_clear(void);
void platform_status_puts(const char *text);
void platform_status_write_line(int line, const char *text);
void platform_set_disk_led(int drive, int on_off);
void platform_set_hard_led(int drive, int on_off);
void platform_set_turbo_led(bool enabled);
bool platform_file_exists(const char *path);
int platform_last_file_error_code(void);
const char *platform_last_file_error(void);
int platform_sd_detect_state(void);
bool platform_sd_card_present(void);

#endif
