# Hardware-free drawing library: no IDF driver or esp_lcd headers, so it also
# builds on the host for unit tests (src/test).
idf_component_register(
    SRCS "font8x8.cpp" "layer.cpp"
    INCLUDE_DIRS "include")
