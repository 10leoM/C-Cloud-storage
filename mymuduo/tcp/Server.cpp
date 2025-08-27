#include "Server.h"
#include <stdio.h>
#include <unistd.h>
#include "InetAddress.h"
#include "Channel.h"
#include "Acceptor.h"
#include "EventLoopThreadPool.h"
#include "Connection.h"
#include <functional>
#include <errno.h>
#include <cstring>
#include <cctype>

#define READ_BUFFER 1024
Server::Server(EventLoop *loop, const char *ip, uint16_t port): mainReactor(loop), next_conn_id(1)
{
    // next_conn_id = 1;                            // 初始化下一个连接ID
    // mainReactor = std::make_unique<EventLoop>(); // 创建主事件循环

    // 初始化服务器，创建监听套接字
    acceptor = std::make_unique<Acceptor>(mainReactor, ip, port);                           // 创建Acceptor实例,socket,addr同时创建
    std::function<void(int, const InetAddress&, const InetAddress&)> cb =
        std::bind(&Server::NewConnection, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3); // 绑定新连接回调函数
    acceptor->setNewConnectionCallback(cb);                                                       // 设置新连接回调函数

    threadPool = std::make_unique<EventLoopThreadPool>(mainReactor); // 新建线程池
}

Server::~Server()
{
}

// 监听套接字接受新连接，创建Channel并注册到事件循环
void Server::NewConnection(int fd, const InetAddress& local, const InetAddress& peer)
{
    EventLoop *sub_loop = threadPool->nextloop();                                 // 获取对应的子Reactor
    connections[fd] = std::make_shared<Connection>(sub_loop, fd, next_conn_id++, local, peer); // 创建新的连接对象，保存地址
    
    // 确保连接的所有操作都在其专属的EventLoop线程中执行
    sub_loop->runOneFunc([this, fd]() {
        auto conn = connections[fd];
        if (conn) {
            conn->setDeleteConnectionCallback(
                std::bind(&Server::DeleteConnection, this, std::placeholders::_1)); // 设置删除连接的回调函数
            conn->setOnMessageCallback(messageCallback);                 // 设置连接建立的回调函数
            conn->setOnConnectionCallback(onConnectionCallback);         // 打印连接信息 
            if (closeCallback) conn->setCloseCallback(closeCallback);
            if (errorCallback) conn->setErrorCallback(errorCallback);
            if (writeCompleteCallback) conn->setWriteCompleteCallback(writeCompleteCallback);
            if (highWaterMarkCallback) conn->setHighWaterMarkCallback(highWaterMarkCallback, highWaterMark_);
            conn->ConnectionEstablished();                               // 连接建立，注册事件
        }
    });
    
    // printf("创建，计数：%d\n", connections[fd].use_count());
    if (next_conn_id == 1000) // 防止连接ID溢出
        next_conn_id = 1;     // 重置连接ID
}
void Server::DeleteConnection(std::shared_ptr<Connection> const &conn)
{
    // 待处理：关闭连接，删除连接对象
    mainReactor->runOneFunc(std::bind(&Server::HandleCloseInMainReactor, this, conn)); // 在主事件循环中处理连接关闭
}

void Server::HandleCloseInMainReactor(std::shared_ptr<Connection> const &conn)
{
    if (connections.find(conn->GetFd()) != connections.end())
    {
        connections.erase(conn->GetFd()); // 从连接映射中删除连接
    }

    // printf("Current thread ID: %d, Connection fd: %d, 计数：%d\n", CurrentThread::tid(), conn->GetFd(), conn.use_count());
    // 将Connection的销毁调度回其所属的子线程
    EventLoop *conn_loop = conn->GetLoop();
    conn_loop->queueOneFunc(std::bind(&Connection::connectionDestroyed, conn));
}

void Server::setMessageCallback(std::function<void(const std::shared_ptr<Connection> &)> const &fn)
{
    messageCallback = std::move(fn); // 设置业务处理的回调函数
}

void Server::setOnConnectionCallback(std::function<void(const std::shared_ptr<Connection> &)> const &fn)
{
    onConnectionCallback = std::move(fn); // 设置新连接的回调函数,实际绑定NewConnection
}
void Server::setCloseCallback(std::function<void(const std::shared_ptr<Connection> &)> const &fn) { closeCallback = fn; }
void Server::setErrorCallback(std::function<void(const std::shared_ptr<Connection> &)> const &fn) { errorCallback = fn; }
void Server::setWriteCompleteCallback(std::function<void(const std::shared_ptr<Connection> &)> const &fn) { writeCompleteCallback = fn; }
void Server::setHighWaterMarkCallback(std::function<void(const std::shared_ptr<Connection> &, size_t)> const &fn, size_t mark) { highWaterMarkCallback = fn; highWaterMark_ = mark; }

void Server::start()
{
    threadPool->start(); // 启动线程池，创建子事件循环
    mainReactor->loop(); // 启动主事件循环，开始监听连接
}

void Server::SetThreadPoolSize(int size)
{
    if (size <= 0)
    {
        fprintf(stderr, "Thread pool size must be positive.\n");
        return;
    }
    threadPool->SetThreadNums(size); // 设置线程池大小
}