idf_build_get_property(target IDF_TARGET)
if(target STREQUAL "linux")
    # Emulator: the screen goes to the emulator's web page.
    idf_component_register(
        SRCS "display_linux.cpp" "orientation.cpp" "renderer.cpp"
        INCLUDE_DIRS "include"
        REQUIRES gfx
        PRIV_REQUIRES emulator log)
else()
    idf_component_register(
        SRCS "display.cpp" "orientation.cpp" "renderer.cpp"
        INCLUDE_DIRS "include"
        REQUIRES board esp_lcd esp_driver_spi gfx
        PRIV_REQUIRES espressif__esp_lcd_ili9341)
endif()
