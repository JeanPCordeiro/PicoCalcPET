# PicoCalcTRS Milestones

## M1 - Compatibility + Regression Harness

### Goal
Make every firmware iteration repeatable and safe by combining:
- automated build/integration checks
- manual on-device compatibility checks focused on Model III DOS/BASIC flows

### Scope
- Add a scriptable regression harness for local CI-like checks.
- Add a standard manual compatibility matrix for device testing.
- Define pass/fail gates for each release candidate UF2.

### Deliverables
- [regression-m1.sh](/workspaces/PicoCalcTRS/scripts/regression-m1.sh)
- [m1-compatibility-checklist.md](/workspaces/PicoCalcTRS/docs/m1-compatibility-checklist.md)
- a generated run report at `dist/regression/m1-report.txt`

### Exit Criteria
- UF2 build succeeds.
- Stable UF2 artifact exists in `dist/`.
- Only expected `sdltrs` patches remain (`0001`, `0004`).
- `trs_cmd_rom.c` shim include is active in build metadata.
- Manual checklist passes on device for LDOS/TRSDOS/BASIC core workflows.

---

## M2 - FDC Fidelity Hardening

### Goal
Increase disk-controller behavior parity on Pico with desktop `sdltrs` expectations.

### Scope
- Validate and harden write/flush paths under repeated file operations.
- Expand disk image format/edge-case test coverage.
- Investigate and remove remaining compatibility hacks where possible.

### Exit Criteria
- No known regressions in DOS create/edit/delete workflows.
- Stable behavior on repeated write-heavy sessions.
- Documented known limitations (if any) with clear repro cases.

---

## M3 - Video Fidelity Completion

### Goal
Close remaining gaps between Pico frontend output and expected Model III behavior.

### Scope
- Finalize charset/semigraphics correctness.
- Validate cursor behavior and screen mode edge cases.
- Validate scrolling and line update semantics across DOS/BASIC apps.

### Exit Criteria
- Cursor, scrolling, and glyph rendering pass checklist scenarios.
- No visual regressions across LDOS/TRSDOS/BASIC screens.
- Video behavior documented with before/after notes for maintained changes.
