#ifndef PICOCALC_PET2001_H
#define PICOCALC_PET2001_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "mos6510.h"

#ifndef FILENAME_MAX
#define FILENAME_MAX 260
#endif

enum {
    PET2001_RAM_SIZE = 0x8000
};

typedef enum {
    PET2001_KEYBOARD_GRAPHICS = 0,
    PET2001_KEYBOARD_BUSINESS = 1
} pet2001_keyboard_layout_t;

typedef struct {
    char combined[FILENAME_MAX];
    char basic[FILENAME_MAX];
    char editor[FILENAME_MAX];
    char kernal[FILENAME_MAX];
    char characters[FILENAME_MAX];
    pet2001_keyboard_layout_t keyboard_layout;
} pet2001_rom_paths_t;

typedef struct {
    uint8_t port_a;
    uint8_t port_b;
    uint8_t ddr_a;
    uint8_t ddr_b;
    uint8_t ctrl_a;
    uint8_t ctrl_b;
} pet2001_pia_t;

typedef struct {
    uint8_t reg[16];
} pet2001_via_t;

typedef struct pet2001_t {
    uint8_t ram[PET2001_RAM_SIZE];
    uint8_t video[1000];
    bool video_dirty[1000];
    uint8_t basic_rom[0x3000];
    uint8_t editor_rom[0x0800];
    uint8_t kernal_rom[0x1000];
    uint8_t character_rom[0x1000];
    uint8_t io[0x0800];
    uint8_t key_matrix[10];
    uint8_t key_latch_frames[10][8];
    uint16_t typeahead[64];
    pet2001_pia_t pia1;
    pet2001_pia_t pia2;
    pet2001_via_t via;
    mos6510_regs_t cpu;
    uint16_t pc;
    uint16_t basic_rom_base;
    uint16_t basic_rom_size;
    uint16_t last_prg_start;
    uint16_t last_prg_end;
    uint32_t last_prg_size;
    char cbm_filename[64];
    uint8_t cbm_filename_len;
    uint8_t cbm_logical_file;
    uint8_t cbm_device;
    uint8_t cbm_secondary;
    uint32_t cycles_executed;
    uint32_t io_reads;
    uint32_t io_writes;
    uint32_t video_writes;
    uint32_t cbm_load_attempts;
    uint32_t cbm_directory_loads;
    char cbm_disk_path[2][FILENAME_MAX];
    bool cbm_disk_mounted[2];
    uint16_t last_io_read_addr;
    uint16_t last_io_write_addr;
    uint8_t last_io_read_value;
    uint8_t last_io_write_value;
    uint8_t selected_key_row;
    uint16_t key_row_scan_mask;
    uint32_t key_row_changes;
    uint32_t frames_executed;
    uint8_t retrace_poll_counter;
    uint8_t typeahead_head;
    uint8_t typeahead_tail;
    uint8_t typeahead_wait_frames;
    int cpu_rmw_flag;
    bool retrace_signal;
    bool roms_loaded;
    pet2001_keyboard_layout_t keyboard_layout;
    char last_error[96];
    pet2001_rom_paths_t rom_paths;
} pet2001_t;

bool pet2001_init(pet2001_t *pet);
bool pet2001_load_roms(pet2001_t *pet, const pet2001_rom_paths_t *paths);
bool pet2001_load_prg(pet2001_t *pet, const char *path);
bool pet2001_save_prg(pet2001_t *pet, const char *path);
void pet2001_reset(pet2001_t *pet);
void pet2001_run_frame(pet2001_t *pet);
void pet2001_step_cycles(pet2001_t *pet, uint32_t cycles);
void pet2001_key_event(pet2001_t *pet, int platform_key, bool pressed);
bool pet2001_queue_text(pet2001_t *pet, const char *text);
bool pet2001_kernal_trap(pet2001_t *pet);
bool pet2001_mount_disk(pet2001_t *pet, int drive, const char *path);
void pet2001_unmount_disk(pet2001_t *pet, int drive);
void pet2001_toggle_keyboard_layout(pet2001_t *pet);
const char *pet2001_keyboard_layout_name(const pet2001_t *pet);
uint8_t pet2001_read(pet2001_t *pet, uint16_t address);
void pet2001_write(pet2001_t *pet, uint16_t address, uint8_t value);

#endif
