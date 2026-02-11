cmake_minimum_required(VERSION 3.15)
project(wvbridge)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 设置Objective-C++编译器
enable_language(OBJCXX)

# 查找Java环境
find_package(Java REQUIRED)
include(UseJava)

# 设置JAVA_HOME环境变量
if (NOT DEFINED ENV{JAVA_HOME})
    message(FATAL_ERROR "JAVA_HOME环境变量未设置")
endif ()

set(JAVA_HOME $ENV{JAVA_HOME})

# 设置JNI和JAWT包含路径
set(JNI_INCLUDE_DIRS
        ${JAVA_HOME}/include
        ${JAVA_HOME}/include/darwin
)

# 设置JNI和JAWT库路径
set(JNI_LIBRARIES
        ${JAVA_HOME}/lib/libjawt.dylib
        ${JAVA_HOME}/lib/libinstrument.dylib
)

# 源文件
set(SOURCES
        src/libs.mm
        src/utils.mm
        src/progress.mm
        src/navigation.mm
        src/ui_delegate.mm
)

# 创建共享库
add_library(wvbridge SHARED ${SOURCES})

# 设置Objective-C++编译标志
set_target_properties(wvbridge PROPERTIES
        COMPILE_FLAGS "-x objective-c++"
)

# 包含目录
target_include_directories(wvbridge PRIVATE
        ${JNI_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接库
target_link_libraries(wvbridge PRIVATE
        ${JNI_LIBRARIES}
        "-framework WebKit"
        "-framework Cocoa"
        "-framework Foundation"
        "-framework AppKit"
        "-framework QuartzCore"
)

# 设置输出目录
set_target_properties(wvbridge PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 设置库的输出名称
set_target_properties(wvbridge PROPERTIES
        OUTPUT_NAME "wvbridge"
        PREFIX "lib"
        SUFFIX ".dylib"
)

# 安装规则
install(TARGETS wvbridge
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
)
