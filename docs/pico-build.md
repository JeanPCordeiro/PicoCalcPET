# Pico Build

## Purpose

This document describes how to configure and build the PicoCalc-targeted firmware path for the RP2350-based PicoCalc.

The hardware build is separate from the default host build.

## Requirements

You need:

- a checked-out repository with submodules initialized
- the Raspberry Pi Pico SDK available locally
- a configured toolchain for Pico SDK builds

## SDK Setup

Set `PICO_SDK_PATH` to your local Pico SDK checkout.

Example:

```bash
export PICO_SDK_PATH=$HOME/pico/pico-sdk
```

If you prefer auto-fetch behavior, you can instead enable:

```bash
-DPICO_SDK_FETCH_FROM_GIT=ON
```

Using a local checked-out SDK is recommended for repeatable embedded builds.

## Configure Command

From the repo root:

```bash
export PICO_SDK_PATH=$HOME/pico-sdk
cmake -S . -B build-pico -DPICOCALC_PLATFORM=ON -DPICO_NO_PICOTOOL=1
```

If `PICO_SDK_PATH` is not set, configuration will fail early.

`-DPICO_NO_PICOTOOL=1` is useful in restricted environments where the SDK cannot fetch `picotool` from GitHub during configuration.

Important consequence:

- with `-DPICO_NO_PICOTOOL=1`, the build still succeeds
- `.elf`, `.bin`, and `.hex` are generated
- `.uf2` is not generated

## Build Command

```bash
cmake --build build-pico -j2
```

With the current Pico target, that build completes successfully and produces:

- `build-pico/firmware/PicoCalcTRS.elf`
- `build-pico/firmware/PicoCalcTRS.bin`
- `build-pico/firmware/PicoCalcTRS.hex`

To generate `build-pico/firmware/PicoCalcTRS.uf2`, configure without `-DPICO_NO_PICOTOOL=1` and make sure `picotool` is available to the Pico SDK.

## Reproducible UF2 Command

The repo now includes [build-pico-uf2.sh](/workspaces/PicoCalcTRS/scripts/build-pico-uf2.sh), which:

- uses `PICO_SDK_PATH` or defaults to `$HOME/pico-sdk`
- builds and installs a local `picotool` under `/tmp/picotool/install` if needed
- configures the firmware with UF2 generation enabled
- builds `build-pico-uf2/firmware/PicoCalcTRS.uf2`
- copies a stable flash artifact to [dist/PicoCalcTRS.uf2](/workspaces/PicoCalcTRS/dist/PicoCalcTRS.uf2)

Usage:

```bash
./scripts/build-pico-uf2.sh
```

Optional environment overrides:

- `PICO_SDK_PATH`
- `PICOTOOL_SRC_DIR`
- `PICOTOOL_INSTALL_DIR`
- `BUILD_DIR`
- `PICOCALC_ENABLE_FDC_DIAG`
- `PICOCALC_ENABLE_DISK_FAULT_DIAG`

## Current Pico Path Status

The Pico build path is now wired to:

- build the firmware target as `PicoCalcTRS`
- include Pico SDK through the vendored starter import file
- switch the platform backend to `platform_picocalc.c`
- compile selected `picocalc-text-starter` driver sources
- compile selected `sdltrs` core sources
- use the local frontend and compatibility layer
- emit a flashable `PicoCalcTRS.uf2` through the helper script

## Included PicoCalc Starter Sources

Current Pico-side build wiring pulls in:

- `drivers/display.c`
- `drivers/fat32.c`
- `drivers/keyboard.c`
- `drivers/lcd.c`
- `drivers/sdcard.c`
- `drivers/southbridge.c`
- `drivers/font-8x10.c`

This is intentionally narrower than the full upstream starter project.

## Current Limitations

The Pico path is now functional for ROM + DOS/BASIC + two-drive workflows, but not feature-complete.

Remaining limits:

- cassette/audio/printer remain stubbed
- no RTC emulation (DOS date/time prompts follow Model III behavior)
- write-heavy disk operations are stable but conservative (extra retries can make operations slower)
- frontend is functional and intentional, but still not a full desktop-equivalent UI

## Vendor Patch Adaptation

The Pico build now uses a vendor+patch flow for:

- core `sdltrs` files
- selected `picocalc-text-starter` driver files (`fat32.c`)

In short:

- upstream sources remain in `third_party/sdltrs/src/*.c` and `third_party/picocalc-text-starter/drivers/*.c`
- Pico-specific source diffs are maintained as patch files in `patches/sdltrs/*.patch` and `patches/picocalc-text-starter/*.patch`
- build copies upstream sources into the build directory, applies patches, then compiles

Details are documented in [vendor-patch-workflow.md](/workspaces/PicoCalcTRS/docs/vendor-patch-workflow.md).

## SD-Card Layout

The firmware currently looks for a Model III ROM in this order:

1. command-line argument, where available
2. `PICOCALC_TRS_ROM`
3. `/TRS80/ROMS/model3.rom`
4. `/TRS80/ROMS/trs80m3.rom`

At startup, the firmware shows a disk picker for D0 and D1. The picker lists files under `/TRS80/DISKS`, lets either drive be set to `none`, and accepts only `.dsk`, `.dmk`, `.jv1`, or `.jv3` extensions. Uppercase extensions are also accepted. Files whose names begin with `.` are hidden.

If no command-line or environment disk override is present, the picker is used. When disk images are available, the selector starts on the first listed image; otherwise it starts on `none`.

There is no automatic `disk0.*`/`disk1.*` filename convention. Any visible supported disk image can be selected for either drive.

On PicoCalc, the existence checks are routed through the FAT32 layer.

Keyboard control mapping:

- PicoCalc Esc sends TRS BREAK.
- PicoCalc BRK (Shift+Esc) presses the TRS reset button.

Runtime speed control:

- The sdltrs Z80 loop uses Model III t-state accounting and calls `trs_timer_sync_with_host()`.
- The PicoCalc SDL compatibility shim maps `SDL_GetTicks()` and `SDL_Delay()` to Pico SDK time functions, so that throttle path can sleep against real wall-clock time.

## Suggested Workflow

1. initialize submodules
2. set `PICO_SDK_PATH`
3. run `./scripts/regression-m1.sh` for M1 automated regression gates and UF2 build
4. or configure manually with `-DPICOCALC_PLATFORM=ON`
5. flash the resulting image:
   - preferred stable artifact: [dist/PicoCalcTRS.uf2](/workspaces/PicoCalcTRS/dist/PicoCalcTRS.uf2)
   - use `.uf2` if `picotool` is available and UF2 generation is enabled
   - otherwise use the generated `.elf` or `.hex` with your preferred flashing workflow

## M1 Regression Harness

Run:

```bash
./scripts/regression-m1.sh
```

What it checks:

- UF2 build success
- expected `sdltrs` patch set shape (`0001`, `0004`)
- UF2 artifact presence and hash/size capture
- compile-time `trs_cmd_rom` shim injection guard
- runtime ROM/disk directory guards
- disk picker wiring and filter guards
- guard against legacy `disk0.*`/`disk1.*` filename probing
- keyboard control mapping guard for PicoCalc BRK as TRS reset
- SDL timing shim guard for Pico SDK-backed ticks/delay
- optional host build (`RUN_HOST_BUILD=1`)

For on-device verification, use:

- [m1-compatibility-checklist.md](/workspaces/PicoCalcTRS/docs/m1-compatibility-checklist.md)
- [milestones.md](/workspaces/PicoCalcTRS/docs/milestones.md)

## Host Build Reminder

The host build remains:

```bash
cmake -S . -B build
cmake --build build -j2
```

That path is useful for integration and link-step iteration without needing the Pico SDK on every machine.
