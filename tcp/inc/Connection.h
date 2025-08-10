#pragma once

#include <functional>
#include "Channel.h"
#include "Buffer.h"
#include "Macro.h"
#include "EventLoop.h"
#include "HttpContext.h"
#include "TimeStamp.h"

class EventLoop;
class Channel;
class Buffer;
class HttpContext;
class TimeStamp;

enum connectionState
{ // 连接状态
    Invalid = 1,
    Handshaking,
    Connected,
    Closed,
    Failed,
};

class Connection : public std::enable_shared_from_this<Connection>
{
private:
    EventLoop *loop;       // 事件循环,借用资源
    int fd;                // 文件描述符
    int conn_id;           // 连接ID
    connectionState state; // 连接状态

    std::unique_ptr<Channel> channel;
    std::unique_ptr<Buffer> readBuffer;                                                // 读取缓冲区
    std::unique_ptr<Buffer> sendBuffer;                                                // 发送缓冲区
    std::function<void(const std::shared_ptr<Connection> &)> onMessageCallback;        // 连接建立的回调函数
    std::function<void(const std::shared_ptr<Connection> &)> deleteConnectionCallback; // 删除连接的回调函数
    std::function<void(const std::shared_ptr<Connection> &)> onConnectionCallback;     // 新连接的回调函数

    std::unique_ptr<HttpContext> context; // HTTP上下文，用于处理HTTP请求

    TimeStamp last_active_time; // 最近一次活跃的时间戳

    void ReadNonBlocking();  // 非阻塞读取数据F
    void WriteNonBlocking(); // 非阻塞写入数据
    // void ReadBlocking();     // 阻塞读取数据
    // void WriteBlocking();    // 阻塞写入数据

public:
    DISALLOW_COPY_AND_MOVE(Connection);

    Connection(EventLoop *_loop, int fd, int conn_id); // 构造函数，传入事件循环、文件描述符和连接ID
    ~Connection();

    void connectionDestroyed();                                                                           // 连接销毁时调用
    void setDeleteConnectionCallback(std::function<void(const std::shared_ptr<Connection> &)> const &fn); // 设置删除连接的回调函数
    void setOnMessageCallback(std::function<void(const std::shared_ptr<Connection> &)> const &fn);        // 设置业务处理的回调函数
    void setOnConnectionCallback(std::function<void(const std::shared_ptr<Connection> &)> const &fn);     // 设置连接信息打印函数

    void ConnectionEstablished(); // 连接建立时调用，设置回调函数并注册事件
    void HandleEvent();           // 处理事件，调用回调函数
    void HandleWrite();           // 处理写事件
    void HandleClose();           // 处理连接关闭事件

    void Read();
    void Write();
    void Send(const std::string &msg);      // 发送消息
    void Send(const char *msg, size_t len); // 发送C风格字符串
    void Send(const char *msg);             // 发送C风格字符串
    void SendFile(int filefd, int size);    // 发送文件

    connectionState GetState();          // 获取连接状态
    void SetSendBuffer(const char *str); // 设置发送缓冲区内容
    int GetFd();                         // 获取Socket对象
    Buffer *GetReadBuffer();             // 获取读取缓冲区
    Buffer *GetSendBuffer();             // 获取发送缓冲区
    EventLoop *GetLoop();                // 获取事件循环
    HttpContext *GetContext();           // 获取HTTP上下文

    TimeStamp GetTimeStamp() const;            // 获取最近一次活跃的时间戳
    void UpdateTimeStamp(const TimeStamp &ts); // 更新最近一次活跃的时间戳
};
