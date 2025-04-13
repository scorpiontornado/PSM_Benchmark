cmake_minimum_required(VERSION 3.10)

set(THIS SubgraphMatching)
project(${THIS})

include_directories(
        ${PROJECT_SOURCE_DIR}/
)

# 设置编译选项
set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -std=c++20 -O3 -g -Wall -march=native -pthread")

# 查找 OpenMP（必须放在调用子目录前）
find_package(OpenMP REQUIRED)

# 查找 pthread 库
find_package(Threads REQUIRED)

# 添加子目录
add_subdirectory(graph)
add_subdirectory(matching)
add_subdirectory(utility)
