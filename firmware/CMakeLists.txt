message(STATUS "Entering PicoCalcPET firmware")
message(STATUS "Local firmware code lives under ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "Vendor integration is documented under docs/")

set(PICOCALC_STARTER_DRIVERS_DIR "${PICOCALC_TEXT_STARTER_DIR}/drivers")
set(PICOCALC_AUDIO_PIO "${PICOCALC_STARTER_DRIVERS_DIR}/audio.pio")
set(VICE_SRC_DIR "${VICE_DIR}/src")

add_library(vice_pet_reference INTERFACE)
target_include_directories(vice_pet_reference INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/compat/vice
    ${VICE_SRC_DIR}
    ${VICE_SRC_DIR}/pet
)

add_library(pet_core STATIC
    emu/pet2001.c
    emu/vice_6502_cpu.c
)

target_include_directories(pet_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(pet_core PUBLIC vice_pet_reference)

add_library(pet_frontend STATIC
    frontend/pet_frontend.c
    emu/pet_app_identity.c
)

target_include_directories(pet_frontend PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(pet_frontend PUBLIC vice_pet_reference)

if(PICOCALC_PLATFORM)
    pico_sdk_init()

    target_compile_definitions(pet_core PUBLIC PICOCALC_PLATFORM=1)
    target_compile_definitions(pet_frontend PUBLIC PICOCALC_PLATFORM=1)
    pico_generate_pio_header(pet_frontend ${PICOCALC_AUDIO_PIO})

    target_sources(pet_frontend PRIVATE
        assets/generated/pet2001_logo.c
        platform/platform_picocalc.c
        platform/platform_file_picocalc.c
        ${PICOCALC_STARTER_DRIVERS_DIR}/audio.c
        ${PICOCALC_STARTER_DRIVERS_DIR}/display.c
        ${PICOCALC_STARTER_DRIVERS_DIR}/fat32.c
        ${PICOCALC_STARTER_DRIVERS_DIR}/font-5x10.c
        ${PICOCALC_STARTER_DRIVERS_DIR}/font-8x10.c
        ${PICOCALC_STARTER_DRIVERS_DIR}/keyboard.c
        ${PICOCALC_STARTER_DRIVERS_DIR}/lcd.c
        ${PICOCALC_STARTER_DRIVERS_DIR}/picocalc.c
        ${PICOCALC_STARTER_DRIVERS_DIR}/sdcard.c
        ${PICOCALC_STARTER_DRIVERS_DIR}/southbridge.c
    )
    target_include_directories(pet_frontend PUBLIC
        ${PICOCALC_TEXT_STARTER_DIR}
        ${PICOCALC_STARTER_DRIVERS_DIR}
    )
    set_source_files_properties(
        ${PICOCALC_STARTER_DRIVERS_DIR}/display.c
        PROPERTIES COMPILE_DEFINITIONS "debug=display_debug"
    )
    target_link_libraries(pet_frontend PUBLIC
        pico_stdlib
        hardware_gpio
        hardware_i2c
        hardware_spi
        hardware_pio
        hardware_clocks
    )
else()
    target_sources(pet_frontend PRIVATE
        platform/platform_host.c
        platform/platform_file_host.c
    )
endif()

if(NOT PICOCALC_PLATFORM)
    add_executable(pet_cpu_smoke
        emu/pet_cpu_smoke.c
        platform/platform_file_host.c
    )
    target_link_libraries(pet_cpu_smoke PRIVATE pet_core)
endif()

add_library(picocalc_vendor_headers INTERFACE)

target_include_directories(picocalc_vendor_headers INTERFACE
    ${PICOCALC_TEXT_STARTER_DIR}
    ${PICOCALC_STARTER_DRIVERS_DIR}
)

add_executable(PicoCalcPET
    main.c
)

target_include_directories(PicoCalcPET PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(PicoCalcPET PRIVATE
    pet_core
    pet_frontend
)

if(PICOCALC_PLATFORM)
    pico_enable_stdio_usb(PicoCalcPET 0)
    pico_enable_stdio_uart(PicoCalcPET 1)
    pico_add_extra_outputs(PicoCalcPET)
    if(PICO_NO_PICOTOOL)
        message(WARNING "PICO_NO_PICOTOOL=1 disables UF2 generation; the build will emit ELF/BIN/HEX outputs only.")
    endif()
endif()
