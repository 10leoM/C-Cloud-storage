#include "Acceptor.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "Channel.h"
#include "util.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>

Acceptor::Acceptor(EventLoop *_loop, const char *ip, uint16_t port)
    : loop(_loop), listenFd(-1), acceptChannel(nullptr), newConnectionCallback(nullptr)
{
    Create();                                                                                         // 创建监听套接字
    int opt = 1;                                                                                      // 设置端口重用选项
    errif(setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0, "setsockopt error"); // 设置端口重用，避免地址已被占用的错误
    InetAddress addr(ip, port);                                                                       // 创建InetAddress对象
    Bind(&addr);                                                                                      // 与IP地址绑定
    Listen();                                                                                         // 监听套接字

    acceptChannel = std::make_unique<Channel>(loop, listenFd);                           // 创建监听套接字的通道
    std::function<void()> acceptCallback = std::bind(&Acceptor::acceptConnection, this); // 设置接受新连接的回调函数
    acceptChannel->setReadCallback(acceptCallback);
    acceptChannel->enableReading(false); // 设置可读并注册,LT模式
    printf("Server started, listening on %s:%d\n", ip, port);
}

Acceptor::~Acceptor()
{
    close(listenFd);
}

void Acceptor::setNewConnectionCallback(std::function<void(int, const InetAddress&, const InetAddress&)> cb)
{
    newConnectionCallback = cb; // 设置新连接回调函数
}

// 建立新连接的服务函数
void Acceptor::acceptConnection()
{
    InetAddress peer_addr;
    int clnt_fd = Accept(&peer_addr);
    setnonblocking(clnt_fd);                        // 设置新连接为非阻塞模式
    InetAddress local_addr; // 查询本端地址
    local_addr.addr_len = sizeof(local_addr.addr);
    if (::getsockname(clnt_fd, (sockaddr*)&local_addr.addr, &local_addr.addr_len) < 0)
    {
        perror("getsockname error");
    }
    printf("New client connected: fd %d, peer %s:%d -> local %s:%d\n",
           clnt_fd,
           inet_ntoa(peer_addr.addr.sin_addr), ntohs(peer_addr.addr.sin_port),
           inet_ntoa(local_addr.addr.sin_addr), ntohs(local_addr.addr.sin_port));
    if (newConnectionCallback)
        newConnectionCallback(clnt_fd, local_addr, peer_addr);
}

void Acceptor::Create()
{
    errif((listenFd = socket(AF_INET, SOCK_STREAM, 0)) == -1, "socket create error");
}

void Acceptor::Bind(InetAddress *addr)
{
    errif((bind(listenFd, (sockaddr *)&addr->addr, addr->addr_len)) == -1, "socket bind error");
}
void Acceptor::Listen()
{
    errif((listen(listenFd, 128)) == -1, "socket listen error");
}
void Acceptor::setnonblocking(int fd) // 设置监听套接字为非阻塞模式
{
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}

int Acceptor::Accept(InetAddress *addr)
{
    int fd;
    errif((fd = accept(this->listenFd, (sockaddr *)&addr->addr, &addr->addr_len)) == -1, "socket accept error");
    return fd;
}

