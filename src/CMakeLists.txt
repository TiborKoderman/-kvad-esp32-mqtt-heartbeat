# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(
    SRCS ${app_sources}
        SRCS "src/main.cpp"
    INCLUDE_DIRS "include" "."
    REQUIRES fsm littlefs mqtt nvs_flash esp_netif esp_event esp_wifi
)