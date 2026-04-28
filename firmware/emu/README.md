# Emulator Layer

This directory will hold local glue around the selected `sdltrs` emulator core files.

Expected responsibilities:

- emulator startup and reset
- ROM loading hooks
- memory and video integration glue
- execution loop orchestration
- phase-specific wrappers for optional peripherals

Vendor code should remain under `third_party/sdltrs/`.
