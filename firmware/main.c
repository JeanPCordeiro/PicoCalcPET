#include <stdarg.h>
#include <stdbool.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emu/pet2001.h"
#include "frontend/pet_frontend.h"
#include "platform/platform.h"
#include "platform/platform_file.h"

#ifdef PICOCALC_PLATFORM
#include "audio/sample_audio.h"
#include "pico/time.h"
#include "picocalc_vice_petsound.h"
#endif

extern const char *program_name;

#define PICOCALC_PET_ROM_DIR "/PET2001/ROMS"
#define PICOCALC_PET_PRG_DIR "/PET2001/PRG"
#define PICOCALC_PET_DISK_DIR "/PET2001/DISK"
#define PICOCALC_PET_QUICK_SAVE_PATH PICOCALC_PET_PRG_DIR "/SAVED.PRG"
#define PICOCALC_PET_FRAME_CYCLES 16667u
#define PICOCALC_PET_SOUND_SAMPLE_RATE 22050u
#define PICOCALC_PET_FUNCTION_HELP "F1 DBG  F2 KBD  F3 PRG  F4 SAVE  F5 D64"

static bool pet_debug_status;

#ifdef PICOCALC_PLATFORM
static uint8_t pet_audio_stream_sample(void *user_data)
{
    (void)user_data;
    return picocalc_vice_petsound_render_u8();
}
#endif

static void status_printf(const char *format, ...)
{
    char buffer[96];
    va_list args;

    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    platform_status_puts(buffer);
}

static const char *path_leaf_name(const char *path)
{
    const char *cursor;
    const char *leaf;

    if (path == NULL || path[0] == '\0') {
        return "none";
    }

    leaf = path;
    for (cursor = path; *cursor != '\0'; ++cursor) {
        if (*cursor == '/' || *cursor == '\\') {
            leaf = cursor + 1;
        }
    }

    return (leaf[0] != '\0') ? leaf : path;
}

static bool copy_env_path(const char *env_name, char *buffer, size_t buffer_size)
{
    const char *env_path;

    env_path = getenv(env_name);
    if (env_path == NULL || env_path[0] == '\0') {
        return false;
    }

    strncpy(buffer, env_path, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
    return platform_file_exists(buffer);
}

static void set_rom_path(char *buffer, size_t buffer_size, const char *filename)
{
    snprintf(buffer, buffer_size, PICOCALC_PET_ROM_DIR "/%s", filename);
}

static void set_split_rom_profile(pet2001_rom_paths_t *paths, const char *basic,
                                  const char *editor, const char *kernal,
                                  pet2001_keyboard_layout_t keyboard_layout)
{
    set_rom_path(paths->basic, sizeof(paths->basic), basic);
    set_rom_path(paths->editor, sizeof(paths->editor), editor);
    set_rom_path(paths->kernal, sizeof(paths->kernal), kernal);
    set_rom_path(paths->characters, sizeof(paths->characters), "characters.bin");
    paths->keyboard_layout = keyboard_layout;
}

static bool split_rom_profile_exists(const pet2001_rom_paths_t *paths)
{
    return platform_file_exists(paths->basic) &&
           platform_file_exists(paths->editor) &&
           platform_file_exists(paths->kernal) &&
           platform_file_exists(paths->characters);
}

static bool select_pet_rom_paths(int argc, char **argv, pet2001_rom_paths_t *paths)
{
    bool have_combined;
    bool have_env_split;

    memset(paths, 0, sizeof(*paths));

    if (argc > 1 && argv[1][0] != '\0') {
        strncpy(paths->combined, argv[1], sizeof(paths->combined) - 1);
        have_combined = platform_file_exists(paths->combined);
        if (have_combined) {
            return true;
        }
    }

    have_combined = copy_env_path("PICOCALC_PET_ROM", paths->combined,
                                  sizeof(paths->combined));
    if (!have_combined) {
        snprintf(paths->combined, sizeof(paths->combined),
                 PICOCALC_PET_ROM_DIR "/pet2001.rom");
        have_combined = platform_file_exists(paths->combined);
    }
    if (have_combined) {
        return true;
    }
    paths->combined[0] = '\0';

    copy_env_path("PICOCALC_PET_BASIC_ROM", paths->basic, sizeof(paths->basic));
    copy_env_path("PICOCALC_PET_EDITOR_ROM", paths->editor, sizeof(paths->editor));
    copy_env_path("PICOCALC_PET_KERNAL_ROM", paths->kernal, sizeof(paths->kernal));
    copy_env_path("PICOCALC_PET_CHAR_ROM", paths->characters, sizeof(paths->characters));

    have_env_split = paths->basic[0] != '\0' ||
                     paths->editor[0] != '\0' ||
                     paths->kernal[0] != '\0' ||
                     paths->characters[0] != '\0';
    if (have_env_split) {
        if (paths->characters[0] == '\0') {
            set_rom_path(paths->characters, sizeof(paths->characters), "characters.bin");
        }
        paths->keyboard_layout = strstr(path_leaf_name(paths->basic), "basic4") != NULL
                                      ? PET2001_KEYBOARD_BUSINESS
                                      : PET2001_KEYBOARD_GRAPHICS;
        return split_rom_profile_exists(paths);
    }

    set_split_rom_profile(paths, "basic1.bin", "edit1.bin", "kernal1.bin",
                          PET2001_KEYBOARD_GRAPHICS);
    if (split_rom_profile_exists(paths)) {
        return true;
    }

    set_split_rom_profile(paths, "basic4.bin", "edit4.bin", "kernal4.bin",
                          PET2001_KEYBOARD_BUSINESS);
    if (split_rom_profile_exists(paths)) {
        return true;
    }

    return false;
}

static void mark_pet_video_dirty(pet2001_t *pet)
{
    if (pet != NULL) {
        memset(pet->video_dirty, true, sizeof(pet->video_dirty));
    }
}

static bool path_has_extension(const char *name, const char *extension)
{
    size_t name_len;
    size_t extension_len;

    if (name == NULL || extension == NULL) {
        return false;
    }
    name_len = strlen(name);
    extension_len = strlen(extension);
    return name_len >= extension_len &&
           strcasecmp(name + name_len - extension_len, extension) == 0;
}

static int filter_files_by_extension(platform_disk_image_t *files, int count,
                                     const char *extension)
{
    int read_index;
    int write_index = 0;

    if (files == NULL || count <= 0 || extension == NULL) {
        return 0;
    }
    for (read_index = 0; read_index < count; ++read_index) {
        if (path_has_extension(files[read_index].name, extension)) {
            if (write_index != read_index) {
                files[write_index] = files[read_index];
            }
            write_index++;
        }
    }
    return write_index;
}

static void restore_pet_screen(pet2001_t *pet)
{
    pet_frontend_clear();
    mark_pet_video_dirty(pet);
    pet_frontend_render_video(pet);
}

static void disk_mount_line(const pet2001_t *pet, int drive, char *line,
                            size_t line_size)
{
    const char *name = "none";

    if (line == NULL || line_size == 0) {
        return;
    }
    if (pet != NULL && drive >= 0 && drive <= 1 && pet->cbm_disk_mounted[drive]) {
        name = path_leaf_name(pet->cbm_disk_path[drive]);
    }
    snprintf(line, line_size, "D%d: %s", drive, name);
}

static void draw_file_picker(const platform_disk_image_t *files, int count,
                             const char *title, const char *footer,
                             const char *context0, const char *context1,
                             int selected, int top)
{
    char line[PET_FRONTEND_COLS + 1];
    int list_row = context0 != NULL || context1 != NULL ? 5 : 2;
    int visible_rows = 23 - list_row;
    int row;

    pet_frontend_clear();
    pet_frontend_write_centered(0, title);
    pet_frontend_write_centered(23, footer);
    if (context0 != NULL) {
        pet_frontend_write_line(2, context0);
    }
    if (context1 != NULL) {
        pet_frontend_write_line(3, context1);
    }

    for (row = 0; row < visible_rows; ++row) {
        int index = top + row;

        if (index >= count) {
            break;
        }
        snprintf(line, sizeof(line), "%c %-36.36s",
                 index == selected ? '>' : ' ',
                 files[index].name);
        pet_frontend_write_line(row + list_row, line);
    }
    pet_frontend_flush();
}

static int pick_file(platform_disk_image_t *files, int count,
                     const char *title, const char *footer,
                     const char *context0, const char *context1)
{
    int selected = 0;
    int top = 0;
    int visible_rows = context0 != NULL || context1 != NULL ? 18 : 21;

    if (files == NULL || count <= 0) {
        return -1;
    }

    draw_file_picker(files, count, title, footer, context0, context1, selected, top);
    for (;;) {
        int key = PLATFORM_KEY_NONE;

        if (!platform_poll_key(&key, true)) {
            continue;
        }

        if (key == PLATFORM_KEY_ESC || key == PLATFORM_KEY_BREAK) {
            return -1;
        }
        if (key == PLATFORM_KEY_ENTER) {
            return selected;
        }
        if (key == PLATFORM_KEY_UP && selected > 0) {
            selected--;
            if (selected < top) {
                top = selected;
            }
            draw_file_picker(files, count, title, footer, context0, context1,
                             selected, top);
        } else if (key == PLATFORM_KEY_DOWN && selected + 1 < count) {
            selected++;
            if (selected >= top + visible_rows) {
                top = selected - visible_rows + 1;
            }
            draw_file_picker(files, count, title, footer, context0, context1,
                             selected, top);
        }
    }
}

static int pick_prg(platform_disk_image_t *programs, int count)
{
    return pick_file(programs, count, "Select PET PRG",
                     "UP/DOWN ENTER=LOAD ESC=CANCEL", NULL, NULL);
}

static void draw_drive_picker(const pet2001_t *pet, int selected)
{
    char disk0[PET_FRONTEND_COLS + 1];
    char disk1[PET_FRONTEND_COLS + 1];

    disk_mount_line(pet, 0, disk0, sizeof(disk0));
    disk_mount_line(pet, 1, disk1, sizeof(disk1));
    pet_frontend_clear();
    pet_frontend_write_centered(0, "Select Disk Drive");
    pet_frontend_write_line(2, disk0);
    pet_frontend_write_line(3, disk1);
    pet_frontend_write_line(6, selected == 0 ? "> Drive 0" : "  Drive 0");
    pet_frontend_write_line(7, selected == 1 ? "> Drive 1" : "  Drive 1");
    pet_frontend_write_centered(23, "UP/DOWN ENTER=SELECT ESC=CANCEL");
    pet_frontend_flush();
}

static int pick_disk_drive(const pet2001_t *pet)
{
    int selected = 0;

    draw_drive_picker(pet, selected);
    for (;;) {
        int key = PLATFORM_KEY_NONE;

        if (!platform_poll_key(&key, true)) {
            continue;
        }

        if (key == PLATFORM_KEY_ESC || key == PLATFORM_KEY_BREAK) {
            return -1;
        }
        if (key == PLATFORM_KEY_ENTER) {
            return selected;
        }
        if (key == PLATFORM_KEY_UP || key == PLATFORM_KEY_DOWN) {
            selected = selected == 0 ? 1 : 0;
            draw_drive_picker(pet, selected);
        }
    }
}

static int pick_disk_image(const pet2001_t *pet, platform_disk_image_t *images,
                           int count, int drive)
{
    char title[32];
    char disk0[PET_FRONTEND_COLS + 1];
    char disk1[PET_FRONTEND_COLS + 1];

    disk_mount_line(pet, 0, disk0, sizeof(disk0));
    disk_mount_line(pet, 1, disk1, sizeof(disk1));
    snprintf(title, sizeof(title), "Mount D64 Drive %d", drive);
    return pick_file(images, count, title, "UP/DOWN ENTER=MOUNT ESC=CANCEL",
                     disk0, disk1);
}

static bool load_prg_from_picker(pet2001_t *pet)
{
    platform_disk_image_t programs[64];
    char path[FILENAME_MAX];
    int count;
    int selected;

    if (pet == NULL) {
        return false;
    }

    count = platform_list_disk_images(PICOCALC_PET_PRG_DIR, programs,
                                      (int)(sizeof(programs) / sizeof(programs[0])));
    count = filter_files_by_extension(programs, count, ".prg");
    if (count <= 0) {
        pet_frontend_refresh_status(pet, "no PRG in /PET2001/PRG");
        return false;
    }

    selected = pick_prg(programs, count);
    restore_pet_screen(pet);
    if (selected < 0) {
        pet_frontend_refresh_status(pet, "PRG load cancelled");
        return false;
    }

    snprintf(path, sizeof(path), PICOCALC_PET_PRG_DIR "/%s", programs[selected].name);
    if (!pet2001_load_prg(pet, path)) {
        pet_frontend_refresh_status(pet,
                                    pet->last_error[0] != '\0' ? pet->last_error
                                                               : "PRG load failed");
        return false;
    }

    snprintf(path, sizeof(path), "loaded %.31s", programs[selected].name);
    pet_frontend_refresh_status(pet, path);
    return true;
}

static bool mount_disk_from_picker(pet2001_t *pet)
{
    platform_disk_image_t images[64];
    char path[FILENAME_MAX];
    char message[PET_FRONTEND_COLS + 1];
    int count;
    int drive;
    int selected;

    if (pet == NULL) {
        return false;
    }

    drive = pick_disk_drive(pet);
    if (drive < 0) {
        restore_pet_screen(pet);
        pet_frontend_refresh_status(pet, "disk mount cancelled");
        return false;
    }

    count = platform_list_disk_images(PICOCALC_PET_DISK_DIR, images,
                                      (int)(sizeof(images) / sizeof(images[0])));
    count = filter_files_by_extension(images, count, ".d64");
    if (count <= 0) {
        restore_pet_screen(pet);
        pet_frontend_refresh_status(pet, "no D64 in /PET2001/DISK");
        return false;
    }

    selected = pick_disk_image(pet, images, count, drive);
    restore_pet_screen(pet);
    if (selected < 0) {
        pet_frontend_refresh_status(pet, "disk mount cancelled");
        return false;
    }

    snprintf(path, sizeof(path), PICOCALC_PET_DISK_DIR "/%s", images[selected].name);
    if (!pet2001_mount_disk(pet, drive, path)) {
        pet_frontend_refresh_status(pet,
                                    pet->last_error[0] != '\0' ? pet->last_error
                                                               : "D64 mount failed");
        return false;
    }

    snprintf(message, sizeof(message), "D%d mounted %.27s", drive,
             images[selected].name);
    pet_frontend_refresh_status(pet, message);
    return true;
}

static bool save_prg_snapshot(pet2001_t *pet)
{
    char message[PET_FRONTEND_COLS + 1];

    if (pet == NULL) {
        return false;
    }

    if (!pet2001_save_prg(pet, PICOCALC_PET_QUICK_SAVE_PATH)) {
        pet_frontend_refresh_status(pet,
                                    pet->last_error[0] != '\0' ? pet->last_error
                                                               : "PRG save failed");
        return false;
    }

    snprintf(message, sizeof(message), "saved SAVED.PRG %04X-%04X",
             pet->last_prg_start, pet->last_prg_end);
    pet_frontend_refresh_status(pet, message);
    return true;
}

static void show_missing_rom_screen(const pet2001_rom_paths_t *paths)
{
    char detail[96];
    char detect[96];
    bool sd_present;

    sd_present = platform_sd_card_present();
    pet_frontend_clear();
    pet_frontend_write_centered(1, "PET 2001 ROM Missing");
    if (sd_present) {
        pet_frontend_write_centered(4, "Copy ROM files to SD:");
    } else {
        pet_frontend_write_centered(4, "Insert an SD card with:");
    }
    pet_frontend_write_line(6, paths->basic);
    pet_frontend_write_line(7, paths->editor);
    pet_frontend_write_line(8, paths->kernal);
    pet_frontend_write_line(9, paths->characters);
    pet_frontend_write_centered(11, "BASIC 4 uses basic4/edit4/kernal4");
    pet_frontend_write_centered(12, "or provide one combined image:");
    pet_frontend_write_line(13, paths->combined);
    snprintf(detect, sizeof(detect), "SD detect: gpio=%d present=%d",
             platform_sd_detect_state(), sd_present ? 1 : 0);
    snprintf(detail, sizeof(detail), "Last probe: %s (%d)",
             platform_last_file_error(), platform_last_file_error_code());
    pet_frontend_write_line(14, detect);
    pet_frontend_write_line(15, detail);
    pet_frontend_status_line(0, PICOCALC_PET_FUNCTION_HELP);
    pet_frontend_status_line(1, "D0:none D1:none");
    pet_frontend_status_line(2, "PET2001 1.00MHz ROM:missing RAM:32K");
    pet_frontend_status_line(3, "Place ROMs under /PET2001/ROMS");
    pet_frontend_flush();
}

static void run_pet_loop(pet2001_t *pet)
{
    uint32_t frame = 0;
#ifdef PICOCALC_PLATFORM
    absolute_time_t next_frame_time = get_absolute_time();
#endif

    if (pet == NULL) {
        return;
    }

    for (;;) {
        int key = PLATFORM_KEY_NONE;

        while (platform_poll_key(&key, false)) {
            if (key == PLATFORM_KEY_BREAK) {
                pet2001_reset(pet);
                pet_frontend_clear();
                if (pet_debug_status) {
                    pet_frontend_refresh_debug_status(pet);
                } else {
                    pet_frontend_refresh_status(pet, "reset");
                }
            } else if (key == PLATFORM_KEY_F1) {
                pet_debug_status = !pet_debug_status;
                if (pet_debug_status) {
                    pet_frontend_refresh_debug_status(pet);
                } else {
                    pet_frontend_refresh_status(pet, "running ROM");
                }
            } else if (key == PLATFORM_KEY_F2) {
                char message[40];
                pet2001_toggle_keyboard_layout(pet);
                snprintf(message, sizeof(message), "keyboard:%s",
                         pet2001_keyboard_layout_name(pet));
                pet_frontend_refresh_status(pet, message);
            } else if (key == PLATFORM_KEY_F3) {
                load_prg_from_picker(pet);
            } else if (key == PLATFORM_KEY_F4) {
                save_prg_snapshot(pet);
            } else if (key == PLATFORM_KEY_F5) {
                mount_disk_from_picker(pet);
            } else {
                pet2001_key_event(pet, key, true);
            }
        }

        pet2001_run_frame(pet);
        pet_frontend_render_video(pet);

        if ((frame & 0x03u) == 0) {
            if (pet_debug_status) {
                pet_frontend_refresh_debug_status(pet);
            } else if (pet->video_writes == 0) {
                pet_frontend_refresh_status(pet, "running ROM; no video writes yet");
            } else {
                pet_frontend_refresh_status(pet, "running ROM");
            }
        }
        pet_frontend_flush();
        frame++;

#ifdef PICOCALC_PLATFORM
        next_frame_time = delayed_by_us(next_frame_time, PICOCALC_PET_FRAME_CYCLES);
        if (absolute_time_diff_us(get_absolute_time(), next_frame_time) < -50000) {
            next_frame_time = get_absolute_time();
        } else {
            sleep_until(next_frame_time);
        }
#endif

#ifndef PICOCALC_PLATFORM
        if (frame >= 60) {
            break;
        }
#endif
    }
}

int main(int argc, char **argv)
{
    pet2001_t pet;
    pet2001_rom_paths_t rom_paths;
    bool sd_present;
    bool rom_found;

    program_name = "PicoCalcPET";

    platform_init();
    pet_frontend_init();
    pet_frontend_show_boot_banner();
    platform_status_puts("Boot: display+input ready");

    pet_frontend_show_boot_stage("Checking SD card...");
    sd_present = platform_sd_card_present();
    status_printf("Boot: SD card %s", sd_present ? "present" : "absent");

    pet_frontend_show_boot_stage("Probing PET ROMs...");
    rom_found = select_pet_rom_paths(argc, argv, &rom_paths);
    if (!rom_found) {
        platform_status_puts("Boot: PET ROMs not found");
        show_missing_rom_screen(&rom_paths);
        return 2;
    }

    status_printf("Boot: ROM %s", rom_paths.combined[0] != '\0'
                  ? path_leaf_name(rom_paths.combined)
                  : path_leaf_name(rom_paths.kernal));

    pet_frontend_show_boot_stage("Initializing PET 2001-8...");
    if (!pet2001_init(&pet) || !pet2001_load_roms(&pet, &rom_paths)) {
        pet_frontend_show_boot_stage("PET initialization failed");
        if (pet.last_error[0] != '\0') {
            pet_frontend_write_line(7, pet.last_error);
            pet_frontend_status_line(3, pet.last_error);
            status_printf("Boot: PET init failed: %s", pet.last_error);
        }
        return 3;
    }

    pet2001_reset(&pet);
#ifdef PICOCALC_PLATFORM
    picocalc_sample_audio_stream(pet_audio_stream_sample, NULL,
                                 PICOCALC_PET_SOUND_SAMPLE_RATE);
#endif
    pet_frontend_show_boot_stage("Running PET ROM...");
    pet2001_step_cycles(&pet, 250000);
    pet_frontend_render_video(&pet);
    if (pet.video_writes == 0) {
        pet_frontend_show_stub_screen(&pet);
    } else if (pet_debug_status) {
        pet_frontend_refresh_debug_status(&pet);
    } else {
        pet_frontend_refresh_status(&pet, "ROM touched video RAM");
    }
    pet_frontend_flush();
    platform_status_puts("Boot: PET ROM loop running");
    run_pet_loop(&pet);

    return 0;
}
