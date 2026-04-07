# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/codespace/pico-sdk/tools/pioasm"
  "/workspaces/PicoCalcTRS/build-pico-uf2/pioasm"
  "/workspaces/PicoCalcTRS/build-pico-uf2/pioasm-install"
  "/workspaces/PicoCalcTRS/build-pico-uf2/firmware/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/tmp"
  "/workspaces/PicoCalcTRS/build-pico-uf2/firmware/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp"
  "/workspaces/PicoCalcTRS/build-pico-uf2/firmware/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src"
  "/workspaces/PicoCalcTRS/build-pico-uf2/firmware/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/workspaces/PicoCalcTRS/build-pico-uf2/firmware/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/workspaces/PicoCalcTRS/build-pico-uf2/firmware/pico-sdk/src/rp2_common/pico_cyw43_driver/pioasm/src/pioasmBuild-stamp${cfgdir}") # cfgdir has leading slash
endif()
