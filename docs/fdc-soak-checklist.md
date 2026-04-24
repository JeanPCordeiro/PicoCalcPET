# PicoCalcTRS FDC Soak Checklist

Use this checklist for disk/FDC validation before changing disk-controller code. The goal is to separate media-specific problems from emulator regressions.

## Result Codes

- `pass`: completes without hang, write fault, or unexpected not-ready behavior.
- `partial`: completes with expected warnings/prompts or noticeable slowness.
- `fail`: hangs, corrupts media, reports unexpected write fault, or cannot complete.
- `nt`: not tested yet.

## Image Matrix

| System | Format | Image | Boot | DIR/read | Create/update | FORMAT :1 | BACKUP :0 :1 | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| LDOS | DMK | scratch/known-good | nt | nt | nt | nt | nt | |
| LDOS | DSK | scratch/known-good | nt | nt | nt | nt | nt | |
| TRSDOS | DSK | scratch/known-good | nt | nt | nt | nt | nt | |
| TRSDOS | JV1 | scratch/known-good | nt | nt | nt | nt | nt | |
| TRSDOS | JV3 | scratch/known-good | nt | nt | nt | nt | nt | |

## Hardware Smoke Notes

| Date | Case | Media | Result | Notes |
| --- | --- | --- | --- | --- |
| 2026-04-24 | `BACKUP :0 :1` | user-tested disk set, details not recorded | pass | Completed on PicoCalcTRS; optional disk clicks were audible and did not hang the workflow. |

## Soak Cases

### Boot And Read

- Boot DOS from D0.
- Run directory/listing commands repeatedly.
- Confirm `D0:*` activity appears during reads and returns to `D0:.`.
- Confirm no unexpected `Unknown error code` loop.

### Create And Update

- Create a small file.
- Update it.
- Reboot and confirm the update persisted.
- Repeat on both D0 and D1 where the DOS supports it.

### FORMAT

- Use only scratch disk images.
- Run `FORMAT :1`.
- Record total time if practical.
- Confirm completion without unexpected write fault.
- Confirm the formatted image remains mountable after reboot.

### BACKUP

- Use scratch destination images.
- Run `BACKUP :0 :1`.
- Record expected prompts, such as pack-ID mismatch prompts.
- Confirm expected abort/continue behavior.
- Confirm no crash, hang, or unexpected media corruption.

### File Churn Loop

- Create several small files.
- Delete some files.
- Re-create files with the same names.
- Reboot and verify directory state.

## Failure Notes Template

```text
Case:
System:
Format:
D0 image:
D1 image:
UF2 sha256:
Result:
Observed issue:
Expected behavior:
Repro steps:
Media-specific: yes/no/unknown
Notes:
```

## Safety Rules

- Use scratch destination images for write, format, and backup testing.
- Keep known-good source images read-only when possible.
- Save failing images before retrying if corruption is suspected.
- Do not tune write retries until failures are reproducible.
