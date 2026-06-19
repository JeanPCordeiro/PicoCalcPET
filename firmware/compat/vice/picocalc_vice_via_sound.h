#ifndef PICOCALC_VICE_VIA_SOUND_H
#define PICOCALC_VICE_VIA_SOUND_H

#include <stdbool.h>
#include <stdint.h>

#include "types.h"

typedef void (*picocalc_vice_via_sound_output_fn)(bool active, bool level,
                                                  CLOCK cpu_clk,
                                                  void *user_data);

void picocalc_vice_via_sound_init(int *rmw_flag);
void picocalc_vice_via_sound_reset(CLOCK cpu_clk);
void picocalc_vice_via_sound_store(uint16_t reg, uint8_t value);
uint8_t picocalc_vice_via_sound_peek(uint16_t reg);
void picocalc_vice_via_sound_run_until(CLOCK cpu_clk);
void picocalc_vice_via_sound_set_output_callback(
    picocalc_vice_via_sound_output_fn callback, void *user_data);

#endif
