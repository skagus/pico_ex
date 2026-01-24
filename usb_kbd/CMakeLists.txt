cmake_minimum_required(VERSION 3.13)

# This file assumes that PICO_SDK_PATH is set as an environment variable
# pointing to the location of the Pico SDK.
# You can get the SDK with: git clone https://github.com/raspberrypi/pico-sdk.git
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(usb_kbd C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

# Add the subdirectory with our source files
add_subdirectory(src)
