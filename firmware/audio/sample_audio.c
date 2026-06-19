#include "sample_audio.h"

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/time.h"

#include "audio.pio.h"
#include "drivers/audio.h"

#define PICOCALC_SAMPLE_AUDIO_LEFT_SM LEFT_CHANNEL
#define PICOCALC_SAMPLE_AUDIO_RIGHT_SM RIGHT_CHANNEL
#define PICOCALC_SAMPLE_AUDIO_LEFT_PIN AUDIO_LEFT_PIN
#define PICOCALC_SAMPLE_AUDIO_RIGHT_PIN AUDIO_RIGHT_PIN
#define PICOCALC_SAMPLE_AUDIO_PWM_HZ 31250u

typedef struct {
    const uint8_t *samples;
    size_t sample_count;
    size_t position;
    uint32_t pwm_period;
    picocalc_sample_audio_stream_fn stream_callback;
    void *stream_user_data;
    bool loop;
    bool playing;
    bool timer_active;
    bool streaming;
} picocalc_sample_audio_state_t;

static PIO picocalc_sample_audio_pio = pio0;
static bool picocalc_sample_audio_initialised;
static uint picocalc_sample_audio_offset;
static repeating_timer_t picocalc_sample_audio_timer;
static picocalc_sample_audio_state_t picocalc_sample_audio_state;

static void picocalc_sample_audio_stop_sms(void)
{
    pio_sm_set_enabled(picocalc_sample_audio_pio,
                       PICOCALC_SAMPLE_AUDIO_LEFT_SM, false);
    pio_sm_set_enabled(picocalc_sample_audio_pio,
                       PICOCALC_SAMPLE_AUDIO_RIGHT_SM, false);
}

static void picocalc_sample_audio_put(uint32_t duty)
{
    if (!pio_sm_is_tx_fifo_full(picocalc_sample_audio_pio,
                                PICOCALC_SAMPLE_AUDIO_LEFT_SM)) {
        pio_sm_put(picocalc_sample_audio_pio,
                   PICOCALC_SAMPLE_AUDIO_LEFT_SM, duty);
    }
    if (!pio_sm_is_tx_fifo_full(picocalc_sample_audio_pio,
                                PICOCALC_SAMPLE_AUDIO_RIGHT_SM)) {
        pio_sm_put(picocalc_sample_audio_pio,
                   PICOCALC_SAMPLE_AUDIO_RIGHT_SM, duty);
    }
}

static void picocalc_sample_audio_start_pwm(void)
{
    uint32_t pwm_period;

    picocalc_sample_audio_stop_sms();
    pio_sm_clear_fifos(picocalc_sample_audio_pio,
                       PICOCALC_SAMPLE_AUDIO_LEFT_SM);
    pio_sm_clear_fifos(picocalc_sample_audio_pio,
                       PICOCALC_SAMPLE_AUDIO_RIGHT_SM);

    audio_pwm_program_init(picocalc_sample_audio_pio,
                           PICOCALC_SAMPLE_AUDIO_LEFT_SM,
                           picocalc_sample_audio_offset,
                           PICOCALC_SAMPLE_AUDIO_LEFT_PIN);
    audio_pwm_program_init(picocalc_sample_audio_pio,
                           PICOCALC_SAMPLE_AUDIO_RIGHT_SM,
                           picocalc_sample_audio_offset,
                           PICOCALC_SAMPLE_AUDIO_RIGHT_PIN);

    pwm_period = clock_get_hz(clk_sys) / (PICOCALC_SAMPLE_AUDIO_PWM_HZ * 3u);
    if (pwm_period < 16u) {
        pwm_period = 16u;
    }
    picocalc_sample_audio_state.pwm_period = pwm_period & ~1u;

    pio_sm_put_blocking(picocalc_sample_audio_pio,
                        PICOCALC_SAMPLE_AUDIO_LEFT_SM,
                        picocalc_sample_audio_state.pwm_period);
    pio_sm_exec(picocalc_sample_audio_pio, PICOCALC_SAMPLE_AUDIO_LEFT_SM,
                pio_encode_pull(false, false));
    pio_sm_exec(picocalc_sample_audio_pio, PICOCALC_SAMPLE_AUDIO_LEFT_SM,
                pio_encode_out(pio_isr, 32));
    pio_sm_put_blocking(picocalc_sample_audio_pio,
                        PICOCALC_SAMPLE_AUDIO_RIGHT_SM,
                        picocalc_sample_audio_state.pwm_period);
    pio_sm_exec(picocalc_sample_audio_pio, PICOCALC_SAMPLE_AUDIO_RIGHT_SM,
                pio_encode_pull(false, false));
    pio_sm_exec(picocalc_sample_audio_pio, PICOCALC_SAMPLE_AUDIO_RIGHT_SM,
                pio_encode_out(pio_isr, 32));

    picocalc_sample_audio_put(picocalc_sample_audio_state.pwm_period / 2u);
    pio_sm_set_enabled(picocalc_sample_audio_pio,
                       PICOCALC_SAMPLE_AUDIO_LEFT_SM, true);
    pio_sm_set_enabled(picocalc_sample_audio_pio,
                       PICOCALC_SAMPLE_AUDIO_RIGHT_SM, true);
}

static bool picocalc_sample_audio_timer_callback(repeating_timer_t *timer)
{
    uint8_t sample;
    uint32_t duty;

    (void)timer;
    if (!picocalc_sample_audio_state.playing) {
        picocalc_sample_audio_state.timer_active = false;
        return false;
    }

    if (picocalc_sample_audio_state.streaming) {
        if (picocalc_sample_audio_state.stream_callback == NULL) {
            sample = 128;
        } else {
            sample = picocalc_sample_audio_state
                         .stream_callback(picocalc_sample_audio_state.stream_user_data);
        }
        duty = ((uint32_t)sample * picocalc_sample_audio_state.pwm_period) / 255u;
        picocalc_sample_audio_put(duty);
        return true;
    }

    if (picocalc_sample_audio_state.samples == NULL ||
        picocalc_sample_audio_state.sample_count == 0) {
        picocalc_sample_audio_state.timer_active = false;
        return false;
    }

    if (picocalc_sample_audio_state.position >=
        picocalc_sample_audio_state.sample_count) {
        if (!picocalc_sample_audio_state.loop) {
            picocalc_sample_audio_state.playing = false;
            picocalc_sample_audio_state.timer_active = false;
            picocalc_sample_audio_stop_sms();
            return false;
        }
        picocalc_sample_audio_state.position = 0;
    }

    sample = picocalc_sample_audio_state
                 .samples[picocalc_sample_audio_state.position++];
    duty = ((uint32_t)sample * picocalc_sample_audio_state.pwm_period) / 255u;
    picocalc_sample_audio_put(duty);
    return true;
}

static void picocalc_sample_audio_init(void)
{
    if (picocalc_sample_audio_initialised) {
        return;
    }

    picocalc_sample_audio_offset =
        pio_add_program(picocalc_sample_audio_pio, &audio_pwm_program);
    picocalc_sample_audio_initialised = true;
}

void picocalc_sample_audio_play(const uint8_t *samples, size_t sample_count,
                                uint32_t sample_rate, bool loop)
{
    if (samples == NULL || sample_count == 0 || sample_rate == 0) {
        picocalc_sample_audio_stop();
        return;
    }

    picocalc_sample_audio_stop();
    audio_stop();
    picocalc_sample_audio_init();

    picocalc_sample_audio_state.samples = samples;
    picocalc_sample_audio_state.sample_count = sample_count;
    picocalc_sample_audio_state.position = 0;
    picocalc_sample_audio_state.loop = loop;
    picocalc_sample_audio_state.playing = true;
    picocalc_sample_audio_state.streaming = false;
    picocalc_sample_audio_state.stream_callback = NULL;
    picocalc_sample_audio_state.stream_user_data = NULL;

    picocalc_sample_audio_start_pwm();
    picocalc_sample_audio_state.timer_active =
        add_repeating_timer_us(-(int64_t)(1000000u / sample_rate),
                               picocalc_sample_audio_timer_callback,
                               NULL, &picocalc_sample_audio_timer);
    if (!picocalc_sample_audio_state.timer_active) {
        picocalc_sample_audio_stop();
    }
}

void picocalc_sample_audio_stream(picocalc_sample_audio_stream_fn callback,
                                  void *user_data, uint32_t sample_rate)
{
    if (callback == NULL || sample_rate == 0) {
        picocalc_sample_audio_stop();
        return;
    }

    picocalc_sample_audio_stop();
    audio_stop();
    picocalc_sample_audio_init();

    picocalc_sample_audio_state.samples = NULL;
    picocalc_sample_audio_state.sample_count = 0;
    picocalc_sample_audio_state.position = 0;
    picocalc_sample_audio_state.loop = true;
    picocalc_sample_audio_state.streaming = true;
    picocalc_sample_audio_state.stream_callback = callback;
    picocalc_sample_audio_state.stream_user_data = user_data;
    picocalc_sample_audio_state.playing = true;

    picocalc_sample_audio_start_pwm();
    picocalc_sample_audio_state.timer_active =
        add_repeating_timer_us(-(int64_t)(1000000u / sample_rate),
                               picocalc_sample_audio_timer_callback,
                               NULL, &picocalc_sample_audio_timer);
    if (!picocalc_sample_audio_state.timer_active) {
        picocalc_sample_audio_stop();
    }
}

void picocalc_sample_audio_stop(void)
{
    if (picocalc_sample_audio_state.timer_active) {
        cancel_repeating_timer(&picocalc_sample_audio_timer);
    }
    picocalc_sample_audio_state.timer_active = false;
    picocalc_sample_audio_state.playing = false;
    picocalc_sample_audio_state.samples = NULL;
    picocalc_sample_audio_state.sample_count = 0;
    picocalc_sample_audio_state.position = 0;
    picocalc_sample_audio_state.streaming = false;
    picocalc_sample_audio_state.stream_callback = NULL;
    picocalc_sample_audio_state.stream_user_data = NULL;
    if (picocalc_sample_audio_initialised) {
        picocalc_sample_audio_stop_sms();
    }
}

bool picocalc_sample_audio_is_playing(void)
{
    return picocalc_sample_audio_state.playing;
}
