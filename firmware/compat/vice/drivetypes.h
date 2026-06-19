#ifndef PICOCALC_PET_VICE_DRIVETYPES_H
#define PICOCALC_PET_VICE_DRIVETYPES_H

#include <stdint.h>

typedef struct drivefunc_context_s {
    void (*parallel_set_bus)(uint8_t);
    void (*parallel_set_eoi)(uint8_t);
    void (*parallel_set_dav)(uint8_t);
    void (*parallel_set_ndac)(uint8_t);
    void (*parallel_set_nrfd)(uint8_t);
} drivefunc_context_t;

#endif
