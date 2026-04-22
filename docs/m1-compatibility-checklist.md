# M1 Compatibility Checklist (On Device)

Use this checklist after generating a UF2 with:

```bash
./scripts/regression-m1.sh
```

or:

```bash
./scripts/build-pico-uf2.sh
```

Optional debug builds:

```bash
PICOCALC_ENABLE_FDC_DIAG=ON PICOCALC_ENABLE_DISK_FAULT_DIAG=ON ./scripts/build-pico-uf2.sh
```

## Test Assets

- Model III ROM:
  - `/roms/model3.rom` on SD, or embedded ROM fallback
- Disk images:
  - `/disks/disk0.dmk` (LDOS 5.3.1 media)
  - `/disks/disk1.dmk` (optional second disk)
  - alternate smoke: `TRSDOS13.DSK` as disk0

## Core Boot Cases

1. `No SD card` boot:
- Expect embedded ROM fallback.
- Expect BASIC prompt (or equivalent ROM startup), not a hang.

2. `SD + ROM present` boot:
- Expect ROM load from SD.
- Expect status lines showing ROM and disk detection.

3. `SD + disk0 present` boot to DOS:
- Expect DOS prompt path (`Date?`/`Time?` flow where applicable).
- No loop with `Unknown error code`.

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
- Insert disk0 and disk1.
- Access both drives from DOS commands.
- Expect drive 0 and 1 usable.

2. Invalid-drive safety:
- Any access pattern selecting > drive 1 should fail safely (not-ready behavior), not crash or corrupt state.

## Video/UI Cases

1. Cursor visibility:
- TRS cursor visible in expected contexts.
- Firmware shell cursor not visible in TRS area.

2. Scrolling:
- Fill screen with lines and confirm upward scroll behavior (no full-clear glitch).

3. Color separation:
- TRS text is green.
- status area text is cyan.
- separator line visible between TRS and status areas.

## Result Recording

Record results in a simple table per run:

- `Date`
- `UF2 SHA/size`
- `ROM source` (`SD` or `embedded`)
- `LDOS boot` (`pass/fail`)
- `TRSDOS boot` (`pass/fail`)
- `BUILD TEST/BAS` (`pass/fail`)
- `CMD"S"` (`pass/fail`)
- `FORMAT :1` (`pass/fail`)
- `BACKUP :0 :1` (`pass/fail`)
- `Notes`
