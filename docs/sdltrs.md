# `sdltrs` Reference

## Purpose

This document summarizes the vendored `sdltrs` project as it exists in `third_party/sdltrs/` and describes how it should be used by this repository.

It is a project-local integration reference focused on extracting a TRS-80 Model III emulator core for PicoCalc.

## Upstream Role In This Project

`sdltrs` is the primary emulator reference and source provider for:

- Z80 CPU execution
- TRS-80 machine behavior
- memory map and I/O behavior
- ROM loading logic
- text video behavior
- keyboard matrix behavior
- later disk and peripheral emulation

In this repository it should not be treated as a desktop application to port wholesale.

## Observed Top-Level Layout

Key files and directories in the vendored tree:

- `CMakeLists.txt`
- `BUILDING.md`
- `README.md`
- `src/`
- `html/`
- `diskimages/`
- `icons/`
- `bin/`

The relevant code for this repository is concentrated in `src/`.

## Upstream Build Model

The upstream CMake project:

- builds a single executable named `sdltrs`
- compiles emulator code and SDL frontend code into one target
- expects SDL development files
- optionally enables debugger and readline support

This means the upstream build is not directly suitable for PicoCalc firmware.

## Source Inventory

Observed source files in `src/` include:

- CPU and state
  - `z80.c`, `z80.h`
  - `trs_state_save.c`, `trs_state_save.h`
- memory and I/O
  - `trs_memory.c`, `trs_memory.h`
  - `trs_io.c`
  - `trs_interrupt.c`
- ROM and machine variants
  - `trs_cmd_rom.c`
  - `trs_clones.c`
  - `trs_cp500.c`
- storage and peripherals
  - `trs_disk.c`
  - `trs_hard.c`
  - `trs_stringy.c`
  - `trs_uart.c`
  - `trs_printer.c`
  - `trs_cassette.c`
- desktop/frontend
  - `main.c`
  - `trs_sdl_interface.c`
  - `trs_sdl_gui.c`
  - `trs_sdl_keyboard.c`
  - `PasteManager.c`

## How PicoCalcTRS Should Use It

The recommended project-local use is:

- compile selected emulator/core files from `third_party/sdltrs/src`
- exclude SDL desktop frontend files
- provide replacement host-facing functions in local PicoCalc code
- provide only a tiny SDL compatibility shim for residual dependencies

## File Classification For Embedded Use

### Exclude From The Embedded Build

These files are SDL/frontend oriented:

- `src/main.c`
- `src/trs_sdl_interface.c`
- `src/trs_sdl_gui.c`
- `src/trs_sdl_keyboard.c`
- `src/PasteManager.c`

### Strong Keep Candidates For First Bring-Up

- `src/z80.c`
- `src/error.c`
- `src/trs_memory.c`
- `src/trs_io.c`
- `src/trs_interrupt.c`
- `src/trs_cmd_rom.c`
- `src/trs_clones.c`
- `src/trs_cp500.c`
- `src/trs_state_save.c`

### Later-Phase Candidates

- `src/trs_disk.c`
- `src/trs_hard.c`
- `src/trs_stringy.c`
- `src/trs_uart.c`
- `src/trs_printer.c`
- `src/trs_imp_exp.c`
- `src/trs_cassette.c`

### Optional Tooling

- `src/debug.c`
- `src/dis.c`
- `src/trs_mkdisk.c`

## SDL Coupling Observed In The Vendor Tree

Most SDL use is concentrated in the frontend files, but there are a few leaks outside them.

### Residual SDL use outside frontend files

- `src/trs_interrupt.c`
  - host timing through `SDL_GetTicks()` and `SDL_Delay()`
- `src/trs_cassette.c`
  - SDL audio APIs
- selected headers include `SDL_types.h`
  - `src/z80.h`
  - `src/trs_memory.h`
  - `src/trs_state_save.h`

## Embedded Port Rule

Do not emulate all of SDL.

Instead:

- exclude SDL frontend files
- replace frontend entry points locally
- provide only a narrow SDL shim for timing and integer typedefs

## Host-Facing Functions To Replace Locally

The SDL frontend currently provides functions the core expects through `trs.h`.

These should be implemented in PicoCalc-owned code:

- `trs_sdl_init()`
- `trs_get_event()`
- `trs_screen_init()`
- `trs_screen_reset()`
- `trs_screen_write_char()`
- `trs_screen_update()`
- `trs_screen_mode()`
- `trs_screen_refresh()`
- `trs_screen_caption()`
- `trs_disk_led()`
- `trs_hard_led()`
- `trs_turbo_led()`
- `trs_get_mouse_pos()`
- `trs_set_mouse_pos()`

The mouse hooks can likely remain stubs for early Model III work.

## Keyboard Notes

`trs_sdl_keyboard.c` mixes:

- useful TRS-80 key matrix behavior
- SDL joystick and host event translation

For this repository, the clean first approach is:

- do not compile `trs_sdl_keyboard.c`
- implement the needed keyboard functions locally
- reuse its matrix behavior as a reference, not as a required compiled unit

Local replacements are expected for:

- `trs_kb_mem_read()`
- `trs_kb_reset()`
- `trs_kb_heartbeat()`
- `trs_kb_bracket()`
- `clear_key_queue()`

Optional:

- `trs_xlate_keysym()`

## Audio And Cassette Notes

`trs_cassette.c` is not a good first target for zero-modification vendor integration because it depends on SDL audio behavior.

Recommended policy:

- exclude cassette and sound for the first bring-up
- defer audio integration until ROM boot, screen, and keyboard work

## Build Notes For This Repository

The upstream build expects desktop dependencies:

- SDL2 or SDL 1.2
- optional GNU readline for debugger support

The embedded build in this repository should not depend on those desktop requirements for the first milestone.

Instead, the local build should:

- compile selected source files directly
- include local compatibility headers before SDL headers
- avoid building the upstream executable target

## Files Worth Reading First

For implementation work, these vendored files are the most useful starting points:

- [trs.h](/workspaces/PicoCalcTRS/third_party/sdltrs/src/trs.h)
- [z80.h](/workspaces/PicoCalcTRS/third_party/sdltrs/src/z80.h)
- [z80.c](/workspaces/PicoCalcTRS/third_party/sdltrs/src/z80.c)
- [trs_memory.h](/workspaces/PicoCalcTRS/third_party/sdltrs/src/trs_memory.h)
- [trs_memory.c](/workspaces/PicoCalcTRS/third_party/sdltrs/src/trs_memory.c)
- [trs_io.c](/workspaces/PicoCalcTRS/third_party/sdltrs/src/trs_io.c)
- [trs_interrupt.c](/workspaces/PicoCalcTRS/third_party/sdltrs/src/trs_interrupt.c)
- [trs_cmd_rom.c](/workspaces/PicoCalcTRS/third_party/sdltrs/src/trs_cmd_rom.c)

For comparison and replacement design:

- [trs_sdl_interface.c](/workspaces/PicoCalcTRS/third_party/sdltrs/src/trs_sdl_interface.c)
- [trs_sdl_keyboard.c](/workspaces/PicoCalcTRS/third_party/sdltrs/src/trs_sdl_keyboard.c)

## Summary

For PicoCalcTRS, `sdltrs` is best understood as:

- a mature emulator codebase with valuable machine logic
- a source tree that mixes core emulation with SDL desktop frontend concerns
- something to extract from selectively, not transplant wholesale

The clean embedded path is to keep the vendor tree pristine, compile only the pieces we need, and replace the host layer locally.
