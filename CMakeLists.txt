# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)

# Set platform to RP2350 (native RP2350B support with RISC-V)
set(PICO_PLATFORM rp2350-arm-s CACHE STRING "Pico platform")
set(PICO_BOARD weact_studio_rp2350b_core CACHE STRING "Board type")

# Include the Pico SDK import
include(pico_sdk_import.cmake)

# Project name
project(tesla_bms_rp2350 C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Skip picotool to avoid build issues on Windows
set(SKIP_PICO_PROGRAM 1)
set(PICO_NO_FLASH 0)
set(PICO_COPY_TO_RAM 0)
set(PICO_NO_DIS_OUTPUT 1)

# Initialize the Pico SDK
pico_sdk_init()

# Add include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_SOURCE_DIR}/can2040/src
)

# Create the main executable
add_executable(tesla_bms_rp2350
    src/main.c
    src/batman.c
    src/param.c
    src/ina228.c
    src/adc_monitor.c
    src/coulomb_counter.c
    src/can_interface.c
    can2040/src/can2040.c
)

# Link libraries
target_link_libraries(tesla_bms_rp2350
    pico_stdlib
    hardware_spi
    hardware_i2c
    hardware_uart
    hardware_adc
    hardware_pwm
    hardware_dma
    hardware_gpio
    hardware_pio
    hardware_irq
)

# Enable USB serial output
pico_enable_stdio_usb(tesla_bms_rp2350 1)
pico_enable_stdio_uart(tesla_bms_rp2350 0)

# Create map/bin/hex/uf2 files
# Note: pico_add_extra_outputs requires picotool which has build issues on Windows
# The .elf file will still be created and can be converted to .uf2 manually
# pico_add_extra_outputs(tesla_bms_rp2350)

# Print memory usage (disabled on Windows due to path issues)
# add_custom_command(TARGET tesla_bms_rp2350 POST_BUILD
#     COMMAND ${CMAKE_SIZE} $<TARGET_FILE:tesla_bms_rp2350>
#     COMMENT "Memory usage:"
# )

