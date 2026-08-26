cmake_minimum_required(VERSION 3.13)

# Include Pico SDK
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(st7735 C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

# Create executable
add_executable(${PROJECT_NAME}
    src/main.c
    src/st7735s.c
)

# Generate the PIO header file from our .pio file and add it to our executable
#pico_generate_pio_header(${PROJECT_NAME}
#    ${CMAKE_CURRENT_LIST_DIR}/src/spi.pio
#)

# Include directories (src directory & generated PIO header directory)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/src
)

# Link to the necessary SDK libraries
target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    hardware_gpio
    hardware_spi
    hardware_dma
)

# Enable USB stdio, Disable UART stdio
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

# Add other output formats (e.g., .uf2, .hex, .bin)
pico_add_extra_outputs(${PROJECT_NAME})

