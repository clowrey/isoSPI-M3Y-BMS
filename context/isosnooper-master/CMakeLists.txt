cmake_minimum_required(VERSION 3.13)

# include the SDK
include(pico_sdk_import.cmake)

project(my_project C CXX)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

######

add_executable(isosnooper
    isosnooper.cpp
    isosnoop.cpp
    isospi_master.cpp
    )

pico_generate_pio_header(isosnooper ${CMAKE_CURRENT_LIST_DIR}/isospi_master.pio)
pico_generate_pio_header(isosnooper ${CMAKE_CURRENT_LIST_DIR}/isosnoop.pio)

# pull in common dependencies
target_link_libraries(isosnooper pico_stdlib hardware_dma hardware_pio)

set(PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS 3000)

# enable usb output, disable uart output
pico_enable_stdio_usb(isosnooper 1)
pico_enable_stdio_uart(isosnooper 0)

# create map/bin/hex file etc.
pico_add_extra_outputs(isosnooper)
