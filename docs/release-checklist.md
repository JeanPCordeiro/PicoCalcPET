# PicoCalcTRS Release Checklist

Use this checklist for release candidates and tagged UF2 builds.

## Supported Surface

- TRS-80 Model III target.
- ROM boot from `/TRS80/ROMS/model3.rom` or `/TRS80/ROMS/trs80m3.rom`.
- Embedded Model III ROM fallback when built into the UF2.
- 64x16 Model III text display on PicoCalc.
- PicoCalc keyboard mapped to the TRS-80 keyboard matrix.
- PicoCalc Esc as TRS BREAK.
- PicoCalc BRK (Shift+Esc) as TRS reset button.
- SD disk images under `/TRS80/DISKS`.
- Startup picker for D0 and D1, including `none`.
- Disk image extensions `.dsk`, `.dmk`, `.jv1`, and `.jv3`; uppercase extensions are accepted.
- Two active floppy drives, D0 and D1.
- LDOS/TRSDOS/BASIC core workflows.
- DOS file create/update paths.
- `FORMAT`/`BACKUP` compatibility paths, with conservative write retries.
- Real-time Z80 throttle through Pico SDK timing.
- Standard Model III cassette-port game sound through PicoCalc PWM audio.
- Optional F4 disk seek/access click effect, default off.
- Three-row operator status panel with disk activity indicators.

## Intentional Non-Goals

- Cassette data I/O.
- Printer support.
- RTC/date-time emulation.
- Desktop SDL menus/debugger UI.
- Joystick/mouse support.
- Other TRS-80 model targets.

## Known Issues And Limits

- Write-heavy disk operations can be slow because the write path favors safety and retries.
- FDC support is high-confidence beta, but still needs longer soak testing.
- Procedural semigraphics rendering is implemented, but still needs broader app/game validation.
- Inverse, alternate charset, expanded text, cursor, and scrolling edge cases need broader app/game validation.
- Game compatibility matrix is not complete.
- Game audio is verified with SCARFMAN, but needs broader testing.
- Optional disk seek/access clicks are implemented behind F4, but still need on-device tuning; motor hum remains planned.
- The operator status panel still needs on-device validation across no-disk boot, DOS boot, reads, and writes.

## Required Build Checks

Run:

```bash
./scripts/regression-m1.sh
```

Optional host build guard:

```bash
RUN_HOST_BUILD=1 ./scripts/regression-m1.sh
```

Expected result:

- `28` passes.
- `0` failures.
- `dist/PicoCalcTRS.uf2` exists and is non-empty.
- `dist/regression/m1-report.txt` records commit, worktree state, UF2 path, UF2 size, and UF2 SHA-256.

## Required On-Device Smoke Tests

Record ROM source, disk images, and pass/fail result for each item. Use `dist/regression/m1-report.txt` for the build commit and UF2 size/hash.

- Boot with no SD card, using embedded ROM fallback if available.
- Boot with SD ROM present.
- Boot with D0 set to `none` and D1 set to `none`.
- Boot DOS from D0.
- Verify D0/D1 picker can attach images or leave either drive empty.
- Verify operator panel row 1 shows machine, ROM source, PC, and audio state.
- Verify operator panel row 2 shows `D0:*` / `D0:.` activity during disk access.
- Verify operator panel row 3 returns to controls after transient messages.
- Press Esc and confirm TRS BREAK behavior.
- Press BRK (Shift+Esc) and confirm TRS reset behavior.
- Create or update a DOS/BASIC file and confirm persistence after reboot.
- Run `FORMAT :1` on a scratch disk image.
- Run `BACKUP :0 :1` far enough to confirm expected prompts/behavior. Hardware smoke passed on 2026-04-24 with user-tested media; still repeat for release candidate media.
- Update [fdc-soak-checklist.md](/workspaces/PicoCalcTRS/docs/fdc-soak-checklist.md) with any disk-image or long-run results gathered during release testing.
- Run SCARFMAN and confirm speed and audio do not hang.
- Press F4, perform disk reads/writes, and confirm optional disk clicks are audible without disturbing game audio.
- Update [game-compatibility.md](/workspaces/PicoCalcTRS/docs/game-compatibility.md) with any game results gathered during release testing.
- Update [video-fidelity-checklist.md](/workspaces/PicoCalcTRS/docs/video-fidelity-checklist.md) with any focused video results gathered during release testing.

## Release Record Template

```text
Date:
Commit:
UF2 path: dist/PicoCalcTRS.uf2
UF2 size:
UF2 sha256:
ROM source: SD / embedded
D0 image:
D1 image:
regression-m1: pass/fail
host build: pass/fail/skipped
on-device smoke: pass/fail
Known issues:
Notes:
```
