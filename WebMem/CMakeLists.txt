cmake_minimum_required(VERSION 3.10)

project(mymuduo C CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
if(NOT CMAKE_BUILD_TYPE)
    # set(CMAKE_BUILD_TYPE "Release")
    set(CMAKE_BUILD_TYPE "Debug")

endif()

# 查找Boost库
find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})

# Debug模式下的编译选项
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -Wall -Wextra -Werror -Wconversion -Wno-reorder -Wno-unused-parameter -Wno-shadow")
# Release模式下的编译选项
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG -Wall -Wno-shadow -Wno-reorder")

# 通用编译选项
set(CXX_FLAGS
    -Wall
    -Wextra
    -Werror
    -Wconversion
    -Wno-unused-parameter
    -Wold-style-cast
    -Woverloaded-virtual
    -Wpointer-arith
    -Wshadow
    -Wwrite-strings
    -march=native
    -std=c++17
    -rdynamic
)

string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")

# 设置输出目录
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

# 查找依赖库
find_package(Threads REQUIRED)
# find_package(Boost REQUIRED)
include_directories(${PROJECT_SOURCE_DIR})
# 添加子目录
add_subdirectory(base)
add_subdirectory(net)

# 是否构建测试
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    # add_subdirectory(tests)
endif()

# 是否构建示例
option(BUILD_APPLICATION "Build application" ON)
if(BUILD_APPLICATION)
    add_subdirectory(application)
endif()

# 打印配置信息
message(STATUS "CXX_FLAGS = " ${CMAKE_CXX_FLAGS})
message(STATUS "CMAKE_BUILD_TYPE = " ${CMAKE_BUILD_TYPE})
message(STATUS "CMAKE_CXX_FLAGS_DEBUG = " ${CMAKE_CXX_FLAGS_DEBUG})
message(STATUS "CMAKE_CXX_FLAGS_RELEASE = " ${CMAKE_CXX_FLAGS_RELEASE}) 