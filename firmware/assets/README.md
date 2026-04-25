# Assets

This directory is reserved for firmware-side runtime assets such as:

- default configuration files
- character set data if needed
- startup screens
- non-vendor helper assets
- disk activity source sounds

ROM images and user disk images should not be committed here.

Disk activity sounds:

- `sounds_spin.pcm` is the raw 44.1 kHz stereo s16le motor/spindle source.
- `sounds_track.pcm` is the raw 44.1 kHz stereo s16le track/access source.
- `sounds_spin.mp3` and `sounds_track.mp3` are reference/compressed copies.
- `generated/disk_sfx_assets.*` contains 11.025 kHz mono unsigned PCM arrays
  generated from the `.pcm` files with `scripts/generate-disk-sfx-assets.sh`.
