# PicoCalcTRS Milestones

## Current Snapshot (April 2026)

- Model III boots with SD ROM and embedded-ROM fallback.
- Disk drives `:0` and `:1` are active.
- DOS/BASIC write workflows are working in on-device tests.
- `FORMAT` now completes (slowly, due conservative write retry policy).
- Firmware target/artifact is now named `PicoCalcTRS`.
- Build path emits `dist/PicoCalcTRS.uf2` and supports quiet release plus optional diagnostics.
- Runtime SD-card layout is `/TRS80/ROMS` for ROMs and `/TRS80/DISKS` for disks.
- Startup disk picker attaches images to D0/D1 or leaves either drive as `none`.
- Disk filenames are picker-selected; there is no automatic `disk0.*`/`disk1.*` filename convention.
- M1 regression now guards the disk picker wiring, supported extension filter, hidden-file filter, and absence of legacy filename probing.
- PicoCalc Esc sends TRS BREAK; PicoCalc BRK (Shift+Esc) presses the TRS reset button.
- SDL timing shim now uses Pico SDK ticks/delay, enabling real-time Z80 throttling.
- Standard Model III game sound is active through the PicoCalc PWM audio driver; SCARFMAN has been verified after audio update throttling.
- Latest M1 regression run: 20 passes, 0 failures.

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
- stable UF2 copy at `dist/PicoCalcTRS.uf2`
- guards for runtime ROM/disk directories, disk picker policy, BRK reset mapping, and Pico SDK-backed timing

---

## M2 - FDC Fidelity Hardening

Status: `in progress (high confidence beta)`

### Completed in M2
- WRITEM compatibility path for DOS workflows.
- DMK write-track compatibility relaxations needed by FORMAT/BACKUP flows.
- FAT32/stdio write-path hardening and retries for long write sessions.
- Two-drive safety with `NDRIVES=2` and invalid-drive handling.
- Real-time throttle path connected to Pico SDK timing.
- Standard Model III cassette-port game sound routed to PicoCalc PWM audio with rate-limited hardware updates.

### Remaining
- Longer soak testing (`FORMAT`/`BACKUP`/file churn loops).
- On-device speed validation against Model III timing-sensitive games.
- Broader game-audio compatibility sweep beyond SCARFMAN.
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
