cmake_minimum_required(VERSION 3.13)

option(PICOCALC_PLATFORM "Build with Pico SDK and PicoCalc starter drivers" OFF)

if(PICOCALC_PLATFORM)
    if(NOT DEFINED PICO_PLATFORM)
        set(PICO_PLATFORM rp2350 CACHE STRING "Pico platform")
    endif()
    if(NOT DEFINED PICO_BOARD)
        set(PICO_BOARD pico2 CACHE STRING "Pico board")
    endif()
    include(${CMAKE_CURRENT_SOURCE_DIR}/third_party/picocalc-text-starter/pico_sdk_import.cmake)
endif()

project(PicoCalcTRS C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PICOCALC_TEXT_STARTER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/picocalc-text-starter")
set(SDLTRS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sdltrs")

message(STATUS "Configuring PicoCalcTRS")
message(STATUS "picocalc-text-starter: ${PICOCALC_TEXT_STARTER_DIR}")
message(STATUS "sdltrs: ${SDLTRS_DIR}")

if(NOT EXISTS "${PICOCALC_TEXT_STARTER_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Missing vendor tree: third_party/picocalc-text-starter")
endif()

if(NOT EXISTS "${SDLTRS_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Missing vendor tree: third_party/sdltrs")
endif()

add_subdirectory(firmware)
