cmake_minimum_required(VERSION 3.10)
project(day25)
set(CMAKE_CXX_STANDARD 17)

# 设置头文件路径
set(PINE_SRC_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/tcp/inc ${PROJECT_SOURCE_DIR}/http/inc ${PROJECT_SOURCE_DIR}/timer/inc ${PROJECT_SOURCE_DIR}/base/inc ${PROJECT_SOURCE_DIR}/log/inc)

# 设置动态库文件
file(GLOB_RECURSE pine_sources ${PROJECT_SOURCE_DIR}/tcp/*.cpp ${PROJECT_SOURCE_DIR}/http/*.cpp ${PROJECT_SOURCE_DIR}/timer/*.cpp ${PROJECT_SOURCE_DIR}/base/*.cpp ${PROJECT_SOURCE_DIR}/log/*.cpp ${PROJECT_SOURCE_DIR}/log/Logger.cpp)
include_directories(${PINE_SRC_INCLUDE_DIR})        # 添加头文件路径
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
add_library(pine_shared SHARED ${pine_sources})

# 设置编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wall -Wextra -std=c++17 -pthread") # 动态库编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter -Wno-attributes") # 忽略一些警告
# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls") # 调试编译选项
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb ") # 调试编译选项
set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -fPIC") # 链接选项

# 生成可执行文件程序
file(GLOB_RECURSE main_sources ${PROJECT_SOURCE_DIR}/test/*.cpp)
foreach(source_file ${main_sources})
    get_filename_component(file_name ${source_file} NAME_WE)    # 获取文件名（不含路径和扩展名）
    add_executable(${file_name} ${source_file})                 # 添加可执行文件
    target_link_libraries(${file_name} pine_shared)             # 链接 pine_shared 动态库
        target_include_directories(${file_name} PRIVATE ${PROJECT_SOURCE_DIR}/tcp/inc ${PROJECT_SOURCE_DIR}/http/inc ${PROJECT_SOURCE_DIR}/timer/inc ${PROJECT_SOURCE_DIR}/base/inc ${PROJECT_SOURCE_DIR}/log/inc) # 添加头文件路径
    set_target_properties(${file_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/test/bin)
    
endforeach(source_file ${main_sources})

# 设置安装路径