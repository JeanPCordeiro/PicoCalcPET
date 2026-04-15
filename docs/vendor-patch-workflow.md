# Vendor Patch Workflow (No Source Fork)

This repository now uses a vendor+patch model for core `sdltrs` emulation files:

- Upstream sources stay in `third_party/sdltrs/src/*.c`
- Pico-specific changes live in `patches/sdltrs/*.patch`
- Build copies upstream sources into the build directory, applies patches, then compiles patched copies.

## Why this is better

- `third_party/sdltrs` stays clean and easy to update
- Local changes are explicit and reviewable in one patch file
- Reduces long-lived `*_embedded.c` forks

## Current status

- Patched vendor files:
  - `trs_disk.c`
  - `trs_memory.c`
- Unpatched vendor files with local adaptation:
  - `trs_cmd_rom.c` via compile-time stdio shim (`firmware/compat/sdltrs_cmd_rom_stdio_shim.h`) and `platform_file_*` bridge
  - `trs_interrupt.c` via local post-reset policy scrub (`firmware/emu/picocalc_reset_policy.c`)
- Legacy `firmware/emu/*_embedded.c` files are no longer compiled by Pico builds.
- `grafyx_m3_read_byte()` / `grafyx_m3_write_byte()` behavior now lives in local frontend stubs (`firmware/emu/sdltrs_peripheral_stubs.c`), so the memory patch only carries RAM-size constraints.

## Update flow when upstream changes

1. Update `third_party/sdltrs` submodule
2. Reconfigure/build (`./scripts/build-pico-uf2.sh`)
3. If a patch fails, refresh the affected `patches/sdltrs/*.patch` file
4. Rebuild and test on device

## Patch files

- `patches/sdltrs/0001-picocalc-trs_disk.patch`
- `patches/sdltrs/0004-picocalc-trs_memory.patch`

These patches are applied during CMake configure for `PICOCALC_PLATFORM=ON`.

## Patch minimization notes

- `0001` (`trs_disk`): still required for Pico FAT32 stdio bridge, strict DMK probing fix, and a direct `NDRIVES=2` limit for RP2350 memory fit.
- `0004` (`trs_memory`): currently required for RP2350 RAM fit (`MAX_MEMORY_SIZE`, `MEGAMEM_START`).

## Non-patch adaptations

- `trs_cmd_rom.c`:
  - source is kept upstream
  - Pico build force-includes a local shim that remaps stdio calls to `platform_file_*`
  - no functional `sdltrs` source diff is required
- `trs_interrupt.c`:
  - source is kept upstream
  - after each local reset path, firmware scrubs Model III DOS date/time cache bytes in RAM to preserve no-RTC UX (`Date?` / `Time?` prompt flow)
