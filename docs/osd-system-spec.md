# OSD System Specification

## Purpose

Define an on-screen display (OSD) system that can be opened:

- at startup (before handing control to TRS ROM/DOS)
- during runtime (while emulator is running)

The OSD must manage disk images for `d0` and `d1` and expose core emulator controls without leaving the device.

## Scope

Included:

- disk image selection/eject for `d0` and `d1`
- startup boot chooser (ROM + disks)
- runtime quick menu
- reset/reboot actions
- persistent "last used media" configuration
- safe handoff between OSD and emulator loop

Excluded (first OSD milestone):

- full file browser with nested directories and sorting options
- cassette/audio/printer configuration
- state save/load UI
- advanced visual themes

## UX Model

### Entry Points

1. Startup OSD:
- shown at every boot for configurable timeout (default: 3 seconds)
- auto-continues with last profile if no key press

2. Runtime OSD:
- opened by hotkey (`F4`)
- pauses emulator execution while menu is active

### Exit Paths

- `Resume`: return to emulator unchanged
- `Apply`: commit staged media changes without forced reset
- `Apply + Reset`: commit changes and perform TRS reset (optional safety path)
- `Cancel`: discard in-menu changes

## Information Architecture

Top-level menu:

1. `Media`
2. `Boot`
3. `Machine`
4. `System`
5. `About`

### Media

- `Drive 0`:
  - show current mounted image or `Empty`
  - actions: `Select`, `Eject`, `Write Protect: On/Off`
- `Drive 1`:
  - same actions as drive 0
- `Quick Swap D0<->D1`
- `Apply Media Changes`

### Boot

- `ROM Source`: `Auto`, `SD`, `Embedded`
- `ROM File`: picker from `/roms`
- `Boot With Startup OSD`: `On/Off`
- `Startup Timeout`: integer seconds

### Machine

- `Warm Reset`
- `Cold Reset`
- `Send BREAK`
- `Turbo`: `On/Off` (if implemented)

### System

- `Save Current Profile`
- `Load Profile`
- `Factory Defaults`
- `Diagnostics Overlay`: `Off/On` (maps to debug mode)

### About

- firmware version
- git short hash (if available at build time)
- build profile (`release` / `diag`)

## File/Path Conventions

Default search roots:

- ROMs: `/roms/`
- Disks: `/TRS80/DISKS/` (only)

Supported disk extensions (MVP):

- `.dmk`
- `.dsk`
- `.jv1`
- `.jv3`

Config file:

- `/config/picocalc_trs_osd.cfg`

Emulator disk auto-mount convention:

- `d0`: `/TRS80/DISKS/disk0.dmk`
- `d1`: `/TRS80/DISKS/disk1.dmk`
- no legacy `/disks` fallback

## Runtime Semantics

Disk changes are staged in OSD state and applied explicitly.

Design note:

- Real TRS floppy workflows commonly allow disk swaps during operation without full system reset. The OSD should preserve that behavior by default.

Rules:

- Mount/eject changes do not touch running emulation until `Apply`.
- `Apply` must support runtime disk swap without forcing reset (TRS-80 style behavior).
- `Apply + Reset` remains available as fallback for software/workflows that expect a reset.
- If no media change, `Resume` returns instantly without reset.
- OSD actions must never block indefinitely on SD I/O.

## Input Mapping (Proposed)

- `Up/Down`: navigate entries
- `Left/Right`: change value
- `Enter`: select/confirm
- `Esc`: back/cancel
- `Del` or `Backspace`: eject selected drive
- `F4`: toggle OSD

## Rendering Constraints

- OSD overlays TRS area and status area cleanly.
- OSD uses fixed-width text rendering already present in frontend.
- Must remain readable with current color scheme.
- No scrolling artifacts when opening/closing overlay.
- Runtime status rows outside OSD use `SYS` / `DRV` / `MSG`.
- OSD apply actions should update `MSG` with concise user-facing results.

## Persistence Model

Persist:

- last ROM path/source
- last `d0` and `d1` image paths
- write-protect flags
- startup OSD settings

Do not persist:

- transient runtime counters
- temporary diagnostics text

## Failure Handling

If media load fails:

- keep previous mounted media unchanged
- show concise error in OSD (file missing, open failed, invalid format)
- allow user to retry/select alternate file

If SD missing:

- OSD remains functional with reduced options
- ROM source can fall back to embedded
- drives forced to `Empty`

## Implementation Plan

### OSD-1 (MVP)

- startup splash + timeout
- runtime hotkey open/close
- `Media` page with `d0`/`d1` select/eject
- `Apply` (no-reset disk swap)
- optional `Apply + Reset`

### OSD-2

- `Boot` + `Machine` pages
- persistent config file
- profile save/load

### OSD-3

- richer browser (pagination/filter)
- optional diagnostics page integration

## Suggested Code Ownership

- `firmware/frontend/`: OSD UI state + rendering + input mapping
- `firmware/platform/`: directory listing/file stat wrappers for OSD
- `firmware/main.c` or boot coordinator: startup OSD flow and timeout
- `firmware/emu/`: reset/apply hooks for staged media changes

## Acceptance Criteria

1. At boot, user can select `d0`/`d1` and continue to TRS.
2. During runtime, user can open OSD, swap disk, apply changes, and continue without mandatory reset.
3. Optional reset path exists (`Apply + Reset`) for compatibility recovery.
4. Invalid path/format does not crash emulator.
5. Last-used boot profile restores after reboot.
6. Default release build keeps OSD enabled and diagnostics optional.
