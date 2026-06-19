#include "vice.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diskimage.h"
#include "drive.h"
#include "ieee.h"
#include "lib.h"
#include "log.h"
#include "picocalc_vice_petsound.h"
#include "platform/platform_file.h"
#include "serial.h"
#include "sound.h"
#include "vdrive/vdrive.h"
#include "vdrive/vdrive-iec.h"

static serial_t vice_serial_devices[SERIAL_MAXDEVICES];
static vdrive_t *vice_current_vdrive;
static sound_chip_t *vice_pet_sound_chip;
static bool vice_pet_sound_initialised;
static diskunit_context_t vice_disk_units[NUM_DISK_UNITS];
diskunit_context_t *diskunit_context[NUM_DISK_UNITS] = {
    &vice_disk_units[0],
    &vice_disk_units[1],
    &vice_disk_units[2],
    &vice_disk_units[3],
};
CLOCK maincpu_clk;
const char machine_name[] = "PET";
int sound_state_changed;
int sound_playdev_reopen;
int sid_state_changed;

void lib_init(void)
{
}

void *lib_malloc(size_t size)
{
    return malloc(size);
}

void *lib_calloc(size_t nmemb, size_t size)
{
    return calloc(nmemb, size);
}

void *lib_realloc(void *p, size_t size)
{
    return realloc(p, size);
}

void lib_free(void *ptr)
{
    free(ptr);
}

char *lib_strdup(const char *str)
{
    size_t len;
    char *copy;

    if (str == NULL) {
        return NULL;
    }
    len = strlen(str) + 1u;
    copy = lib_malloc(len);
    if (copy != NULL) {
        memcpy(copy, str, len);
    }
    return copy;
}

char *lib_msprintf(const char *fmt, ...)
{
    va_list args;
    va_list copy_args;
    int len;
    char *out;

    va_start(args, fmt);
    va_copy(copy_args, args);
    len = vsnprintf(NULL, 0, fmt, copy_args);
    va_end(copy_args);
    if (len < 0) {
        va_end(args);
        return NULL;
    }
    out = lib_malloc((size_t)len + 1u);
    if (out != NULL) {
        vsnprintf(out, (size_t)len + 1u, fmt, args);
    }
    va_end(args);
    return out;
}

log_t log_open(const char *id)
{
    (void)id;
    return LOG_DEFAULT;
}

int log_error(log_t log, const char *format, ...)
{
    (void)log;
    (void)format;
    return 0;
}

int log_debug(log_t log, const char *format, ...)
{
    (void)log;
    (void)format;
    return 0;
}

void drive_cpu_execute_all(CLOCK clk_value)
{
    (void)clk_value;
}

int resources_get_int(const char *name, int *value)
{
    if (value == NULL) {
        return -1;
    }
    if (name != NULL && strcmp(name, "CB2Lowpass") == 0) {
        *value = 5000;
        return 0;
    }
    *value = 0;
    return 0;
}

uint16_t sound_chip_register(sound_chip_t *chip)
{
    vice_pet_sound_chip = chip;
    return 0;
}

void sound_reset(void)
{
    if (vice_pet_sound_chip != NULL && vice_pet_sound_chip->reset != NULL) {
        vice_pet_sound_chip->reset(NULL, maincpu_clk);
    }
}

char *sid_sound_machine_dump_state(sound_t *psid)
{
    (void)psid;
    return NULL;
}

void sid_sound_machine_enable(int enable)
{
    (void)enable;
}

void picocalc_vice_petsound_init(int sample_rate, int cycles_per_sec)
{
    extern void pet_sound_chip_init(void);

    if (vice_pet_sound_chip == NULL) {
        pet_sound_chip_init();
    }
    if (vice_pet_sound_chip != NULL && vice_pet_sound_chip->init != NULL) {
        vice_pet_sound_chip->init(NULL, sample_rate, cycles_per_sec);
        vice_pet_sound_initialised = true;
    }
}

void picocalc_vice_petsound_reset(CLOCK cpu_clk)
{
    maincpu_clk = cpu_clk;
    if (vice_pet_sound_chip != NULL && vice_pet_sound_chip->reset != NULL) {
        vice_pet_sound_chip->reset(NULL, cpu_clk);
    }
}

uint8_t picocalc_vice_petsound_render_u8(void)
{
    int16_t sample = 0;
    CLOCK delta = 0;
    sound_t *psid = NULL;
    int mixed;

    if (!vice_pet_sound_initialised || vice_pet_sound_chip == NULL ||
        vice_pet_sound_chip->calculate_samples == NULL) {
        return 128;
    }
    vice_pet_sound_chip->calculate_samples(&psid, &sample, 1,
                                           SOUND_OUTPUT_MONO, 1, &delta);
    mixed = 128 + (sample >> 5);
    if (mixed < 0) {
        mixed = 0;
    } else if (mixed > 255) {
        mixed = 255;
    }
    return (uint8_t)mixed;
}

void ieee_drive_parallel_set_atn(int state, diskunit_context_t *ctxptr)
{
    (void)state;
    (void)ctxptr;
}

serial_t *serial_device_get(unsigned int unit)
{
    return unit < SERIAL_MAXDEVICES ? &vice_serial_devices[unit] : NULL;
}

unsigned int serial_device_type_get(unsigned int unit)
{
    (void)unit;
    return SERIAL_DEVICE_VIRT;
}

void serial_device_type_set(unsigned int type, unsigned int unit)
{
    (void)type;
    (void)unit;
}

vdrive_t *file_system_get_vdrive(unsigned int unit)
{
    (void)unit;
    return vice_current_vdrive;
}

void picocalc_vice_parallel_set_vdrive(vdrive_t *vdrive)
{
    serial_t *device = &vice_serial_devices[8];

    vice_current_vdrive = vdrive;
    memset(device, 0, sizeof(*device));
    device->inuse = vdrive != NULL ? 1 : 0;
    device->device = 8;
    device->getf = vdrive_iec_read;
    device->putf = vdrive_iec_write;
    device->openf = vdrive_iec_open;
    device->closef = vdrive_iec_close;
    device->flushf = vdrive_iec_flush;
    device->listenf = vdrive_iec_listen;
}

void vdrive_alloc_buffer(bufferinfo_t *p, int mode)
{
    if (p == NULL) {
        return;
    }
    p->mode = mode;
    if (p->buffer == NULL) {
        p->buffer = lib_malloc(256);
    }
}

void vdrive_free_buffer(bufferinfo_t *p)
{
    if (p == NULL) {
        return;
    }
    if (p->buffer != NULL) {
        lib_free(p->buffer);
        p->buffer = NULL;
    }
    p->mode = BUFFER_NOT_IN_USE;
    p->bufptr = 0;
    p->length = 0;
}

int vdrive_get_max_sectors_per_head(vdrive_t *vdrive, unsigned int track)
{
    (void)vdrive;
    return disk_image_sector_per_track(DISK_IMAGE_TYPE_D64, track);
}

int vdrive_get_max_sectors(vdrive_t *vdrive, unsigned int track)
{
    return vdrive_get_max_sectors_per_head(vdrive, track);
}

int vdrive_read_sector(vdrive_t *vdrive, uint8_t *buf, unsigned int track,
                       unsigned int sector)
{
    disk_addr_t dadr;

    if (vdrive == NULL || vdrive->image == NULL) {
        return -1;
    }
    dadr.track = track;
    dadr.sector = sector;
    return disk_image_read_sector(vdrive->image, buf, &dadr);
}

int vdrive_write_sector(vdrive_t *vdrive, const uint8_t *buf, unsigned int track,
                        unsigned int sector)
{
    disk_addr_t dadr;

    if (vdrive == NULL || vdrive->image == NULL) {
        return -1;
    }
    dadr.track = track;
    dadr.sector = sector;
    return disk_image_write_sector(vdrive->image, buf, &dadr);
}

int disk_image_read_sector(const disk_image_t *image, uint8_t *buf,
                           const disk_addr_t *dadr)
{
    platform_file_t *file;
    long offset = 0;
    unsigned int track;

    if (image == NULL || buf == NULL || dadr == NULL ||
        image->media.fsimage == NULL || dadr->track < 1 ||
        dadr->track > 35) {
        return -1;
    }

    for (track = 1; track < dadr->track; ++track) {
        offset += (long)disk_image_sector_per_track(DISK_IMAGE_TYPE_D64, track) * 256L;
    }
    if (dadr->sector >= disk_image_sector_per_track(DISK_IMAGE_TYPE_D64, dadr->track)) {
        return -1;
    }
    offset += (long)dadr->sector * 256L;
    file = (platform_file_t *)image->media.fsimage;
    if (platform_fseek(file, offset, SEEK_SET) != 0) {
        return -1;
    }
    for (unsigned int i = 0; i < 256; ++i) {
        int ch = platform_getc(file);

        if (ch == EOF) {
            return -1;
        }
        buf[i] = (uint8_t)ch;
    }
    return 0;
}

int disk_image_write_sector(disk_image_t *image, const uint8_t *buf,
                            const disk_addr_t *dadr)
{
    platform_file_t *file;
    long offset = 0;
    unsigned int track;

    if (image == NULL || buf == NULL || dadr == NULL ||
        image->media.fsimage == NULL || dadr->track < 1 ||
        dadr->track > 35) {
        return -1;
    }
    for (track = 1; track < dadr->track; ++track) {
        offset += (long)disk_image_sector_per_track(DISK_IMAGE_TYPE_D64, track) * 256L;
    }
    if (dadr->sector >= disk_image_sector_per_track(DISK_IMAGE_TYPE_D64, dadr->track)) {
        return -1;
    }
    offset += (long)dadr->sector * 256L;
    file = (platform_file_t *)image->media.fsimage;
    if (platform_fseek(file, offset, SEEK_SET) != 0) {
        return -1;
    }
    for (unsigned int i = 0; i < 256; ++i) {
        if (platform_putc(buf[i], file) == EOF) {
            return -1;
        }
    }
    return 0;
}

int disk_image_check_sector(const disk_image_t *image, unsigned int track,
                            unsigned int sector)
{
    (void)image;
    return track >= 1 && track <= 35 &&
           sector < disk_image_sector_per_track(DISK_IMAGE_TYPE_D64, track) ? 0 : -1;
}

unsigned int disk_image_sector_per_track(unsigned int format, unsigned int track)
{
    (void)format;
    if (track >= 1 && track <= 17) {
        return 21;
    }
    if (track <= 24) {
        return 19;
    }
    if (track <= 30) {
        return 18;
    }
    if (track <= 35) {
        return 17;
    }
    return 0;
}

int vdrive_bam_free_block_count(vdrive_t *vdrive)
{
    uint8_t bam[256];
    int total = 0;

    if (vdrive_read_sector(vdrive, bam, 18, 0) != 0) {
        return 0;
    }
    for (unsigned int track = 1; track <= 35; ++track) {
        if (track != 18) {
            total += bam[4 + (track - 1) * 4];
        }
    }
    return total;
}

int vdrive_bam_free_sector(vdrive_t *vdrive, unsigned int track, unsigned int sector)
{
    uint8_t bam[256];
    unsigned int offset;
    uint8_t mask;

    if (track < 1 || track > 35 ||
        sector >= disk_image_sector_per_track(DISK_IMAGE_TYPE_D64, track) ||
        vdrive_read_sector(vdrive, bam, 18, 0) != 0) {
        return 0;
    }

    offset = 4 + (track - 1) * 4;
    mask = (uint8_t)(1u << (sector & 7u));
    if ((bam[offset + 1u + (sector >> 3)] & mask) == 0) {
        bam[offset + 1u + (sector >> 3)] |= mask;
        bam[offset]++;
        return vdrive_write_sector(vdrive, bam, 18, 0) == 0 ? 1 : 0;
    }
    return 0;
}

int vdrive_bam_allocate_sector(vdrive_t *vdrive, unsigned int track, unsigned int sector)
{
    uint8_t bam[256];
    unsigned int offset;
    uint8_t mask;

    if (track < 1 || track > 35 || track == 18 ||
        sector >= disk_image_sector_per_track(DISK_IMAGE_TYPE_D64, track) ||
        vdrive_read_sector(vdrive, bam, 18, 0) != 0) {
        return 0;
    }

    offset = 4 + (track - 1) * 4;
    mask = (uint8_t)(1u << (sector & 7u));
    if ((bam[offset + 1u + (sector >> 3)] & mask) == 0) {
        return 0;
    }
    bam[offset + 1u + (sector >> 3)] &= (uint8_t)~mask;
    if (bam[offset] > 0) {
        bam[offset]--;
    }
    return vdrive_write_sector(vdrive, bam, 18, 0) == 0 ? 1 : 0;
}

int vdrive_bam_alloc_next_free_sector_interleave(vdrive_t *vdrive,
                                                 unsigned int *track,
                                                 unsigned int *sector,
                                                 unsigned int interleave)
{
    uint8_t bam[256];
    unsigned int start_track;
    unsigned int start_sector;

    if (track == NULL || sector == NULL ||
        vdrive_read_sector(vdrive, bam, 18, 0) != 0) {
        return -1;
    }

    start_track = (*track >= 1 && *track <= 35 && *track != 18) ? *track : 17;
    start_sector = *sector + (interleave != 0 ? interleave : 10);

    for (unsigned int pass = 0; pass < 2; ++pass) {
        for (unsigned int t = pass == 0 ? start_track : 1; t <= 35; ++t) {
            unsigned int sectors;

            if (t == 18) {
                continue;
            }
            sectors = disk_image_sector_per_track(DISK_IMAGE_TYPE_D64, t);
            for (unsigned int s_count = 0; s_count < sectors; ++s_count) {
                unsigned int s = (pass == 0 && t == start_track)
                                     ? (start_sector + s_count) % sectors
                                     : s_count;
                unsigned int offset = 4 + (t - 1) * 4;
                uint8_t mask = (uint8_t)(1u << (s & 7u));

                if ((bam[offset + 1u + (s >> 3)] & mask) == 0) {
                    continue;
                }
                if (!vdrive_bam_allocate_sector(vdrive, t, s)) {
                    return -1;
                }
                *track = t;
                *sector = s;
                return 0;
            }
        }
    }
    return -1;
}

int vdrive_bam_write_bam(vdrive_t *vdrive)
{
    (void)vdrive;
    return 0;
}

int vdrive_bam_alloc_first_free_sector(vdrive_t *vdrive, unsigned int *track,
                                       unsigned int *sector)
{
    if (track == NULL || sector == NULL) {
        return -1;
    }
    *track = 17;
    *sector = 0;
    return vdrive_bam_alloc_next_free_sector_interleave(vdrive, track, sector, 10);
}

int vdrive_bam_alloc_next_free_sector(vdrive_t *vdrive, unsigned int *track,
                                      unsigned int *sector)
{
    return vdrive_bam_alloc_next_free_sector_interleave(vdrive, track, sector, 10);
}

void vdrive_bam_setup_bam(vdrive_t *vdrive)
{
    (void)vdrive;
}

void vdrive_rel_scratch(vdrive_t *vdrive, unsigned int t, unsigned int s)
{
    (void)vdrive;
    (void)t;
    (void)s;
}

int vdrive_switch(vdrive_t *vdrive, int part)
{
    if (vdrive == NULL || part < 0 || part >= NUM_DRIVES ||
        vdrive->images[part] == NULL) {
        return -1;
    }
    vdrive->image = vdrive->images[part];
    vdrive->current_part = part;
    return 0;
}

int vdrive_ispartvalid(vdrive_t *vdrive, int part)
{
    return vdrive != NULL && part >= 0 && part < NUM_DRIVES &&
           vdrive->images[part] != NULL;
}

int vdrive_realpart(vdrive_t *vdrive, int part)
{
    (void)vdrive;
    return part;
}

void vdrive_set_last_read(unsigned int track, unsigned int sector, uint8_t *buffer)
{
    (void)track;
    (void)sector;
    (void)buffer;
}

void vdrive_get_last_read(unsigned int *track, unsigned int *sector, uint8_t **buffer)
{
    if (track != NULL) {
        *track = 0;
    }
    if (sector != NULL) {
        *sector = 0;
    }
    if (buffer != NULL) {
        *buffer = NULL;
    }
}

int vdrive_command_set_error(vdrive_t *vdrive, int code,
                             unsigned int track, unsigned int sector)
{
    (void)track;
    (void)sector;
    if (vdrive != NULL) {
        vdrive->last_code = code;
    }
    return code;
}

int vdrive_command_switchtraverse(vdrive_t *vdrive, cbmdos_cmd_parse_plus_t *cmd)
{
    (void)vdrive;
    (void)cmd;
    return 0;
}

int vdrive_command_execute(vdrive_t *vdrive, const uint8_t *buf, unsigned int length)
{
    (void)vdrive;
    (void)buf;
    (void)length;
    return 0;
}

int vdrive_rel_open(vdrive_t *vdrive, unsigned int secondary,
                    cbmdos_cmd_parse_plus_t *cmd_parse)
{
    (void)vdrive;
    (void)secondary;
    (void)cmd_parse;
    return SERIAL_ERROR;
}

int vdrive_rel_read(vdrive_t *vdrive, uint8_t *data, unsigned int secondary)
{
    (void)vdrive;
    (void)secondary;
    if (data != NULL) {
        *data = 0;
    }
    return SERIAL_ERROR;
}

int vdrive_rel_write(vdrive_t *vdrive, uint8_t data, unsigned int secondary)
{
    (void)vdrive;
    (void)data;
    (void)secondary;
    return SERIAL_ERROR;
}

int vdrive_rel_close(vdrive_t *vdrive, unsigned int secondary)
{
    (void)vdrive;
    (void)secondary;
    return SERIAL_OK;
}

void vdrive_rel_listen(vdrive_t *vdrive, unsigned int secondary)
{
    (void)vdrive;
    (void)secondary;
}
