cmake_minimum_required(VERSION 3.10)
project(serial_lib)

# 头文件路径
include_directories(include)

# 根据不同的操作系统平台选择对应的底层源文件
if(WIN32)
    set(SERIAL_SRCS
        src/serial.cc
        src/impl/win.cc
        src/impl/list_ports/list_ports_win.cc
    )
    # Windows 下枚举串口必须链接 setupapi 库
    set(SERIAL_LIBS setupapi) 
else()
    set(SERIAL_SRCS
        src/serial.cc
        src/impl/unix.cc
        src/impl/list_ports/list_ports_unix.cc
    )
    set(SERIAL_LIBS pthread)
endif()

# 编译为静态库
add_library(serial STATIC ${SERIAL_SRCS})

# 链接系统依赖库
target_link_libraries(serial PRIVATE ${SERIAL_LIBS})

# 允许外部工程通过 target_link_libraries 自动包含 include 目录
target_include_directories(serial PUBLIC include)