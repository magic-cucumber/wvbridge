cmake_minimum_required(VERSION 3.15)
project(wvbridge_platform_common LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(WVBRIDGE_COMMON_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(wvbridge_platform_common OBJECT
        src/java_runtime.c
        src/javascript.cpp
        src/listener_support.cpp
        src/logger.cpp
        src/page-loading-start-listener.cpp
        src/page-loading-progress-listener.cpp
        src/page-loading-end-listener.cpp
        src/url-change-listener.cpp
        src/can-go-back-change-listener.cpp
        src/can-go-forward-change-listener.cpp
        src/webview-fatal-error-listener.cpp
)
add_library(wvbridge::platform_common ALIAS wvbridge_platform_common)

set_target_properties(wvbridge_platform_common PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)

target_include_directories(wvbridge_platform_common PUBLIC
        "${WVBRIDGE_COMMON_INCLUDE_DIR}"
)

find_package(Threads REQUIRED)

if (NOT DEFINED ENV{JAVA_HOME})
    message(FATAL_ERROR "JAVA_HOME not set")
endif ()

file(TO_CMAKE_PATH "$ENV{JAVA_HOME}" WVBRIDGE_COMMON_JAVA_HOME)
set(WVBRIDGE_COMMON_JNI_PLATFORM_INCLUDE_DIR "")

if (WIN32)
    set(WVBRIDGE_COMMON_JNI_PLATFORM_INCLUDE_DIR "${WVBRIDGE_COMMON_JAVA_HOME}/include/win32")
elseif (APPLE)
    set(WVBRIDGE_COMMON_JNI_PLATFORM_INCLUDE_DIR "${WVBRIDGE_COMMON_JAVA_HOME}/include/darwin")
elseif (UNIX)
    set(WVBRIDGE_COMMON_JNI_PLATFORM_INCLUDE_DIR "${WVBRIDGE_COMMON_JAVA_HOME}/include/linux")
endif ()

target_include_directories(wvbridge_platform_common PUBLIC
        "${WVBRIDGE_COMMON_JAVA_HOME}/include"
        "${WVBRIDGE_COMMON_JNI_PLATFORM_INCLUDE_DIR}"
)

target_link_libraries(wvbridge_platform_common PUBLIC Threads::Threads)
