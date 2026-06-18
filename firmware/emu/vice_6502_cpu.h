#ifndef PICOCALC_PET_VICE_6502_CPU_H
#define PICOCALC_PET_VICE_6502_CPU_H

#include <stdint.h>

#include "pet2001.h"

uint32_t vice_6502_step(pet2001_t *pet, uint32_t cycle_budget);

#endif
