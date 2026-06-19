#include "picocalc_vice_via_sound.h"

#include <stdbool.h>
#include <string.h>

#include "alarm.h"
#include "interrupt.h"
#include "lib.h"
#include "petsound.h"
#include "via.h"

extern CLOCK maincpu_clk;

static via_context_t sound_via;
static alarm_context_t sound_alarm_context;
static interrupt_cpu_status_t sound_int_status;
static unsigned int sound_pending_int[1];
static char *sound_int_name[1];
static bool sound_via_ready;
static bool sound_output_active;
static picocalc_vice_via_sound_output_fn sound_output_callback;
static void *sound_output_user_data;

static void notify_output(bool active, bool level, CLOCK cpu_clk)
{
    sound_output_active = active;
    if (sound_output_callback != NULL) {
        sound_output_callback(active, level, cpu_clk, sound_output_user_data);
    }
}

static void set_int(via_context_t *via_context, unsigned int int_num,
                    int value, CLOCK rclk)
{
    (void)via_context;
    interrupt_set_irq(&sound_int_status, int_num, value, rclk);
}

static void restore_int(via_context_t *via_context, unsigned int int_num,
                        int value)
{
    (void)via_context;
    interrupt_restore_irq(&sound_int_status, (int)int_num, value);
}

static void set_cb2(via_context_t *via_context, int state, int offset)
{
    CLOCK clk = *via_context->clk_ptr - offset;

    petsound_store_manual(state, clk);
    notify_output(sound_output_active, state != 0, clk);
}

static void set_line(via_context_t *via_context, int state)
{
    (void)via_context;
    (void)state;
}

static uint8_t store_pcr(via_context_t *via_context, uint8_t byte, uint16_t addr)
{
    (void)via_context;
    (void)addr;
    return byte;
}

static bool is_sr_shift_out_by_t2(uint8_t acr)
{
    uint8_t control = (uint8_t)(acr & VIA_ACR_SR_CONTROL);

    return control == VIA_ACR_SR_OUT_FREE_T2 || control == VIA_ACR_SR_OUT_T2;
}

static bool sound_active(uint8_t acr, uint8_t t2ll)
{
    return !is_sr_shift_out_by_t2(acr) || t2ll != 0;
}

static void store_acr(via_context_t *via_context, uint8_t byte)
{
    bool active = sound_active(byte, via_context->via[VIA_T2LL]);

    petsound_store_onoff(active);
    notify_output(active, via_context->cb2_out_state, *via_context->clk_ptr);
}

static void store_t2l(via_context_t *via_context, uint8_t byte)
{
    bool active = sound_active(via_context->via[VIA_ACR], byte);

    petsound_store_onoff(active);
    notify_output(active, via_context->cb2_out_state, *via_context->clk_ptr);
}

static void noop_store(via_context_t *via_context, uint8_t byte)
{
    (void)via_context;
    (void)byte;
}

static void noop_store_port(via_context_t *via_context, uint8_t byte,
                            uint8_t old_value, uint16_t addr)
{
    (void)via_context;
    (void)byte;
    (void)old_value;
    (void)addr;
}

static uint8_t read_pra(via_context_t *via_context, uint16_t addr)
{
    (void)via_context;
    (void)addr;
    return 0xFF;
}

static uint8_t read_prb(via_context_t *via_context)
{
    (void)via_context;
    return 0xFF;
}

static void reset(via_context_t *via_context)
{
    (void)via_context;
    petsound_store_manual(1, maincpu_clk);
    notify_output(false, true, maincpu_clk);
}

static void init_interrupt_status(void)
{
    memset(&sound_int_status, 0, sizeof(sound_int_status));
    memset(sound_pending_int, 0, sizeof(sound_pending_int));
    sound_int_status.num_ints = 1;
    sound_int_status.pending_int = sound_pending_int;
    sound_int_status.int_name = sound_int_name;
    sound_int_status.irq_pending_clk = CLOCK_MAX;
}

void picocalc_vice_via_sound_init(int *rmw_flag)
{
    if (!sound_via_ready) {
        alarm_context_init(&sound_alarm_context, "PETSoundVIA");
        init_interrupt_status();
        viacore_setup_context(&sound_via);
        sound_via.myname = lib_msprintf("PETSoundVIA");
        sound_via.my_module_name = lib_msprintf("PETSNDVIA");
        sound_via.clk_ptr = &maincpu_clk;
        sound_via.rmw_flag = rmw_flag;
        sound_via.write_offset = 0;
        sound_via.irq_line = IK_IRQ;
        sound_via.store_pra = noop_store_port;
        sound_via.store_prb = noop_store_port;
        sound_via.store_pcr = store_pcr;
        sound_via.store_acr = store_acr;
        sound_via.store_sr = noop_store;
        sound_via.store_t2l = store_t2l;
        sound_via.read_pra = read_pra;
        sound_via.read_prb = read_prb;
        sound_via.set_int = set_int;
        sound_via.restore_int = restore_int;
        sound_via.set_ca2 = set_line;
        sound_via.set_cb1 = set_line;
        sound_via.set_cb2 = set_cb2;
        sound_via.reset = reset;
        viacore_init(&sound_via, &sound_alarm_context, &sound_int_status);
        sound_via_ready = true;
    }
    sound_via.rmw_flag = rmw_flag;
}

void picocalc_vice_via_sound_reset(CLOCK cpu_clk)
{
    if (!sound_via_ready) {
        return;
    }
    maincpu_clk = cpu_clk;
    alarm_unset(sound_via.t1_zero_alarm);
    alarm_unset(sound_via.t2_zero_alarm);
    alarm_unset(sound_via.t2_underflow_alarm);
    alarm_unset(sound_via.t2_shift_alarm);
    alarm_unset(sound_via.phi2_sr_alarm);
    init_interrupt_status();
    sound_output_active = false;
    viacore_reset(&sound_via);
}

void picocalc_vice_via_sound_store(uint16_t reg, uint8_t value)
{
    if (!sound_via_ready) {
        return;
    }
    viacore_store(&sound_via, reg & 0x0F, value);
}

uint8_t picocalc_vice_via_sound_peek(uint16_t reg)
{
    if (!sound_via_ready) {
        return 0xFF;
    }
    return viacore_peek(&sound_via, reg & 0x0F);
}

void picocalc_vice_via_sound_run_until(CLOCK cpu_clk)
{
    if (!sound_via_ready) {
        return;
    }
    while (cpu_clk > alarm_context_next_pending_clk(&sound_alarm_context)) {
        alarm_context_dispatch(&sound_alarm_context, cpu_clk);
    }
}

void picocalc_vice_via_sound_set_output_callback(
    picocalc_vice_via_sound_output_fn callback, void *user_data)
{
    sound_output_callback = callback;
    sound_output_user_data = user_data;
}
