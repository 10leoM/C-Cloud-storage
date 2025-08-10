set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_executable(http_upload http_upload.cc)
# 手动添加stdc++fs
target_link_libraries(http_upload mymuduo_net stdc++fs mysqlclient)

add_executable(route_test route_test.cc)