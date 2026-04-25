# PicoCalcTRS Game Compatibility Matrix

Use this matrix for on-device game testing. The goal is to classify behavior before changing emulator code.

## Result Codes

- `pass`: works well enough for normal play.
- `partial`: boots or plays, but has visible or audible issues.
- `fail`: does not boot, hangs, corrupts display, or cannot be controlled.
- `nt`: not tested yet.

## Failure Areas

Use one or more tags in the `Likely area` column:

- `timing`
- `keyboard`
- `video`
- `disk`
- `audio`
- `media`
- `unknown`

## Matrix

| Title | Type | Media | Boot | Keyboard | Speed | Video | Disk | Audio | Likely area | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| SCARFMAN | timing/action/audio | disk | pass | nt | pass | nt | nt | pass | audio | Verified no audio hang after PWM update throttling. |
| DOS BACKUP workflow | disk utility | disk | pass | nt | partial | nt | pass | pass | disk/audio | `BACKUP :0 :1` hardware smoke passed; optional F4 disk SFX was audible and did not hang the workflow. |
| Big Five action game | arcade/action | disk | nt | nt | nt | nt | nt | nt | unknown | Placeholder for Big Five-style action timing/video check. |
| BASIC game | BASIC | BASIC/disk | nt | nt | nt | nt | nt | nt | unknown | Placeholder for keyboard and BASIC runtime behavior. |
| Semigraphics-heavy game | video | disk | nt | nt | nt | nt | nt | nt | unknown | Placeholder for semigraphics fidelity check. |
| Disk-boot game | disk boot | disk | nt | nt | nt | nt | nt | nt | unknown | Placeholder for boot and disk access behavior. |

## Test Notes Template

```text
Title:
Disk/image:
ROM source:
UF2 sha256:
Boot:
Keyboard:
Speed:
Video:
Disk:
Audio:
Likely area:
Repro steps:
Notes:
```

## Testing Rules

- Test on PicoCalc hardware, not only host builds.
- Record the UF2 SHA-256 from `dist/regression/m1-report.txt`.
- Keep disk images unchanged during read-only compatibility testing.
- Use scratch disk images for write/format tests.
- Classify failures before patching code.
- Prefer small, reproducible fixes over broad emulator changes.

## Hardware Smoke Notes

- 2026-04-24: SCARFMAN runs without the previous audio hang after rate-limited PWM updates.
- 2026-04-24: `BACKUP :0 :1` completed on user-tested media with optional F4 disk SFX enabled/audible.
