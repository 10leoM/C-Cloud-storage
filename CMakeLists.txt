cmake_minimum_required(VERSION 3.10)
project(myWebMem)
set(CMAKE_CXX_STANDARD 17)
# (auto-generated update to include new tests)

# 设置头文件路径
set(PINE_SRC_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/mymuduo/tcp/inc ${PROJECT_SOURCE_DIR}/http/inc ${PROJECT_SOURCE_DIR}/mymuduo/timer/inc ${PROJECT_SOURCE_DIR}/mymuduo/base/inc ${PROJECT_SOURCE_DIR}/mymuduo/log/inc ${PROJECT_SOURCE_DIR}/mymuduo/router/inc)

# 设置动态库文件
file(GLOB_RECURSE pine_sources ${PROJECT_SOURCE_DIR}/mymuduo/tcp/*.cpp ${PROJECT_SOURCE_DIR}/http/*.cpp ${PROJECT_SOURCE_DIR}/mymuduo/timer/*.cpp ${PROJECT_SOURCE_DIR}/mymuduo/base/*.cpp ${PROJECT_SOURCE_DIR}/mymuduo/log/*.cpp ${PROJECT_SOURCE_DIR}/mymuduo/log/Logger.cpp ${PROJECT_SOURCE_DIR}/mymuduo/router/*.cpp)
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
    target_include_directories(${file_name} PRIVATE ${PROJECT_SOURCE_DIR}/mymuduo/tcp/inc ${PROJECT_SOURCE_DIR}/http/inc ${PROJECT_SOURCE_DIR}/mymuduo/timer/inc ${PROJECT_SOURCE_DIR}/mymuduo/base/inc ${PROJECT_SOURCE_DIR}/mymuduo/log/inc ${PROJECT_SOURCE_DIR}/mymuduo/router/include_external_msproject()) # 添加头文件路径
    set_target_properties(${file_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/test/bin)
endforeach(source_file ${main_sources})

# 生成 http_upload 可执行文件
file(GLOB_RECURSE http_upload_sources ${PROJECT_SOURCE_DIR}/application/*.cpp, ${PROJECT_SOURCE_DIR}/application/src/*.cpp)
file(GLOB_RECURSE http_upload_sources
    ${PROJECT_SOURCE_DIR}/application/*.cpp
    ${PROJECT_SOURCE_DIR}/application/src/*.cpp
)
set(http_upload_include ${PROJECT_SOURCE_DIR}/application/inc ${PROJECT_SOURCE_DIR}/application/src/inc)
include_directories(${http_upload_include})
add_executable(http_upload ${http_upload_sources})
# 需要 MySQL C API 和 experimental::filesystem
target_link_libraries(http_upload pine_shared mysqlclient stdc++fs)
target_include_directories(http_upload PRIVATE ${PROJECT_SOURCE_DIR}/mymuduo/tcp/inc ${PROJECT_SOURCE_DIR}/http/inc ${PROJECT_SOURCE_DIR}/mymuduo/timer/inc ${PROJECT_SOURCE_DIR}/mymuduo/base/inc ${PROJECT_SOURCE_DIR}/mymuduo/log/inc ${PROJECT_SOURCE_DIR}/mymuduo/router/inc) # 添加头文件路径
set_target_properties(http_upload PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/application/bin)

# OpenSSL: 为 jwt-cpp 或其它使用到 EVP_* 的组件提供链接
find_package(OpenSSL QUIET)
if (OpenSSL_FOUND)
    if (TARGET OpenSSL::Crypto)
        target_link_libraries(http_upload OpenSSL::Crypto)
    else()
        target_link_libraries(http_upload crypto)
    endif()
    if (TARGET OpenSSL::SSL)
        target_link_libraries(http_upload OpenSSL::SSL)
    endif()
else()
    # 若未找到包，但系统存在 libcrypto，则尝试直接链接
    find_library(SYS_CRYPTO_LIB crypto)
    if(SYS_CRYPTO_LIB)
        target_link_libraries(http_upload ${SYS_CRYPTO_LIB})
    endif()
endif()

# ---- Redis++ 与 Hiredis（已安装情况下的精简链接）----
# 直接查找并链接（未找到会静默跳过，若需强制可改为 FATAL_ERROR）
find_path(REDIS_PLUS_PLUS_INCLUDE_DIR sw/redis++/redis++.h)
if(REDIS_PLUS_PLUS_INCLUDE_DIR)
    target_include_directories(http_upload PRIVATE ${REDIS_PLUS_PLUS_INCLUDE_DIR})
endif()
find_library(REDIS_PLUS_PLUS_LIB NAMES redis++ redis++)
if(REDIS_PLUS_PLUS_LIB)
    target_link_libraries(http_upload ${REDIS_PLUS_PLUS_LIB})
endif()
find_library(HIREDIS_LIB hiredis)
if(HIREDIS_LIB)
    target_link_libraries(http_upload ${HIREDIS_LIB})
endif()

# 设置安装路径