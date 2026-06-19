#ifndef PICOCALC_PET_VICE_SID_H
#define PICOCALC_PET_VICE_SID_H

#include "sound.h"

char *sid_sound_machine_dump_state(sound_t *psid);
void sid_sound_machine_enable(int enable);

#endif
