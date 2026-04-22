#ifndef PICOCALC_TRS_OSD_MENU_H
#define PICOCALC_TRS_OSD_MENU_H

#include <stdbool.h>
#include <stdio.h>

void osd_startup_menu(char disk0_path[FILENAME_MAX], bool *disk0_enabled,
                      char disk1_path[FILENAME_MAX], bool *disk1_enabled,
                      int timeout_seconds);
void osd_runtime_menu(void);

#endif
