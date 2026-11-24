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

set(SHERPA_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/sherpa/include")
set(SHERPA_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}")
if(NOT EXISTS "${SHERPA_INCLUDE_DIR}/sherpa-onnx/c-api/cxx-api.h")
    message(FATAL_ERROR "Missing sherpa-onnx C++ headers. Run scripts/prepare-native-deps.sh first.")
endif()
if(NOT EXISTS "${SHERPA_LIB_DIR}/libsherpa-onnx-cxx-api.so")
    message(FATAL_ERROR "Missing sherpa-onnx C++ native library. Run scripts/prepare-native-deps.sh first.")
endif()
if(NOT EXISTS "${SHERPA_LIB_DIR}/libonnxruntime.so")
    message(FATAL_ERROR "Missing ONNX Runtime native library. Run scripts/prepare-native-deps.sh first.")
endif()

add_library(native_app_glue STATIC
    "${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c"
)
target_include_directories(native_app_glue PUBLIC "${NATIVE_APP_GLUE_DIR}")

add_library(sherpa-onnx-cxx-api SHARED IMPORTED)
set_target_properties(sherpa-onnx-cxx-api PROPERTIES
    IMPORTED_LOCATION "${SHERPA_LIB_DIR}/libsherpa-onnx-cxx-api.so"
)

add_library(onnxruntime SHARED IMPORTED)
set_target_properties(onnxruntime PROPERTIES
    IMPORTED_LOCATION "${SHERPA_LIB_DIR}/libonnxruntime.so"
)

add_library(jarvis_native_app SHARED
    native-app.cpp
    native-lib.cpp
)

target_include_directories(jarvis_native_app PRIVATE
    "${NATIVE_APP_GLUE_DIR}"
    "${SHERPA_INCLUDE_DIR}"
)

find_library(android-lib android)
find_library(log-lib log)
find_library(egl-lib EGL)
find_library(gles-lib GLESv2)
find_library(aaudio-lib aaudio)

target_link_libraries(jarvis_native_app
    native_app_glue
    sherpa-onnx-cxx-api
    onnxruntime
    ${android-lib}
    ${log-lib}
    ${egl-lib}
    ${gles-lib}
    ${aaudio-lib}
)
