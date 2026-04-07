# `picocalc-text-starter` Reference

## Purpose

This document summarizes the vendored `picocalc-text-starter` project as it exists in `third_party/picocalc-text-starter/` and describes how it should be used by this repository.

It is not a replacement for the upstream documentation. It is a project-local reference focused on TRS-80 firmware integration.

## Upstream Role In This Project

`picocalc-text-starter` is the hardware and platform foundation for the PicoCalc target.

In this repository, it should be treated as the provider of:

- Pico SDK project patterns
- PicoCalc board initialization
- text display access
- keyboard input
- SD card and FAT32 access
- southbridge access
- serial and basic audio support

It should not be treated as the long-term home of emulator application code.

## Observed Top-Level Layout

From the vendored tree:

- `CMakeLists.txt`
- `main.c`
- `commands.c`, `songs.c`, `tests.c`
- `drivers/`
- `docs/`
- `pico_sdk_import.cmake`

The most important reusable part for this project is `drivers/`.

## Upstream Build Model

The upstream CMake project:

- uses Pico SDK import through `pico_sdk_import.cmake`
- sets `PICO_BOARD` to `pico`
- builds a single executable named `picocalc-text-starter`
- compiles the demo REPL and all driver modules together

The executable includes:

- demo application files
- driver sources
- Pico SDK support libraries

For this repository, the main takeaway is:

- we should reuse the driver modules and startup pattern
- we should not extend the upstream demo executable in place

## Driver Inventory

Observed driver files under `drivers/`:

- `audio.c`, `audio.h`, `audio.pio`
- `clib.c`
- `display.c`, `display.h`
- `fat32.c`, `fat32.h`
- `font-5x10.c`, `font-8x10.c`, `font.h`
- `keyboard.c`, `keyboard.h`
- `lcd.c`, `lcd.h`
- `onboard_led.c`, `onboard_led.h`
- `picocalc.c`, `picocalc.h`
- `sdcard.c`, `sdcard.h`
- `serial.c`, `serial.h`
- `southbridge.c`, `southbridge.h`

## Most Relevant Drivers For PicoCalcTRS

### Core Bring-Up

These are the most relevant for the first milestone:

- `drivers/picocalc.*`
- `drivers/display.*`
- `drivers/keyboard.*`
- `drivers/lcd.*`
- `drivers/sdcard.*`
- `drivers/fat32.*`
- `drivers/southbridge.*`

### Useful Later

- `drivers/serial.*`
- `drivers/audio.*`
- `drivers/clib.c`

## Notable Upstream Design Choices

Based on the vendored README and docs:

- the starter is text-oriented, not graphics-oriented
- display output is ANSI terminal-like rather than framebuffer-first
- keyboard and display are integrated with standard C stdio
- SD card file I/O can be exposed through `drivers/clib.c`
- the starter ships with a REPL only as a demo and explicitly expects it to be replaced

These traits are a good fit for a Model III text bring-up.

## Key APIs Mentioned In Upstream Docs

### `picocalc_init`

From upstream docs:

- initializes the southbridge, display, and keyboard
- connects display and keyboard to C stdio

This is likely the first platform entry point to reuse from local firmware code.

### Display Driver

From upstream docs:

- emulates an ANSI terminal
- does not keep a full framebuffer in RAM
- supports special graphics and configurable phosphor-like colors

For this project, that suggests:

- the first Model III screen implementation can target text cells
- full pixel framebuffer emulation may not be necessary initially

## Expected Use In This Repository

The local `firmware/platform/` layer should wrap starter capabilities instead of exposing the upstream layout directly to the rest of the project.

Suggested ownership:

- `firmware/platform/` calls into starter drivers
- `firmware/frontend/` consumes local platform abstractions
- `firmware/emu/` does not call starter drivers directly

## What To Reuse

Reuse:

- initialization flow
- driver modules
- Pico SDK assumptions that match the PicoCalc hardware
- SD/FAT support
- keyboard scanning
- text display output

## What To Avoid

Avoid:

- editing the vendored `main.c`
- embedding emulator-specific logic in the upstream demo files
- coupling emulator core code directly to starter-specific headers where a local wrapper is cleaner

## Integration Strategy

The preferred path is:

1. keep the upstream project pristine in `third_party/picocalc-text-starter/`
2. pull required include paths and source files into the local build
3. wrap starter services inside `firmware/platform/`
4. let all emulator-facing code use local interfaces, not raw vendor APIs

## Files Worth Reading First

For implementation work, these vendored files are likely the most useful starting points:

- [picocalc.h](/workspaces/PicoCalcTRS/third_party/picocalc-text-starter/drivers/picocalc.h)
- [picocalc.c](/workspaces/PicoCalcTRS/third_party/picocalc-text-starter/drivers/picocalc.c)
- [display.h](/workspaces/PicoCalcTRS/third_party/picocalc-text-starter/drivers/display.h)
- [display.c](/workspaces/PicoCalcTRS/third_party/picocalc-text-starter/drivers/display.c)
- [keyboard.h](/workspaces/PicoCalcTRS/third_party/picocalc-text-starter/drivers/keyboard.h)
- [keyboard.c](/workspaces/PicoCalcTRS/third_party/picocalc-text-starter/drivers/keyboard.c)
- [fat32.h](/workspaces/PicoCalcTRS/third_party/picocalc-text-starter/drivers/fat32.h)
- [fat32.c](/workspaces/PicoCalcTRS/third_party/picocalc-text-starter/drivers/fat32.c)

## Summary

For PicoCalcTRS, `picocalc-text-starter` is best understood as:

- a reusable embedded text-platform toolkit
- a source of proven PicoCalc device drivers
- a starting point for platform integration

It is not the place where the TRS-80 emulator application should live.
