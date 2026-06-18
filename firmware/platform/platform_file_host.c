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

int platform_putc(int ch, platform_file_t *file)
{
    return putc(ch, file);
}

char *platform_fgets(char *buffer, int size, platform_file_t *file)
{
    return fgets(buffer, size, file);
}

void platform_rewind(platform_file_t *file)
{
    rewind(file);
}

int platform_fseek(platform_file_t *file, long offset, int whence)
{
    return fseek(file, offset, whence);
}

long platform_ftell(platform_file_t *file)
{
    return ftell(file);
}

int platform_fclose(platform_file_t *file)
{
    return fclose(file);
}
