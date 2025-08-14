#pragma once

#include <functional>
#include "Macro.h"
#include <memory>

class EventLoop;
class Socket;
class InetAddress;
class Channel;
class Acceptor
{
private:
    EventLoop *loop; // 借用的事件循环，裸指针管理
    int listenFd;    // 监听套接字文件描述符
    // Socket *sock;
    std::unique_ptr<Channel> acceptChannel;            // 监听套接字的通道，独属资源
    std::function<void(int, const InetAddress&, const InetAddress&)> newConnectionCallback; // 传递 fd / local / peer

public:
    DISALLOW_COPY_AND_MOVE(Acceptor);
    Acceptor(EventLoop *_loop, const char *ip, uint16_t port); // 构造函数，传入事件循环和监听地址
    ~Acceptor();

    void Create();                   // 创建监听套接字
    void Bind(InetAddress *addr);    // 与IP地址绑定
    void Listen();                   // 监听套接字
    void setnonblocking(int fd);     // 设置监听套接字为非阻塞模式
    int Accept(InetAddress *addr);   // 接受新连接
    void setNewConnectionCallback(std::function<void(int, const InetAddress&, const InetAddress&)> cb); // 设置新连接回调函数
    void acceptConnection();                            // 接受新连接并调用回调函数
};
