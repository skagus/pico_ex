# The project name is inherited from the root CMakeLists.txt

# Create our executable
add_executable(${PROJECT_NAME}
	main.c
	usb_desc.c
	tud_if.c
	kbd.c
	usb_cdc.c
	usb_hid.c
	board.c
)

# Disable default stdio (we use TinyUSB directly)
pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

# Link to the necessary SDK libraries
target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    tinyusb_device
)

# Add TinyUSB include directory
target_include_directories(${PROJECT_NAME} PRIVATE
	${CMAKE_CURRENT_LIST_DIR}
)

# Add other output formats (e.g., .uf2)
pico_add_extra_outputs(${PROJECT_NAME})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND arm-none-eabi-objdump -S -d 
            ${PROJECT_BINARY_DIR}/src/${PROJECT_NAME}.elf 
            > ${PROJECT_BINARY_DIR}/src/${PROJECT_NAME}.lst
    COMMENT "Generating disassembly file with source code..."
)