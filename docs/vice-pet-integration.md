# VICE PET Integration

This project vendors VICE under `third_party/vice` and uses it as the PET implementation source.

Current integration state:

- VICE 3.9 source is unpacked under `third_party/vice`.
- `firmware/compat/vice/config.h` provides the small generated-config surface needed to include VICE headers in Pico/host builds.
- `pet_core` includes VICE CPU register definitions through `mos6510.h`.
- `firmware/emu/vice_6502_cpu.c` includes VICE `6510core.c` inside a narrow local adapter.
- VICE opcode execution is routed through `pet2001_read()` and `pet2001_write()`.
- A host smoke test executes `LDA #$42; STA $0010; JMP $0005` through the VICE core.
- The local PET core now has PIA1, PIA2, and VIA register shells under `$E810`, `$E820`, and `$E840`.
- PIA1 keyboard row select/read behavior follows VICE's PET notes: port A low nibble selects the row, port B returns active-low row bits.
- I/O activity counters expose ROM access progress in the operator panel.
- The status panel shows `KR` for the current keyboard row and `S` for the 10-bit row scan mask.
- Firmware now stays in a continuous CPU/video/status loop on Pico builds.
- Dirty PET video RAM cells are rendered by `pet_frontend_render_video()`.
- The local PET core still owns the embedded memory map and firmware-facing API.
- ROM files are loaded into PET 2001 regions locally:
  - BASIC: `$C000-$DFFF`
  - editor: `$E000-$E7FF`
  - kernal: `$F000-$FFFF`
  - character ROM: local character buffer
- Reset initializes VICE `mos6510_regs_t` state and reads the 6502 reset vector from kernal offsets `$0FFC-$0FFD`.

Next integration step:

1. Map PicoCalc key events into the PET keyboard matrix.
2. Add enough VIA timer/IFR behavior for cursor/editor stability.
3. Refine PETSCII/screen-code glyph mapping and reverse video.
4. Pull fuller PET VIA/PIA behavior from VICE files or wrap those files behind the same local API boundary.

Do not compile VICE desktop UI, monitor UI, snapshots, printer, drive emulation, file dialogs, GTK, SDL, or network code for the embedded target.
