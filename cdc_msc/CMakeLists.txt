cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(pico_cdc_msc C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(pico_cdc_msc
    src/main.c
    src/msc_disk.c
    src/usb_descriptors.c
)

# PIO 어셈블리 소스에서 C 헤더 자동 생성
# pico_generate_pio_header(pico_cdc_msc ${CMAKE_CURRENT_LIST_DIR}/i2s_1ch.pio)

target_include_directories(pico_cdc_msc PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(pico_cdc_msc
    pico_stdlib
    tinyusb_device
)

# USB-CDC 출력 활성화, UART 출력 비활성화
pico_enable_stdio_usb(pico_cdc_msc 1)
pico_enable_stdio_uart(pico_cdc_msc 0)

pico_add_extra_outputs(pico_cdc_msc)
