idf_component_register(
    SRCS "buildinfo.cpp"
    INCLUDE_DIRS "include")

if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    # CYD_GIT_DESCRIBE comes from build.sh (via a container build argument),
    # since the build context has no .git. The timestamp is taken here, when
    # CMake configures the (always fresh) container build.
    if(DEFINED ENV{CYD_GIT_DESCRIBE} AND NOT "$ENV{CYD_GIT_DESCRIBE}" STREQUAL "")
        set(describe "$ENV{CYD_GIT_DESCRIBE}")
    else()
        set(describe "unknown")
    endif()
    string(TIMESTAMP stamp "%s" UTC)  # seconds since the Unix epoch
    set(CYD_BUILDINFO_GENERATED_VERSION "${describe}-${stamp}")
    configure_file(buildinfo_gen.h.in "${CMAKE_CURRENT_BINARY_DIR}/buildinfo_gen.h" @ONLY)
    target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
endif()
