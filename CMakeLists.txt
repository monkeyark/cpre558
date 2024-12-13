# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(cpre558)

# Add required components
idf_build_set_property(COMPONENTS 
    "bt" 
    "nvs_flash" 
    "driver" 
    "esp_common" 
    "esp_event" 
    "esp_ble" 
    APPEND)

idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES freertos
)