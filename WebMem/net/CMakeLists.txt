set(net_SRCS
    Acceptor.cc
    Buffer.cc
    Channel.cc
    Connector.cc
    EventLoop.cc
    EventLoopThread.cc
    EventLoopThreadPool.cc
    InetAddress.cc
    Poller.cc
    poller/DefaultPoller.cc
    poller/PollPoller.cc
    poller/EPollPoller.cc
    Socket.cc
    SocketsOps.cc
    TcpConnection.cc
    TcpServer.cc
    TcpClient.cc
    Timer.cc
    TimerQueue.cc
    HttpServer.cc
    HttpContext.cc
)

set(net_HEADERS
    Acceptor.h
    Buffer.h
    Channel.h
    Connector.h
    EventLoop.h
    EventLoopThread.h
    EventLoopThreadPool.h
    InetAddress.h
    Poller.h
    poller/PollPoller.h
    poller/EPollPoller.h
    Socket.h
    SocketsOps.h
    TcpConnection.h
    TcpServer.h
    TcpClient.h
    Timer.h
    TimerId.h
    TimerQueue.h
)

add_library(mymuduo_net ${net_SRCS})
target_link_libraries(mymuduo_net mymuduo_base pthread rt)

# 设置包含目录
target_include_directories(mymuduo_net PUBLIC ${PROJECT_SOURCE_DIR})

# 安装
install(TARGETS mymuduo_net DESTINATION lib)
install(FILES ${net_HEADERS} DESTINATION include/mymuduo/net)

# add_subdirectory(http)
# add_subdirectory(inspect)
# add_subdirectory(poller)

if(NOT CMAKE_BUILD_NO_EXAMPLES)
    # 这里将在后续课程中添加示例程序
endif() 