#include "platform_file.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"

#include "drivers/fat32.h"

struct platform_file_handle {
    bool is_embedded;
    fat32_file_t fat32_file;
    const uint8_t *embedded_data;
    size_t embedded_size;
    size_t embedded_pos;
};

#ifdef PICOCALC_EMBEDDED_MODEL3_ROM
extern const unsigned char embedded_model3_rom[];
extern const unsigned int embedded_model3_rom_len;
#endif

static bool is_embedded_model3_path(const char *path)
{
    return path != NULL &&
           (strcmp(path, "embedded:model3.rom") == 0 ||
            strcmp(path, "__embedded_model3__") == 0);
}

bool platform_embedded_model3_rom_available(void)
{
#ifdef PICOCALC_EMBEDDED_MODEL3_ROM
    return embedded_model3_rom_len > 0;
#else
    return false;
#endif
}

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

#ifdef PICOCALC_EMBEDDED_MODEL3_ROM
    if (is_embedded_model3_path(path) && platform_embedded_model3_rom_available()) {
        memset(file, 0, sizeof(*file));
        file->is_embedded = true;
        file->embedded_data = (const uint8_t *)embedded_model3_rom;
        file->embedded_size = (size_t)embedded_model3_rom_len;
        file->embedded_pos = 0;
        return file;
    }
#endif

    memset(file, 0, sizeof(*file));

    for (attempt = 0; attempt < 8; ++attempt) {
        if (fat32_open(&file->fat32_file, path) == FAT32_OK) {
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

    if (file->is_embedded) {
        if (file->embedded_pos >= file->embedded_size) {
            return EOF;
        }

        return (int)file->embedded_data[file->embedded_pos++];
    }

    if (fat32_read(&file->fat32_file, &ch, 1, &bytes_read) != FAT32_OK || bytes_read != 1) {
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
    if (file == NULL) {
        return;
    }

    if (file->is_embedded) {
        file->embedded_pos = 0;
    } else {
        fat32_seek(&file->fat32_file, 0);
    }
}

int platform_fclose(platform_file_t *file)
{
    if (file == NULL) {
        return EOF;
    }

    if (!file->is_embedded) {
        fat32_close(&file->fat32_file);
    }
    free(file);
    return 0;
}
