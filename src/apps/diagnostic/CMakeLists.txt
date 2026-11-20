# Diagnostic image: identifies panel orientation, colour order and inversion
# on a given board. Shares src/components and the base sdkconfig.defaults and
# partition table with the main cydium image. Build with
# `./build.sh --target diagnostic`.
cmake_minimum_required(VERSION 3.16)

set(CYD_SRC_DIR ${CMAKE_CURRENT_LIST_DIR}/../..)
set(EXTRA_COMPONENT_DIRS ${CYD_SRC_DIR}/components)
set(SDKCONFIG_DEFAULTS "${CYD_SRC_DIR}/sdkconfig.defaults;${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(diagnostic)
