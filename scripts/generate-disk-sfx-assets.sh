#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
assets_dir="${repo_root}/firmware/assets"
out_dir="${assets_dir}/generated"
sample_rate=11025

mkdir -p "${out_dir}"

python3 - "$sample_rate" \
    "${assets_dir}/sounds_spin.pcm" "${assets_dir}/sounds_track.pcm" \
    "${out_dir}/disk_sfx_assets.h" "${out_dir}/disk_sfx_assets.c" <<'PY'
import array
import pathlib
import sys

sample_rate = int(sys.argv[1])
spin_path = pathlib.Path(sys.argv[2])
track_path = pathlib.Path(sys.argv[3])
header_path = pathlib.Path(sys.argv[4])
source_path = pathlib.Path(sys.argv[5])

SOURCE_RATE = 44100
SOURCE_CHANNELS = 2
DECIMATE = SOURCE_RATE // sample_rate
if SOURCE_RATE % sample_rate != 0:
    raise SystemExit("sample_rate must divide 44100 exactly")

def load_s16le_stereo(path):
    raw = path.read_bytes()
    data = array.array("h")
    data.frombytes(raw)
    if sys.byteorder != "little":
        data.byteswap()
    return data

def convert(path):
    data = load_s16le_stereo(path)
    out = bytearray()
    frame_count = len(data) // SOURCE_CHANNELS
    for frame in range(0, frame_count, DECIMATE):
        acc = 0
        count = 0
        for sub in range(DECIMATE):
            index = (frame + sub) * SOURCE_CHANNELS
            if index + 1 >= len(data):
                break
            acc += (int(data[index]) + int(data[index + 1])) // 2
            count += 1
        if count == 0:
            continue
        sample = acc // count
        u8 = max(0, min(255, (sample + 32768) >> 8))
        out.append(u8)
    return bytes(out)

spin = convert(spin_path)
track = convert(track_path)

def c_array(name, data):
    lines = [f"const uint8_t {name}[] = {{"]
    for offset in range(0, len(data), 12):
        chunk = data[offset:offset + 12]
        lines.append("    " + ", ".join(f"0x{byte:02x}" for byte in chunk) + ",")
    lines.append("};")
    return "\n".join(lines)

header_path.write_text(f"""#ifndef PICOCALC_TRS_DISK_SFX_ASSETS_H
#define PICOCALC_TRS_DISK_SFX_ASSETS_H

#include <stddef.h>
#include <stdint.h>

#define PICOCALC_DISK_SFX_SAMPLE_RATE {sample_rate}u

extern const uint8_t picocalc_disk_sfx_spin[];
extern const size_t picocalc_disk_sfx_spin_len;
extern const uint8_t picocalc_disk_sfx_track[];
extern const size_t picocalc_disk_sfx_track_len;

#endif
""")

source_path.write_text(f"""#include \"disk_sfx_assets.h\"

{c_array("picocalc_disk_sfx_spin", spin)}
const size_t picocalc_disk_sfx_spin_len = sizeof(picocalc_disk_sfx_spin);

{c_array("picocalc_disk_sfx_track", track)}
const size_t picocalc_disk_sfx_track_len = sizeof(picocalc_disk_sfx_track);
""")
PY

echo "Generated ${out_dir}/disk_sfx_assets.h"
echo "Generated ${out_dir}/disk_sfx_assets.c"
