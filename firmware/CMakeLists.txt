message(STATUS "Entering firmware scaffold")
message(STATUS "Local firmware code lives under ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "Vendor integration is documented under docs/")

set(SDLTRS_SRC_DIR "${SDLTRS_DIR}/src")
set(PICOCALC_STARTER_DRIVERS_DIR "${PICOCALC_TEXT_STARTER_DIR}/drivers")
set(PICOCALC_AUDIO_PIO "${PICOCALC_STARTER_DRIVERS_DIR}/audio.pio")

set(SDLTRS_CORE_SOURCES
    ${SDLTRS_SRC_DIR}/error.c
    ${SDLTRS_SRC_DIR}/trs_clones.c
    ${SDLTRS_SRC_DIR}/trs_cp500.c
    ${SDLTRS_SRC_DIR}/trs_interrupt.c
    ${SDLTRS_SRC_DIR}/trs_io.c
    ${SDLTRS_SRC_DIR}/trs_state_save.c
    ${SDLTRS_SRC_DIR}/z80.c
)

if(PICOCALC_PLATFORM)
    list(APPEND SDLTRS_CORE_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/emu/trs_cmd_rom_embedded.c
        ${CMAKE_CURRENT_SOURCE_DIR}/emu/trs_disk_embedded.c
        ${CMAKE_CURRENT_SOURCE_DIR}/emu/trs_memory_embedded.c
    )
else()
    list(APPEND SDLTRS_CORE_SOURCES
        ${SDLTRS_SRC_DIR}/trs_cmd_rom.c
        ${SDLTRS_SRC_DIR}/trs_disk.c
        ${SDLTRS_SRC_DIR}/trs_memory.c
    )
endif()

add_library(sdltrs_frontend_stubs STATIC
    compat/sdl_compat.c
    frontend/trs_frontend_stub.c
    frontend/trs_keyboard_stub.c
    emu/sdltrs_peripheral_stubs.c
)

target_include_directories(sdltrs_frontend_stubs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/compat
    ${SDLTRS_SRC_DIR}
)

if(PICOCALC_PLATFORM)
    pico_sdk_init()
    pico_generate_pio_header(sdltrs_frontend_stubs ${PICOCALC_AUDIO_PIO})
    target_compile_definitions(sdltrs_frontend_stubs PUBLIC PICOCALC_PLATFORM=1)
    target_sources(sdltrs_frontend_stubs PRIVATE
        ${CMAKE_SOURCE_DIR}/fonts/trs80_model3_font_5x18.c
        ${PICOCALC_STARTER_DRIVERS_DIR}/audio.c
        platform/platform_picocalc.c
        platform/platform_time_picocalc.c
        ${PICOCALC_STARTER_DRIVERS_DIR}/display.c
        ${PICOCALC_STARTER_DRIVERS_DIR}/fat32.c
        ${PICOCALC_STARTER_DRIVERS_DIR}/font-5x10.c
        ${PICOCALC_STARTER_DRIVERS_DIR}/keyboard.c
        ${PICOCALC_STARTER_DRIVERS_DIR}/lcd.c
        ${PICOCALC_STARTER_DRIVERS_DIR}/picocalc.c
        platform/platform_disk_stdio.c
        ${PICOCALC_STARTER_DRIVERS_DIR}/sdcard.c
        ${PICOCALC_STARTER_DRIVERS_DIR}/southbridge.c
        ${PICOCALC_STARTER_DRIVERS_DIR}/font-8x10.c
    )
    target_include_directories(sdltrs_frontend_stubs PUBLIC
        ${PICOCALC_TEXT_STARTER_DIR}
        ${PICOCALC_STARTER_DRIVERS_DIR}
    )
    set_source_files_properties(
        ${PICOCALC_STARTER_DRIVERS_DIR}/display.c
        PROPERTIES COMPILE_DEFINITIONS "debug=display_debug"
    )
    target_link_libraries(sdltrs_frontend_stubs PUBLIC
        pico_stdlib
        hardware_gpio
        hardware_i2c
        hardware_spi
        hardware_pio
        hardware_clocks
    )
else()
    target_sources(sdltrs_frontend_stubs PRIVATE
        platform/platform_host.c
        platform/platform_file_host.c
    )
endif()

if(PICOCALC_PLATFORM)
    target_sources(sdltrs_frontend_stubs PRIVATE
        platform/platform_file_picocalc.c
    )
endif()

add_library(sdltrs_core STATIC
    ${SDLTRS_CORE_SOURCES}
)

target_include_directories(sdltrs_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/compat
    ${SDLTRS_SRC_DIR}
)

target_compile_definitions(sdltrs_core PUBLIC
    ROM_PATH=""
)

if(PICOCALC_PLATFORM)
    target_compile_definitions(sdltrs_core PUBLIC PICOCALC_PLATFORM=1)
    target_include_directories(sdltrs_core PUBLIC
        ${PICOCALC_TEXT_STARTER_DIR}
        ${PICOCALC_STARTER_DRIVERS_DIR}
    )
endif()

add_library(picocalc_vendor_headers INTERFACE)

target_include_directories(picocalc_vendor_headers INTERFACE
    ${PICOCALC_TEXT_STARTER_DIR}
    ${PICOCALC_STARTER_DRIVERS_DIR}
)

add_executable(picocalc_trs_scaffold
    main.c
)

target_include_directories(picocalc_trs_scaffold PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/compat
)

target_link_libraries(picocalc_trs_scaffold PRIVATE
    sdltrs_core
    sdltrs_frontend_stubs
)

if(PICOCALC_PLATFORM)
    pico_enable_stdio_usb(picocalc_trs_scaffold 0)
    pico_enable_stdio_uart(picocalc_trs_scaffold 1)
    pico_add_extra_outputs(picocalc_trs_scaffold)
    if(PICO_NO_PICOTOOL)
        message(WARNING "PICO_NO_PICOTOOL=1 disables UF2 generation; the build will emit ELF/BIN/HEX outputs only.")
    endif()
endif()
