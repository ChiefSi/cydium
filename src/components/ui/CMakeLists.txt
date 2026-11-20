idf_component_register(
    SRCS "menu.cpp"
    INCLUDE_DIRS "include"
    REQUIRES display gfx
    PRIV_REQUIRES settings buildinfo sim)

if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    # Convert the menu icon PNG to a header at build time.
    if(IS_ABSOLUTE "${CONFIG_CYD_MENU_ICON_PNG}")
        set(icon_png "${CONFIG_CYD_MENU_ICON_PNG}")
    else()
        set(icon_png "${COMPONENT_DIR}/${CONFIG_CYD_MENU_ICON_PNG}")
    endif()
    if(NOT EXISTS "${icon_png}")
        message(FATAL_ERROR "CYD_MENU_ICON_PNG: ${icon_png} not found")
    endif()

    idf_build_get_property(python PYTHON)
    set(icon_header "${CMAKE_CURRENT_BINARY_DIR}/menu_icon.h")
    set(converter "${COMPONENT_DIR}/tools/png2rgb565.py")
    add_custom_command(
        OUTPUT "${icon_header}"
        COMMAND "${python}" "${converter}" --size ${CONFIG_CYD_MENU_ICON_SIZE}
                --name kMenuIcon "${icon_png}" "${icon_header}"
        DEPENDS "${icon_png}" "${converter}"
        COMMENT "Converting menu icon ${icon_png}"
        VERBATIM)
    add_custom_target(ui_menu_icon DEPENDS "${icon_header}")
    add_dependencies(${COMPONENT_LIB} ui_menu_icon)
    target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
endif()
