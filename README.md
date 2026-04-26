# PicoCalcTRS

TRS-80 Model III emulator firmware for the ClockworkPi PicoCalc with an RP2350 board.

This project is planned as:

- Firmware base: `picocalc-text-starter`
- Emulator reference/core source: `sdltrs`
- Target machine: TRS-80 Model III

## Authorship and Acknowledgements

PicoCalcTRS is a firmware port and integration work by Jean Pierre CORDEIRO.

This project is built with gratitude to Blair Leduc, creator of [`picocalc-text-starter`](https://github.com/BlairLeduc/picocalc-text-starter), which provides the PicoCalc firmware foundation, and to Mark Grebe and Jens Guenther for [`sdltrs`](https://gitlab.com/jengun/sdltrs), which provides the TRS-80 emulator reference and core behavior used by this port.

## Project Direction

The PicoCalc starter gives us the embedded platform pieces we need:

- text display output
- keyboard input
- SD card access
- serial/debug access
- Pico SDK project structure

`sdltrs` gives us the TRS-80 emulation logic and hardware behavior we want to preserve:

- Z80 CPU execution
- TRS-80 Model III memory map
- keyboard matrix behavior
- video memory and character rendering rules
- floppy/disk controller behavior
- ROM boot expectations

The important constraint is that `sdltrs` is a desktop emulator built around SDL and host OS facilities. On the PicoCalc, we will need to separate the emulation core from the desktop-facing layers and replace those layers with PicoCalc drivers.

## Current Status

Current firmware status and functionality:

- Model III ROM boot works from SD and embedded fallback.
- Disk drives `:0` and `:1` are integrated.
- LDOS/TRSDOS/BASIC core workflows are working in current on-device tests.
- Build helper emits a stable UF2 in `dist/`.
- Release and debug build profiles are both supported.
- The firmware artifact is `PicoCalcTRS.uf2`.
- SD-card ROM lookup uses `/TRS80/ROMS`.
- SD-card disk lookup uses `/TRS80/DISKS`.
- Startup includes a disk file picker for attaching images to D0 and D1, including `none`.
- Disk image filenames are not special-cased; the picker attaches whichever listed image the user selects.
- Regression guards ensure the old `disk0.*`/`disk1.*` filename probing does not return.
- PicoCalc Esc maps to TRS BREAK; PicoCalc BRK (Shift+Esc) maps to the TRS reset button.
- The SDL timing shim uses Pico SDK time so the Z80 throttle path can target real Model III speed.
- Standard Model III cassette-port game sound is wired to the PicoCalc PWM audio driver, with rate-limited updates for timing-heavy games such as SCARFMAN.
- Optional PCM-backed disk motor/track SFX is available with F4; F5 mutes audio.
- Current M1 regression status: 29 automated checks passing, 0 failing.

Expected SD-card layout:

```text
/TRS80/ROMS/model3.rom
/TRS80/ROMS/trs80m3.rom
/TRS80/DISKS/ldos.dmk
/TRS80/DISKS/games.dsk
```

Disk images may also use `.dmk`, `.jv3`, or `.jv1`; uppercase extensions are accepted.
Files whose names begin with `.` are hidden from the picker.

## Roadmap

Current project state:

1. first-release feature work is frozen
2. usability/operator panel work is complete
3. audio work is complete for first release
4. remaining work is release-candidate validation and compatibility matrix filling
5. post-release validation continues for broader game, video, and FDC image coverage

Explicitly out of scope for this firmware target:

- cassette data I/O
- printer support
- RTC/date-time emulation

The detailed porting plan lives in [docs/porting-plan.md](/workspaces/PicoCalcTRS/docs/porting-plan.md).

The vendor integration specification lives in [docs/vendor-integration.md](/workspaces/PicoCalcTRS/docs/vendor-integration.md).

The vendor setup instructions live in [docs/vendor-setup.md](/workspaces/PicoCalcTRS/docs/vendor-setup.md).

The Pico SDK build notes live in [docs/pico-build.md](/workspaces/PicoCalcTRS/docs/pico-build.md).

The release checklist lives in [docs/release-checklist.md](/workspaces/PicoCalcTRS/docs/release-checklist.md).

The game compatibility matrix lives in [docs/game-compatibility.md](/workspaces/PicoCalcTRS/docs/game-compatibility.md).

The video fidelity checklist lives in [docs/video-fidelity-checklist.md](/workspaces/PicoCalcTRS/docs/video-fidelity-checklist.md).

The FDC soak checklist lives in [docs/fdc-soak-checklist.md](/workspaces/PicoCalcTRS/docs/fdc-soak-checklist.md).

The stable flash artifact produced by the helper script is [dist/PicoCalcTRS.uf2](/workspaces/PicoCalcTRS/dist/PicoCalcTRS.uf2).

Build profile toggles (helper script env vars):

- default release (quiet): `./scripts/build-pico-uf2.sh`
- enable FDC trace lines: `PICOCALC_ENABLE_FDC_DIAG=ON ./scripts/build-pico-uf2.sh`
- enable fault diagnostics (`D2 E/W/U`, `DSK ...`): `PICOCALC_ENABLE_DISK_FAULT_DIAG=ON ./scripts/build-pico-uf2.sh`

Vendor reference notes:

- [docs/picocalc-text-starter.md](/workspaces/PicoCalcTRS/docs/picocalc-text-starter.md)
- [docs/sdltrs.md](/workspaces/PicoCalcTRS/docs/sdltrs.md)
