#include "platform_file.h"

#include <stdio.h>

platform_file_t *platform_fopen(const char *path, const char *mode)
{
    return fopen(path, mode);
}

int platform_getc(platform_file_t *file)
{
    return getc(file);
}

char *platform_fgets(char *buffer, int size, platform_file_t *file)
{
    return fgets(buffer, size, file);
}

void platform_rewind(platform_file_t *file)
{
    rewind(file);
}

int platform_fclose(platform_file_t *file)
{
    return fclose(file);
}

bool platform_embedded_model3_rom_available(void)
{
    return false;
}
