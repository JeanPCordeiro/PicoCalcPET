# Platform Layer

This directory will wrap reusable platform services from `picocalc-text-starter`.

Expected responsibilities:

- board initialization
- display access
- keyboard scanning
- SD card and file access
- timing
- serial or debug support

Only this layer should know about PicoCalc-specific hardware APIs.
