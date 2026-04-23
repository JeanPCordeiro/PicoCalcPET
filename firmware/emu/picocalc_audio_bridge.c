#include "emu/picocalc_audio_bridge.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef PICOCALC_PLATFORM
#include "drivers/audio.h"
#endif

/*
 * PicoCalc audio is frequency-based PWM, not sample-amplitude PCM.
 * This bridge approximates sdltrs sound-producing callbacks well enough for
 * in-game beeps/effects, while keeping cassette I/O semantics lightweight.
 */

static int bridge_cassette_motor;
static int bridge_cassette_out_value;
static int bridge_model4_sound_value = -1;
static int bridge_cassette_input_bit;
static int bridge_orch_left;
static int bridge_orch_right;
static bool bridge_audio_initialised;
static uint32_t bridge_last_left_hz;
static uint32_t bridge_last_right_hz;
static bool bridge_output_valid;

#define BRIDGE_FLUSH_SIGNAL (-500)

extern int trs_sound;

static void bridge_audio_lazy_init(void)
{
#ifdef PICOCALC_PLATFORM
    if (!bridge_audio_initialised) {
        audio_init();
        bridge_audio_initialised = true;
    }
#endif
}

static uint32_t bridge_map_cassette_tone(int value)
{
    switch (value & 0x3) {
    case 0:
        return 520u;
    case 1:
        return 880u;
    case 2:
        return 660u;
    case 3:
        return 1320u;
    default:
        return 0u;
    }
}

static uint32_t bridge_map_orch_sample_to_frequency(int sample)
{
    /* Signed 8-bit sample mapped to practical PWM tone range. */
    int v = ((sample ^ 0x80) & 0xFF);
    return 140u + (uint32_t)((v * 2600) / 255);
}

static void bridge_render_output(void)
{
#ifdef PICOCALC_PLATFORM
    uint32_t left = 0;
    uint32_t right = 0;

    if (!trs_sound) {
        if (!bridge_output_valid || bridge_last_left_hz != 0 || bridge_last_right_hz != 0) {
            bridge_audio_lazy_init();
            audio_stop();
            bridge_last_left_hz = 0;
            bridge_last_right_hz = 0;
            bridge_output_valid = true;
        }
        return;
    }

    if (bridge_cassette_motor == 0) {
        if (bridge_orch_left != 0 || bridge_orch_right != 0) {
            left = bridge_map_orch_sample_to_frequency(bridge_orch_left);
            right = bridge_map_orch_sample_to_frequency(bridge_orch_right);
        } else if (bridge_model4_sound_value >= 0) {
            left = bridge_model4_sound_value ? 1700u : 950u;
            right = left;
        } else {
            left = bridge_map_cassette_tone(bridge_cassette_out_value);
            right = left;
        }
    }

    if (bridge_output_valid && left == bridge_last_left_hz && right == bridge_last_right_hz) {
        return;
    }

    bridge_audio_lazy_init();
    if (left == 0 && right == 0) {
        audio_stop();
    } else {
        audio_play_sound(left, right);
    }
    bridge_last_left_hz = left;
    bridge_last_right_hz = right;
    bridge_output_valid = true;
#endif
}

void picocalc_audio_bridge_reset(void)
{
    bridge_cassette_motor = 0;
    bridge_cassette_out_value = 0;
    bridge_model4_sound_value = -1;
    bridge_cassette_input_bit = 0;
    bridge_orch_left = 0;
    bridge_orch_right = 0;
    bridge_last_left_hz = 0;
    bridge_last_right_hz = 0;
    bridge_output_valid = false;
    bridge_render_output();
}

void picocalc_audio_bridge_set_cassette_motor(int value)
{
    bridge_cassette_motor = value ? 1 : 0;
    if (bridge_cassette_motor) {
        bridge_cassette_input_bit = 0;
    }
    bridge_render_output();
}

void picocalc_audio_bridge_cassette_out(int value)
{
    bridge_cassette_out_value = (value & 0x3);
    if (bridge_cassette_motor) {
        bridge_cassette_input_bit = bridge_cassette_out_value & 1;
    }
    bridge_render_output();
}

int picocalc_audio_bridge_cassette_in(void)
{
    return bridge_cassette_input_bit ? 0x80 : 0x00;
}

void picocalc_audio_bridge_sound_out(int value)
{
    bridge_model4_sound_value = value ? 1 : 0;
    bridge_render_output();
}

void picocalc_audio_bridge_orch90_out(int channels, int value)
{
    int sample = (value & 0xFF);

    if (value == BRIDGE_FLUSH_SIGNAL) {
        bridge_orch_left = 0;
        bridge_orch_right = 0;
    } else {
        if (channels & 1) {
            bridge_orch_left = sample;
        }
        if (channels & 2) {
            bridge_orch_right = sample;
        }
    }
    bridge_render_output();
}

void picocalc_audio_bridge_orch90_flush(void)
{
    bridge_orch_left = 0;
    bridge_orch_right = 0;
    bridge_render_output();
}

void picocalc_audio_bridge_cassette_update(void)
{
    bridge_render_output();
}

void picocalc_audio_bridge_cassette_kickoff(void)
{
    bridge_render_output();
}
