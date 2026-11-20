idf_component_register(
    SRCS "display.cpp"
    INCLUDE_DIRS "include"
    REQUIRES board esp_lcd esp_driver_spi
    PRIV_REQUIRES espressif__esp_lcd_ili9341)
