idf_component_register(
  INCLUDE_DIRS "include"   # public headers visible to dependents
  SRCS "wifi.cpp"          # WiFi manager implementation
  REQUIRES "esp_wifi" "esp_netif" "esp_event" "lwip"
)

# Add ArduinoJson from PlatformIO lib_deps - try to find it in common locations
set(ARDUINOJSON_INCLUDE_DIR "")
foreach(ENV_NAME esp32s3-n16r8 esp32-s3-devkitc-1 esp32-s3)
  if(EXISTS "${CMAKE_SOURCE_DIR}/.pio/libdeps/${ENV_NAME}/ArduinoJson/src")
    set(ARDUINOJSON_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/.pio/libdeps/${ENV_NAME}/ArduinoJson/src")
    break()
  endif()
endforeach()

if(ARDUINOJSON_INCLUDE_DIR)
  target_include_directories(${COMPONENT_LIB} PUBLIC "${ARDUINOJSON_INCLUDE_DIR}")
else()
  message(FATAL_ERROR "ArduinoJson not found in .pio/libdeps")
endif()