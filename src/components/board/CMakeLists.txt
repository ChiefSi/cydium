idf_build_get_property(target IDF_TARGET)
if(target STREQUAL "linux")
    # Emulator: backlight and BOOT button go through the emulator's web page.
    idf_component_register(
        SRCS "board_linux.cpp"
        INCLUDE_DIRS "include"
        PRIV_REQUIRES emulator)
else()
    idf_component_register(
        SRCS "board.cpp"
        INCLUDE_DIRS "include"
        REQUIRES esp_driver_gpio esp_driver_ledc esp_adc)
endif()
