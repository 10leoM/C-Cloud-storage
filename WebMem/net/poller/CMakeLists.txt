set(poller_SRCS
    DefaultPoller.cc
    PollPoller.cc
    EPollPoller.cc
)

add_library(mymuduo_net_poller ${poller_SRCS})
target_link_libraries(mymuduo_net_poller mymuduo_net)

install(TARGETS mymuduo_net_poller DESTINATION lib)

file(GLOB HEADERS "*.h")
install(FILES ${HEADERS} DESTINATION include/mymuduo/net/poller) 