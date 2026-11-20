cmake_minimum_required(VERSION 3.16)
# Each simulation is a component in components/sims/.
set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_LIST_DIR}/components/sims)
if(IDF_TARGET STREQUAL "linux")
    # The emulator (see components/emulator): build only what main needs, as
    # most IDF components don't support the linux target.
    set(COMPONENTS main)
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(cydium)

if(IDF_TARGET STREQUAL "linux")
    # nvs_flash turns on --coverage for IDF's own host tests, which makes the
    # emulator write gcov data (or complain) on exit. Drop it.
    idf_component_get_property(nvs_lib nvs_flash COMPONENT_LIB)
    foreach(prop COMPILE_OPTIONS INTERFACE_COMPILE_OPTIONS LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
        get_target_property(values ${nvs_lib} ${prop})
        if(values)
            list(REMOVE_ITEM values --coverage)
            set_target_properties(${nvs_lib} PROPERTIES ${prop} "${values}")
        endif()
    endforeach()
endif()
