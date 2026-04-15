#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include "trs.h"
#include "trs_disk.h"
#include "trs_memory.h"
#include "error.h"
#include "picocalc_reset_policy.h"

const char *program_name = "picocalc_trs_scaffold";
volatile bool user_interrupt;

int stringy;
int trs_io_debug_flags;
int grafyx_microlabs;
int cassette_default_sample_rate;

void do_emt_resetdisk(void) {}
void trs_cassette_reset(int poweron) { (void)poweron; }
void trs_uart_init(void) {}
void grafyx_write_mode(int value) { (void)value; }
void grafyx_m3_reset(void) {}
void hrg_onoff(int enable) { (void)enable; }
void m6845_text(int onoff) { (void)onoff; }
void eg3210_char(int character, int scanline, int byte) { (void)character; (void)scanline; (void)byte; }
void trs_cassette_update(int dummy) { (void)dummy; }
void trs_orch90_flush(int dummy) { (void)dummy; }
void trs_orch90_out(int chan, int value) { (void)chan; (void)value; }
void assert_state_void(int dummy) { (void)dummy; }
void transition_out(int dummy) { (void)dummy; }
void trs_cassette_kickoff(int dummy) { (void)dummy; }
void trs_uart_set_avail(int dummy) { (void)dummy; }
void trs_uart_set_empty(int dummy) { (void)dummy; }
void genie3s_char(int character, int scanline, int byte) { (void)character; (void)scanline; (void)byte; }
void genie3s_hrg(int value) { (void)value; }
void genie3s_hrg_write(unsigned int position, int byte) { (void)position; (void)byte; }
Uint8 genie3s_hrg_read(unsigned int position) { (void)position; return 0; }
void grafyx_write_x(int value) { (void)value; }
void grafyx_write_y(int value) { (void)value; }
void grafyx_write_data(int value) { (void)value; }
int grafyx_read_data(void) { return 0; }
int grafyx_read_mode(void) { return 0; }
void grafyx_write_xoffset(int value) { (void)value; }
void grafyx_write_yoffset(int value) { (void)value; }
void grafyx_write_overlay(int value) { (void)value; }
void grafyx_m3_write_mode(int value) { (void)value; }
Uint8 grafyx_m3_read_byte(unsigned int position)
{
    return mem_video_page_read((int)VIDEO_START + (int)position);
}

int grafyx_m3_write_byte(unsigned int position, int value)
{
    (void)position;
    (void)value;
    return 0;
}
void hrg_write_data(int address, int data) { (void)address; (void)data; }
int hrg_read_data(int address) { (void)address; return 0; }
int lowe_le18;
int lowe_le18_read(void) { return 0; }
void lowe_le18_write_data(int value) { (void)value; }
void lowe_le18_write_control(int value) { (void)value; }
void lnw80_screen_write_char(int position, int value) { (void)position; (void)value; }
void trs_debug(void) {}
void trs_hard_debug(void) {}
void trs_timer(int on_off) { (void)on_off; }
void trs_keyboard_save(FILE *file) { (void)file; }
void trs_keyboard_load(FILE *file) { (void)file; }
void trs_set_keypad_joystick(void) {}
void trs_open_joystick(void) {}
void trs_joy_button_down(void) {}
void trs_joy_button_up(void) {}
void trs_joy_hat(Uint8 value) { (void)value; }
void trs_joy_axis(Uint8 axis, short value, int bounce) { (void)axis; (void)value; (void)bounce; }
void trs_main_save(FILE *file) { (void)file; }
void trs_main_load(FILE *file) { (void)file; }
void trs_cassette_save(FILE *file) { (void)file; }
void trs_cassette_load(FILE *file) { (void)file; }
void trs_hard_save(FILE *file) { (void)file; }
void trs_hard_load(FILE *file) { (void)file; }
void trs_stringy_save(FILE *file) { (void)file; }
void trs_stringy_load(FILE *file) { (void)file; }
void trs_uart_save(FILE *file) { (void)file; }
void trs_uart_load(FILE *file) { (void)file; }
void trs_imp_exp_save(FILE *file) { (void)file; }
void trs_imp_exp_load(FILE *file) { (void)file; }
static void emt_set_error(int errnum)
{
    Z80_A = (Uint8)errnum;
    Z80_F &= ~ZERO_MASK;
}

static bool emt_is_cmd_s(const char *cmd)
{
    size_t i;

    if (cmd == NULL) {
        return false;
    }

    i = 0;
    while (cmd[i] == ' ' || cmd[i] == '\t') {
        ++i;
    }

    if (cmd[i] != 'S' && cmd[i] != 's') {
        return false;
    }
    ++i;

    while (cmd[i] == ' ' || cmd[i] == '\t') {
        ++i;
    }

    return cmd[i] == '\0';
}

void do_emt_system(void)
{
    const char *cmd = (const char *)mem_pointer(Z80_HL, 0);

    /*
     * Pico firmware intentionally does not expose host-shell execution.
     * For CMD"S" compatibility, interpret a bare S as "press reset button".
     */
    if (emt_is_cmd_s(cmd)) {
        trs_reset(0);
        picocalc_apply_post_reset_policy();
    }

    Z80_A = 0;
    Z80_F |= ZERO_MASK;
    Z80_BC = 0;
}

void do_emt_mouse(void)
{
    emt_set_error(ENOSYS);
}

void do_emt_getddir(void)
{
    emt_set_error(ENOSYS);
    Z80_BC = 0xFFFF;
}

void do_emt_setddir(void)
{
    emt_set_error(ENOSYS);
}

void do_emt_open(void)
{
    emt_set_error(ENOSYS);
    Z80_DE = 0xFFFF;
}

void do_emt_close(void)
{
    emt_set_error(ENOSYS);
}

void do_emt_read(void)
{
    emt_set_error(ENOSYS);
    Z80_BC = 0xFFFF;
}

void do_emt_write(void)
{
    emt_set_error(ENOSYS);
    Z80_BC = 0xFFFF;
}

void do_emt_lseek(void)
{
    int i;

    emt_set_error(ENOSYS);
    for (i = 0; i < 8 && (Z80_HL + i) < 0x10000; ++i) {
        mem_write(Z80_HL + i, 0xFF);
    }
}

void do_emt_strerror(void)
{
    const char *msg;
    int size;

    if (Z80_HL + Z80_BC > 0x10000) {
        Z80_A = EFAULT;
        Z80_F &= ~ZERO_MASK;
        Z80_BC = 0xFFFF;
        return;
    }

    errno = 0;
    msg = strerror(Z80_A);
    size = (int)strlen(msg);

    if (errno != 0) {
        Z80_A = errno;
        Z80_F &= ~ZERO_MASK;
    } else if (Z80_BC < (unsigned int)(size + 2)) {
        Z80_A = ERANGE;
        Z80_F &= ~ZERO_MASK;
        size = (int)Z80_BC - 1;
    } else {
        Z80_A = 0;
        Z80_F |= ZERO_MASK;
    }

    if (mem_pointer(Z80_HL, 1) != NULL) {
        memcpy(mem_pointer(Z80_HL, 1), msg, (size_t)size);
        mem_write(Z80_HL + size++, '\r');
        mem_write(Z80_HL + size, '\0');
    }

    if (errno == 0) {
        Z80_BC = (Uint16)size;
    } else {
        Z80_BC = 0xFFFF;
    }
}

void do_emt_time(void)
{
    time_t now = time(0) + trs_timeoffset;

    if (Z80_A == 1) {
#if __alpha
        const struct tm *loctm = localtime(&now);
        now += loctm->tm_gmtoff;
#else
        const struct tm loctm = *(localtime(&now));
        const struct tm gmtm = *(gmtime(&now));
        const int daydiff = loctm.tm_mday - gmtm.tm_mday;

        now += (loctm.tm_sec - gmtm.tm_sec)
            + (loctm.tm_min - gmtm.tm_min) * 60
            + (loctm.tm_hour - gmtm.tm_hour) * 3600;

        switch (daydiff) {
        case 0:
        case 1:
        case -1:
            now += 24 * 3600 * daydiff;
            break;
        case 30:
        case 29:
        case 28:
        case 27:
            now -= 24 * 3600;
            break;
        case -30:
        case -29:
        case -28:
        case -27:
            now += 24 * 3600;
            break;
        default:
            error("trouble computing local time in emt_time");
            break;
        }
#endif
    } else if (Z80_A != 0) {
        error("unsupported function code %d to emt_time", Z80_A);
    }

    Z80_BC = (Uint16)((now >> 16) & 0xFFFF);
    Z80_DE = (Uint16)(now & 0xFFFF);
}

void do_emt_opendir(void)
{
    emt_set_error(ENOSYS);
    Z80_DE = 0xFFFF;
}

void do_emt_closedir(void)
{
    emt_set_error(ENOSYS);
}

void do_emt_readdir(void)
{
    emt_set_error(ENOSYS);
    Z80_BC = 0xFFFF;
}

void do_emt_chdir(void)
{
    emt_set_error(ENOSYS);
}

void do_emt_getcwd(void)
{
    emt_set_error(ENOSYS);
    Z80_BC = 0xFFFF;
}

void do_emt_misc(void)
{
    switch (Z80_A) {
    case 0:
        Z80_HL = 0;
        break;
    case 1:
        trs_exit(0);
        break;
    case 2:
        error("ZBX debugger disabled");
        break;
    case 3:
        trs_reset(0);
        picocalc_apply_post_reset_policy();
        break;
    case 4:
        Z80_HL = 0;
        break;
    case 5:
        Z80_HL = trs_model;
        break;
    case 6:
        Z80_HL = (Uint16)trs_disk_getsize(Z80_BC);
        break;
    case 7:
        trs_disk_setsize(Z80_BC, Z80_HL);
        break;
    case 10:
        Z80_HL = grafyx_microlabs ? 1 : 0;
        break;
    case 11:
        grafyx_microlabs = Z80_HL ? 1 : 0;
        break;
    case 12:
        Z80_HL = 0;
        Z80_BC = (Uint16)!turbo_mode;
        break;
    case 13:
        turbo_mode = !Z80_BC;
        trs_timer_mode(turbo_mode);
        break;
    case 14:
        Z80_HL = (Uint16)stretch_amount;
        break;
    case 15:
        stretch_amount = Z80_HL;
        break;
    case 16:
        Z80_HL = (Uint16)trs_disk_doubler;
        break;
    case 17:
        if (Z80_HL < 4) {
            trs_disk_doubler = Z80_HL;
        }
        break;
    case 18:
        Z80_HL = trs_sound ? 1 : 0;
        break;
    case 19:
        trs_sound = Z80_HL ? 1 : 0;
        break;
    case 20:
        Z80_HL = trs_disk_truedam ? 1 : 0;
        break;
    case 21:
        trs_disk_truedam = Z80_HL ? 1 : 0;
        break;
    case 24:
        Z80_HL = lowercase ? 1 : 0;
        break;
    case 25:
        lowercase = Z80_HL ? 1 : 0;
        break;
    default:
        error("unsupported function code %d to emt_misc", Z80_A);
        break;
    }
}

void do_emt_ftruncate(void)
{
    emt_set_error(ENOSYS);
}

void do_emt_opendisk(void)
{
    emt_set_error(ENOSYS);
    Z80_DE = 0xFFFF;
    Z80_BC = 1;
}

void do_emt_closedisk(void)
{
    emt_set_error(ENOSYS);
}
void trs_hard_init(int poweron) { (void)poweron; }
void trs_hard_attach(int drive, const char *diskname) { (void)drive; (void)diskname; }
void trs_hard_remove(int drive) { (void)drive; }
const char *trs_hard_getfilename(int unit) { (void)unit; return ""; }
int trs_hard_getwriteprotect(int unit) { (void)unit; return 1; }
void trs_hard_getgeometry(int unit, int *cyls, int *head, int *secs)
{
    (void)unit;
    if (cyls != NULL) *cyls = 0;
    if (head != NULL) *head = 0;
    if (secs != NULL) *secs = 0;
}
void trs_hard_out(int port, int value) { (void)port; (void)value; }
int trs_hard_in(int port) { (void)port; return 0xff; }
void stringy_init(void) {}
const char *stringy_get_name(int unit) { (void)unit; return ""; }
int stringy_get_writeprotect(int unit) { (void)unit; return 1; }
int stringy_insert(int unit, const char *name) { (void)unit; (void)name; return -1; }
void stringy_remove(int unit) { (void)unit; }
int stringy_create(const char *name) { (void)name; return -1; }
void stringy_out(int port, int value) { (void)port; (void)value; }
int stringy_in(int port) { (void)port; return 0xff; }
void trs_uart_reset_out(int value) { (void)value; }
void trs_uart_baud_out(int value) { (void)value; }
void trs_uart_control_out(int value) { (void)value; }
void trs_uart_data_out(int value) { (void)value; }
int trs_uart_modem_in(void) { return 0; }
int trs_uart_switches_in(void) { return 0; }
int trs_uart_status_in(void) { return 0; }
int trs_uart_data_in(void) { return 0; }
