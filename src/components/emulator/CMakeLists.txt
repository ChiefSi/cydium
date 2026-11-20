# Host side of the emulator image: the cydium project built for ESP-IDF's
# `linux` target (./build.sh builds it unless --no-build-emulator). Serves the
# screen on a local web page, takes the mouse as the touch panel, and keeps
# the emulated flash (NVS settings) in a file. Empty on real chips.
idf_build_get_property(target IDF_TARGET)
if(NOT target STREQUAL "linux")
    idf_component_register()
    return()
endif()

idf_component_register(
    SRCS "emulator.cpp" "flash.cpp" "options.cpp" "server.cpp" "static_link.cpp"
    INCLUDE_DIRS "include"
    REQUIRES gfx
    PRIV_REQUIRES esp_partition esp_timer log
    EMBED_TXTFILES "page.html"
    # flash.cpp and options.cpp work from static constructors, which must not
    # be dropped.
    WHOLE_ARCHIVE)

# The partition table, to set up a new flash file on first run.
idf_build_get_property(build_dir BUILD_DIR)
target_add_binary_data(${COMPONENT_LIB} "${build_dir}/partition_table/partition-table.bin" BINARY
                       DEPENDS partition_table_bin)

# Link statically, so the binary runs on hosts with an older C library than
# the build container's. static_link.cpp stands in for dlsym(RTLD_NEXT).
target_link_options(${COMPONENT_LIB} INTERFACE -static -Wl,--wrap=dlsym)
