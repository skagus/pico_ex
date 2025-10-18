# The project name is inherited from the root CMakeLists.txt


# Create our executable
add_executable(${PROJECT_NAME}
    main.cpp
)

# Enable USB stdio
pico_enable_stdio_usb(pio_blink_project 1)

# Disable UART stdio
pico_enable_stdio_uart(pio_blink_project 0)

# Generate the PIO header file from our .pio file and add it to our executable
pico_generate_pio_header(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/spi.pio
)

# Link to the necessary SDK libraries
target_link_libraries(${PROJECT_NAME}
	pico_stdlib
	hardware_pio
	hardware_clocks
	hardware_gpio
	hardware_spi
	hardware_dma
)

# Enable USB serial output
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

# Add other output formats (e.g., .uf2)
pico_add_extra_outputs(${PROJECT_NAME})
