#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "trs.h"
#include "z80.h"
#include "trs_keyboard_internal.h"
#include "platform/platform.h"
#include "emu/picocalc_reset_policy.h"

#ifdef PICOCALC_PLATFORM
#include "pico/time.h"
#include "drivers/audio.h"
#include "audio/sample_audio.h"
#include "disk_sfx_assets.h"
#endif

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
int trs_show_led = 1;
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
static unsigned int trs_diag_last_position = UINT_MAX;
static Uint8 trs_diag_last_character = ' ';
static Uint8 trs_diag_last_mode;
static unsigned int trs_diag_oob_position = UINT_MAX;
static unsigned int trs_diag_write_count;
static unsigned int trs_diag_refresh_count;
static unsigned int trs_diag_cursor_count;
static unsigned int trs_diag_oob_count;
static unsigned int trs_diag_publish_tick;
static char trs_diag_disk_fault_line[80];
#ifdef PICOCALC_PLATFORM
static int trs_audio_cassette_motor;
static int trs_audio_last_value;
static int trs_audio_have_value;
static tstate_t trs_audio_last_transition;
static unsigned int trs_audio_current_hz;
static uint32_t trs_audio_last_activity_ms;
static uint32_t trs_audio_last_update_ms;
static alarm_id_t trs_audio_silence_alarm = -1;
static int trs_disk_sfx_enabled;
static int trs_disk_sfx_mode;
static uint32_t trs_disk_sfx_active_until_ms;
static uint32_t trs_disk_sfx_last_pulse_ms;
static int trs_disk_sfx_last_drive = -1;
static alarm_id_t trs_disk_sfx_alarm = -1;

#define TRS_AUDIO_MIN_HZ 100u
#define TRS_AUDIO_MAX_HZ 2000u
#define TRS_AUDIO_IDLE_MS 35
#define TRS_AUDIO_UPDATE_MS 8
#define TRS_AUDIO_HZ_DEADBAND 8u
#define TRS_DISK_SFX_IDLE_MS 650u
#define TRS_DISK_SFX_MIN_PULSE_MS 160u

enum {
    TRS_DISK_SFX_IDLE = 0,
    TRS_DISK_SFX_MOTOR,
    TRS_DISK_SFX_PULSE
};

static void trs_audio_stop(void);
static int64_t trs_disk_sfx_stop_callback(alarm_id_t id, void *user_data);

#define TRS_DISK_SFX_STATUS (trs_disk_sfx_enabled != 0)

static void trs_disk_sfx_cancel(void)
{
    if (trs_disk_sfx_alarm >= 0) {
        cancel_alarm(trs_disk_sfx_alarm);
        trs_disk_sfx_alarm = -1;
    }
    if (trs_disk_sfx_mode != TRS_DISK_SFX_IDLE) {
        picocalc_sample_audio_stop();
        trs_disk_sfx_mode = TRS_DISK_SFX_IDLE;
    }
}

static void trs_disk_sfx_schedule_alarm(uint32_t delay_ms)
{
    if (trs_disk_sfx_alarm >= 0) {
        cancel_alarm(trs_disk_sfx_alarm);
        trs_disk_sfx_alarm = -1;
    }

    trs_disk_sfx_alarm = add_alarm_in_ms((int32_t)delay_ms,
                                         trs_disk_sfx_stop_callback,
                                         NULL, false);
    if (trs_disk_sfx_alarm < 0) {
        trs_disk_sfx_cancel();
    }
}

static uint32_t trs_disk_sfx_track_ms(void)
{
    return (uint32_t)((picocalc_disk_sfx_track_len * 1000u) /
                      PICOCALC_DISK_SFX_SAMPLE_RATE);
}

static void trs_disk_sfx_start_motor(int drive, uint32_t now_ms)
{
    uint32_t delay_ms = trs_disk_sfx_active_until_ms - now_ms;

    (void)drive;
    picocalc_sample_audio_play(picocalc_disk_sfx_spin,
                               picocalc_disk_sfx_spin_len,
                               PICOCALC_DISK_SFX_SAMPLE_RATE, true);
    trs_disk_sfx_mode = TRS_DISK_SFX_MOTOR;
    trs_disk_sfx_schedule_alarm(delay_ms > 0 ? delay_ms : 1);
}

static int64_t trs_disk_sfx_stop_callback(alarm_id_t id, void *user_data)
{
    uint32_t now_ms;
    uint32_t delay_ms;

    (void)user_data;
    if (trs_disk_sfx_alarm != id) {
        return 0;
    }

    trs_disk_sfx_alarm = -1;
    now_ms = to_ms_since_boot(get_absolute_time());
    if (trs_disk_sfx_mode == TRS_DISK_SFX_PULSE &&
        now_ms < trs_disk_sfx_active_until_ms) {
        trs_disk_sfx_start_motor(trs_disk_sfx_last_drive, now_ms);
        return 0;
    }

    if (trs_disk_sfx_mode == TRS_DISK_SFX_MOTOR &&
        now_ms < trs_disk_sfx_active_until_ms) {
        delay_ms = trs_disk_sfx_active_until_ms - now_ms;
        trs_disk_sfx_schedule_alarm(delay_ms > 0 ? delay_ms : 1);
        return 0;
    }

    if (trs_disk_sfx_mode != TRS_DISK_SFX_IDLE) {
        audio_stop();
        trs_disk_sfx_mode = TRS_DISK_SFX_IDLE;
    }
    return 0;
}

static void trs_disk_sfx_activity(int drive)
{
    uint32_t now_ms;
    uint32_t track_ms;

    if (!trs_sound || !trs_disk_sfx_enabled || drive < 0 || drive > 1) {
        return;
    }
    if (trs_audio_current_hz != 0) {
        return;
    }

    now_ms = to_ms_since_boot(get_absolute_time());
    trs_disk_sfx_active_until_ms = now_ms + TRS_DISK_SFX_IDLE_MS;
    trs_disk_sfx_last_drive = drive;

    if (now_ms - trs_disk_sfx_last_pulse_ms < TRS_DISK_SFX_MIN_PULSE_MS) {
        if (trs_disk_sfx_mode == TRS_DISK_SFX_IDLE) {
            trs_disk_sfx_start_motor(drive, now_ms);
        }
        return;
    }

    picocalc_sample_audio_play(picocalc_disk_sfx_track,
                               picocalc_disk_sfx_track_len,
                               PICOCALC_DISK_SFX_SAMPLE_RATE, false);
    trs_disk_sfx_mode = TRS_DISK_SFX_PULSE;
    trs_disk_sfx_last_pulse_ms = now_ms;
    track_ms = trs_disk_sfx_track_ms();
    trs_disk_sfx_schedule_alarm(track_ms > 0 ? track_ms : 1);
}

static void trs_disk_sfx_toggle(void)
{
    trs_disk_sfx_enabled = !trs_disk_sfx_enabled;
    if (!trs_disk_sfx_enabled) {
        trs_disk_sfx_cancel();
    }
    platform_status_puts(trs_disk_sfx_enabled ? "DSK SFX:on" : "DSK SFX:off");
    platform_status_refresh_operator((uint16_t)Z80_PC, z80_state.clockMHz,
                                     trs_sound != 0, TRS_DISK_SFX_STATUS);
}

static void trs_audio_toggle_sound(void)
{
    trs_sound = !trs_sound;
    if (!trs_sound) {
        trs_audio_stop();
    }
    platform_status_puts(trs_sound ? "Audio:on" : "Audio:off");
    platform_status_refresh_operator((uint16_t)Z80_PC, z80_state.clockMHz,
                                     trs_sound != 0, TRS_DISK_SFX_STATUS);
}

static int64_t trs_audio_silence_callback(alarm_id_t id, void *user_data)
{
    uint32_t now_ms;
    uint32_t idle_ms;

    (void)user_data;
    if (trs_audio_silence_alarm != id) {
        return 0;
    }

    now_ms = to_ms_since_boot(get_absolute_time());
    idle_ms = now_ms - trs_audio_last_activity_ms;
    if (idle_ms < TRS_AUDIO_IDLE_MS) {
        return (int64_t)(TRS_AUDIO_IDLE_MS - idle_ms) * 1000;
    }

    audio_stop();
    trs_audio_silence_alarm = -1;
    trs_audio_current_hz = 0;
    return 0;
}

static void trs_audio_arm_silence_timer(void)
{
    if (trs_audio_silence_alarm < 0) {
        trs_audio_silence_alarm = add_alarm_in_ms(TRS_AUDIO_IDLE_MS,
                                                  trs_audio_silence_callback,
                                                  NULL, false);
    }
}

static void trs_audio_stop(void)
{
    trs_disk_sfx_cancel();
    if (trs_audio_silence_alarm >= 0) {
        cancel_alarm(trs_audio_silence_alarm);
        trs_audio_silence_alarm = -1;
    }
    audio_stop();
    trs_audio_have_value = 0;
    trs_audio_current_hz = 0;
    trs_audio_last_update_ms = 0;
}

static int trs_audio_frequency_changed(unsigned int hz)
{
    unsigned int delta;

    if (trs_audio_current_hz == 0) {
        return 1;
    }

    delta = (trs_audio_current_hz > hz)
        ? trs_audio_current_hz - hz
        : hz - trs_audio_current_hz;
    return delta >= TRS_AUDIO_HZ_DEADBAND;
}

static void trs_audio_transition(int value)
{
    tstate_t now;
    tstate_t delta_t;
    uint32_t now_ms;
    unsigned int hz;

    if (!trs_sound || trs_audio_cassette_motor) {
        trs_audio_stop();
        return;
    }

    trs_disk_sfx_cancel();

    value &= 0x03;
    now = z80_state.t_count;
    if (!trs_audio_have_value) {
        trs_audio_last_value = value;
        trs_audio_last_transition = now;
        trs_audio_have_value = (value != 0);
        return;
    }

    if (value == trs_audio_last_value) {
        return;
    }

    delta_t = now - trs_audio_last_transition;
    trs_audio_last_value = value;
    trs_audio_last_transition = now;

    if (delta_t == 0 || z80_state.clockMHz <= 0.0f) {
        return;
    }

    hz = (unsigned int)((z80_state.clockMHz * 1000000.0f) /
                        (2.0f * (float)delta_t) + 0.5f);
    if (hz < TRS_AUDIO_MIN_HZ || hz > TRS_AUDIO_MAX_HZ) {
        return;
    }

    now_ms = to_ms_since_boot(get_absolute_time());
    trs_audio_last_activity_ms = now_ms;
    trs_audio_arm_silence_timer();

    if (trs_audio_current_hz != 0) {
        if (now_ms - trs_audio_last_update_ms < TRS_AUDIO_UPDATE_MS) {
            return;
        }
        if (!trs_audio_frequency_changed(hz)) {
            return;
        }
    }

    audio_play_sound(hz, hz);
    trs_audio_current_hz = hz;
    trs_audio_last_update_ms = now_ms;
}
#else
#define TRS_DISK_SFX_STATUS false
#endif
#if PICOCALC_ENABLE_FDC_DIAG
static Uint8 trs_diag_fdc_tag = '?';
static Uint8 trs_diag_fdc_value;
static Uint8 trs_diag_fdc_status;
static int trs_diag_fdc_drive;
static int trs_diag_fdc_side;
static int trs_diag_fdc_density;
static unsigned int trs_diag_fdc_pc;
static unsigned long trs_diag_fdc_select_count;
static unsigned long trs_diag_fdc_command_count;
static unsigned long trs_diag_fdc_status_count;
static unsigned long trs_diag_fdc_notrdy_count;
static unsigned long trs_diag_fdc_event_seq;
static unsigned long trs_diag_fdc_last_render_seq;
#endif

static void picocalc_press_trs_reset_button(void)
{
    platform_status_puts("BRK: TRS reset");
    clear_key_queue();
    trs_kb_reset();
    trs_reset(0);
    picocalc_apply_post_reset_policy();
}

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

static void trs_diag_publish(int force)
{
#if !PICOCALC_ENABLE_FDC_DIAG && !PICOCALC_ENABLE_DISK_FAULT_DIAG
    if (!force) {
        trs_diag_publish_tick++;
        if ((trs_diag_publish_tick & 0x3Fu) != 0) {
            return;
        }
    }
    platform_status_refresh_operator((uint16_t)Z80_PC, z80_state.clockMHz,
                                     trs_sound != 0, TRS_DISK_SFX_STATUS);
#else
    char line0[80];
    char line1[80];
    char line2[80];
    char posbuf[16];
    unsigned int total = (unsigned int)trs_screen_total_cells();
    int col = -1;
    int row = -1;
    Uint8 mode = trs_diag_last_mode;
#if !PICOCALC_ENABLE_FDC_DIAG
    char flag_expanded = (trs_screen_mode_flags & EXPANDED) ? 'E' : '.';
    char flag_inverse = (trs_screen_mode_flags & INVERSE) ? 'I' : '.';
    char flag_alternate = (trs_screen_mode_flags & ALTERNATE) ? 'A' : '.';
    char flag_reverse = (trs_screen_mode_flags & REVERSE) ? 'R' : '.';
#endif

    if (!force) {
        trs_diag_publish_tick++;
        if ((trs_diag_publish_tick & 0x3Fu) != 0) {
            return;
        }
    }

    if (trs_diag_last_position < total && trs_screen_cols > 0) {
        col = (int)(trs_diag_last_position % (unsigned int)trs_screen_cols);
        row = (int)(trs_diag_last_position / (unsigned int)trs_screen_cols);
    }

    if (row >= 0 && col >= 0) {
        snprintf(posbuf, sizeof(posbuf), "%02d,%02d", row, col);
    } else {
        snprintf(posbuf, sizeof(posbuf), "--,--");
    }

    snprintf(line0, sizeof(line0), "D0 W:%lu P:%04u RC:%s CH:%02X M:%02X",
             (unsigned long)trs_diag_write_count,
             (unsigned int)trs_diag_last_position,
             posbuf,
             (unsigned int)trs_diag_last_character,
             (unsigned int)mode);
    snprintf(line1, sizeof(line1), "D1 CUR:%04u V:%d S:%d E:%d C:%lu",
             (unsigned int)trs_cursor_position,
             trs_cursor_visible,
             trs_cursor_start,
             trs_cursor_end,
             (unsigned long)trs_diag_cursor_count);
#if PICOCALC_ENABLE_FDC_DIAG
    snprintf(line2, sizeof(line2), "D2 F:%c V:%02X S:%02X D:%d/%d/%d P:%04X C:%lu N:%lu",
             (char)trs_diag_fdc_tag,
             (unsigned int)trs_diag_fdc_value,
             (unsigned int)trs_diag_fdc_status,
             trs_diag_fdc_drive,
             trs_diag_fdc_side,
             trs_diag_fdc_density,
             (unsigned int)(trs_diag_fdc_pc & 0xFFFFu),
             (unsigned long)trs_diag_fdc_command_count,
             (unsigned long)trs_diag_fdc_notrdy_count);
#else
#if PICOCALC_ENABLE_DISK_FAULT_DIAG
    if (trs_diag_disk_fault_line[0] != '\0') {
        snprintf(line2, sizeof(line2), "%s", trs_diag_disk_fault_line);
    } else {
        snprintf(line2, sizeof(line2), "D2 R:%lu O:%lu@%04u F:%c%c%c%c",
                 (unsigned long)trs_diag_refresh_count,
                 (unsigned long)trs_diag_oob_count,
                 (unsigned int)trs_diag_oob_position,
                 flag_expanded, flag_inverse, flag_alternate, flag_reverse);
    }
#else
    snprintf(line2, sizeof(line2), "D2 R:%lu O:%lu@%04u F:%c%c%c%c",
             (unsigned long)trs_diag_refresh_count,
             (unsigned long)trs_diag_oob_count,
             (unsigned int)trs_diag_oob_position,
             flag_expanded, flag_inverse, flag_alternate, flag_reverse);
#endif
#endif

    platform_status_write_line(0, line0);
    platform_status_write_line(1, line1);
    platform_status_write_line(2, line2);
#endif
}

#if PICOCALC_ENABLE_FDC_DIAG
static void trs_diag_publish_fdc_line(void)
{
    char line2[80];

    snprintf(line2, sizeof(line2), "D2 F:%c V:%02X S:%02X D:%d/%d/%d P:%04X C:%lu N:%lu",
             (char)trs_diag_fdc_tag,
             (unsigned int)trs_diag_fdc_value,
             (unsigned int)trs_diag_fdc_status,
             trs_diag_fdc_drive,
             trs_diag_fdc_side,
             trs_diag_fdc_density,
             (unsigned int)(trs_diag_fdc_pc & 0xFFFFu),
             (unsigned long)trs_diag_fdc_command_count,
             (unsigned long)trs_diag_fdc_notrdy_count);
    platform_status_write_line(2, line2);
}
#endif

void picocalc_trs_diag_disk_event(Uint8 tag, Uint8 value, Uint8 status,
                                  int drive, int side, int density, unsigned int pc)
{
#if PICOCALC_ENABLE_FDC_DIAG
    int should_render = 0;

    trs_diag_fdc_tag = tag;
    trs_diag_fdc_value = value;
    trs_diag_fdc_status = status;
    trs_diag_fdc_drive = drive;
    trs_diag_fdc_side = side;
    trs_diag_fdc_density = density;
    trs_diag_fdc_pc = pc;
    trs_diag_fdc_event_seq++;

    if (tag == 'S') {
        trs_diag_fdc_select_count++;
    } else if (tag == 'C') {
        trs_diag_fdc_command_count++;
    } else if (tag == 'N') {
        trs_diag_fdc_notrdy_count++;
        should_render = 1;
    }

    if (!should_render) {
        if ((trs_diag_fdc_event_seq - trs_diag_fdc_last_render_seq) >= 128ul) {
            should_render = 1;
        }
    }

    if (should_render) {
        trs_diag_fdc_last_render_seq = trs_diag_fdc_event_seq;
        trs_diag_publish_fdc_line();
    }
#else
#if PICOCALC_ENABLE_DISK_FAULT_DIAG
    int keep_existing_error = 0;

    if (trs_diag_disk_fault_line[0] == 'D' &&
        trs_diag_disk_fault_line[1] == '2' &&
        trs_diag_disk_fault_line[2] == ' ' &&
        trs_diag_disk_fault_line[3] == 'E' &&
        tag != 'E') {
        keep_existing_error = 1;
    }

    if (keep_existing_error) {
        return;
    }

    if (tag == 'W' || tag == 'N' || tag == 'U' || tag == 'E') {
        snprintf(trs_diag_disk_fault_line, sizeof(trs_diag_disk_fault_line),
                 "D2 %c CMD:%02X ST:%02X D:%d/%d/%d PC:%04X",
                 (char)tag,
                 (unsigned int)value,
                 (unsigned int)status,
                 drive,
                 side,
                 density,
                 pc & 0xFFFFu);
        platform_status_write_line(2, trs_diag_disk_fault_line);
    }
#else
    (void)tag;
    (void)value;
    (void)status;
    (void)drive;
    (void)side;
    (void)density;
    (void)pc;
#endif
#endif
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
    trs_diag_disk_fault_line[0] = '\0';
    trs_clear_hidden_cells();
    platform_screen_configure(trs_screen_cols, trs_screen_rows);
    trs_screen_refresh();
    trs_diag_publish(1);
}

void trs_screen_reset(void)
{
    trs_fill_screen(' ');
    trs_diag_disk_fault_line[0] = '\0';
    trs_cursor_position = UINT_MAX;
    trs_cursor_visible = 0;
    trs_cursor_start = 0;
    trs_cursor_end = trs_m6845_raster - 1;
    platform_screen_configure(trs_screen_cols, trs_screen_rows);
    trs_screen_refresh();
    trs_diag_publish(1);
}

void trs_screen_write_char(unsigned int position, Uint8 character)
{
    if (position >= (unsigned int)trs_screen_chars) {
        trs_diag_oob_count++;
        trs_diag_oob_position = position;
        trs_diag_publish(1);
        return;
    }

    trs_screen_cells[position].ch = character;
    trs_render_cell(position);
    trs_diag_last_position = position;
    trs_diag_last_character = character;
    trs_diag_last_mode = (Uint8)trs_screen_mode_flags;
    if (trs_cursor_visible && position == trs_cursor_position) {
        trs_diag_last_mode = (Uint8)(trs_diag_last_mode | PLATFORM_CELL_UNDERSCORE);
    }
    trs_diag_write_count++;
    trs_diag_publish(0);
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
    trs_diag_last_mode = (Uint8)trs_screen_mode_flags;
    trs_diag_publish(1);
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
    trs_diag_refresh_count++;
    trs_diag_publish(0);
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
    trs_diag_last_position = UINT_MAX;
    trs_diag_last_character = ' ';
    trs_diag_last_mode = 0;
    trs_diag_oob_position = UINT_MAX;
    trs_diag_write_count = 0;
    trs_diag_refresh_count = 0;
    trs_diag_cursor_count = 0;
    trs_diag_oob_count = 0;
    trs_diag_publish_tick = 0;
#if PICOCALC_ENABLE_FDC_DIAG
    trs_diag_fdc_tag = '?';
    trs_diag_fdc_value = 0;
    trs_diag_fdc_status = 0;
    trs_diag_fdc_drive = -1;
    trs_diag_fdc_side = 0;
    trs_diag_fdc_density = 0;
    trs_diag_fdc_pc = 0;
    trs_diag_fdc_select_count = 0;
    trs_diag_fdc_command_count = 0;
    trs_diag_fdc_status_count = 0;
    trs_diag_fdc_notrdy_count = 0;
    trs_diag_fdc_event_seq = 0;
    trs_diag_fdc_last_render_seq = 0;
#endif
    trs_fill_screen(' ');
    trs_screen_init(1);
    trs_diag_publish(1);
}

void trs_disk_led(int drive, int on_off)
{
    platform_set_disk_led(drive, on_off);
#ifdef PICOCALC_PLATFORM
    if (on_off) {
        trs_disk_sfx_activity(drive);
    }
#endif
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
        if (keycode == PLATFORM_KEY_BREAK) {
            picocalc_press_trs_reset_button();
            return;
        }
#ifdef PICOCALC_PLATFORM
        if (keycode == PLATFORM_KEY_F5) {
            trs_audio_toggle_sound();
            return;
        }
        if (keycode == PLATFORM_KEY_F4) {
            trs_disk_sfx_toggle();
            return;
        }
#endif
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
#ifdef PICOCALC_PLATFORM
    trs_audio_cassette_motor = value ? 1 : 0;
    if (trs_audio_cassette_motor) {
        trs_audio_stop();
    }
#else
    (void)value;
#endif
}

void trs_cassette_out(int value)
{
#ifdef PICOCALC_PLATFORM
    trs_audio_transition(value);
#else
    (void)value;
#endif
}

int trs_cassette_in(void)
{
    return 0;
}

void trs_sound_out(int value)
{
#ifdef PICOCALC_PLATFORM
    trs_audio_transition(value ? 1 : 2);
#else
    (void)value;
#endif
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
    trs_diag_cursor_count++;

    if (previous_visible && previous_position < (unsigned int)trs_screen_total_cells()) {
        trs_render_cell(previous_position);
    }
    if (trs_cursor_visible) {
        trs_render_cell(trs_cursor_position);
    }
    trs_diag_publish(0);
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
