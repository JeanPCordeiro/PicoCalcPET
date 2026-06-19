#ifndef PICOCALC_PET_VICE_PETSOUND_BRIDGE_H
#define PICOCALC_PET_VICE_PETSOUND_BRIDGE_H

#include <stdint.h>

#include "types.h"

void picocalc_vice_petsound_init(int sample_rate, int cycles_per_sec);
void picocalc_vice_petsound_reset(CLOCK cpu_clk);
uint8_t picocalc_vice_petsound_render_u8(void);

#endif
