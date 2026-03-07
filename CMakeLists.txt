cmake_minimum_required(VERSION 3.20)
project(fw_t018_g474 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}
    main_host.c
    App/app.c
    BSP/bsp_port_stub.c
    Drivers/drv_adc_internal.c
    Drivers/drv_beep.c
    Drivers/drv_button.c
    Drivers/drv_freq_ic.c
    Drivers/drv_mux4051.c
    Drivers/drv_oled_ssd1306.c
    Measurements/measurements.c
    Calib/calib.c
)

target_compile_definitions(${PROJECT_NAME} PRIVATE BSP_USE_HAL_PORT=0)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/App
    ${CMAKE_CURRENT_SOURCE_DIR}/BSP
    ${CMAKE_CURRENT_SOURCE_DIR}/Drivers
    ${CMAKE_CURRENT_SOURCE_DIR}/Measurements
    ${CMAKE_CURRENT_SOURCE_DIR}/Calib
)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()
