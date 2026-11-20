cmake_minimum_required(VERSION 3.16)
# Each simulation is a component in components/sims/.
set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_LIST_DIR}/components/sims)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(cydium)
