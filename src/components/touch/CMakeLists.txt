idf_build_get_property(target IDF_TARGET)
if(target STREQUAL "linux")
    # Emulator: the mouse on the emulator's web page is the touch panel.
    idf_component_register(
        SRCS "touch_linux.cpp" "calibration.cpp"
        INCLUDE_DIRS "include"
        REQUIRES gfx
        PRIV_REQUIRES emulator)
else()
    idf_component_register(
        SRCS "touch.cpp" "calibration.cpp"
        INCLUDE_DIRS "include"
        REQUIRES board esp_driver_gpio gfx)
endif()
