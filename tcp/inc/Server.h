#pragma once

#include "Epoll.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Macro.h"
#include "CurrentThread.h"
#include "EventLoopThreadPool.h"
#include <map>
#include <memory>

class EventLoop;           // 前向声明
class Acceptor;            // 前向声明
class Connection;          // 前向声明
class EventLoopThreadPool; // 前向声明

class Server // 服务器，监听连接
{
private:
    EventLoop *mainReactor;                                 // 事件循环
    std::unique_ptr<Acceptor> acceptor;                     // Acceptor，用于接受新连接
    std::map<int, std::shared_ptr<Connection>> connections; // 存储连接的映射，键为文件描述符，值为Connection对象指针
    // std::vector<std::unique_ptr<EventLoop>> subReactors; // 工作线程的事件循环
    // std::unique_ptr<ThreadPool> threadPool;              // 线程池，用于处理连接的任务

    std::unique_ptr<EventLoopThreadPool> threadPool;                               // 线程池，用于处理连接的任务
    int next_conn_id;                                                              // 下一个连接ID
    std::function<void(const std::shared_ptr<Connection> &)> messageCallback;      // 业务处理的回调函数
    std::function<void(const std::shared_ptr<Connection> &)> onConnectionCallback; // 新连接的回调函数

public:
    DISALLOW_COPY_AND_MOVE(Server);

    Server(EventLoop *loop, const char *ip = "127.0.0.1", uint16_t port = 8080); // 构造函数，传入事件循环
    ~Server();                                                                   // 析构函数

    void start();                                                                                     // 启动服务器，开始监听连接
    void NewConnection(int fd);                                                                       // 接受新TCP连接，创建Channel并注册到事件循环
    void setMessageCallback(std::function<void(const std::shared_ptr<Connection> &)> const &fn);      // 设置业务处理的回调函数
    void setOnConnectionCallback(std::function<void(const std::shared_ptr<Connection> &)> const &fn); // 打印新连接的信息，不创建连接
    void DeleteConnection(std::shared_ptr<Connection> const &conn);                                   // 断开TCP连接
    void HandleCloseInMainReactor(std::shared_ptr<Connection> const &conn);                           // 在主事件循环中处理连接关闭
    void SetThreadPoolSize(int size);                                                                 // 设置线程池大小
};