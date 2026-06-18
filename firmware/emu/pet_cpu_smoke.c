#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "pet2001.h"

int main(void)
{
    pet2001_t pet;

    if (!pet2001_init(&pet)) {
        fprintf(stderr, "pet2001_init failed\n");
        return 1;
    }

    pet.roms_loaded = true;
    pet.cpu.pc = 0x0000;
    pet.cpu.sp = 0xFF;
    pet.cpu.p = P_INTERRUPT | P_UNUSED;

    pet2001_write(&pet, 0x0000, 0xA9); /* LDA #$42 */
    pet2001_write(&pet, 0x0001, 0x42);
    pet2001_write(&pet, 0x0002, 0x85); /* STA $10 */
    pet2001_write(&pet, 0x0003, 0x10);
    pet2001_write(&pet, 0x0004, 0x4C); /* JMP $0005 */
    pet2001_write(&pet, 0x0005, 0x05);
    pet2001_write(&pet, 0x0006, 0x00);

    pet2001_step_cycles(&pet, 24);

    if (pet.cpu.a != 0x42) {
        fprintf(stderr, "A=%02X, expected 42\n", pet.cpu.a);
        return 1;
    }

    if (pet2001_read(&pet, 0x0010) != 0x42) {
        fprintf(stderr, "$0010=%02X, expected 42\n", pet2001_read(&pet, 0x0010));
        return 1;
    }

    if (pet.cpu.pc != 0x0005) {
        fprintf(stderr, "PC=%04X, expected 0005\n", (unsigned int)pet.cpu.pc);
        return 1;
    }

    pet2001_write(&pet, 0x8000, 0x01);
    if (pet.video[0] != 0x01 || !pet.video_dirty[0] || pet.video_writes == 0) {
        fprintf(stderr, "video write tracking failed\n");
        return 1;
    }

    pet2001_write(&pet, 0xE811, 0x04); /* PIA1 port A data register visible */
    pet2001_write(&pet, 0xE810, 0x03); /* select keyboard row 3 */
    if (pet.selected_key_row != 3) {
        fprintf(stderr, "selected_key_row=%u, expected 3\n", pet.selected_key_row);
        return 1;
    }

    pet2001_write(&pet, 0xE813, 0x04); /* PIA1 port B data register visible */
    pet.key_matrix[3] = 0x02;
    if (pet2001_read(&pet, 0xE812) != 0xFD || pet.io_writes == 0 || pet.io_reads == 0) {
        fprintf(stderr, "PET PIA keyboard row read failed: %02X\n", pet2001_read(&pet, 0xE812));
        return 1;
    }

    pet2001_write(&pet, 0xE84E, 0x82);
    if ((pet2001_read(&pet, 0xE84E) & 0x82) != 0x82) {
        fprintf(stderr, "PET VIA IER set/read failed\n");
        return 1;
    }

    pet2001_key_event(&pet, 'A', true);
    if ((pet.key_matrix[4] & 0x01) == 0 || (pet.key_matrix[8] & 0x01) == 0) {
        fprintf(stderr, "PET keyboard matrix latch failed\n");
        return 1;
    }
    for (int i = 0; i < 8; ++i) {
        pet2001_run_frame(&pet);
    }
    if ((pet.key_matrix[4] & 0x01) != 0 || (pet.key_matrix[8] & 0x01) != 0) {
        fprintf(stderr, "PET keyboard matrix release failed\n");
        return 1;
    }

    pet2001_toggle_keyboard_layout(&pet);
    pet2001_key_event(&pet, 'a', true);
    if ((pet.key_matrix[3] & 0x01) == 0) {
        fprintf(stderr, "PET business keyboard matrix failed\n");
        return 1;
    }

    {
        const char *prg_path = "/tmp/pet_cpu_smoke.prg";
        FILE *fp = fopen(prg_path, "wb");
        if (fp == NULL) {
            fprintf(stderr, "failed to create smoke PRG\n");
            return 1;
        }
        fputc(0x01, fp);
        fputc(0x04, fp);
        fputc(0x11, fp);
        fputc(0x22, fp);
        fclose(fp);

        pet.keyboard_layout = PET2001_KEYBOARD_BUSINESS;
        if (!pet2001_load_prg(&pet, prg_path) ||
            pet.ram[0x0401] != 0x11 ||
            pet.ram[0x0402] != 0x22 ||
            pet.ram[0x002A] != 0x03 ||
            pet.ram[0x002B] != 0x04) {
            fprintf(stderr, "PET PRG load failed\n");
            return 1;
        }
        pet2001_key_event(&pet, 'a', true);
        pet2001_key_event(&pet, '4', true);
        pet2001_key_event(&pet, '6', true);
        if ((pet.key_matrix[4] & 0xC1) != 0xC1) {
            fprintf(stderr, "PET PRG graphics-keyboard aliases failed: %02X\n",
                    pet.key_matrix[4]);
            return 1;
        }
        if (!pet2001_save_prg(&pet, "/tmp/pet_cpu_smoke_saved.prg")) {
            fprintf(stderr, "PET PRG save failed: %s\n", pet.last_error);
            return 1;
        }
        fp = fopen("/tmp/pet_cpu_smoke_saved.prg", "rb");
        if (fp == NULL ||
            fgetc(fp) != 0x01 ||
            fgetc(fp) != 0x04 ||
            fgetc(fp) != 0x11 ||
            fgetc(fp) != 0x22) {
            if (fp != NULL) {
                fclose(fp);
            }
            fprintf(stderr, "PET PRG save contents failed\n");
            return 1;
        }
        fclose(fp);

        for (int row = 0; row < 10; ++row) {
            pet.key_matrix[row] = 0;
            for (int col = 0; col < 8; ++col) {
                pet.key_latch_frames[row][col] = 0;
            }
        }
        pet2001_queue_text(&pet, "a\n");
        pet2001_run_frame(&pet);
        if ((pet.key_matrix[3] & 0x01) == 0) {
            fprintf(stderr, "PET typeahead key injection failed\n");
            return 1;
        }
    }

    {
        const char *d64_path = "/tmp/pet_cpu_smoke.d64";

        remove(d64_path);
        setenv("PICOCALC_PET_D64", d64_path, 1);
        pet2001_init(&pet);
        pet.roms_loaded = true;
        pet.cpu.sp = 0xFD;
        pet.ram[0x01FE] = 0x34;
        pet.ram[0x01FF] = 0x12;
        pet.ram[0x0200] = 'T';
        pet.ram[0x0201] = 'E';
        pet.ram[0x0202] = 'S';
        pet.ram[0x0203] = 'T';
        pet.cpu.pc = 0xFFBD;
        pet.cpu.a = 4;
        pet.cpu.x = 0x00;
        pet.cpu.y = 0x02;
        if (!pet2001_kernal_trap(&pet) || pet.cpu.pc != 0x1235) {
            fprintf(stderr, "PET SETNAM trap failed\n");
            return 1;
        }
        pet.cpu.sp = 0xFD;
        pet.ram[0x01FE] = 0x34;
        pet.ram[0x01FF] = 0x12;
        pet.cpu.pc = 0xFFBA;
        pet.cpu.a = 1;
        pet.cpu.x = 8;
        pet.cpu.y = 0;
        if (!pet2001_kernal_trap(&pet)) {
            fprintf(stderr, "PET SETLFS trap failed\n");
            return 1;
        }
        pet.ram[0x002C] = 0x01;
        pet.ram[0x002D] = 0x04;
        pet.ram[0x0401] = 0x11;
        pet.ram[0x0402] = 0x22;
        pet.cpu.sp = 0xFD;
        pet.ram[0x01FE] = 0x34;
        pet.ram[0x01FF] = 0x12;
        pet.cpu.pc = 0xFFD8;
        pet.cpu.a = 0x2C;
        pet.cpu.x = 0x03;
        pet.cpu.y = 0x04;
        if (!pet2001_kernal_trap(&pet) || (pet.cpu.p & P_CARRY) != 0) {
            fprintf(stderr, "PET D64 SAVE trap failed: %s\n", pet.last_error);
            return 1;
        }
        pet.ram[0x0401] = 0;
        pet.ram[0x0402] = 0;
        pet.cpu.sp = 0xFD;
        pet.ram[0x01FE] = 0x34;
        pet.ram[0x01FF] = 0x12;
        pet.cpu.pc = 0xFFD5;
        pet.cpu.a = 0;
        pet.cpu.x = 0;
        pet.cpu.y = 0;
        if (!pet2001_kernal_trap(&pet) || (pet.cpu.p & P_CARRY) != 0 ||
            pet.ram[0x0401] != 0x11 ||
            pet.ram[0x0402] != 0x22) {
            fprintf(stderr, "PET D64 LOAD trap failed: %s\n", pet.last_error);
            return 1;
        }

        pet.ram[0x0401] = 0;
        pet.ram[0x0402] = 0;
        pet.ram[0x0200] = 'T';
        pet.ram[0x0201] = 'E';
        pet.ram[0x0202] = 'S';
        pet.ram[0x0203] = 'T';
        pet.ram[0x00D1] = 4;
        pet.ram[0x00D3] = 0x60;
        pet.ram[0x00D4] = 8;
        pet.ram[0x00DA] = 0x00;
        pet.ram[0x00DB] = 0x02;
        pet.cpu.sp = 0xFB;
        pet.ram[0x01FC] = 0x73;
        pet.ram[0x01FD] = 0xF3;
        pet.ram[0x01FE] = 0x17;
        pet.ram[0x01FF] = 0xF4;
        pet.cpu.pc = 0xF4A5;
        if (!pet2001_kernal_trap(&pet) ||
            (pet.cpu.p & P_CARRY) != 0 ||
            pet.cpu.pc != 0xF418 ||
            pet.ram[0x0401] != 0x11 ||
            pet.ram[0x0402] != 0x22) {
            fprintf(stderr, "PET BASIC4 D64 open trap failed: %s\n", pet.last_error);
            return 1;
        }
    }

    puts("VICE 6502 smoke passed");
    return 0;
}
