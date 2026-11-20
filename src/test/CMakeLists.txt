# Host unit tests for the hardware-free code (gfx, touch mapping, sim and
# sims). Built and
# run in the container by `./build.sh --test`; uses Unity from ESP-IDF.
cmake_minimum_required(VERSION 3.16)
project(cydium_host_tests C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED ENV{IDF_PATH})
    message(FATAL_ERROR "IDF_PATH is not set (Unity comes from ESP-IDF)")
endif()
set(unity_dir "$ENV{IDF_PATH}/components/unity/unity/src")
add_library(unity STATIC "${unity_dir}/unity.c")
target_include_directories(unity PUBLIC "${unity_dir}")

set(components "${CMAKE_CURRENT_LIST_DIR}/../components")
add_executable(host_tests
    test_main.cpp
    test_gfx.cpp
    test_touch_map.cpp
    test_sim.cpp
    "${components}/gfx/font8x8.cpp"
    "${components}/gfx/layer.cpp"
    "${components}/sim/manager.cpp"
    "${components}/sims/bounce/bounce.cpp"
    "${components}/sims/starfield/starfield.cpp")
target_include_directories(host_tests PRIVATE
    "${components}/gfx/include"
    "${components}/touch/include"
    "${components}/sim/include"
    "${components}/sims/bounce/include"
    "${components}/sims/starfield/include")
target_compile_options(host_tests PRIVATE -Wall -Wextra -Werror)
target_link_libraries(host_tests PRIVATE unity)

enable_testing()
add_test(NAME host_tests COMMAND host_tests)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME png2rgb565
    COMMAND "${Python3_EXECUTABLE}" -m unittest -v test_png2rgb565
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}")
