# PicoCalc PET 2001 Emulator Codex Specification

## Purpose

Create a new ClockworkPi PicoCalc RP2350 firmware project for a Commodore PET 2001 emulator, using the same integration principles proven by PicoCalcTRS.

This file is intended to be handed to Codex as the implementation specification for the new repository. The desired result is not a desktop emulator port. It is a Pico SDK firmware image that boots directly into an embedded PET 2001 machine on the PicoCalc.

## Existing Repo Lessons To Reuse

PicoCalcTRS is structured around three local layers:

- `firmware/platform/`: PicoCalc hardware services, SD/FAT access, timing, display, keyboard, status UI.
- `firmware/frontend/`: host-facing emulator adapters for screen, keyboard, reset, audio, status indicators, and runtime event pumping.
- `firmware/emu/`: machine-specific policy and peripheral stubs.

Keep this separation. The PET emulator core must not call PicoCalc starter drivers directly. All hardware access must go through a local platform API.

PicoCalcTRS treats upstream emulator projects as read-only vendors:

- vendor trees live under `third_party/`
- local firmware code lives under `firmware/`
- unavoidable vendor changes are kept as patch files under `patches/`
- CMake copies and patches selected vendor files into the build directory, then compiles those patched copies
- desktop frontends are excluded instead of shimmed wholesale

Use the same policy for the PET project.

## Project Name And Layout

Recommended new repository name:

```text
PicoCalcPET
```

Recommended initial layout:

```text
.
|-- CMakeLists.txt
|-- README.md
|-- docs/
|   |-- pet2001-codex-spec.md
|   |-- porting-plan.md
|   |-- vendor-integration.md
|   |-- pico-build.md
|   `-- release-checklist.md
|-- firmware/
|   |-- CMakeLists.txt
|   |-- main.c
|   |-- compat/
|   |-- emu/
|   |-- frontend/
|   |-- platform/
|   `-- assets/
|-- fonts/
|-- patches/
|   |-- picocalc-text-starter/
|   `-- vice/
|-- roms/
|-- scripts/
`-- third_party/
    |-- picocalc-text-starter/
    `-- vice/
```

The current PicoCalcTRS workspace has submodule declarations for `picocalc-text-starter` and `sdltrs`, but the vendor directories are not checked out in this local copy. In the new PET repo, initialize vendors explicitly and keep setup documented.

## Target Machine

Target the original Commodore PET 2001 first.

Minimum model:

- MOS 6502 CPU at approximately 1 MHz.
- PET 2001-8 memory profile first: 8 KB RAM at `$0000-$1FFF`.
- Optional later PET 2001-4 mode: 4 KB RAM at `$0000-$0FFF`.
- 40 x 25 text display.
- Video RAM at `$8000`, mirrored through the `$8xxx` range on early PETs.
- I/O at `$E800-$EFFF`.
- ROM regions:
  - `$C000-$DFFF`: BASIC ROM area.
  - `$E000-$E7FF`: editor ROM.
  - `$F000-$FFFF`: kernal ROM, including reset/IRQ/NMI vectors.
- Devices needed for first boot:
  - 6502 CPU.
  - RAM/ROM/video memory map.
  - PETSCII character display.
  - keyboard matrix through PET PIA behavior.
  - enough VIA/PIA behavior for ROM keyboard scan, cursor, timers, and basic startup.

Defer these until the ROM, screen, and keyboard are stable:

- IEEE-488 disk drives.
- Datasette data I/O.
- printer/user port.
- cycle-perfect video blanking/snow behavior.
- SuperPET, 80-column PETs, BASIC 2/4, CBM 3000/4000/8000 families.

Useful public references:

- VICE official site states that VICE emulates the PET family and provides separate emulators for Commodore 8-bit machines: https://vice-emu.sourceforge.io/
- VICE manual PET section says the PET emulator covers 2001, 3032, 4032, 8032, 8096, 8296, and SuperPET models, and identifies PET 2001 as BASIC 1: https://vice-emu.sourceforge.io/vice_1.html
- Commodore PET overview and model summary, including 6502, 40 x 25 display, RAM/ROM notes, PIA/VIA devices: https://en.wikipedia.org/wiki/Commodore_PET
- PET 2001 memory map summary with RAM, video RAM, I/O, and ROM regions: https://de.wikipedia.org/wiki/PET_2001

## Upstream Emulator Strategy

Use VICE as the primary PET behavior reference.

Preferred vendor:

```text
third_party/vice/
```

Recommended approach:

- Treat VICE as a reference and source provider, not as an application to port wholesale.
- Start by reading VICE PET machine files, 6502 core, PIA/VIA files, PET keyboard, PET video, ROM loader, and memory map code.
- Exclude GTK, SDL, monitor UI, command-line UI, file dialogs, snapshots, network, printer frontends, and full drive emulation from the first embedded build.
- Compile only a small selected core subset if practical. If the VICE dependency graph is too large, reimplement the PET 2001 core locally using VICE as a behavioral reference.

Important licensing note:

- VICE is GPL-licensed. If compiling VICE code into the firmware, the new project and distributed firmware must be GPL-compatible and provide corresponding source as required.
- If preserving a permissive license is important, do not compile VICE code. Instead, use a permissively licensed 6502 core and write local PET 2001 memory, video, keyboard, PIA, and VIA logic from public documentation and behavior tests.

Recommended first decision for Codex:

1. Inspect VICE source structure after vendoring.
2. Classify PET-relevant files into `keep`, `reference only`, and `exclude`.
3. Prefer a small local PET 2001 core if VICE integration pulls in broad desktop/resource infrastructure.

## PicoCalc Platform Strategy

Reuse the PicoCalcTRS platform model almost directly.

Keep these ideas:

- `platform_init()`
- `platform_poll_key()`
- `platform_screen_configure(cols, rows)`
- `platform_screen_write_cell(col, row, ch, mode)`
- `platform_screen_flush()`
- `platform_status_*()`
- `platform_file_exists()`
- `platform_fopen()` bridge for FAT32 and embedded ROM data
- host platform stubs for non-Pico builds

Adjust for PET:

- Screen geometry is 40 x 25, not 64 x 16.
- PicoCalc display is 320 x 320. A 40 x 25 PET display fits naturally with 8 x 10 cells: 320 x 250 pixels, leaving about 70 pixels for a compact operator panel.
- Keep a PET-specific font under `fonts/`, preferably an 8 x 10 or 8 x 8 PETSCII font rendered at 8 x 10 cell size.
- Use monochrome green or white PET-style phosphor colors, plus restrained cyan/status colors if useful.
- Do not use the TRS-80 5 x 18 font path.

Recommended SD-card layout:

```text
/PET2001/ROMS/basic1.bin
/PET2001/ROMS/edit1.bin
/PET2001/ROMS/kernal1.bin
/PET2001/ROMS/characters.bin
/PET2001/PRG/hello.prg
/PET2001/TAPES/demo.tap
```

Also allow a single combined ROM image if that is easier for early bring-up:

```text
/PET2001/ROMS/pet2001.rom
```

Embed a fallback ROM only if legally redistributable in the repository. Otherwise support embedded ROM hooks but keep them disabled unless the user places ROM files locally.

## Firmware Boot Flow

`firmware/main.c` should mirror the PicoCalcTRS boot style:

1. Set `program_name = "PicoCalcPET"`.
2. Initialize platform/display/input.
3. Show a boot banner.
4. Check SD presence.
5. Probe PET ROM files under `/PET2001/ROMS`.
6. If ROM files are missing, show a clear missing-ROM screen with exact paths and last FAT error.
7. Initialize PET machine:
   - configure PET 2001-8 model
   - load BASIC/editor/kernal/character ROMs
   - clear RAM and video RAM
   - reset CPU from the 6502 reset vector
8. Start the emulation loop.

Initial command-line/env overrides for host builds:

- `PICOCALC_PET_ROM`
- `PICOCALC_PET_BASIC_ROM`
- `PICOCALC_PET_EDITOR_ROM`
- `PICOCALC_PET_KERNAL_ROM`
- `PICOCALC_PET_CHAR_ROM`

## Emulator Core API

Create a local PET API independent from VICE or any particular CPU core:

```c
typedef struct pet2001_t pet2001_t;

bool pet2001_init(pet2001_t *pet);
bool pet2001_load_roms(pet2001_t *pet, const pet2001_rom_paths_t *paths);
void pet2001_reset(pet2001_t *pet);
void pet2001_run_frame(pet2001_t *pet);
void pet2001_step_cycles(pet2001_t *pet, uint32_t cycles);
void pet2001_key_event(pet2001_t *pet, int platform_key, bool pressed);
uint8_t pet2001_read(pet2001_t *pet, uint16_t address);
void pet2001_write(pet2001_t *pet, uint16_t address, uint8_t value);
```

The frontend should call this API. The platform should not know about PET internals.

For the first implementation, a simple loop is acceptable:

- poll keyboard
- execute a batch of 6502 cycles
- refresh dirty screen cells
- update status panel
- pace to approximately 1 MHz

Use wall-clock pacing through Pico SDK time, analogous to the `SDL_GetTicks()` / `SDL_Delay()` compatibility shim in PicoCalcTRS.

## Memory Map Requirements

Implement PET 2001-8 first:

```text
$0000-$1FFF  RAM
$2000-$7FFF  open/unmapped or expansion, read as stable open-bus policy
$8000-$83E7  1000-byte screen RAM for 40x25
$83E8-$8FFF  video mirror/open behavior, acceptable to mirror for first milestone
$9000-$BFFF  expansion ROM sockets, initially unmapped
$C000-$DFFF  BASIC ROM
$E000-$E7FF  editor ROM
$E800-$EFFF  I/O: PIA/VIA devices
$F000-$FFFF  kernal ROM
```

Writes to ROM are ignored. Writes to video RAM update the screen dirty map. Reads from unimplemented I/O must return deterministic values compatible with ROM boot; refine as tests discover missing behavior.

## Video Requirements

First milestone:

- Render 40 columns by 25 rows.
- Keep a 1000-cell PET video shadow.
- Render only dirty cells where possible.
- Translate PET screen codes to glyph indices.
- Support reverse video bit if used by the ROM/editor.
- Use a PET character ROM if available; otherwise use a generated PETSCII-compatible font as a placeholder and document the limitation.

PicoCalc mapping:

- Cell width: 8 pixels.
- Cell height: 10 pixels.
- PET screen area: 320 x 250.
- Status panel: remaining lower area.

Do not build a bitmap framebuffer for the whole display unless needed. Blit glyph cells like PicoCalcTRS does for TRS cells.

## Keyboard Requirements

Implement a PET graphics-keyboard matrix, not a generic ASCII terminal.

First milestone behavior:

- Map PicoCalc printable keys to PET symbolic keys.
- Implement Shift behavior explicitly.
- Map Enter/Return, Backspace/Delete, cursor keys, Home/Clear, Stop/Run, and PET special characters.
- Provide local control shortcuts:
  - Esc: PET STOP/RUN STOP.
  - Shift+Esc or PicoCalc BRK: PET reset.
  - F1: optional help/status overlay.
  - F5: optional sound toggle later.

VICE notes that PET 2001 used the original graphics/chiclet keyboard and that practical emulators often use the graphics keyboard matrix for it. Follow that strategy for the first version.

Use the PicoCalcTRS keyboard implementation as a pattern:

- maintain matrix state
- maintain a short key queue or synthetic tap state
- stretch key presses long enough for ROM scanning
- separate platform key symbols from emulated matrix bits

## PIA/VIA Requirements

The PET ROM depends on MOS 6520 PIA and MOS 6522 VIA behavior.

First implementation must support enough for:

- keyboard row selection and matrix read
- interrupt/timer behavior required for cursor blink and keyboard scan
- VIA port defaults used by ROM startup

Recommended order:

1. Implement register storage and DDR behavior for PIA/VIA.
2. Wire keyboard matrix through the correct PIA port behavior.
3. Add timer countdown/interrupt behavior only as needed for ROM/editor stability.
4. Keep unimplemented IEEE-488 and cassette bits deterministic.

## Storage Scope

First milestone: no disk, no tape data I/O.

Second milestone:

- add PRG loader from `/PET2001/PRG`
- implement a startup file picker similar to the TRS disk picker
- inject/load PRG through a ROM-friendly path or a monitor-style helper

Later:

- TAP/T64 tape support
- IEEE-488 disk image support through VICE drive code or a simplified device layer

Do not pull in VICE full drive emulation until the base PET is stable.

## Audio Scope

Original PET 2001 has no standard rich sound chip. Some machines/software use a simple beeper on VIA CB2 or an external speaker mod.

First milestone:

- no audio

Later:

- optional square-wave beeper driven by relevant VIA output transitions
- F5 audio toggle and status indicator, mirroring PicoCalcTRS ergonomics

## CMake Requirements

Use the same top-level pattern as PicoCalcTRS:

- `PICOCALC_PLATFORM=ON` enables Pico SDK and PicoCalc starter drivers.
- Default host build remains possible for link/test iteration.
- `PICO_PLATFORM=rp2350`, `PICO_BOARD=pico2`.
- local compatibility headers precede vendor include paths.
- selected vendor files are compiled into static libraries.
- local firmware target links `pet_core` and `pet_frontend`.

Suggested targets:

```text
pet_core
pet_frontend
picocalc_vendor_headers
PicoCalcPET
```

If using patched vendor files:

- copy vendor file to `${CMAKE_CURRENT_BINARY_DIR}/patched-vice/...`
- apply patch with `patch`
- compile the patched copy
- keep original vendor tree untouched

## Compatibility Layer

Do not create a broad SDL/GTK/VICE host environment.

Allow only tiny compatibility shims:

- integer typedefs if vendor headers need them
- time/tick functions
- file-open bridge to `platform_fopen()`
- simple logging macros
- allocation wrappers only if required

If a compile attempt requires window creation, menu APIs, resource UI, monitor UI, sockets, or OS dialogs, the source selection is too broad.

## Operator Panel

Keep a compact PET operator panel in the bottom display area.

Suggested lines:

```text
PET2001 1.00MHz ROM:SD PC:FCE2 RAM:32K
KBD:ready TAPE:none PRG:none
ESC=STOP BRK=RESET F1=HELP
```

During diagnostics, replace the bottom line with transient messages:

- ROM loaded
- missing ROM details
- keymap mode
- reset
- PRG selected/loaded

## Milestones

### M1: Firmware Shell

Success criteria:

- Pico SDK build configures.
- `PicoCalcPET` target builds for host and Pico.
- PicoCalc display, keyboard, SD-card probe, and missing-ROM screen work.
- No PET emulation yet beyond a stub screen.

### M2: ROM Boot Skeleton

Success criteria:

- 6502 core linked.
- PET memory map implemented.
- ROM images load from SD or host path.
- CPU reset vector is read and execution begins.
- Host build can run a short deterministic stepping test.

### M3: PET Text Display

Success criteria:

- Writes to `$8000` render to 40 x 25 PicoCalc cells.
- PETSCII/screen-code glyph mapping is readable.
- ROM startup screen is visible or enough trace exists to identify the missing device behavior.

### M4: Keyboard And Editor

Success criteria:

- PET BASIC prompt accepts typed commands.
- `PRINT`, cursor movement, delete, shift symbols, and STOP/RESET mappings work.
- Key repeat/tap stretching is usable.

### M5: Timing And Stability

Success criteria:

- emulator paces near 1 MHz
- screen refresh and keyboard polling remain responsive
- BASIC can run loops without watchdog-style stalls
- reset returns to a clean prompt repeatedly

### M6: PRG Loader

Success criteria:

- Startup picker lists `.prg` files under `/PET2001/PRG`
- selected PRG can be loaded into RAM
- simple PET 2001 BASIC programs run

## Regression Script Requirements

Create `scripts/regression-m1.sh` patterned after PicoCalcTRS.

Initial checks:

- Pico UF2 build completes when SDK/tooling is available.
- expected target name is `PicoCalcPET`.
- runtime ROM directory is `/PET2001/ROMS`.
- PET screen geometry code contains 40 x 25.
- no SDL/GTK desktop frontend files are compiled.
- vendor patch files, if any, exist and are applied only during build.
- missing-ROM screen names all required ROM paths.
- operator panel is wired.
- host build succeeds if `RUN_HOST_BUILD=1`.

## Acceptance Criteria For First Usable Release

The first release is acceptable when:

- firmware boots on PicoCalc RP2350
- PET 2001 BASIC 1 reaches a visible prompt
- keyboard supports normal BASIC entry
- reset shortcut works
- screen is readable and stable
- SD-card ROM loading has clear errors
- build helper emits `dist/PicoCalcPET.uf2`
- release docs describe ROM placement and known limitations

Explicit first-release non-goals:

- full IEEE-488 disk emulation
- cassette data I/O
- printer/user-port support
- all PET model variants
- cycle-perfect obscure video behavior
- save states

## Codex Implementation Guidance

When implementing, first inspect existing PicoCalcTRS files and copy the shape, not the TRS-specific machine behavior.

Start from:

- `firmware/platform/platform.h`
- `firmware/platform/platform_picocalc.c`
- `firmware/platform/platform_file_picocalc.c`
- `firmware/frontend/trs_frontend_stub.c`
- `firmware/frontend/trs_keyboard_stub.c`
- `firmware/CMakeLists.txt`
- `scripts/build-pico-uf2.sh`
- `scripts/regression-m1.sh`

Rename and adapt these into PET-specific modules:

```text
trs_frontend_stub.c      -> pet_frontend.c
trs_keyboard_stub.c      -> pet_keyboard.c
picocalc_reset_policy.c  -> pet_reset_policy.c, if needed
sdl_compat.c             -> vice_compat.c or remove if not compiling VICE
```

Keep changes small and milestone-driven. Do not attempt to integrate full VICE, drive emulation, printer, monitor, screenshots, settings UI, and PET variants in the first pass. The first valuable machine is a booting PET 2001 BASIC prompt with correct text and keyboard.
