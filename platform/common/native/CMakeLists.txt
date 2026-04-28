cmake_minimum_required(VERSION 3.15)
project(wvbridge_platform_common LANGUAGES C)

set(WVBRIDGE_COMMON_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(wvbridge_platform_common OBJECT
        src/java_runtime.c
        src/java_caller.c
        src/utils.c
)
add_library(wvbridge::platform_common ALIAS wvbridge_platform_common)

set_target_properties(wvbridge_platform_common PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)

target_include_directories(wvbridge_platform_common PUBLIC
        "${WVBRIDGE_COMMON_INCLUDE_DIR}"
)

find_package(Threads REQUIRED)

find_package(JNI QUIET)

if (JNI_INCLUDE_DIRS)
    set(WVBRIDGE_COMMON_JNI_INCLUDE_DIRS ${JNI_INCLUDE_DIRS})
else ()
    if (NOT DEFINED ENV{JAVA_HOME})
        message(FATAL_ERROR "JAVA_HOME not set")
    endif ()

    set(WVBRIDGE_COMMON_JAVA_HOME "$ENV{JAVA_HOME}")
    set(WVBRIDGE_COMMON_JNI_PLATFORM_INCLUDE_DIR "")

    if (WIN32)
        set(WVBRIDGE_COMMON_JNI_PLATFORM_INCLUDE_DIR "${WVBRIDGE_COMMON_JAVA_HOME}/include/win32")
    elseif (APPLE)
        set(WVBRIDGE_COMMON_JNI_PLATFORM_INCLUDE_DIR "${WVBRIDGE_COMMON_JAVA_HOME}/include/darwin")
    elseif (UNIX)
        set(WVBRIDGE_COMMON_JNI_PLATFORM_INCLUDE_DIR "${WVBRIDGE_COMMON_JAVA_HOME}/include/linux")
    endif ()

    set(WVBRIDGE_COMMON_JNI_INCLUDE_DIRS
            "${WVBRIDGE_COMMON_JAVA_HOME}/include"
            "${WVBRIDGE_COMMON_JNI_PLATFORM_INCLUDE_DIR}"
    )
endif ()

target_include_directories(wvbridge_platform_common PUBLIC
        ${WVBRIDGE_COMMON_JNI_INCLUDE_DIRS}
)

if (TARGET JNI::JNI)
    target_link_libraries(wvbridge_platform_common PUBLIC JNI::JNI Threads::Threads)
elseif (JNI_LIBRARIES)
    target_link_libraries(wvbridge_platform_common PUBLIC ${JNI_LIBRARIES} Threads::Threads)
else ()
    target_link_libraries(wvbridge_platform_common PUBLIC Threads::Threads)
endif ()
