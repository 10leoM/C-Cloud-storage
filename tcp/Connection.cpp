#include "Connection.h"
#include <unistd.h>
#include <string.h>
#include <cctype>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <iostream>
#include <sys/sendfile.h>
#include "Logger.h"

const int READ_BUFFER = 1024;
Connection::Connection(EventLoop *_loop, int fd, int conn_id)
    : loop(_loop), fd(fd), conn_id(conn_id), state(connectionState::Invalid), last_active_time(TimeStamp::Now())
{
    channel = std::make_unique<Channel>(loop, fd);
    readBuffer = std::make_unique<Buffer>();
    sendBuffer = std::make_unique<Buffer>();
    context = std::make_unique<HttpContext>();

    // channel->enableReading(true);
}

Connection::~Connection()
{
    // close(fd); // 关闭文件描述符
    // 注意：deleteConnectionCallback不需要在这里调用，因为它是一个回调函数
    printf("Connection destructor called, fd: %d\n", fd);
    close(fd);
}

void Connection::connectionDestroyed()
{
    // 将该操作从析构处，移植该处，增加性能，因为在析构前，当前`TcpConnection`已经相当于关闭了。
    // 已经可以将其从loop处离开
    loop->removeChannel(channel.get()); // 从事件循环中移除Channel
    // printf("connectionDestroyed, fd: %d, 计数：%d\n", fd, shared_from_this().use_count());
}

void Connection::setDeleteConnectionCallback(std::function<void(const std::shared_ptr<Connection> &)> const &fn)
{
    deleteConnectionCallback = std::move(fn);
}

void Connection::setOnMessageCallback(std::function<void(const std::shared_ptr<Connection> &)> const &fn)
{
    onMessageCallback = std::move(fn);
}

void Connection::setOnConnectionCallback(std::function<void(const std::shared_ptr<Connection> &)> const &fn)
{
    onConnectionCallback = std::move(fn);
}

void Connection::ConnectionEstablished()
{
    std::function<void()> cb = std::bind(&Connection::HandleEvent, this); // +1
    // printf("connection setHandleEventCallback, fd: %d, 计数：%d\n", fd, shared_from_this().use_count()); // +1
    channel->setReadCallback(std::move(cb));                              // 设置Channel的事件处理回调函数
    channel->setWriteCallback(std::bind(&Connection::HandleWrite, this)); // 设置写事件回调函数
    channel->Tie(shared_from_this());                                     // 绑定Connection对象到Channel
    state = connectionState::Connected;
    channel->enableReading(true); // 延迟注册事件
    if (onConnectionCallback)     // 打印新连接的信息
        onConnectionCallback(shared_from_this());
}

void Connection::Read()
{
    // assert(state == connectionState::Connected);
    if (state != connectionState::Connected)
        return;
    readBuffer->RetrieveAll(); // 先将缓冲区的数据全部读取出来
    ReadNonBlocking();
}

void Connection::Write()
{
    // assert(state == connectionState::Connected);
    if (state != connectionState::Connected)
        return;
    WriteNonBlocking();
    // sendBuffer->RetrieveAll(); // 先将缓冲区的数据全部读取出来
}

void Connection::Send(const std::string &msg) // 发送消息
{
    Send(msg.c_str(), msg.size());
}

void Connection::Send(const char *msg) // 发送C风格字符串
{
    Send(msg, strlen(msg));
}

void Connection::Send(const char *msg, size_t len) // 发送C风格字符串
{
    int remaining = len;
    int send_size = 0;

    // 如果发送缓冲区空，尝试直接发送
    if (sendBuffer->GetReadablebytes() == 0)
    {
        send_size = static_cast<int>(write(fd, msg, remaining));
        if (send_size >= 0)
        {
            // 说明发送了部分数据
            remaining -= send_size;
        }
        else if ((send_size == -1) && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
        {
            // 说明此时TCP缓冲区是满的，没有办法写入，什么都不做
            send_size = 0;
        }
        else
        {
            LOG_ERROR << "TcpConnection::Send - TcpConnection Send ERROR";
            return;
        }
    }
    // 将剩余的数据加入到sendBuffer中，等待后续发送
    assert(remaining <= len);
    if (remaining > 0)
    {
        sendBuffer->Append(msg + send_size, remaining);
        // 到达这一步时
        // 1. 还没有监听写事件，在此时进行了监听
        // 2. 监听了写事件，并且已经触发了，此时再次监听，强制触发一次，如果强制触发失败，仍然可以等待后续TCP缓冲区可写。
        if (!channel->isWriting())
            channel->enableWriting(true);
    }
}

void Connection::SendFile(int filefd, int size) // 发送文件
{
    size_t send_size = 0;
    size_t data_size = static_cast<size_t>(size);
    std::cout<< "Connection::SendFile, filefd: " << filefd << ", size: " << data_size << std::endl;
    // 一次性把文件写完，虽然肯定不行。
    while (send_size < data_size)
    {
        ssize_t bytes_write = sendfile(fd, filefd, (off_t *)&send_size, data_size - send_size);
        if (bytes_write == -1 && errno == EINTR) // 程序正常中断、继续写入
        {
            printf("continue writing\n");
            continue;
        }
        else if (bytes_write == -1 && ((errno == EAGAIN) || (errno == EWOULDBLOCK))) // 非阻塞IO，这个条件表示数据全部写入完毕
        {
            continue;
        }
        else if (bytes_write == -1) // 其他错误
        {
            printf("other error, size: %zu\n", data_size);
            break;
        }
        send_size += bytes_write; // 更新已发送数据大小
    }
}

void Connection::ReadNonBlocking() // 非阻塞读取数据
{
    char buf[READ_BUFFER]; // 这个buf大小无所谓
    // printf("handleReadEvent fd: %d\n", fd);
    while (true)
    {
        bzero(&buf, sizeof(buf));
        ssize_t bytes_read = read(fd, buf, sizeof(buf));
        if (bytes_read > 0)
        {
            readBuffer->Append(buf, bytes_read);
        }
        else if (bytes_read == -1 && errno == EINTR) // 程序正常中断、继续读取
        {
            printf("continue reading\n");
            continue;
        }
        else if (bytes_read == -1 && ((errno == EAGAIN) || (errno == EWOULDBLOCK))) // 非阻塞IO，这个条件表示数据全部读取完毕
        {
            // printf("finish reading once, processing data, size: %zu\n", readBuffer->GetReadablebytes());
            break;
        }
        else if (bytes_read == 0) // EOF，客户端断开连接
        {
            printf("EOF, client fd %d disconnected\n", fd);
            HandleClose();
            break;
        }
        else
        {
            printf("Other error on client fd %d\n", fd);
            HandleClose();
            break;
        }
    }
}

void Connection::WriteNonBlocking() // 非阻塞写入数据
{
    int remaining = sendBuffer->GetReadablebytes();
    int send_size = static_cast<int>(write(fd, sendBuffer->Peek(), remaining));
    if ((send_size == -1) && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
    {
        // 说明此时TCP缓冲区是满的，没有办法写入，什么都不做
        // 主要是防止，在Send时write后监听EPOLLOUT，但是TCP缓冲区还是满的，
        send_size = 0;
    }
    else if (send_size == -1)
    {
        LOG_ERROR << "TcpConnection::Send - TcpConnection Send ERROR";
    }

    remaining -= send_size;
    sendBuffer->Retrieve(send_size);
    if (remaining == 0)
    {
        // 发送完成，关闭写事件
        channel->disableWriting();
    }
    // 继续等待写事件触发
}

connectionState Connection::GetState() // 获取连接状态
{
    return state;
}

void Connection::HandleClose() // 关闭连接
{
    if (state == connectionState::Closed)
        return;
    state = connectionState::Closed;
    deleteConnectionCallback(shared_from_this());
}

void Connection::SetSendBuffer(const char *str) // 设置发送缓冲区内容
{
    sendBuffer.get()->RetrieveAll(); // 清空发送缓冲区
    sendBuffer.get()->Append(str);   // 设置新的发送缓冲区内容
}

int Connection::GetFd() // 获取Socket对象
{
    return fd;
}

Buffer *Connection::GetReadBuffer() // 获取读取缓冲区
{
    return readBuffer.get();
}

Buffer *Connection::GetSendBuffer() // 获取发送缓冲区
{
    return sendBuffer.get();
}

EventLoop *Connection::GetLoop() // 获取事件循环
{
    return loop;
}

HttpContext *Connection::GetContext() // 获取HTTP上下文
{
    return context.get();
}

void Connection::HandleEvent() // 处理事件，调用回调函数
{
    // LOG_INFO << "HandleEvent, fd: " << fd << ", state: " << state;
    Read();
    if (onMessageCallback)
    {
        onMessageCallback(shared_from_this());
    }
    else
    {
        printf("No handleEventCallback set for fd: %d\n", fd);
    }
}

void Connection::HandleWrite() // 处理写事件
{
    LOG_INFO << "HandleWrite, fd: " << fd << ", state: " << state;
    Write();
}

TimeStamp Connection::GetTimeStamp() const // 获取最近一次活跃的时间戳
{
    return last_active_time;
}

void Connection::UpdateTimeStamp(const TimeStamp &ts) // 更新最近一次活跃的时间戳
{
    last_active_time = ts;
}