#define _GNU_SOURCE 1

#include "platform_disk_stdio.h"

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "pico/stdlib.h"

#include "drivers/fat32.h"

typedef struct {
    fat32_file_t file;
    bool writable;
    bool append;
} picocalc_disk_cookie_t;

typedef struct {
    bool read;
    bool write;
    bool create;
    bool truncate;
    bool append;
} picocalc_disk_mode_t;

static void picocalc_disk_set_errno_from_fat32(fat32_error_t result)
{
    switch (result) {
    case FAT32_OK:
        errno = 0;
        break;
    case FAT32_ERROR_NO_CARD:
        errno = ENODEV;
        break;
    case FAT32_ERROR_INIT_FAILED:
    case FAT32_ERROR_READ_FAILED:
    case FAT32_ERROR_WRITE_FAILED:
    case FAT32_ERROR_NOT_MOUNTED:
        errno = EIO;
        break;
    case FAT32_ERROR_FILE_NOT_FOUND:
    case FAT32_ERROR_DIR_NOT_FOUND:
        errno = ENOENT;
        break;
    case FAT32_ERROR_FILE_EXISTS:
        errno = EEXIST;
        break;
    case FAT32_ERROR_DISK_FULL:
        errno = ENOSPC;
        break;
    case FAT32_ERROR_NOT_A_DIRECTORY:
        errno = ENOTDIR;
        break;
    case FAT32_ERROR_NOT_A_FILE:
        errno = EISDIR;
        break;
    case FAT32_ERROR_INVALID_FORMAT:
    case FAT32_ERROR_INVALID_PATH:
    case FAT32_ERROR_INVALID_POSITION:
    case FAT32_ERROR_INVALID_PARAMETER:
    case FAT32_ERROR_INVALID_SECTOR_SIZE:
    case FAT32_ERROR_INVALID_CLUSTER_SIZE:
    case FAT32_ERROR_INVALID_FATS:
    case FAT32_ERROR_INVALID_RESERVED_SECTORS:
    case FAT32_ERROR_DIR_NOT_EMPTY:
    default:
        errno = EINVAL;
        break;
    }
}

static bool picocalc_disk_parse_mode(const char *mode, picocalc_disk_mode_t *parsed_mode)
{
    const char *cursor;

    if (mode == NULL || parsed_mode == NULL || mode[0] == '\0') {
        return false;
    }

    memset(parsed_mode, 0, sizeof(*parsed_mode));

    switch (mode[0]) {
    case 'r':
        parsed_mode->read = true;
        break;
    case 'w':
        parsed_mode->write = true;
        parsed_mode->create = true;
        parsed_mode->truncate = true;
        break;
    case 'a':
        parsed_mode->write = true;
        parsed_mode->create = true;
        parsed_mode->append = true;
        break;
    default:
        return false;
    }

    for (cursor = mode + 1; *cursor != '\0'; ++cursor) {
        if (*cursor == 'b') {
            continue;
        }

        if (*cursor == '+') {
            parsed_mode->read = true;
            parsed_mode->write = true;
            continue;
        }

        return false;
    }

    return true;
}

static bool picocalc_disk_open_file(fat32_file_t *file, const char *path)
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

static bool picocalc_disk_create_file(fat32_file_t *file, const char *path)
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

static ssize_t picocalc_disk_cookie_read(void *cookie, char *buffer, size_t size)
{
    picocalc_disk_cookie_t *disk_cookie = cookie;
    size_t bytes_read = 0;
    fat32_error_t result;

    if (disk_cookie == NULL || buffer == NULL) {
        errno = EINVAL;
        return -1;
    }

    result = fat32_read(&disk_cookie->file, buffer, size, &bytes_read);
    if (result != FAT32_OK) {
        picocalc_disk_set_errno_from_fat32(result);
        return -1;
    }

    return (ssize_t)bytes_read;
}

static ssize_t picocalc_disk_cookie_write(void *cookie, const char *buffer, size_t size)
{
    picocalc_disk_cookie_t *disk_cookie = cookie;
    size_t bytes_written = 0;
    fat32_error_t result;

    if (disk_cookie == NULL || buffer == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (!disk_cookie->writable) {
        errno = EROFS;
        return -1;
    }

    if (disk_cookie->append && fat32_seek(&disk_cookie->file, fat32_size(&disk_cookie->file)) != FAT32_OK) {
        errno = EIO;
        return -1;
    }

    result = fat32_write(&disk_cookie->file, buffer, size, &bytes_written);
    if (result != FAT32_OK) {
        picocalc_disk_set_errno_from_fat32(result);
        return -1;
    }

    return (ssize_t)bytes_written;
}

static int picocalc_disk_cookie_seek(void *cookie, off_t *offset, int whence)
{
    picocalc_disk_cookie_t *disk_cookie = cookie;
    uint32_t base;
    int64_t target;

    if (disk_cookie == NULL || offset == NULL) {
        errno = EINVAL;
        return -1;
    }

    switch (whence) {
    case SEEK_SET:
        base = 0;
        break;
    case SEEK_CUR:
        base = fat32_tell(&disk_cookie->file);
        break;
    case SEEK_END:
        base = fat32_size(&disk_cookie->file);
        break;
    default:
        errno = EINVAL;
        return -1;
    }

    target = (int64_t)base + (int64_t)(*offset);
    if (target < 0) {
        errno = EINVAL;
        return -1;
    }

    if (fat32_seek(&disk_cookie->file, (uint32_t)target) != FAT32_OK) {
        errno = EIO;
        return -1;
    }

    *offset = (off_t)target;
    return 0;
}

static int picocalc_disk_cookie_close(void *cookie)
{
    picocalc_disk_cookie_t *disk_cookie = cookie;

    if (disk_cookie == NULL) {
        errno = EINVAL;
        return EOF;
    }

    fat32_close(&disk_cookie->file);
    free(disk_cookie);
    return 0;
}

FILE *picocalc_disk_fopen(const char *path, const char *mode)
{
    picocalc_disk_cookie_t *cookie;
    cookie_io_functions_t io_functions;
    picocalc_disk_mode_t parsed_mode;
    FILE *stream;

    if (path == NULL || !picocalc_disk_parse_mode(mode, &parsed_mode)) {
        errno = EINVAL;
        return NULL;
    }

    cookie = calloc(1, sizeof(*cookie));
    if (cookie == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    if (parsed_mode.create && parsed_mode.truncate) {
        if (!picocalc_disk_create_file(&cookie->file, path)) {
            free(cookie);
            errno = EIO;
            return NULL;
        }
    } else {
        if (!picocalc_disk_open_file(&cookie->file, path)) {
            free(cookie);
            errno = ENOENT;
            return NULL;
        }
    }

    if (parsed_mode.write && (cookie->file.attributes & FAT32_ATTR_READ_ONLY)) {
        fat32_close(&cookie->file);
        free(cookie);
        errno = EROFS;
        return NULL;
    }

    if (parsed_mode.append && fat32_seek(&cookie->file, fat32_size(&cookie->file)) != FAT32_OK) {
        fat32_close(&cookie->file);
        free(cookie);
        errno = EIO;
        return NULL;
    }

    cookie->writable = parsed_mode.write;
    cookie->append = parsed_mode.append;

    io_functions.read = picocalc_disk_cookie_read;
    io_functions.write = picocalc_disk_cookie_write;
    io_functions.seek = picocalc_disk_cookie_seek;
    io_functions.close = picocalc_disk_cookie_close;

    stream = fopencookie(cookie, mode, io_functions);
    if (stream == NULL) {
        fat32_close(&cookie->file);
        free(cookie);
        errno = EIO;
        return NULL;
    }

    setvbuf(stream, NULL, _IONBF, 0);

    return stream;
}

int picocalc_disk_stat(const char *path, struct stat *st)
{
    fat32_file_t file;

    if (path == NULL || st == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (!picocalc_disk_open_file(&file, path)) {
        errno = ENOENT;
        return -1;
    }

    memset(st, 0, sizeof(*st));
    st->st_mode = S_IFREG | ((file.attributes & FAT32_ATTR_READ_ONLY) ? 0444 : 0666);
    st->st_size = (off_t)fat32_size(&file);
    fat32_close(&file);
    return 0;
}

int ftruncate(int fd, off_t length)
{
    (void)fd;
    (void)length;
    errno = EROFS;
    return -1;
}
