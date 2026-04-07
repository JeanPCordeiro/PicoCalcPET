# PicoCalc TRS-80 Model III Porting Plan

## Goal

Build firmware for the ClockworkPi PicoCalc RP2350 that boots and runs a TRS-80 Model III using the Pico SDK firmware structure from `picocalc-text-starter` and hardware/emulation behavior derived from `sdltrs`.

## High-Level Architecture

The cleanest path is to split the project into three layers:

### 1. Platform Layer

Owns PicoCalc-specific services:

- display output
- keyboard scanning/input events
- SD card file access
- timing
- optional audio
- persistent configuration

This should come from `picocalc-text-starter` and remain the only layer that knows about PicoCalc drivers.

### 2. Emulator Core

Owns TRS-80 state and execution:

- Z80 CPU state
- RAM and ROM
- memory-mapped I/O
- video memory
- keyboard matrix
- interrupts and timing
- floppy/cassette/printer state when added

This layer should be written so it does not depend on SDL, terminal escape codes, or desktop file dialogs.

### 3. Front-End Glue

Bridges PicoCalc I/O to the emulator core:

- converts PicoCalc key events into TRS-80 matrix bits
- converts TRS-80 video memory into PicoCalc text cells
- loads ROM and disk images from SD card
- runs the main emulation loop

## What To Reuse From Each Upstream

### From `picocalc-text-starter`

Reuse:

- Pico SDK build structure
- PicoCalc initialization
- display driver
- keyboard driver
- SD/FAT file support
- serial/debug helpers as needed

Discard or replace:

- demo REPL
- example command shell
- any features unrelated to emulation startup

### From `sdltrs`

Reuse conceptually first, then in code where practical:

- TRS-80 machine model behavior
- memory map and I/O register behavior
- ROM loading flow
- video and keyboard emulation logic
- disk controller logic later

Do not carry over directly unless isolated cleanly:

- SDL window/event/audio code
- desktop menu/text UI
- host clipboard support
- platform-specific file dialogs
- printer/debugger UI

The detailed vendor strategy and `sdltrs` file classification live in [vendor-integration.md](/workspaces/PicoCalcTRS/docs/vendor-integration.md).

## Recommended Porting Strategy

### Phase 1: Skeleton Firmware

Create a Pico SDK app that:

- boots on PicoCalc
- initializes display, keyboard, SD card
- shows a status screen
- can read a ROM file from SD card

Success criteria:

- firmware builds
- firmware flashes
- ROM file can be opened from SD

### Phase 2: Core Bring-Up

Integrate the smallest possible emulation slice:

- Z80 core
- 48K RAM
- Model III ROM
- basic memory map
- instruction stepping

Success criteria:

- ROM executes without immediate crash
- emulation loop is stable
- basic reset path works

### Phase 3: Text Video

Implement the Model III text screen:

- map TRS-80 video RAM
- translate characters to PicoCalc text display cells
- support inverse/video attributes only if required for ROM usability
- update only changed cells if full redraw is too slow

Success criteria:

- ROM screen is visible
- boot text is readable
- cursor behavior is usable

### Phase 4: Keyboard Matrix

Replace host keyboard handling with PicoCalc keyboard mapping:

- define a TRS-80 Model III key matrix map
- translate PicoCalc scan/input events
- handle shifted symbols carefully
- add a way to trigger BREAK, RESET, and maybe a local menu

Success criteria:

- keys work in ROM BASIC / monitor
- key repeat is acceptable
- special keys have a clear mapping

### Phase 5: Timing And Main Loop

Make the emulator feel correct:

- establish target CPU timing
- schedule screen refresh separately from CPU stepping if needed
- avoid blocking input and redraw
- measure whether full-speed emulation is realistic on RP2350

Success criteria:

- usable responsiveness
- no major keyboard lag
- stable runtime over several minutes

### Phase 6: Storage

Add disk support through SD card backed images:

- mount disk images from SD
- start with read-only or single-drive support if needed
- expose a simple file naming convention before adding menus

Success criteria:

- boot disk image loads
- file access from TRS-80 software works

### Phase 7: Extras

Optional after the machine is usable:

- sound
- cassette
- configuration UI
- save states
- serial/printer emulation

## Key Design Decisions

### 1. Treat `sdltrs` As A Reference Port, Not A Drop-In Dependency

This is the most important decision. Desktop SDL emulators often intertwine:

- CPU stepping
- UI/menu handling
- keyboard events
- rendering
- host file management

Trying to compile that directly on Pico firmware usually creates more work than extracting only the machine logic we need.

### 2. Boot Model III First, Ignore Other TRS-80 Variants

`sdltrs` supports multiple models. The PicoCalc target should aggressively narrow scope:

- one machine model
- one screen mode path
- one keyboard mapping
- one ROM loading path

That keeps RAM use and code complexity under control.

### 3. Start Text-Only And Keep Rendering Simple

The PicoCalc starter is already text oriented, which is a strong fit for a Model III. Use that to our advantage before worrying about pixel-perfect or optional graphics features.

## Biggest Technical Risks

### 1. Memory Pressure

Potential consumers:

- emulator state
- disk buffers
- ROM images
- display backing store
- SD/FAT stack

We should keep the first milestone minimal and avoid loading extra subsystems early.

### 2. Tight Coupling Inside `sdltrs`

The emulator core may not already be cleanly separated from SDL/platform code. Expect some refactoring or selective extraction.

### 3. Timing Accuracy Versus Simplicity

A bootable machine does not require perfect timing. Some software later will. We should optimize for correctness of the common boot path first, then tighten timing once the core machine works.

### 4. Keyboard Mapping Friction

The PicoCalc keyboard is not identical to a TRS-80 keyboard. We will likely need:

- a clear default keymap
- a few function combos
- maybe an overlay/help screen

## Suggested Initial Repo Layout

```text
.
├── README.md
├── docs/
│   └── porting-plan.md
├── firmware/
│   ├── CMakeLists.txt
│   ├── main.c
│   ├── platform/
│   ├── emu/
│   └── assets/
└── third_party/
    ├── picocalc-text-starter/
    └── sdltrs/
```

This does not mean both upstream projects must remain intact forever. It simply gives us a safe staging area while we learn what should be imported, wrapped, or rewritten.

## Practical Next Step

The next implementation step should be:

1. vendor `picocalc-text-starter` and `sdltrs` into `third_party/`
2. create a local PicoCalc firmware app outside the vendor trees
3. add a minimal SDL compatibility shim for typedefs and timing
4. implement the PicoCalc frontend replacements for screen, input, and runtime hooks
5. create an `emu/` stub with reset, step, and framebuffer hooks

That gives us a hardware-tested shell before we begin pulling emulation code across.
