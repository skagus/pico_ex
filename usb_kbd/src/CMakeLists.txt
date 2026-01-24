# The project name is inherited from the root CMakeLists.txt

# Create our executable
add_executable(${PROJECT_NAME}
	main.c
	usb_desc.c
	tud_if.c
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