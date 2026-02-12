cmake_minimum_required(VERSION 3.15)
project(wvbridge)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 生成位置无关代码 (对于共享库是必须的)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)


# 检查 JAVA_HOME
if (NOT DEFINED ENV{JAVA_HOME})
    message(FATAL_ERROR "JAVA_HOME not set")
endif ()

set(JAVA_HOME $ENV{JAVA_HOME})

set(JNI_INCLUDE_DIRS
        ${JAVA_HOME}/include
        ${JAVA_HOME}/include/linux
)

find_library(JAWT_LIB jawt
        PATHS
            ${JAVA_HOME}/lib
            ${JAVA_HOME}/lib/server
            ${JAVA_HOME}/lib/amd64
            ${JAVA_HOME}/lib/i386
        NO_DEFAULT_PATH
)

if (NOT JAWT_LIB)
    message(FATAL_ERROR "can't found libjawt.so on JAVA_HOME")
endif()

message(STATUS "Found JAWT Library: ${JAWT_LIB}")

find_package(PkgConfig REQUIRED)

find_package(X11 REQUIRED)

pkg_check_modules(WEBKIT REQUIRED webkitgtk-6.0)

set(SOURCES
        src/libs.cpp
        src/utils.h
        src/utils.cpp
        src/gtk.h
        src/gtk.cpp
        src/navigation.h
        src/navigation.cpp
        src/progress.h
        src/progress.cpp
)

# 创建共享库
add_library(wvbridge SHARED ${SOURCES})

# 包含目录
target_include_directories(wvbridge PRIVATE
        ${JNI_INCLUDE_DIRS}
        ${X11_INCLUDE_DIR}
        ${WEBKIT_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(wvbridge PRIVATE
        ${JAWT_LIB}           # Java AWT
        ${X11_LIBRARIES}      # X11
        ${WEBKIT_LIBRARIES}   # WebKitGTK 6.0 & GTK
)

set_target_properties(wvbridge PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

set_target_properties(wvbridge PROPERTIES
        OUTPUT_NAME "wvbridge"
        PREFIX "lib"
        SUFFIX ".so" # Linux 共享库后缀
)

install(TARGETS wvbridge
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
)
