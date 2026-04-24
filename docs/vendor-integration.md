# Vendor Integration Specification

## Goal

Use both upstream projects as read-only vendors:

- `picocalc-text-starter`
- `sdltrs`

The local project must integrate them without modifying either upstream source tree in place.

## Non-Goals

This project should not:

- rewrite `picocalc-text-starter`
- convert `sdltrs` into a Pico SDK project inside the vendor tree
- maintain ad hoc edits directly inside `third_party/`
- emulate the full SDL API just to keep the desktop frontend alive

## Source Of Truth

The intended structure is:

```text
.
├── docs/
│   ├── porting-plan.md
│   └── vendor-integration.md
├── firmware/
│   ├── compat/
│   ├── frontend/
│   ├── platform/
│   └── emu/
└── third_party/
    ├── picocalc-text-starter/
    └── sdltrs/
```

All PicoCalc-specific logic belongs in `firmware/`.

All vendor code remains in `third_party/`.

## Vendor Policy

The repository should treat both upstreams as immutable inputs.

Preferred import methods:

1. `git submodule`
2. `git subtree`
3. copied vendor snapshot if simplicity matters more than update history

Recommended choice:

- use `git submodule` for both vendors

Why:

- upstream history remains intact
- updates are explicit
- local integration code stays separate
- it discourages accidental in-place edits

## Integration Model

The firmware should be built from three local layers around the vendor trees.

### 1. Platform Layer

Wraps reusable PicoCalc starter facilities:

- display
- keyboard
- SD card
- timing
- serial/debug

This layer depends on `picocalc-text-starter`.

### 2. Emulator Layer

Compiles selected `sdltrs` source files that implement TRS-80 behavior:

- Z80 CPU
- memory map
- I/O
- interrupt logic
- ROM loading
- optional peripherals later

This layer depends on `sdltrs`.

### 3. Frontend And Compatibility Layer

Provides the host-facing functions that the `sdltrs` core expects, but implemented for PicoCalc instead of SDL desktop APIs.

This layer is local to the repo.

## `picocalc-text-starter` Integration Rules

The starter project should be used as a platform dependency, not as the main application to be edited.

Allowed uses:

- include its headers
- link its reusable modules
- follow its initialization pattern
- reuse its drivers and board support code

Avoid:

- editing demo code in the vendor tree
- repurposing its example application as the long-term home of emulator code

Instead, create a local firmware app that calls into starter-provided modules.

## `sdltrs` Integration Rules

The `sdltrs` tree should be treated as an emulator code vendor, not as a desktop application to port wholesale.

Allowed uses:

- compile selected core source files directly from `third_party/sdltrs/src`
- include vendor headers
- satisfy required host symbols from local PicoCalc code

Avoid:

- compiling the SDL desktop frontend
- emulating all of SDL
- modifying vendor source files directly unless a formal patch workflow is introduced

## `sdltrs` File Classification

The following classification comes from inspection of the current upstream source layout.

### Exclude From PicoCalc Build

These files are SDL/frontend oriented and should not be compiled for the embedded target:

- `src/main.c`
- `src/trs_sdl_interface.c`
- `src/trs_sdl_gui.c`
- `src/trs_sdl_keyboard.c`
- `src/PasteManager.c`

Why they should be excluded:

- desktop window creation
- SDL event loop
- clipboard integration
- menu/dialog UI
- joystick enumeration through SDL
- SDL surface and renderer logic

### Strong Keep Candidates

These are the primary files to evaluate for the first embedded build:

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

Bring these in after the machine boots and text I/O works:

- `src/trs_disk.c`
- `src/trs_hard.c`
- `src/trs_stringy.c`
- `src/trs_uart.c`
- `src/trs_printer.c`
- `src/trs_imp_exp.c`
- `src/trs_cassette.c`

### Conditional Or Optional Files

These are useful mostly for desktop/debug tooling or utilities:

- `src/debug.c`
- `src/dis.c`
- `src/trs_mkdisk.c`

## SDL Dependency Findings

SDL is concentrated in a small set of files, but there are some leaks into core-adjacent code.

### SDL-heavy frontend files

- `src/trs_sdl_interface.c`
- `src/trs_sdl_gui.c`
- `src/trs_sdl_keyboard.c`
- `src/PasteManager.c`

These should be replaced by PicoCalc implementations, not shimmed wholesale.

### Small SDL leaks in otherwise useful files

- `src/trs_interrupt.c`
  - uses `SDL_GetTicks()` and `SDL_Delay()`
- `src/trs_cassette.c`
  - uses SDL audio APIs
- headers using `SDL_types.h`
  - `src/z80.h`
  - `src/trs_memory.h`
  - `src/trs_state_save.h`

This leads to the core design rule:

- use a tiny SDL compatibility layer only where needed

## SDL Compatibility Strategy

The project should not attempt to provide a full SDL emulation layer.

Instead, create a very small local compatibility surface in:

```text
firmware/compat/
  SDL.h
  SDL_types.h
  SDL_joystick.h
  sdl_compat.c
```

The compatibility include path should come before vendor include paths in the build.

### Minimal SDL typedef shim

For the first milestone, `SDL_types.h` should provide only what the kept `sdltrs` sources require:

- `Uint8`
- `Uint16`
- `Uint32`
- `Uint64`
- `SDL_PRIu64` if needed by formatting macros

### Minimal SDL timing shim

For the first milestone, `SDL.h` and `sdl_compat.c` should provide:

- `SDL_GetTicks()`
- `SDL_Delay()`

Mapping:

- `SDL_GetTicks()` -> Pico SDK millisecond timer
- `SDL_Delay()` -> Pico SDK sleep primitive or equivalent pacing helper

### What Not To Shim Initially

Do not implement these unless a later subsystem truly requires them:

- window creation
- surfaces
- renderers
- clipboard
- file dialogs
- full event loop
- joystick device management

If the build requires many of these, the build is pulling in the wrong source files.

## PicoCalc Frontend Replacement Surface

The excluded SDL frontend currently provides several host-facing functions used by the core. PicoCalc code should implement compatible replacements for these entry points.

### Screen And Display

- `trs_screen_init()`
- `trs_screen_reset()`
- `trs_screen_write_char()`
- `trs_screen_update()`
- `trs_screen_mode()`
- `trs_screen_refresh()`
- `trs_screen_caption()`

### Host Runtime

- `trs_sdl_init()`
- `trs_get_event()`

### Status Indicators

- `trs_disk_led()`
- `trs_hard_led()`
- `trs_turbo_led()`

### Mouse Hooks

- `trs_get_mouse_pos()`
- `trs_set_mouse_pos()`

The mouse hooks can likely be stubs for the initial Model III target.

## Keyboard Strategy

`src/trs_sdl_keyboard.c` mixes two different concerns:

- TRS-80 keyboard matrix behavior
- SDL-based host input and joystick handling

For a clean vendor integration, the recommended approach is:

1. do not compile `src/trs_sdl_keyboard.c` in the first embedded build
2. implement a local PicoCalc keyboard module
3. reproduce the required TRS-80 matrix behavior in local code

The local keyboard module should provide:

- `trs_kb_mem_read()`
- `trs_kb_reset()`
- `trs_kb_heartbeat()`
- `trs_kb_bracket()`
- `clear_key_queue()`

Optional:

- `trs_xlate_keysym()` if an ASCII-based translation path is convenient

Reason:

- the matrix logic is useful
- the SDL event/joystick parts are not
- separating them in local code is cleaner than trying to fake SDL input

## Audio And Cassette Strategy

`src/trs_cassette.c` uses SDL audio APIs directly. That makes it a poor first target for a no-modification vendor integration.

Recommended milestone policy:

### Milestone 1

Exclude cassette and sound support entirely.

### Later Milestones

Choose one of these:

1. write a small audio compatibility shim if only a narrow SDL audio subset is needed
2. replace cassette/audio integration with a PicoCalc-specific adapter

Recommended choice:

- defer cassette/audio until after ROM boot, screen, and keyboard are working

Current firmware note:

- standard Model III cassette-port game sound now uses a PicoCalc-specific PWM adapter in `firmware/frontend/trs_frontend_stub.c`
- cassette data I/O remains excluded from the embedded build

## Build System Requirements

The top-level build should:

1. import Pico SDK
2. include reusable pieces from `picocalc-text-starter`
3. compile selected `sdltrs` core files
4. compile local frontend replacements
5. compile local SDL compatibility shims
6. link everything into a PicoCalc firmware image

Important include-order rule:

- local compatibility headers must override SDL headers for embedded builds

That means the build should prefer:

- `firmware/compat/SDL.h`
- `firmware/compat/SDL_types.h`

before any system SDL include directory.

## Patch Policy

The preferred state is zero vendor modifications.

If an upstream change becomes unavoidable, use a formal patch workflow instead of editing vendor files manually:

- keep vendor trees pristine
- store local patches under `patches/`
- apply patches during setup or build
- document each patch and why it exists

This should be the exception, not the default.

## First Embedded Build Target

The first embedded target should include only the pieces needed to boot a Model III ROM and reach a visible prompt.

Include:

- Z80 execution
- ROM loading
- memory map
- text video
- keyboard matrix
- timing

Exclude:

- disk controller for initial bring-up; two-drive disk support is now implemented
- cassette data I/O
- sound for initial bring-up; standard Model III game sound is now implemented
- printer support
- debugger UI
- desktop SDL menus
- joystick support

## Decision Summary

The vendor-safe approach is:

1. keep both upstreams read-only under `third_party/`
2. write local PicoCalc wrappers and frontend code under `firmware/`
3. exclude SDL frontend files from `sdltrs`
4. provide only a tiny SDL compatibility shim for the residual dependencies
5. reimplement the keyboard and display host layer locally instead of pretending to be a desktop SDL app

That gives the project:

- clean upstream update paths
- low coupling to desktop code
- a smaller embedded build surface
- a maintainable path toward Model III bring-up on RP2350
