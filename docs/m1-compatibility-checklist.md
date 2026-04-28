# M1 Compatibility Checklist (On Device)

Use this checklist after generating a UF2 with:

```bash
./scripts/regression-m1.sh
```

or:

```bash
./scripts/build-pico-uf2.sh
```

The regression report at `dist/regression/m1-report.txt` records the commit,
worktree state, UF2 path, UF2 size, and UF2 SHA-256.

Optional debug builds:

```bash
PICOCALC_ENABLE_FDC_DIAG=ON PICOCALC_ENABLE_DISK_FAULT_DIAG=ON ./scripts/build-pico-uf2.sh
```

## Test Assets

- Model III ROM:
  - `/TRS80/ROMS/model3.rom` on SD, or embedded ROM fallback
- Disk images:
  - put LDOS/TRSDOS/media images directly under `/TRS80/DISKS`
  - alternate smoke: copy `TRSDOS13.DSK` to `/TRS80/DISKS`
  - image filenames do not need to follow any drive naming convention
  - hidden files such as `.hidden.dmk` should not appear in the picker

## Core Boot Cases

1. `No SD card` boot:
- Expect embedded ROM fallback.
- Expect BASIC prompt (or equivalent ROM startup), not a hang.

2. `SD + ROM present` boot:
- Expect ROM load from SD.
- Expect operator panel row 1 to show machine, ROM source, PC, and audio state.

3. `SD + selected boot disk` boot to DOS:
- Expect DOS prompt path (`Date?`/`Time?` flow where applicable).
- No loop with `Unknown error code`.
- Expect operator panel row 2 to show selected D0/D1 image names and `rw`/`ro` state.

4. `Disk picker` startup:
- Expect D0 picker to list only `.DSK`, `.DMK`, `.JV1`, and `.JV3` images from `/TRS80/DISKS`.
- Expect dot-prefixed files to be hidden.
- Expect the picker to show accepted extensions, sorted image names, image count, and `[none]` choice.
- Select an image for D0 and `none` for D1; expect only D0 to mount.
- Select `none` for D0 and D1; expect ROM BASIC boot with no disk controller attached.

## DOS/BASIC Functional Cases

1. DOS file create path:
- At DOS prompt: `BUILD TEST/BAS`
- Enter program, save, and exit.
- Expect return to DOS prompt without hang.

2. BASIC to DOS return:
- Enter BASIC, run: `CMD"S"`
- Expect reset/return to DOS prompt (no error bounce).

3. Read-write behavior:
- Ensure disk is treated as writable when image is writable.
- Create/update a file and verify changes persist after reboot.

4. Formatting and backup:
- `FORMAT :1` completes without write fault.
- `BACKUP :0 :1` proceeds (LDOS prompt `Different pack IDs! Abort backup ?` is expected; answering `Y` aborts by design).

## FDC/Drive Selection Cases

1. Two-drive behavior:
- Select two disk images in the startup picker.
- Access both drives from DOS commands.
- Expect drive 0 and 1 usable.
- Expect `D0:*` or `D1:*` to appear during recent disk access, then return to `D0:.` or `D1:.`.

2. Invalid-drive safety:
- Any access pattern selecting > drive 1 should fail safely (not-ready behavior), not crash or corrupt state.

## Keyboard Control Cases

1. TRS BREAK:
- Press PicoCalc Esc.
- Expect the guest to receive TRS BREAK, not firmware reset.

2. TRS reset:
- Press PicoCalc BRK (Shift+Esc).
- Expect the guest to behave as if the TRS reset button was pressed.
- Mounted disk selections should remain attached.

## Speed Cases

1. Timing-sensitive game:
- Run a game such as SCARFMAN.
- Expect gameplay speed to be close to a real Model III, not RP2350 flat-out speed.
- Expect game sound to play without hanging the system.

## Audio Cases

1. Model III cassette-port game sound:
- Run SCARFMAN or another game that uses standard Model III sound.
- Expect audible game sound from the PicoCalc audio output.
- Expect no lockup during startup, gameplay, or rapid sound effects.

## Compatibility Sweep Cases

1. Game matrix:
- Test a small set of timing-sensitive, keyboard-heavy, semigraphics-heavy, BASIC, and disk-boot games.
- Record boot, keyboard, speed, video, audio, and disk behavior for each game.
- Update [game-compatibility.md](/workspaces/PicoCalcTRS/docs/game-compatibility.md).
- Treat cassette data I/O, printer behavior, and RTC/date-time behavior as out of scope.

## Video/UI Cases

1. Semigraphics:
- Run a semigraphics-heavy game or visual test.
- Expect Model III `0x80`-`0xBF` block graphics to render as 2x3 filled blocks.
- Check normal and expanded text modes if available.

1. Cursor visibility:
- TRS cursor visible in expected contexts.
- Firmware shell cursor not visible in TRS area.

2. Scrolling:
- Fill screen with lines and confirm upward scroll behavior (no full-clear glitch).

3. Color separation:
- TRS text is green.
- status area text is cyan.
- separator line visible between TRS and status areas.

4. Operator panel:
- Row 1 shows machine, speed, ROM source, PC, and audio state.
- Row 2 shows D0/D1 image names, `rw`/`ro`, and `*`/`.` disk activity.
- Row 3 shows controls by default.
- Row 3 shows transient messages and returns to controls after a short delay.

## Result Recording

Record results in a simple table per run:

- `Date`
- `UF2 SHA/size`
- `Commit/worktree` from `dist/regression/m1-report.txt`
- `ROM source` (`SD` or `embedded`)
- `LDOS boot` (`pass/fail`)
- `TRSDOS boot` (`pass/fail`)
- `BUILD TEST/BAS` (`pass/fail`)
- `CMD"S"` (`pass/fail`)
- `FORMAT :1` (`pass/fail`)
- `BACKUP :0 :1` (`pass/fail`)
- `SCARFMAN audio` (`pass/fail`)
- `Operator panel` (`pass/fail`)
- `Game matrix notes`
- `Notes`
