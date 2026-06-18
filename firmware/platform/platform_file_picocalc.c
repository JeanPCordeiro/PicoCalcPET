#define _GNU_SOURCE 1

#include "platform_file.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "pico/stdlib.h"

#include "drivers/fat32.h"

typedef struct {
    bool is_embedded;
    bool writable;
    fat32_file_t fat32_file;
    const uint8_t *embedded_data;
    size_t embedded_size;
    size_t embedded_pos;
} platform_file_cookie_t;

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

static bool platform_fat32_open_retry(fat32_file_t *file, const char *path)
{
    int attempt;

    for (attempt = 0; attempt < 8; ++attempt) {
        if (fat32_open(file, path) == FAT32_OK) {
            return true;
        }

        sleep_ms(250);
    }

    return false;
}

static bool platform_fat32_create_retry(fat32_file_t *file, const char *path)
{
    int attempt;

    for (attempt = 0; attempt < 8; ++attempt) {
        fat32_error_t result = fat32_create(file, path);

        if (result == FAT32_OK) {
            return true;
        }
        if (result == FAT32_ERROR_FILE_EXISTS) {
            if (fat32_delete(path) == FAT32_OK && fat32_create(file, path) == FAT32_OK) {
                return true;
            }
        }

        sleep_ms(250);
    }

    return false;
}

static ssize_t platform_file_cookie_read(void *cookie, char *buffer, size_t size)
{
    platform_file_cookie_t *file_cookie = cookie;
    size_t bytes_read = 0;

    if (file_cookie == NULL || buffer == NULL) {
        return -1;
    }

    if (size == 0) {
        return 0;
    }

    if (file_cookie->is_embedded) {
        size_t remaining = 0;
        size_t to_copy = 0;

        if (file_cookie->embedded_pos < file_cookie->embedded_size) {
            remaining = file_cookie->embedded_size - file_cookie->embedded_pos;
        }
        to_copy = (remaining < size) ? remaining : size;
        if (to_copy > 0) {
            memcpy(buffer, file_cookie->embedded_data + file_cookie->embedded_pos, to_copy);
            file_cookie->embedded_pos += to_copy;
        }
        return (ssize_t)to_copy;
    }

    if (fat32_read(&file_cookie->fat32_file, buffer, size, &bytes_read) != FAT32_OK) {
        return -1;
    }

    return (ssize_t)bytes_read;
}

static ssize_t platform_file_cookie_write(void *cookie, const char *buffer, size_t size)
{
    platform_file_cookie_t *file_cookie = cookie;
    size_t bytes_written = 0;

    if (file_cookie == NULL || buffer == NULL || file_cookie->is_embedded ||
        !file_cookie->writable) {
        return -1;
    }

    if (size == 0) {
        return 0;
    }

    if (fat32_write(&file_cookie->fat32_file, buffer, size, &bytes_written) != FAT32_OK) {
        return -1;
    }

    return (ssize_t)bytes_written;
}

static int platform_file_cookie_seek(void *cookie, off_t *offset, int whence)
{
    platform_file_cookie_t *file_cookie = cookie;
    int64_t base = 0;
    int64_t target;

    if (file_cookie == NULL || offset == NULL) {
        return -1;
    }

    switch (whence) {
    case SEEK_SET:
        base = 0;
        break;
    case SEEK_CUR:
        base = file_cookie->is_embedded
            ? (int64_t)file_cookie->embedded_pos
            : (int64_t)fat32_tell(&file_cookie->fat32_file);
        break;
    case SEEK_END:
        base = file_cookie->is_embedded
            ? (int64_t)file_cookie->embedded_size
            : (int64_t)fat32_size(&file_cookie->fat32_file);
        break;
    default:
        return -1;
    }

    target = base + (int64_t)(*offset);
    if (target < 0) {
        return -1;
    }

    if (file_cookie->is_embedded) {
        if ((size_t)target > file_cookie->embedded_size) {
            return -1;
        }
        file_cookie->embedded_pos = (size_t)target;
    } else {
        if (fat32_seek(&file_cookie->fat32_file, (uint32_t)target) != FAT32_OK) {
            return -1;
        }
    }

    *offset = (off_t)target;
    return 0;
}

static int platform_file_cookie_close(void *cookie)
{
    platform_file_cookie_t *file_cookie = cookie;

    if (file_cookie == NULL) {
        return EOF;
    }

    if (!file_cookie->is_embedded) {
        fat32_close(&file_cookie->fat32_file);
    }
    free(file_cookie);
    return 0;
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
    platform_file_cookie_t *cookie;
    cookie_io_functions_t io_functions;
    FILE *stream;
    bool writable;

    if (path == NULL || mode == NULL) {
        return NULL;
    }
    writable = strchr(mode, 'w') != NULL || strchr(mode, '+') != NULL;
    if (strcmp(mode, "rb") != 0 && strcmp(mode, "wb") != 0 &&
        strcmp(mode, "r+b") != 0 && strcmp(mode, "rb+") != 0) {
        return NULL;
    }

    cookie = calloc(1, sizeof(*cookie));
    if (cookie == NULL) {
        return NULL;
    }

#ifdef PICOCALC_EMBEDDED_MODEL3_ROM
    if (!writable && is_embedded_model3_path(path) && platform_embedded_model3_rom_available()) {
        cookie->is_embedded = true;
        cookie->embedded_data = (const uint8_t *)embedded_model3_rom;
        cookie->embedded_size = (size_t)embedded_model3_rom_len;
        cookie->embedded_pos = 0;
    } else
#endif
    {
        cookie->is_embedded = false;
        cookie->writable = writable;
        if (strcmp(mode, "wb") == 0) {
            if (!platform_fat32_create_retry(&cookie->fat32_file, path)) {
                free(cookie);
                return NULL;
            }
        } else if (!platform_fat32_open_retry(&cookie->fat32_file, path)) {
            free(cookie);
            return NULL;
        }
    }

    io_functions.read = platform_file_cookie_read;
    io_functions.write = platform_file_cookie_write;
    io_functions.seek = platform_file_cookie_seek;
    io_functions.close = platform_file_cookie_close;

    stream = fopencookie(cookie, mode, io_functions);
    if (stream == NULL) {
        platform_file_cookie_close(cookie);
        return NULL;
    }

    setvbuf(stream, NULL, _IOFBF, 1024);
    return stream;
}

int platform_getc(platform_file_t *file)
{
    if (file == NULL) {
        return EOF;
    }
    return getc(file);
}

int platform_putc(int ch, platform_file_t *file)
{
    if (file == NULL) {
        return EOF;
    }
    return putc(ch, file);
}

char *platform_fgets(char *buffer, int size, platform_file_t *file)
{
    if (buffer == NULL || size <= 1 || file == NULL) {
        return NULL;
    }
    return fgets(buffer, size, file);
}

void platform_rewind(platform_file_t *file)
{
    if (file != NULL) {
        rewind(file);
    }
}

int platform_fseek(platform_file_t *file, long offset, int whence)
{
    if (file == NULL) {
        return -1;
    }
    return fseek(file, offset, whence);
}

long platform_ftell(platform_file_t *file)
{
    if (file == NULL) {
        return -1;
    }
    return ftell(file);
}

int platform_fclose(platform_file_t *file)
{
    if (file == NULL) {
        return EOF;
    }
    return fclose(file);
}
