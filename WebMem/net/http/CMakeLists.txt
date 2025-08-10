set(http_SRCS
    # 这里将在后续课程中添加源文件
)

add_library(mymuduo_http ${http_SRCS})
target_link_libraries(mymuduo_http mymuduo_net)

install(TARGETS mymuduo_http DESTINATION lib)

file(GLOB HEADERS "*.h")
install(FILES ${HEADERS} DESTINATION include/mymuduo/net/http) 