# PicoCalcTRS Milestones

## Current Snapshot (April 2026)

- Model III boots with SD ROM and embedded-ROM fallback.
- Disk drives `:0` and `:1` are active.
- DOS/BASIC write workflows are working in on-device tests.
- `FORMAT` now completes (slowly, due conservative write retry policy).
- Build path supports quiet release and optional diagnostics.

---

## M1 - Compatibility + Regression Harness

Status: `completed`

### Goal
Make every firmware iteration repeatable and safe by combining:
- automated build/integration checks
- manual on-device compatibility checks focused on Model III DOS/BASIC flows

### Delivered
- [regression-m1.sh](/workspaces/PicoCalcTRS/scripts/regression-m1.sh)
- [m1-compatibility-checklist.md](/workspaces/PicoCalcTRS/docs/m1-compatibility-checklist.md)
- generated run report at `dist/regression/m1-report.txt`
- stable UF2 copy in `dist/`

---

## M2 - FDC Fidelity Hardening

Status: `in progress (high confidence beta)`

### Completed in M2
- WRITEM compatibility path for DOS workflows.
- DMK write-track compatibility relaxations needed by FORMAT/BACKUP flows.
- FAT32/stdio write-path hardening and retries for long write sessions.
- Two-drive safety with `NDRIVES=2` and invalid-drive handling.

### Remaining
- Longer soak testing (`FORMAT`/`BACKUP`/file churn loops).
- Optional reduction of compatibility patches where safe.
- Finalize known-issue list with reproducible stress cases.

---

## M3 - Video Fidelity Completion

Status: `partially complete`

### Completed
- TRS cursor rendering + firmware cursor suppression in TRS area.
- TRS scrolling correctness fixes.
- Color split (TRS text vs status area) and separator line.

### Remaining
- Final verification of semigraphics edge cases across app set.
- Optional cleanup/refactor of frontend rendering internals.

---

## M4 - OSD Control Plane

Status: `planned`

### Goal
Add an in-firmware OSD to manage boot media and runtime emulator controls directly on PicoCalc.

### Scope
- startup OSD boot chooser
- runtime hotkey OSD
- `d0`/`d1` mount/eject/select flow
- reset/apply semantics for media changes
- persistent last-used profile

### Spec
- [osd-system-spec.md](/workspaces/PicoCalcTRS/docs/osd-system-spec.md)
