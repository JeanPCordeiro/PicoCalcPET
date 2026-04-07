#include <stdio.h>

#include "platform/platform_file.h"

#define FILE platform_file_t
#define fopen platform_fopen
#define getc platform_getc
#define fgets platform_fgets
#define rewind platform_rewind
#define fclose platform_fclose

#include "../../third_party/sdltrs/src/trs_cmd_rom.c"
