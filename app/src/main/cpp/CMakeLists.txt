cmake_minimum_required(VERSION 3.22.1)
project(jarvis_native_app LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED ANDROID_NDK)
    message(FATAL_ERROR "ANDROID_NDK is required to build the NativeActivity glue.")
endif()

set(NATIVE_APP_GLUE_DIR "${ANDROID_NDK}/sources/android/native_app_glue")
if(NOT EXISTS "${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c")
    message(FATAL_ERROR "android_native_app_glue.c not found in ${NATIVE_APP_GLUE_DIR}")
endif()

add_library(native_app_glue STATIC
    "${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c"
)
target_include_directories(native_app_glue PUBLIC "${NATIVE_APP_GLUE_DIR}")

add_library(jarvis_native_app SHARED
    native-app.cpp
    native-lib.cpp
)

target_include_directories(jarvis_native_app PRIVATE "${NATIVE_APP_GLUE_DIR}")

find_library(android-lib android)
find_library(log-lib log)
find_library(egl-lib EGL)
find_library(gles-lib GLESv2)
find_library(aaudio-lib aaudio)

target_link_libraries(jarvis_native_app
    native_app_glue
    ${android-lib}
    ${log-lib}
    ${egl-lib}
    ${gles-lib}
    ${aaudio-lib}
)
