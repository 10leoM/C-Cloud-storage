set(inspect_SRCS
    # 这里将在后续课程中添加源文件
)

add_library(mymuduo_inspect ${inspect_SRCS})
target_link_libraries(mymuduo_inspect mymuduo_http)

install(TARGETS mymuduo_inspect DESTINATION lib)

file(GLOB HEADERS "*.h")
install(FILES ${HEADERS} DESTINATION include/mymuduo/net/inspect) 