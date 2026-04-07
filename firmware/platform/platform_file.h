#ifndef PICOCALC_TRS_PLATFORM_FILE_H
#define PICOCALC_TRS_PLATFORM_FILE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef PICOCALC_PLATFORM
#include "drivers/fat32.h"
typedef fat32_file_t platform_file_t;
#else
#include <stdio.h>
typedef FILE platform_file_t;
#endif

platform_file_t *platform_fopen(const char *path, const char *mode);
int platform_getc(platform_file_t *file);
char *platform_fgets(char *buffer, int size, platform_file_t *file);
void platform_rewind(platform_file_t *file);
int platform_fclose(platform_file_t *file);

#endif
