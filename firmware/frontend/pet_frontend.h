#ifndef PICOCALC_PET_FRONTEND_H
#define PICOCALC_PET_FRONTEND_H

#include "emu/pet2001.h"

enum {
    PET_FRONTEND_COLS = 40,
    PET_FRONTEND_ROWS = 25,
    PET_FRONTEND_STATUS_LINES = 4
};

void pet_frontend_init(void);
void pet_frontend_clear(void);
void pet_frontend_write_line(int row, const char *text);
void pet_frontend_write_centered(int row, const char *text);
void pet_frontend_status_line(int line, const char *text);
void pet_frontend_flush(void);
void pet_frontend_show_boot_banner(void);
void pet_frontend_show_boot_stage(const char *text);
void pet_frontend_show_stub_screen(const pet2001_t *pet);
void pet_frontend_render_video(pet2001_t *pet);
void pet_frontend_refresh_status(const pet2001_t *pet, const char *message);
void pet_frontend_refresh_debug_status(const pet2001_t *pet);

#endif
