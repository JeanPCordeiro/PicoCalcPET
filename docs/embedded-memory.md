# Embedded Memory Backend

## Purpose

This document explains the local embedded replacement for the `sdltrs` memory backend and why it exists.

The vendored `sdltrs` memory implementation in [trs_memory.c](/workspaces/PicoCalcTRS/third_party/sdltrs/src/trs_memory.c) allocates a large static memory array intended to support many desktop-oriented memory expansion modes.

That is not suitable for the initial PicoCalc RP2350 firmware target.

## Original Problem

The upstream source defines:

- `MAX_MEMORY_SIZE = (4 * 1024 * 1024) + 65536`

That results in a very large static `.bss` allocation.

On the Pico firmware build, this causes the final link to fail with RAM overflow.

## Local Embedded Approach

To keep the vendor tree untouched while making embedded progress, the project now carries a local copy:

- [trs_memory_embedded.c](/workspaces/PicoCalcTRS/firmware/emu/trs_memory_embedded.c)

This file starts as a copy of the upstream `trs_memory.c`, but is used only for the embedded build path.

## Current Embedded Limits

The embedded copy reduces the memory sizing to:

- `MAX_MEMORY_SIZE = 128 * 1024`
- `MEGAMEM_START = 64 * 1024`

The ROM and video limits remain the same as upstream:

- `MAX_ROM_SIZE = 18432`
- `MAX_VIDEO_SIZE = 3072`

## Why 128 KB

For the current bring-up target, we are intentionally narrowing scope to:

- TRS-80 Model III
- text display
- ROM boot
- keyboard input
- no advanced memory expansion hardware

That means we do not need the full desktop-oriented 4 MB memory backing in the first embedded milestone.

128 KB gives us:

- coverage for the 64 KB Z80 address space
- modest headroom for internal mapping assumptions
- a much smaller `.bss` footprint for RP2350 firmware work

## Tradeoff

This local backend is not intended to preserve every desktop memory-expansion feature.

It is a bring-up backend.

That means:

- it is appropriate for early Model III firmware work
- it is not yet the final answer for every `sdltrs` feature

## Build Policy

The vendor source remains untouched.

Current intended split:

- host/reference build can continue to use vendor `trs_memory.c`
- Pico embedded build should use local `trs_memory_embedded.c`

This keeps the upstream reference available while allowing embedded-specific reduction.

## Next Expected Refinements

Possible future refinements include:

1. remove or compile-gate unused expansion paths more aggressively
2. narrow save/load support for embedded builds
3. introduce a dedicated Model III-only memory configuration header
4. replace copied sections with smaller local implementations once behavior is well understood

## Summary

The local embedded memory backend exists because:

- the Pico SDK and toolchain are now configured correctly
- the remaining Pico firmware blocker is memory footprint, not environment
- the upstream `sdltrs` memory sizing is too large for the embedded target

So the project now uses a local embedded memory copy as a practical bridge toward first firmware bring-up.
