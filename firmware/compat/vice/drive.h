#ifndef PICOCALC_PET_VICE_DRIVE_H
#define PICOCALC_PET_VICE_DRIVE_H

#include "types.h"

#define DRIVE_UNIT_MIN 8
#define NUM_DISK_UNITS 4

typedef struct diskunit_context_s {
    int enable;
} diskunit_context_t;

extern diskunit_context_t *diskunit_context[NUM_DISK_UNITS];

void drive_cpu_execute_all(CLOCK clk_value);

#endif
