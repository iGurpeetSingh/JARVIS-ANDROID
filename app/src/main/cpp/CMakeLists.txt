cmake_minimum_required(VERSION 3.22.1)
project(jarvis_native LANGUAGES CXX)

add_library(
    jarvis_native
    SHARED
    native-lib.cpp
)

target_compile_features(jarvis_native PRIVATE cxx_std_17)

find_library(android_log_lib log)
target_link_libraries(jarvis_native PRIVATE ${android_log_lib})
