# PicoCalcTRS Milestones

## Current Snapshot (April 2026)

- Model III boots with SD ROM and embedded-ROM fallback.
- Disk drives `:0` and `:1` are active.
- DOS/BASIC write workflows are working in on-device tests.
- `FORMAT` now completes (slowly, due conservative write retry policy).
- Build path supports quiet release and optional diagnostics.
- First-pass game audio bridge is now wired (TRS sound/cassette/orchestra callbacks routed to PicoCalc audio driver).
- Runtime status rows now use `SYS` / `DRV` / `MSG`.
- Disk activity stars are shown in red on status rows.

## Current Gaps (Next Priorities)

1. OSD completeness
- Add per-drive write-protect toggle in OSD.
- Add profile manager (save/load named media sets).
- Add startup settings UI (startup OSD on/off, timeout editing).
- Add optional hidden/meta visibility toggle in disk picker.

2. Fidelity polish
- Expand FDC edge-case validation (copy-protected and unusual DMK patterns).
- Final keyboard fidelity sweep (shifted symbols, repeat timing, special combos).
- Final video parity checks (cursor/blink/text-mode edge cases in real software).

3. Peripheral coverage
- Cassette signal path is implemented at bridge level, but tape-image fidelity is still not full sdltrs cassette emulation.
- Printer/audio extras are still minimal/stubbed.
- Machine-control features can be expanded in OSD (`BREAK`, warm/cold reset UX).

4. Audio fidelity hardening
- Validate game compatibility across common titles and tune tone mapping as needed.
- Improve Orchestra 85/90 approximation (current bridge is frequency-mapped, not PCM-accurate).
- Add optional mute/volume controls.

5. Robustness and UX
- Improve runtime SD/media error handling UX (card removal, invalid/corrupt image, retry).
- Make OSD error messages more specific by failure class.

6. Project hardening
- Keep extending automated/manual regression matrix coverage (LDOS/TRSDOS/BASIC + media ops).
- Continue reducing and documenting vendor patch footprint where safe.

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
