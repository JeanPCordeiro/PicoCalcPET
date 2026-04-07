#include "platform_file.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"

platform_file_t *platform_fopen(const char *path, const char *mode)
{
    platform_file_t *file;
    int attempt;

    if (path == NULL || mode == NULL) {
        return NULL;
    }

    if (strcmp(mode, "rb") != 0) {
        return NULL;
    }

    file = malloc(sizeof(*file));
    if (file == NULL) {
        return NULL;
    }

    for (attempt = 0; attempt < 8; ++attempt) {
        if (fat32_open(file, path) == FAT32_OK) {
            return file;
        }

        sleep_ms(250);
    }

    free(file);
    return NULL;
}

int platform_getc(platform_file_t *file)
{
    unsigned char ch;
    size_t bytes_read = 0;

    if (file == NULL) {
        return EOF;
    }

    if (fat32_read(file, &ch, 1, &bytes_read) != FAT32_OK || bytes_read != 1) {
        return EOF;
    }

    return (int)ch;
}

char *platform_fgets(char *buffer, int size, platform_file_t *file)
{
    int i;

    if (buffer == NULL || size <= 1 || file == NULL) {
        return NULL;
    }

    for (i = 0; i < size - 1; ++i) {
        int ch = platform_getc(file);

        if (ch == EOF) {
            break;
        }

        buffer[i] = (char)ch;
        if (buffer[i] == '\n') {
            ++i;
            break;
        }
    }

    if (i == 0) {
        return NULL;
    }

    buffer[i] = '\0';
    return buffer;
}

void platform_rewind(platform_file_t *file)
{
    if (file != NULL) {
        fat32_seek(file, 0);
    }
}

int platform_fclose(platform_file_t *file)
{
    if (file == NULL) {
        return EOF;
    }

    fat32_close(file);
    free(file);
    return 0;
}
