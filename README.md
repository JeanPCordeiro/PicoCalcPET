# PicoCalcPET

Commodore PET 2001 emulator firmware for the ClockworkPi PicoCalc with an RP2350 board.

This repository is being brought up from the proven PicoCalcTRS firmware shape, but the target machine is now a 32K Commodore PET 2001-family machine. The current firmware boots PET ROMs on the PicoCalc using the VICE 6502 core with local PET memory, display, keyboard, PIA, and VIA glue.

## Current Status

Firmware bring-up status:

- firmware target/artifact name is `PicoCalcPET`
- VICE 3.9 is vendored under `third_party/vice`
- host builds do not require `sdltrs`
- PET ROM lookup uses `/PET2001/ROMS`
- host overrides support `PICOCALC_PET_ROM`, `PICOCALC_PET_BASIC_ROM`, `PICOCALC_PET_EDITOR_ROM`, `PICOCALC_PET_KERNAL_ROM`, and `PICOCALC_PET_CHAR_ROM`
- the display shell is configured for 40 x 25 PET text
- the emulated PET RAM is 32K at `$0000-$7FFF`
- the operator panel reports normal PET ROM/RAM/keyboard/tape/PRG status
- `F1` toggles debug status with PC, cycle, video, PIA/VIA, and last-I/O telemetry
- the local core uses VICE `mos6510_regs_t` CPU register definitions
- VICE `6510core.c` executes instructions through a local adapter
- IRQ delivery is wired into the VICE CPU adapter
- ROM bytes are loaded into PET memory regions and reset reads the kernal reset vector
- 8K BASIC 1/2 images map at `$C000-$DFFF`; 12K BASIC 4 images map at `$B000-$DFFF`
- PIA1, PIA2, and VIA register shells are present at `$E810`, `$E820`, and `$E840`
- PIA1 keyboard row select/read behavior is implemented with an internal matrix
- PicoCalc key events are mapped into the PET graphics-keyboard matrix
- dirty video RAM cells are rendered to the PicoCalc text display
- the firmware now runs a continuous PET CPU/video/status loop after ROM startup
- PET BASIC ROM boot, screen output, and keyboard input have been verified on device
- full PETSCII glyph mapping, tape/disk, and more accurate timer/CRTC behavior are still pending

Expected SD-card layout:

```text
/PET2001/ROMS/basic1.bin
/PET2001/ROMS/edit1.bin
/PET2001/ROMS/kernal1.bin
/PET2001/ROMS/characters.bin
```

BASIC 4 uses a matched split ROM profile:

```text
/PET2001/ROMS/basic4.bin
/PET2001/ROMS/edit4.bin
/PET2001/ROMS/kernal4.bin
/PET2001/ROMS/characters.bin
```

The firmware probes the complete BASIC 1 profile first, then the complete BASIC 4 profile. Do not mix `basic4.bin` with `edit1.bin` or `kernal1.bin`.

A single combined image is also probed:

```text
/PET2001/ROMS/pet2001.rom
```

The PRG picker lists `.prg` files found under:

```text
/PET2001/PRG
```

Press `F3` after BASIC reaches `READY.`, choose a program with Up/Down, press Enter to load, then type `RUN` and press Return. Esc cancels the picker.

The virtual CBM disk device 8 uses one D64 image:

```text
/PET2001/DISK/pet.d64
```

`LOAD "NAME",8` and `SAVE "NAME",8` are trapped at the PET KERNAL boundary and map PRG files inside that D64 image. If the image does not exist, the first save creates a blank 35-track D64. This is file-level D64 support, not a full IEEE-488 drive CPU yet.

## Build

Host smoke build:

```sh
cmake -S . -B build-host -DPICOCALC_PLATFORM=OFF
cmake --build build-host -j"$(nproc)"
./build-host/firmware/PicoCalcPET
```

M1 regression:

```sh
./scripts/regression-m1.sh
```

Pico UF2 builds require `third_party/picocalc-text-starter` and Pico SDK tooling:

```sh
./scripts/build-pico-uf2.sh
```

The helper emits `dist/PicoCalcPET.uf2` when the Pico vendor/tooling tree is available.

## Vendor Policy

Vendor trees live under `third_party/` and should stay read-only. Local code lives under `firmware/`; unavoidable vendor edits belong under `patches/`.

VICE is the PET behavior source under `third_party/vice`. The current build includes VICE CPU headers and keeps the executable core local while the VICE CPU/PET files are integrated behind the `pet2001_*` API.

See [docs/vice-pet-integration.md](/workspaces/PicoCalcPET/docs/vice-pet-integration.md).

## Next Milestones

1. Refine PETSCII glyph mapping with a real PET font/character ROM renderer.
2. Improve timer/CRTC behavior against VICE for software that depends on tighter timing.
3. Expand keyboard coverage for every PicoCalc symbol/modifier.
4. Add optional tape/disk support.

The detailed implementation spec lives in [docs/pet2001-codex-spec.md](/workspaces/PicoCalcPET/docs/pet2001-codex-spec.md).
