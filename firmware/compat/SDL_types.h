#ifndef PICOCALC_TRS_COMPAT_SDL_TYPES_H
#define PICOCALC_TRS_COMPAT_SDL_TYPES_H

#include <inttypes.h>

typedef unsigned char Uint8;
typedef unsigned short Uint16;
typedef unsigned int Uint32;
typedef unsigned long long Uint64;

#ifndef SDL_PRIu64
#define SDL_PRIu64 PRIu64
#endif

#endif
