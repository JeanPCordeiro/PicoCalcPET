#ifndef PICOCALC_TRS_PLATFORM_DISK_STDIO_H
#define PICOCALC_TRS_PLATFORM_DISK_STDIO_H

#include <stdio.h>
#include <sys/stat.h>

FILE *picocalc_disk_fopen(const char *path, const char *mode);
int picocalc_disk_stat(const char *path, struct stat *st);

#endif
