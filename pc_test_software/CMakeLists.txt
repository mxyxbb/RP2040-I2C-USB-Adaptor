cmake_minimum_required(VERSION 3.10)

# 1. 项目名称
project(RPI2C_Control CXX)

# 2. 设置 C++ 标准 (通常 C++17 或 C++20)
set(CMAKE_CXX_STANDARD 17)

# 3. 定义路径变量 (基于你的目录结构)
# 使用 ${CMAKE_SOURCE_DIR} 确保路径是相对于工程根目录的绝对路径
set(INC_DIR       "${CMAKE_SOURCE_DIR}/include")
set(SRC_DIR       "${CMAKE_SOURCE_DIR}/src")

# 4. 包含头文件目录
# 这里添加了 include 和 third_party/mylib 两个位置
include_directories(
    "${INC_DIR}"
)

# 5. 指定源文件
# 这里直接列出你的两个源文件
set(SOURCES 
    "${SRC_DIR}/main.cpp"
)

# 假设你把 wjwwood/serial 的源码放在了第三方目录下
add_subdirectory(third_party/serial)

# 6. 生成可执行文件
add_executable(RPI2C_Demo ${SOURCES})

# 链接 serial 库
target_link_libraries(RPI2C_Demo PRIVATE serial)