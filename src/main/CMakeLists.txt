idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES board buildinfo display esp_timer gfx settings touch ui)
