idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES board bounce buildinfo display esp_timer gfx settings sim starfield touch ui)
