#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_DIR="${BUILD_DIR:-${REPO_ROOT}/build-pico-uf2}"
DIST_DIR="${DIST_DIR:-${REPO_ROOT}/dist}"
REPORT_DIR="${DIST_DIR}/regression"
REPORT_FILE="${REPORT_DIR}/m1-report.txt"

RUN_BUILD="${RUN_BUILD:-1}"
RUN_HOST_BUILD="${RUN_HOST_BUILD:-0}"

pass_count=0
fail_count=0

mkdir -p "${REPORT_DIR}"

run_id="$(date -u +%Y%m%dT%H%M%SZ)"

echo "PicoCalcTRS M1 Regression Report" > "${REPORT_FILE}"
echo "Run ID: ${run_id}" >> "${REPORT_FILE}"
echo "Repo: ${REPO_ROOT}" >> "${REPORT_FILE}"
echo >> "${REPORT_FILE}"

check_pass() {
    local msg="$1"
    echo "[PASS] ${msg}"
    echo "[PASS] ${msg}" >> "${REPORT_FILE}"
    pass_count=$((pass_count + 1))
}

check_fail() {
    local msg="$1"
    echo "[FAIL] ${msg}" >&2
    echo "[FAIL] ${msg}" >> "${REPORT_FILE}"
    fail_count=$((fail_count + 1))
}

section() {
    local title="$1"
    echo
    echo "== ${title} =="
    {
        echo
        echo "== ${title} =="
    } >> "${REPORT_FILE}"
}

section "Build"
if [[ "${RUN_BUILD}" == "1" ]]; then
    if "${REPO_ROOT}/scripts/build-pico-uf2.sh" >> "${REPORT_FILE}" 2>&1; then
        check_pass "UF2 build completed"
    else
        check_fail "UF2 build failed"
    fi
else
    check_pass "UF2 build skipped (RUN_BUILD=${RUN_BUILD})"
fi

section "Patch Set"
if [[ -f "${REPO_ROOT}/patches/sdltrs/0001-picocalc-trs_disk.patch" ]]; then
    check_pass "Patch 0001 exists"
else
    check_fail "Patch 0001 missing"
fi

if [[ -f "${REPO_ROOT}/patches/sdltrs/0004-picocalc-trs_memory.patch" ]]; then
    check_pass "Patch 0004 exists"
else
    check_fail "Patch 0004 missing"
fi

if [[ -f "${REPO_ROOT}/patches/picocalc-text-starter/0001-picocalc-fat32-next-free-wrap.patch" ]]; then
    check_pass "Starter FAT32 patch exists"
else
    check_fail "Starter FAT32 patch missing"
fi

if [[ ! -f "${REPO_ROOT}/patches/sdltrs/0002-picocalc-trs_interrupt.patch" ]]; then
    check_pass "Patch 0002 removed as intended"
else
    check_fail "Unexpected patch 0002 present"
fi

if [[ ! -f "${REPO_ROOT}/patches/sdltrs/0003-picocalc-trs_cmd_rom.patch" ]]; then
    check_pass "Patch 0003 removed as intended"
else
    check_fail "Unexpected patch 0003 present"
fi

section "Artifacts"
uf2_build="${BUILD_DIR}/firmware/PicoCalcTRS.uf2"
uf2_dist="${DIST_DIR}/PicoCalcTRS.uf2"

if [[ -f "${uf2_build}" ]]; then
    check_pass "Build UF2 exists (${uf2_build})"
else
    check_fail "Build UF2 missing (${uf2_build})"
fi

if [[ -f "${uf2_dist}" ]]; then
    check_pass "Dist UF2 exists (${uf2_dist})"
else
    check_fail "Dist UF2 missing (${uf2_dist})"
fi

if [[ -f "${uf2_dist}" ]]; then
    uf2_size="$(wc -c < "${uf2_dist}")"
    uf2_sha="$(sha256sum "${uf2_dist}" | awk '{print $1}')"
    echo "UF2 size: ${uf2_size}" >> "${REPORT_FILE}"
    echo "UF2 sha256: ${uf2_sha}" >> "${REPORT_FILE}"
    if [[ "${uf2_size}" -gt 0 ]]; then
        check_pass "Dist UF2 is non-empty"
    else
        check_fail "Dist UF2 is empty"
    fi
fi

section "Integration Guards"
compile_commands="${BUILD_DIR}/compile_commands.json"

if [[ -f "${compile_commands}" ]]; then
    check_pass "compile_commands.json exists"
else
    check_fail "compile_commands.json missing"
fi

if [[ -f "${compile_commands}" ]]; then
    if grep -q -- "-include ${REPO_ROOT}/firmware/compat/sdltrs_cmd_rom_stdio_shim.h" "${compile_commands}"; then
        check_pass "trs_cmd_rom stdio shim is injected at compile time"
    else
        check_fail "trs_cmd_rom stdio shim include not found in compile commands"
    fi
fi

if grep -q "^+#define NDRIVES 2" "${REPO_ROOT}/patches/sdltrs/0001-picocalc-trs_disk.patch"; then
    check_pass "Disk patch enforces NDRIVES=2"
else
    check_fail "Disk patch does not show NDRIVES=2"
fi

if grep -q 'PICOCALC_TRS_ROM_DIR "/TRS80/ROMS"' "${REPO_ROOT}/firmware/main.c"; then
    check_pass "Runtime ROM directory is /TRS80/ROMS"
else
    check_fail "Runtime ROM directory is not /TRS80/ROMS"
fi

if grep -q 'PICOCALC_TRS_DISK_DIR "/TRS80/DISKS"' "${REPO_ROOT}/firmware/main.c"; then
    check_pass "Runtime disk directory is /TRS80/DISKS"
else
    check_fail "Runtime disk directory is not /TRS80/DISKS"
fi

if grep -q "platform_list_disk_images" "${REPO_ROOT}/firmware/main.c" &&
   grep -q "run_disk_picker_for_drive" "${REPO_ROOT}/firmware/main.c"; then
    check_pass "Startup disk picker is wired into firmware"
else
    check_fail "Startup disk picker wiring missing"
fi

if grep -q 'disk%d' "${REPO_ROOT}/firmware/main.c"; then
    check_fail "Legacy disk0/disk1 filename probing is still present"
else
    check_pass "Legacy disk0/disk1 filename probing is absent"
fi

if grep -q "name\\[0\\] == '.'" "${REPO_ROOT}/firmware/platform/platform_picocalc.c" &&
   grep -q '".dsk"' "${REPO_ROOT}/firmware/platform/platform_picocalc.c" &&
   grep -q '".dmk"' "${REPO_ROOT}/firmware/platform/platform_picocalc.c" &&
   grep -q '".jv1"' "${REPO_ROOT}/firmware/platform/platform_picocalc.c" &&
   grep -q '".jv3"' "${REPO_ROOT}/firmware/platform/platform_picocalc.c"; then
    check_pass "Disk picker filters dotfiles and supported image extensions"
else
    check_fail "Disk picker filter guard failed"
fi

section "Optional Host Build"
if [[ "${RUN_HOST_BUILD}" == "1" ]]; then
    host_build_dir="${REPO_ROOT}/build-m1-host"
    if cmake -S "${REPO_ROOT}" -B "${host_build_dir}" >> "${REPORT_FILE}" 2>&1 \
       && cmake --build "${host_build_dir}" -j"$(nproc)" >> "${REPORT_FILE}" 2>&1; then
        check_pass "Host build succeeded"
    else
        check_fail "Host build failed"
    fi
else
    check_pass "Host build skipped (RUN_HOST_BUILD=${RUN_HOST_BUILD})"
fi

section "Summary"
echo "Passes: ${pass_count}" | tee -a "${REPORT_FILE}"
echo "Failures: ${fail_count}" | tee -a "${REPORT_FILE}"
echo "Manual checklist: ${REPO_ROOT}/docs/m1-compatibility-checklist.md" | tee -a "${REPORT_FILE}"

if [[ "${fail_count}" -ne 0 ]]; then
    echo
    echo "M1 regression failed. See report: ${REPORT_FILE}" >&2
    exit 1
fi

echo
echo "M1 regression passed. Report: ${REPORT_FILE}"
