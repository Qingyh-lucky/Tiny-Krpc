cmake_minimum_required(VERSION 3.10)
project(KrpcServer)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 获取服务端的源文件
file(GLOB SERVER_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

# 获取 protobuf 生成的 .cc 文件
file(GLOB PROTO_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../*.pb.cc)

# 创建服务端可执行文件
add_executable(server ${SERVER_SRCS} ${PROTO_SRCS})

# 手动指定 hiredis 的包含目录和库文件
include_directories("/root/Krpc/hiredis")  # 指定 hiredis 头文件路径
target_link_libraries(server krpc_core "/usr/lib/x86_64-linux-gnu/libhiredis.so")  # 指定 hiredis 库文件路径

# target_link_libraries(server hiredis)
# 链接必要的库，尤其是 example 中生成的静态库
target_link_libraries(server krpc_core ${LIBS})

# 设置编译选项
target_compile_options(server PRIVATE -std=c++11 -Wall)


# 设置 server 可执行文件输出目录
# set_target_properties(server PROPERTIES
#     RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set_target_properties(server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY /root/Krpc/bin)