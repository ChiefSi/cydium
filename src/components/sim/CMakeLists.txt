# Simulation framework: hardware-free (no IDF driver or esp_lcd headers), so
# it also builds on the host for unit tests (src/test).
idf_component_register(
    SRCS "manager.cpp"
    INCLUDE_DIRS "include"
    REQUIRES gfx)
