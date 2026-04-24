# PicoCalcTRS Video Fidelity Checklist

Use this checklist for focused on-device video validation. The goal is to classify visible issues before changing rendering code.

## Current Renderer Surface

- 64x16 Model III text mode.
- TRS cursor rendering with PicoCalc firmware cursor suppressed in the TRS area.
- TRS scrolling behavior.
- TRS text/status color split.
- Procedural 2x3 block rendering for Model III semigraphics characters `0x80`-`0xBF`.
- Expanded text mode handling.
- Reverse/inverse/alternate mode handling through the local frontend mode flags.

## Test Areas

### Text

- ROM prompt text is readable.
- DOS prompt text is readable.
- BASIC program listings align in columns.
- Full-screen text does not overlap the status area.

### Cursor

- Cursor appears in ROM BASIC input.
- Cursor appears in DOS input.
- Cursor is erased cleanly when moving.
- Cursor does not leave stale underline pixels.

### Scrolling

- Fill the screen with printed lines.
- Confirm upward scrolling preserves line order.
- Confirm no full-screen clear or stale-line flash occurs during normal scroll.

### Semigraphics

- Run a semigraphics-heavy game or visual test.
- Confirm characters `0x80`-`0xBF` render as 2x3 block graphics.
- Confirm empty/full block combinations look consistent.
- Confirm semigraphics in expanded mode are double-width and do not leave stale pixels.

### Attributes

- Verify inverse text where available.
- Verify reverse-video mode where available.
- Verify alternate character set behavior where software uses it.
- Verify expanded text where software uses it.

## Suggested Test Matrix

| Case | Software/Input | Expected | Result | Notes |
| --- | --- | --- | --- | --- |
| ROM prompt | Power-on BASIC | readable prompt and cursor | nt | |
| DOS prompt | Boot LDOS/TRSDOS | readable prompt and cursor | nt | |
| Text scroll | BASIC print loop | clean upward scroll | nt | |
| Semigraphics | semigraphics-heavy game | 2x3 block graphics | nt | |
| Expanded text | app/game using expanded mode | double-width cells, no stale pixels | nt | |
| Inverse/reverse | app/game using attributes | correct foreground/background swap | nt | |

## Notes Template

```text
Case:
Software/image:
ROM source:
UF2 sha256:
Result:
Observed issue:
Likely area:
Repro steps:
Notes:
```
