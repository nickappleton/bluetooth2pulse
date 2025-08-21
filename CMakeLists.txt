cmake_minimum_required(VERSION 3.12)

if(NOT DEFINED ENV{PICO_EXAMPLES_PATH} OR NOT DEFINED ENV{PICO_SDK_PATH})
  message(FATAL_ERROR "you must set both the PICO_EXAMPLES_PATH and PICO_SDK_PATH environment variables")
endif()

include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(bluetooth2pulse C CXX ASM)

pico_sdk_init()

add_executable(bluetooth2pulse main.c)

# We are using the UART for the bluetooth module. Do not put STDIO on it.
pico_enable_stdio_uart(bluetooth2pulse 0)
target_link_libraries(bluetooth2pulse pico_stdlib)

# Stuff for debugging.
if(ENABLE_DEBUGGING)
    pico_enable_stdio_usb(bluetooth2pulse 1)
    target_compile_definitions(bluetooth2pulse PRIVATE BT2P_ENABLE_DEBUG)
endif()

# Generate our PIO header for the LED
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bluetooth2pulse_gen)
pico_generate_pio_header(bluetooth2pulse $ENV{PICO_EXAMPLES_PATH}/pio/ws2812/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/bluetooth2pulse_gen)
target_link_libraries(bluetooth2pulse hardware_pio)

# We need PWM support for the tone generator
target_link_libraries(bluetooth2pulse hardware_pwm)

pico_add_extra_outputs(bluetooth2pulse)
