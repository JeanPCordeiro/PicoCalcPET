#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "trs.h"
#include "trs_keyboard_internal.h"
#include "platform/platform.h"

char romfile1[FILENAME_MAX];
char romfile3[FILENAME_MAX];
char romfile4p[FILENAME_MAX];
char trs_hard_dir[FILENAME_MAX];
char trs_cass_dir[FILENAME_MAX];
char trs_disk_dir[FILENAME_MAX];
char trs_disk_set_dir[FILENAME_MAX];
char trs_state_dir[FILENAME_MAX];
char trs_printer_dir[FILENAME_MAX];
char trs_cmd_file[FILENAME_MAX];
char trs_config_file[FILENAME_MAX];
char trs_state_file[FILENAME_MAX];

int foreground;
int background;
int gui_foreground;
int gui_background;
int fullscreen;
int trs_emu_mouse;
int trs_emtsafe;
int border_width;
int scale = 1;
int resize3;
int resize4;
int text80x24;
int trs_uart_switches;
int trs_show_led;
int trs_keypad_joystick;
int trs_charset1;
int trs_charset3;
int trs_charset4;
int trs_paused;
int trs_printer;
int trs_sound;

typedef struct {
    Uint8 ch;
} trs_cell_t;

static trs_cell_t trs_screen_cells[80 * 24];
static int trs_screen_cols = 64;
static int trs_screen_rows = 16;
static int trs_screen_mode_flags;
static int trs_screen_dirty;
static unsigned int trs_cursor_position = UINT_MAX;
static int trs_cursor_visible;
static int trs_cursor_start;
static int trs_cursor_end;
static int trs_m6845_raster = 12;
static int trs_scale_factor = 2;

static void trs_fill_screen(Uint8 ch)
{
    for (size_t i = 0; i < (sizeof(trs_screen_cells) / sizeof(trs_screen_cells[0])); ++i) {
        trs_screen_cells[i].ch = ch;
    }
}

static int trs_screen_total_cells(void)
{
    return trs_screen_cols * trs_screen_rows;
}

static void trs_render_cell(unsigned int position)
{
    int col;
    int row;
    Uint8 mode;

    if (position >= (sizeof(trs_screen_cells) / sizeof(trs_screen_cells[0]))) {
        return;
    }

    col = position % trs_screen_cols;
    row = position / trs_screen_cols;
    if (row >= trs_screen_rows) {
        return;
    }

    mode = (Uint8)trs_screen_mode_flags;
    if (trs_cursor_visible && position == trs_cursor_position) {
        mode |= PLATFORM_CELL_CURSOR;
    }

    platform_screen_write_cell(col, row, trs_screen_cells[position].ch, mode);
}

int trs_parse_command_line(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return 0;
}

int trs_write_config_file(const char *filename)
{
    (void)filename;
    return 0;
}

int trs_load_config_file(void)
{
    return 0;
}

void trs_screen_init(int resize)
{
    (void)resize;
    trs_screen_cols = text80x24 ? 80 : 64;
    trs_screen_rows = text80x24 ? 24 : 16;
    platform_screen_configure(trs_screen_cols, trs_screen_rows);
    trs_screen_refresh();
}

void trs_screen_reset(void)
{
    trs_fill_screen(' ');
    trs_cursor_position = UINT_MAX;
    trs_cursor_visible = 0;
    trs_cursor_start = 0;
    trs_cursor_end = trs_m6845_raster - 1;
    platform_screen_configure(trs_screen_cols, trs_screen_rows);
    trs_screen_refresh();
}

void trs_screen_write_char(unsigned int position, Uint8 character)
{
    if (position < (sizeof(trs_screen_cells) / sizeof(trs_screen_cells[0]))) {
        trs_screen_cells[position].ch = character;
        trs_render_cell(position);
        trs_screen_dirty = 1;
    }
}

void trs_screen_update(void)
{
    if (trs_screen_dirty) {
        platform_screen_flush();
        trs_screen_dirty = 0;
    }
}

void trs_screen_mode(int mode, int flag)
{
    int updated_mode;

    updated_mode = flag
        ? trs_screen_mode_flags | mode
        : trs_screen_mode_flags & ~mode;

    if (updated_mode == trs_screen_mode_flags) {
        return;
    }

    trs_screen_mode_flags = updated_mode;
    trs_screen_refresh();
}

void trs_screen_80x24(int flag)
{
    if (text80x24 == flag) {
        return;
    }

    text80x24 = flag;
    trs_fill_screen(' ');
    trs_screen_cols = flag ? 80 : 64;
    trs_screen_rows = flag ? 24 : 16;
    trs_cursor_position = UINT_MAX;
    trs_cursor_visible = 0;
    trs_screen_init(1);
}

void trs_screen_refresh(void)
{
    int total = trs_screen_cols * trs_screen_rows;

    for (int i = 0; i < total; ++i) {
        trs_render_cell((unsigned int)i);
    }
    platform_screen_flush();
    trs_screen_dirty = 0;
}

void trs_screen_caption(void)
{
}

void trs_sdl_init(void)
{
    platform_init();
    text80x24 = 0;
    trs_screen_cols = 64;
    trs_screen_rows = 16;
    trs_screen_mode_flags = 0;
    trs_cursor_position = UINT_MAX;
    trs_cursor_visible = 0;
    trs_cursor_start = 0;
    trs_cursor_end = trs_m6845_raster - 1;
    trs_fill_screen(' ');
    trs_screen_init(1);
}

void trs_disk_led(int drive, int on_off)
{
    platform_set_disk_led(drive, on_off);
}

void trs_hard_led(int drive, int on_off)
{
    platform_set_hard_led(drive, on_off);
}

void trs_turbo_led(void)
{
    platform_set_turbo_led(z80_state.keypress != 0);
}

void trs_get_event(int wait)
{
    int keycode;

    if (platform_poll_key(&keycode, wait)) {
        trs_key_event(keycode);
    }
}

void trs_exit(int confirm)
{
    fprintf(stderr, "trs_exit(%d)\n", confirm);
    exit(confirm ? EXIT_SUCCESS : EXIT_FAILURE);
}

void trs_printer_write(int value)
{
    (void)value;
}

int trs_printer_read(void)
{
    return 0;
}

int trs_printer_reset(void)
{
    return 0;
}

void trs_cassette_motor(int value)
{
    (void)value;
}

void trs_cassette_out(int value)
{
    (void)value;
}

int trs_cassette_in(void)
{
    return 0;
}

void trs_sound_out(int value)
{
    (void)value;
}

void trs_get_mouse_pos(int *x, int *y, unsigned int *buttons)
{
    if (x != NULL) {
        *x = 0;
    }
    if (y != NULL) {
        *y = 0;
    }
    if (buttons != NULL) {
        *buttons = 0;
    }
}

void trs_set_mouse_pos(int x, int y)
{
    (void)x;
    (void)y;
}

void m6845_cursor(unsigned int position, int start, int end, int visible)
{
    unsigned int previous_position = trs_cursor_position;
    int previous_visible = trs_cursor_visible;

    trs_cursor_position = position;
    trs_cursor_visible = visible && (position < (unsigned int)trs_screen_total_cells());
    trs_cursor_start = start;
    trs_cursor_end = end;

    if (previous_visible && previous_position < (unsigned int)trs_screen_total_cells()) {
        trs_render_cell(previous_position);
    }
    if (trs_cursor_visible) {
        trs_render_cell(trs_cursor_position);
    }
    trs_screen_dirty = 1;
}

void m6845_screen(int chars, int lines, int raster, int factor)
{
    int geometry_changed = 0;
    int next_cols = trs_screen_cols;
    int next_rows = trs_screen_rows;
    int next_text80x24 = text80x24;

    if (chars && chars != trs_screen_cols) {
        next_cols = chars;
        geometry_changed = 1;
    }

    if (lines && lines != trs_screen_rows) {
        next_rows = lines;
        geometry_changed = 1;
    }

    if (raster && raster != trs_m6845_raster) {
        trs_m6845_raster = raster;
    }

    if (factor && factor != trs_scale_factor) {
        trs_scale_factor = factor;
    }

    if (next_cols == 80 || next_rows == 24) {
        next_text80x24 = 1;
    } else if (next_cols == 64 || next_rows == 16) {
        next_text80x24 = 0;
    }

    if (!geometry_changed) {
        return;
    }

    text80x24 = next_text80x24;
    trs_screen_cols = text80x24 ? 80 : 64;
    trs_screen_rows = text80x24 ? 24 : 16;
    trs_screen_init(1);
}
