# Pico Build

## Purpose

This document describes how to configure and build the PicoCalc-targeted firmware path for the RP2350-based PicoCalc.

The hardware build is separate from the default host scaffold build.

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

With the current scaffold, that build completes successfully and produces:

- `build-pico/firmware/picocalc_trs_scaffold.elf`
- `build-pico/firmware/picocalc_trs_scaffold.bin`
- `build-pico/firmware/picocalc_trs_scaffold.hex`

To generate `build-pico/firmware/picocalc_trs_scaffold.uf2`, configure without `-DPICO_NO_PICOTOOL=1` and make sure `picotool` is available to the Pico SDK.

## Reproducible UF2 Command

The repo now includes [build-pico-uf2.sh](/workspaces/PicoCalcTRS/scripts/build-pico-uf2.sh), which:

- uses `PICO_SDK_PATH` or defaults to `$HOME/pico-sdk`
- builds and installs a local `picotool` under `/tmp/picotool/install` if needed
- configures the firmware with UF2 generation enabled
- builds `build-pico-uf2/firmware/picocalc_trs_scaffold.uf2`
- copies a stable flash artifact to [dist/picocalc_trs_scaffold.uf2](/workspaces/PicoCalcTRS/dist/picocalc_trs_scaffold.uf2)

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

- include Pico SDK through the vendored starter import file
- switch the platform backend to `platform_picocalc.c`
- compile selected `picocalc-text-starter` driver sources
- compile selected `sdltrs` core sources
- use the local frontend and compatibility layer

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

## ROM Placement

The firmware currently looks for a Model III ROM in this order:

1. command-line argument, where available
2. `PICOCALC_TRS_ROM`
3. `roms/model3.rom`
4. `roms/trs80m3.rom`
5. `sdcard/roms/model3.rom`
6. `/roms/model3.rom`

On PicoCalc, the existence checks are routed through the FAT32 layer.

## Disk Placement

Current disk root convention for Pico firmware is:

- `/TRS80/DISKS/disk0.dmk`
- `/TRS80/DISKS/disk1.dmk`

This path is intended to be shared by emulator auto-mount behavior and future OSD media selection.

## Suggested Workflow

1. initialize submodules
2. set `PICO_SDK_PATH`
3. run `./scripts/regression-m1.sh` for M1 automated regression gates and UF2 build
4. or configure manually with `-DPICOCALC_PLATFORM=ON`
5. flash the resulting image:
   - preferred stable artifact: [dist/picocalc_trs_scaffold.uf2](/workspaces/PicoCalcTRS/dist/picocalc_trs_scaffold.uf2)
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
- optional host build (`RUN_HOST_BUILD=1`)

For on-device verification, use:

- [m1-compatibility-checklist.md](/workspaces/PicoCalcTRS/docs/m1-compatibility-checklist.md)
- [milestones.md](/workspaces/PicoCalcTRS/docs/milestones.md)

## Host Build Reminder

The host scaffold build remains:

```bash
cmake -S . -B build
cmake --build build -j2
```

That path is useful for integration and link-step iteration without needing the Pico SDK on every machine.
