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
- Latest M1 regression run: 24 passes, 0 failures.

Explicit non-goals for this firmware target:

- cassette data I/O
- printer support
- RTC/date-time emulation

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

## M2 - Usability Polish

Status: `in progress`

### Goal
Make daily use smoother without adding desktop-emulator complexity.

### Completed
- Redesign the 3 status rows into a compact operator panel:
  - row 1: machine, speed, ROM source, PC, and audio state
  - row 2: D0/D1 image names, read/write state, and disk access/motor activity indicator
  - row 3: controls by default, with transient messages for important events
- Use ASCII disk activity glyphs in row 2:
  - `D0:*` / `D1:*` means active motor or recent disk access
  - `D0:.` / `D1:.` means idle
  - example: `D0:* LDOS.DMK rw   D1:. GAMES.DSK ro`
- Keep diagnostic-only details behind existing debug build flags.
- Improve startup disk picker behavior where needed, including sorting and clearer labels.
- Improve ROM/disk load failure messages.
- Add or refine concise on-device status/help text for paths and controls.

### Remaining
- On-device validation of the operator panel during DOS boot, disk reads, disk writes, and no-disk BASIC boot.
- Tune labels or truncation if real disk names are hard to scan on hardware.

---

## M3 - Release Hardening

Status: `in progress`

### Goal
Turn the current beta into a repeatable release process before taking riskier emulator changes.

### Completed
- Added [release-checklist.md](/workspaces/PicoCalcTRS/docs/release-checklist.md) with:
  - supported feature list
  - intentional non-goals
  - known issues and limits
  - build checks
  - on-device smoke tests
  - release record template
- Regression reports now include commit, worktree state, UF2 path, UF2 size, and UF2 SHA-256.

### Remaining
- Keep supported features and known issues current as later milestones land.
- Use the release checklist on an actual on-device release candidate.

---

## M4 - Game Compatibility Sweep

Status: `in progress`

### Goal
Move from "known apps work" to a small, repeatable game compatibility matrix, mostly through observation before code changes.

### Completed
- Added [game-compatibility.md](/workspaces/PicoCalcTRS/docs/game-compatibility.md) with:
  - result codes
  - subsystem failure tags
  - starter matrix rows
  - test notes template
  - low-risk testing rules

### Remaining
- Fill the matrix with on-device results for SCARFMAN, arcade/action titles, BASIC games, semigraphics-heavy games, and disk-boot games.
- Track each game for boot, keyboard, speed, video, audio, and disk behavior.
- Classify failures by likely subsystem before fixing: timing, keyboard, video, disk, audio, or media-specific.
- Tune timing/input/audio only when failures are reproducible.

---

## M5 - Video Fidelity Completion

Status: `in progress`

### Completed
- TRS cursor rendering + firmware cursor suppression in TRS area.
- TRS scrolling correctness fixes.
- Color split (TRS text vs status area) and separator line.
- Procedural 2x3 block rendering for Model III semigraphics characters `0x80`-`0xBF`.

### Remaining
- Final verification of semigraphics edge cases across app set.
- Verify inverse, alternate charset, expanded text, cursor behavior, and scrolling under real software.
- Fix visible compatibility bugs found during the sweep.

---

## M6 - FDC Fidelity Hardening

Status: `planned`

### Completed Foundation
- WRITEM compatibility path for DOS workflows.
- DMK write-track compatibility relaxations needed by FORMAT/BACKUP flows.
- FAT32/stdio write-path hardening and retries for long write sessions.
- Two-drive safety with `NDRIVES=2` and invalid-drive handling.
- Real-time throttle path connected to Pico SDK timing.

### Planned
- Longer soak testing (`FORMAT`/`BACKUP`/file churn loops).
- Test a small image matrix across LDOS, TRSDOS, `.DSK`, `.DMK`, `.JV1`, and `.JV3`.
- Record known failures and whether they are disk-image specific.
- Review the conservative write retry policy after soak results.
- Optional reduction of compatibility patches where safe.

---

## M7 - Audio Polish

Status: `planned`

### Goal
Keep standard Model III game sound stable and pleasant without implementing cassette data I/O.

### Planned
- Test more Model III sound games beyond SCARFMAN.
- Add optional disk activity sound effects:
  - short non-blocking seek/head-step clicks for RESTORE, SEEK, STEP, STEP IN, and STEP OUT activity
  - optional low-priority motor hum while the emulated disk motor/access indicator is active
  - a `DSK SFX:on/off` style toggle or equivalent build/runtime control
- Keep audio priority explicit:
  - TRS game audio has priority
  - seek clicks may play only when safe or as a very short override
  - motor hum is lowest priority and must never block or override game audio
- Add a mute control if it proves useful during game testing.
- If artifacts or hangs appear, move the PicoCalc PWM update path toward a non-blocking implementation.
