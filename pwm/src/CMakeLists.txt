# The project name is inherited from the root CMakeLists.txt


# Create our executable
add_executable(${PROJECT_NAME}
    main.cpp
)

# Enable USB stdio
pico_enable_stdio_usb(pwm_project 1)

# Disable UART stdio
pico_enable_stdio_uart(pwm_project 0)

# Link to the necessary SDK libraries
target_link_libraries(${PROJECT_NAME}
    pico_stdlib
	hardware_gpio
	hardware_pwm
)

# Enable USB serial output
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

# Add other output formats (e.g., .uf2)
pico_add_extra_outputs(${PROJECT_NAME})
