# PicoCalcTRS

TRS-80 Model III emulator firmware for the ClockworkPi PicoCalc with an RP2350 board.

This project is planned as:

- Firmware base: `picocalc-text-starter`
- Emulator reference/core source: `sdltrs`
- Target machine: TRS-80 Model III

## Project Direction

The PicoCalc starter gives us the embedded platform pieces we need:

- text display output
- keyboard input
- SD card access
- serial/debug access
- Pico SDK project structure

`sdltrs` gives us the TRS-80 emulation logic and hardware behavior we want to preserve:

- Z80 CPU execution
- TRS-80 Model III memory map
- keyboard matrix behavior
- video memory and character rendering rules
- floppy/disk controller behavior
- ROM boot expectations

The important constraint is that `sdltrs` is a desktop emulator built around SDL and host OS facilities. On the PicoCalc, we will need to separate the emulation core from the desktop-facing layers and replace those layers with PicoCalc drivers.

## First Milestone

The first useful target is a minimal Model III bring-up:

1. initialize PicoCalc hardware using the starter firmware
2. load a Model III ROM image from SD card
3. emulate CPU, RAM, ROM, and memory-mapped video
4. render the 64x16 text display on the PicoCalc screen
5. translate PicoCalc keyboard input into the TRS-80 keyboard matrix
6. boot to the ROM prompt

That milestone avoids the riskiest peripherals at first:

- floppy controller
- cassette
- sound
- printer
- debugger UI
- desktop SDL menus

## Current Status

Current firmware status is beyond initial bring-up:

- Model III ROM boot works from SD and embedded fallback.
- Disk drives `:0` and `:1` are integrated.
- LDOS/TRSDOS/BASIC core workflows are working in current on-device tests.
- Runtime status rows use `SYS` / `DRV` / `MSG` layout.
- Disk activity indicator (`*`) is shown in red for `D0`/`D1` activity.
- First-pass audio bridge is integrated (TRS sound/cassette/orchestra callbacks routed to PicoCalc PWM audio).
- Build helper emits a stable UF2 in `dist/`.
- Release and debug build profiles are both supported.

## Roadmap

The detailed porting plan lives in [docs/porting-plan.md](/workspaces/PicoCalcTRS/docs/porting-plan.md).
The OSD control-plane spec lives in [docs/osd-system-spec.md](/workspaces/PicoCalcTRS/docs/osd-system-spec.md).

The vendor integration specification lives in [docs/vendor-integration.md](/workspaces/PicoCalcTRS/docs/vendor-integration.md).

The vendor setup instructions live in [docs/vendor-setup.md](/workspaces/PicoCalcTRS/docs/vendor-setup.md).

The Pico SDK build notes live in [docs/pico-build.md](/workspaces/PicoCalcTRS/docs/pico-build.md).

The stable flash artifact produced by the helper script is [dist/PicoCalcTRS.uf2](/workspaces/PicoCalcTRS/dist/PicoCalcTRS.uf2).

Build profile toggles (helper script env vars):

- default release (quiet): `./scripts/build-pico-uf2.sh`
- enable FDC trace lines: `PICOCALC_ENABLE_FDC_DIAG=ON ./scripts/build-pico-uf2.sh`
- enable fault diagnostics (`D2 E/W/U`, `DSK ...`): `PICOCALC_ENABLE_DISK_FAULT_DIAG=ON ./scripts/build-pico-uf2.sh`

Vendor reference notes:

- [docs/picocalc-text-starter.md](/workspaces/PicoCalcTRS/docs/picocalc-text-starter.md)
- [docs/sdltrs.md](/workspaces/PicoCalcTRS/docs/sdltrs.md)
