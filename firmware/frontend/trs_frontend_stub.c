#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "trs.h"
#include "trs_disk.h"
#include "trs_keyboard_internal.h"
#include "platform/platform.h"
#include "frontend/osd_menu.h"
#include "frontend/status_runtime.h"
#include "emu/picocalc_audio_bridge.h"

#ifndef PICOCALC_ENABLE_FDC_DIAG
#define PICOCALC_ENABLE_FDC_DIAG 0
#endif

#ifndef PICOCALC_ENABLE_DISK_FAULT_DIAG
#define PICOCALC_ENABLE_DISK_FAULT_DIAG 0
#endif

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
int trs_sound = 1;

typedef struct {
    Uint8 ch;
} trs_cell_t;

#define TRS_SCREEN_BUFFER_CELLS 2048

static trs_cell_t trs_screen_cells[TRS_SCREEN_BUFFER_CELLS];
static int trs_screen_cols = 64;
static int trs_screen_rows = 16;
static int trs_screen_chars = 1024;
static int trs_screen_mode_flags;
static int trs_screen_dirty;
static unsigned int trs_cursor_position = UINT_MAX;
static int trs_cursor_visible;
static int trs_cursor_start;
static int trs_cursor_end;
static int trs_m6845_raster = 12;
static int trs_scale_factor = 2;
static unsigned int trs_status_publish_tick;
static char trs_status_message[80] = "Ready";

static void trs_fill_screen(Uint8 ch)
{
    for (size_t i = 0; i < TRS_SCREEN_BUFFER_CELLS; ++i) {
        trs_screen_cells[i].ch = ch;
    }
}

static int trs_screen_total_cells(void)
{
    return trs_screen_chars;
}

static void trs_clear_hidden_cells(void)
{
    int i;

    if (trs_screen_chars < 0) {
        trs_screen_chars = 0;
    } else if (trs_screen_chars > TRS_SCREEN_BUFFER_CELLS) {
        trs_screen_chars = TRS_SCREEN_BUFFER_CELLS;
    }

    for (i = trs_screen_chars; i < TRS_SCREEN_BUFFER_CELLS; ++i) {
        trs_screen_cells[i].ch = ' ';
    }
}

static void trs_render_cell(unsigned int position)
{
    int col;
    int row;
    Uint8 mode;

    if (position >= (unsigned int)trs_screen_chars) {
        return;
    }

    col = position % trs_screen_cols;
    row = position / trs_screen_cols;
    if (row >= trs_screen_rows) {
        return;
    }

    mode = (Uint8)trs_screen_mode_flags;
    if (trs_cursor_visible && position == trs_cursor_position) {
        mode |= PLATFORM_CELL_UNDERSCORE;
    }

    platform_screen_write_cell(col, row, trs_screen_cells[position].ch, mode);
}

static const char *trs_status_leaf_name(const char *path)
{
    const char *leaf = path;
    const char *cursor;

    if (path == NULL || path[0] == '\0') {
        return "none";
    }

    for (cursor = path; *cursor != '\0'; ++cursor) {
        if (*cursor == '/' || *cursor == '\\') {
            leaf = cursor + 1;
        }
    }

    if (leaf[0] == '\0') {
        return path;
    }
    return leaf;
}

static const char *trs_status_rom_label(void)
{
    if (strncmp(romfile3, "embedded:", 9) == 0) {
        return "embedded";
    }
    if (romfile3[0] != '\0') {
        return "sd";
    }
    return "none";
}

static void trs_status_format_drive(char *dst, size_t dst_size, int drive)
{
    const char *path = trs_disk_getfilename(drive);
    const char *mode = "rw";

    if (dst == NULL || dst_size == 0) {
        return;
    }

    if (path == NULL || path[0] == '\0') {
        snprintf(dst, dst_size, "none");
        return;
    }

    if (trs_disk_getwriteprotect(drive)) {
        mode = "ro";
    }
    snprintf(dst, dst_size, "%s(%s)", trs_status_leaf_name(path), mode);
}

static void trs_status_publish(int force)
{
    char line0[80];
    char line1[80];
    char line2[80];
    char d0[26];
    char d1[26];

    if (!force) {
        trs_status_publish_tick++;
        if ((trs_status_publish_tick & 0x3Fu) != 0) {
            return;
        }
    }

    trs_status_format_drive(d0, sizeof(d0), 0);
    trs_status_format_drive(d1, sizeof(d1), 1);
    snprintf(line0, sizeof(line0), "SYS ROM:%s SD:%c AUD:%s F4=OSD",
             trs_status_rom_label(),
             platform_sd_card_present() ? 'Y' : 'N',
             trs_sound ? "on" : "off");
    snprintf(line1, sizeof(line1), "DRV D0:%s D1:%s", d0, d1);
    snprintf(line2, sizeof(line2), "MSG %s", trs_status_message);

    platform_status_write_line(0, line0);
    platform_status_write_line(1, line1);
    platform_status_write_line(2, line2);
}

void picocalc_status_set_message(const char *message)
{
    if (message == NULL || message[0] == '\0') {
        snprintf(trs_status_message, sizeof(trs_status_message), "Ready");
    } else {
        snprintf(trs_status_message, sizeof(trs_status_message), "%s", message);
    }
    trs_status_publish(1);
}

void picocalc_trs_diag_disk_event(Uint8 tag, Uint8 value, Uint8 status,
                                  int drive, int side, int density, unsigned int pc)
{
    if (tag == 'E' || tag == 'N' || tag == 'W' || tag == 'U') {
        char msg[80];
        snprintf(msg, sizeof(msg), "Disk %d %c cmd:%02X st:%02X",
                 drive, (char)tag, (unsigned int)value, (unsigned int)status);
        picocalc_status_set_message(msg);
    } else if (PICOCALC_ENABLE_FDC_DIAG) {
        char msg[80];
        snprintf(msg, sizeof(msg), "FDC %c d:%d/%d/%d pc:%04X",
                 (char)tag, drive, side, density, pc & 0xFFFFu);
        picocalc_status_set_message(msg);
    } else {
        (void)value;
        (void)status;
        (void)side;
        (void)density;
        (void)pc;
    }
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
    trs_screen_chars = trs_screen_cols * trs_screen_rows;
    if (trs_screen_chars > TRS_SCREEN_BUFFER_CELLS) {
        trs_screen_chars = TRS_SCREEN_BUFFER_CELLS;
    }
    trs_clear_hidden_cells();
    platform_screen_configure(trs_screen_cols, trs_screen_rows);
    trs_screen_refresh();
    trs_status_publish(1);
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
    trs_status_publish(1);
}

void trs_screen_write_char(unsigned int position, Uint8 character)
{
    if (position >= (unsigned int)trs_screen_chars) {
        trs_status_publish(1);
        return;
    }

    trs_screen_cells[position].ch = character;
    trs_render_cell(position);
    trs_status_publish(0);
    trs_screen_dirty = 1;
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
    trs_status_publish(1);
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
    trs_status_publish(0);
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
    trs_status_publish_tick = 0;
    snprintf(trs_status_message, sizeof(trs_status_message), "Ready");
    trs_fill_screen(' ');
    trs_screen_init(1);
    trs_status_publish(1);
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
        if (keycode == PLATFORM_KEY_F4) {
            osd_runtime_menu();
            return;
        }
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
    picocalc_audio_bridge_set_cassette_motor(value);
}

void trs_cassette_out(int value)
{
    picocalc_audio_bridge_cassette_out(value);
}

int trs_cassette_in(void)
{
    return picocalc_audio_bridge_cassette_in();
}

void trs_sound_out(int value)
{
    picocalc_audio_bridge_sound_out(value);
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
    trs_status_publish(0);
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
