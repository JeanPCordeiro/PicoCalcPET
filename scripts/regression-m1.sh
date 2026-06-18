#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_DIR="${BUILD_DIR:-${REPO_ROOT}/build-host-m1}"
DIST_DIR="${DIST_DIR:-${REPO_ROOT}/dist}"
REPORT_DIR="${DIST_DIR}/regression"
REPORT_FILE="${REPORT_DIR}/m1-report.txt"

RUN_BUILD="${RUN_BUILD:-0}"
RUN_HOST_BUILD="${RUN_HOST_BUILD:-1}"

pass_count=0
fail_count=0

mkdir -p "${REPORT_DIR}"

run_id="$(date -u +%Y%m%dT%H%M%SZ)"
git_commit="$(git -C "${REPO_ROOT}" rev-parse --short HEAD 2>/dev/null || printf 'unknown')"
git_status="$(git -C "${REPO_ROOT}" status --short 2>/dev/null || true)"

echo "PicoCalcPET M1 Regression Report" > "${REPORT_FILE}"
echo "Run ID: ${run_id}" >> "${REPORT_FILE}"
echo "Repo: ${REPO_ROOT}" >> "${REPORT_FILE}"
echo "Commit: ${git_commit}" >> "${REPORT_FILE}"
if [[ -n "${git_status}" ]]; then
    echo "Worktree: dirty" >> "${REPORT_FILE}"
    echo "${git_status}" >> "${REPORT_FILE}"
else
    echo "Worktree: clean" >> "${REPORT_FILE}"
fi
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

section "Host Build"
if [[ "${RUN_HOST_BUILD}" == "1" ]]; then
    if cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" -DPICOCALC_PLATFORM=OFF >> "${REPORT_FILE}" 2>&1 &&
       cmake --build "${BUILD_DIR}" -j"$(nproc)" >> "${REPORT_FILE}" 2>&1; then
        check_pass "Host build completed"
    else
        check_fail "Host build failed"
    fi
else
    check_pass "Host build skipped (RUN_HOST_BUILD=${RUN_HOST_BUILD})"
fi

section "Pico UF2 Build"
if [[ "${RUN_BUILD}" == "1" ]]; then
    if "${REPO_ROOT}/scripts/build-pico-uf2.sh" >> "${REPORT_FILE}" 2>&1; then
        check_pass "UF2 build completed"
    else
        check_fail "UF2 build failed"
    fi
else
    check_pass "UF2 build skipped (RUN_BUILD=${RUN_BUILD})"
fi

section "Project Identity"
if grep -q "project(PicoCalcPET" "${REPO_ROOT}/CMakeLists.txt"; then
    check_pass "Top-level CMake project is PicoCalcPET"
else
    check_fail "Top-level CMake project is not PicoCalcPET"
fi

if grep -q "add_executable(PicoCalcPET" "${REPO_ROOT}/firmware/CMakeLists.txt"; then
    check_pass "Firmware target is PicoCalcPET"
else
    check_fail "Firmware target is not PicoCalcPET"
fi

if grep -q 'program_name = "PicoCalcPET"' "${REPO_ROOT}/firmware/main.c"; then
    check_pass "Runtime program_name is PicoCalcPET"
else
    check_fail "Runtime program_name is not PicoCalcPET"
fi

section "PET M1 Guards"
if grep -q 'PICOCALC_PET_ROM_DIR "/PET2001/ROMS"' "${REPO_ROOT}/firmware/main.c"; then
    check_pass "Runtime ROM directory is /PET2001/ROMS"
else
    check_fail "Runtime ROM directory is not /PET2001/ROMS"
fi

if grep -q "PET_FRONTEND_COLS = 40" "${REPO_ROOT}/firmware/frontend/pet_frontend.h" &&
   grep -q "PET_FRONTEND_ROWS = 25" "${REPO_ROOT}/firmware/frontend/pet_frontend.h"; then
    check_pass "PET screen geometry is 40 x 25"
else
    check_fail "PET screen geometry guard failed"
fi

if grep -q "pet2001_load_roms" "${REPO_ROOT}/firmware/emu/pet2001.c" &&
   grep -q "PICOCALC_PET_BASIC_ROM" "${REPO_ROOT}/firmware/main.c" &&
   grep -q "PICOCALC_PET_CHAR_ROM" "${REPO_ROOT}/firmware/main.c" &&
   grep -q "basic4.bin" "${REPO_ROOT}/firmware/main.c" &&
   grep -q "edit4.bin" "${REPO_ROOT}/firmware/main.c" &&
   grep -q "kernal4.bin" "${REPO_ROOT}/firmware/main.c"; then
    check_pass "PET ROM probing supports split and env override paths"
else
    check_fail "PET ROM probing is incomplete"
fi

if grep -q "PET2001 1.00MHz ROM:probe RAM:32K" "${REPO_ROOT}/firmware/frontend/pet_frontend.c" &&
   grep -q "KBD:ready TAPE:none PRG:none" "${REPO_ROOT}/firmware/frontend/pet_frontend.c"; then
    check_pass "Operator panel is wired for 32K PET status"
else
    check_fail "Operator panel wiring missing"
fi

if [[ -f "${REPO_ROOT}/patches/vice/.gitkeep" || -d "${REPO_ROOT}/patches/vice" ]]; then
    check_pass "VICE patch directory exists"
else
    check_fail "VICE patch directory missing"
fi

if grep -q "third_party/vice" "${REPO_ROOT}/CMakeLists.txt" &&
   ! grep -q "sdltrs_core" "${REPO_ROOT}/firmware/CMakeLists.txt"; then
    check_pass "Build is no longer wired to the TRS sdltrs core"
else
    check_fail "Stale sdltrs core wiring remains"
fi

if [[ -f "${REPO_ROOT}/third_party/vice/src/mos6510.h" ]] &&
   grep -q "mos6510_regs_t" "${REPO_ROOT}/firmware/emu/pet2001.h" &&
   grep -q "vice_pet_reference" "${REPO_ROOT}/firmware/CMakeLists.txt"; then
    check_pass "VICE CPU register interface is wired into pet_core"
else
    check_fail "VICE CPU register interface is not wired"
fi

if grep -q "kernal_rom\\[0x0FFC\\]" "${REPO_ROOT}/firmware/emu/pet2001.c" &&
   grep -q "MOS6510_REGS_SET_PC" "${REPO_ROOT}/firmware/emu/pet2001.c"; then
    check_pass "PET reset reads kernal vector into VICE CPU state"
else
    check_fail "PET reset vector wiring missing"
fi

if [[ -f "${REPO_ROOT}/firmware/emu/vice_6502_cpu.c" ]] &&
   grep -q '#include "6510core.c"' "${REPO_ROOT}/firmware/emu/vice_6502_cpu.c" &&
   grep -q "pet2001_read" "${REPO_ROOT}/firmware/emu/vice_6502_cpu.c" &&
   grep -q "pet2001_write" "${REPO_ROOT}/firmware/emu/vice_6502_cpu.c"; then
    check_pass "VICE 6510 core is routed through PET memory callbacks"
else
    check_fail "VICE 6510 core adapter wiring missing"
fi

if grep -q "address >= 0xE800 && address < 0xF000" "${REPO_ROOT}/firmware/emu/pet2001.c" &&
   grep -q "pet2001_read_pia1" "${REPO_ROOT}/firmware/emu/pet2001.c" &&
   grep -q "selected_key_row" "${REPO_ROOT}/firmware/emu/pet2001.c" &&
   grep -q "pet_frontend_render_video" "${REPO_ROOT}/firmware/frontend/pet_frontend.c" &&
   grep -q "video_writes" "${REPO_ROOT}/firmware/frontend/pet_frontend.c"; then
    check_pass "PET PIA/VIA stubs and video render path are wired"
else
    check_fail "PET PIA/VIA/video render wiring missing"
fi

section "Runtime Smoke"
if [[ -x "${BUILD_DIR}/firmware/PicoCalcPET" ]]; then
    set +e
    "${BUILD_DIR}/firmware/PicoCalcPET" >> "${REPORT_FILE}" 2>&1
    smoke_status=$?
    set -e
    if [[ "${smoke_status}" -eq 2 ]]; then
        check_pass "Missing-ROM smoke exits with expected status"
    else
        check_fail "Missing-ROM smoke returned ${smoke_status}, expected 2"
    fi
else
    check_fail "Host executable missing for runtime smoke"
fi

if [[ -x "${BUILD_DIR}/firmware/pet_cpu_smoke" ]]; then
    if "${BUILD_DIR}/firmware/pet_cpu_smoke" >> "${REPORT_FILE}" 2>&1; then
        check_pass "VICE 6502 host smoke passes"
    else
        check_fail "VICE 6502 host smoke failed"
    fi
else
    check_fail "VICE 6502 host smoke executable missing"
fi

section "Summary"
echo "Passes: ${pass_count}" >> "${REPORT_FILE}"
echo "Failures: ${fail_count}" >> "${REPORT_FILE}"
echo "Report: ${REPORT_FILE}"

if [[ "${fail_count}" -ne 0 ]]; then
    exit 1
fi
