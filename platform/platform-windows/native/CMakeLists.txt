cmake_minimum_required(VERSION 3.18)
project(wvbridge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT WIN32)
    message(FATAL_ERROR "platform-windows/native can only be built on Windows.")
endif()

if(MINGW)
    message(FATAL_ERROR "MinGW is not supported. Please build with MSVC (Visual Studio toolchain).")
endif()

if(NOT MSVC)
    message(FATAL_ERROR "This target must be built with MSVC.")
endif()

if(NOT DEFINED ENV{JAVA_HOME})
    message(FATAL_ERROR "JAVA_HOME not set")
endif()

set(JAVA_HOME "$ENV{JAVA_HOME}")
set(JNI_INCLUDE_DIRS
    "${JAVA_HOME}/include"
    "${JAVA_HOME}/include/win32"
)

set(JAVA_LIB_DIRS
    "${JAVA_HOME}/lib"
    "${JAVA_HOME}/lib/server"
)

set(WEBVIEW2_VERSION "1.0.3595.46" CACHE STRING "Microsoft.Web.WebView2 NuGet package version")
set(WEBVIEW2_SDK_DIR "" CACHE PATH "Path to extracted Microsoft.Web.WebView2 package root")

function(resolve_webview2_sdk out_var)
    if(WEBVIEW2_SDK_DIR AND EXISTS "${WEBVIEW2_SDK_DIR}/build/native/include/WebView2.h")
        set(${out_var} "${WEBVIEW2_SDK_DIR}" PARENT_SCOPE)
        return()
    endif()

    if(DEFINED ENV{USERPROFILE})
        set(_nuget_candidate "$ENV{USERPROFILE}/.nuget/packages/microsoft.web.webview2/${WEBVIEW2_VERSION}")
        if(EXISTS "${_nuget_candidate}/build/native/include/WebView2.h")
            set(${out_var} "${_nuget_candidate}" PARENT_SCOPE)
            return()
        endif()
    endif()

    set(_dl_dir "${CMAKE_BINARY_DIR}/_deps/webview2")
    set(_nupkg "${_dl_dir}/Microsoft.Web.WebView2.${WEBVIEW2_VERSION}.nupkg")
    set(_extract_root "${_dl_dir}/pkg/${WEBVIEW2_VERSION}")
    set(_download_url "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${WEBVIEW2_VERSION}")

    file(MAKE_DIRECTORY "${_dl_dir}")
    if(NOT EXISTS "${_nupkg}")
        message(STATUS "Downloading WebView2 SDK NuGet: ${_download_url}")
        file(DOWNLOAD
            "${_download_url}"
            "${_nupkg}"
            SHOW_PROGRESS
            STATUS _download_status
            TLS_VERIFY ON
        )
        list(GET _download_status 0 _download_status_code)
        if(NOT _download_status_code EQUAL 0)
            list(GET _download_status 1 _download_status_msg)
            message(FATAL_ERROR "Failed to download WebView2 SDK NuGet: ${_download_status_msg}")
        endif()
    endif()

    if(NOT EXISTS "${_extract_root}/build/native/include/WebView2.h")
        file(MAKE_DIRECTORY "${_extract_root}")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E tar xvf "${_nupkg}"
            WORKING_DIRECTORY "${_extract_root}"
            RESULT_VARIABLE _extract_result
        )
        if(NOT _extract_result EQUAL 0)
            message(FATAL_ERROR "Failed to extract WebView2 NuGet package.")
        endif()
    endif()

    if(NOT EXISTS "${_extract_root}/build/native/include/WebView2.h")
        message(FATAL_ERROR "WebView2 include not found under extracted package.")
    endif()

    set(${out_var} "${_extract_root}" PARENT_SCOPE)
endfunction()

resolve_webview2_sdk(WEBVIEW2_ROOT)
message(STATUS "Using WebView2 SDK at: ${WEBVIEW2_ROOT}")

set(WEBVIEW2_INCLUDE_DIR "${WEBVIEW2_ROOT}/build/native/include")

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|AARCH64|aarch64)$")
    set(WEBVIEW2_ARCH "arm64")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(WEBVIEW2_ARCH "x64")
else()
    set(WEBVIEW2_ARCH "x86")
endif()

set(WEBVIEW2_LOADER_STATIC_LIB "${WEBVIEW2_ROOT}/build/native/${WEBVIEW2_ARCH}/WebView2LoaderStatic.lib")

if(NOT EXISTS "${WEBVIEW2_LOADER_STATIC_LIB}")
    message(FATAL_ERROR "WebView2LoaderStatic.lib not found: ${WEBVIEW2_LOADER_STATIC_LIB}")
endif()

set(WEBVIEW2_LOADER_LIB "${WEBVIEW2_LOADER_STATIC_LIB}")

set(SOURCES
    src/libs.cpp
    src/thread.cpp
    src/utils.cpp
)

add_library(wvbridge SHARED ${SOURCES})

target_include_directories(wvbridge PRIVATE
    ${JNI_INCLUDE_DIRS}
    ${WEBVIEW2_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_directories(wvbridge PRIVATE ${JAVA_LIB_DIRS})

target_link_libraries(wvbridge PRIVATE
    "${WEBVIEW2_LOADER_LIB}"
    jawt.lib
    jvm.lib
    ole32.lib
    oleaut32.lib
    user32.lib
    gdi32.lib
)

target_compile_definitions(wvbridge PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

set_target_properties(wvbridge PROPERTIES
    OUTPUT_NAME "wvbridge"
    PREFIX ""
    SUFFIX ".dll"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

set_target_properties(wvbridge PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
)
