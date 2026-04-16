#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

PICO_SDK_PATH="${PICO_SDK_PATH:-$HOME/pico-sdk}"
PICOTOOL_SRC_DIR="${PICOTOOL_SRC_DIR:-/tmp/picotool}"
PICOTOOL_INSTALL_DIR="${PICOTOOL_INSTALL_DIR:-/tmp/picotool/install}"
BUILD_DIR="${BUILD_DIR:-${REPO_ROOT}/build-pico-uf2}"
DIST_DIR="${DIST_DIR:-${REPO_ROOT}/dist}"
PICOCALC_ENABLE_FDC_DIAG="${PICOCALC_ENABLE_FDC_DIAG:-OFF}"
PICOCALC_ENABLE_DISK_FAULT_DIAG="${PICOCALC_ENABLE_DISK_FAULT_DIAG:-OFF}"

if [[ ! -d "${PICO_SDK_PATH}" ]]; then
    echo "error: PICO_SDK_PATH does not exist: ${PICO_SDK_PATH}" >&2
    exit 1
fi

need_picotool_build=0

if [[ ! -x "${PICOTOOL_INSTALL_DIR}/bin/picotool" ]]; then
    need_picotool_build=1
fi

if [[ ! -f "${PICOTOOL_INSTALL_DIR}/lib/cmake/picotool/picotoolConfig.cmake" ]]; then
    need_picotool_build=1
fi

if [[ "${need_picotool_build}" -eq 1 ]]; then
    echo "Preparing local picotool install under ${PICOTOOL_INSTALL_DIR}"
    rm -rf "${PICOTOOL_SRC_DIR}"
    git clone https://github.com/raspberrypi/picotool "${PICOTOOL_SRC_DIR}"
    cmake -S "${PICOTOOL_SRC_DIR}" -B "${PICOTOOL_SRC_DIR}/build" -DPICO_SDK_PATH="${PICO_SDK_PATH}"
    cmake --build "${PICOTOOL_SRC_DIR}/build" -j"$(nproc)"
    cmake --install "${PICOTOOL_SRC_DIR}/build" --prefix "${PICOTOOL_INSTALL_DIR}"
fi

export PICO_SDK_PATH
export PATH="${PICOTOOL_INSTALL_DIR}/bin:${PATH}"

cmake -S "${REPO_ROOT}" \
    -B "${BUILD_DIR}" \
    -DPICOCALC_PLATFORM=ON \
    -DPICOCALC_ENABLE_FDC_DIAG="${PICOCALC_ENABLE_FDC_DIAG}" \
    -DPICOCALC_ENABLE_DISK_FAULT_DIAG="${PICOCALC_ENABLE_DISK_FAULT_DIAG}" \
    -DPICO_NO_PICOTOOL=0 \
    -Dpicotool_DIR="${PICOTOOL_INSTALL_DIR}/lib/cmake/picotool"

cmake --build "${BUILD_DIR}" -j"$(nproc)"

mkdir -p "${DIST_DIR}"
cp "${BUILD_DIR}/firmware/picocalc_trs_scaffold.uf2" "${DIST_DIR}/picocalc_trs_scaffold.uf2"

echo
echo "UF2 build complete:"
echo "  ${BUILD_DIR}/firmware/picocalc_trs_scaffold.uf2"
echo "Stable copy:"
echo "  ${DIST_DIR}/picocalc_trs_scaffold.uf2"
