#ifndef PICOCALC_TRS_SAMPLE_AUDIO_H
#define PICOCALC_TRS_SAMPLE_AUDIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t (*picocalc_sample_audio_stream_fn)(void *user_data);

void picocalc_sample_audio_play(const uint8_t *samples, size_t sample_count,
                                uint32_t sample_rate, bool loop);
void picocalc_sample_audio_stream(picocalc_sample_audio_stream_fn callback,
                                  void *user_data, uint32_t sample_rate);
void picocalc_sample_audio_stop(void);
bool picocalc_sample_audio_is_playing(void);

#endif
