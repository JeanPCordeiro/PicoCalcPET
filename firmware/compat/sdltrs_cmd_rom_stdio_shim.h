#ifndef PICOCALC_TRS_CMD_ROM_STDIO_SHIM_H
#define PICOCALC_TRS_CMD_ROM_STDIO_SHIM_H

#if defined(PICOCALC_PLATFORM)
#include "platform/platform_file.h"
#define fopen platform_fopen
#define getc platform_getc
#define fgets platform_fgets
#define rewind platform_rewind
#define fclose platform_fclose
#endif

#endif
