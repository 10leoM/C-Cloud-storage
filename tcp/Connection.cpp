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
#include <sys/types.h>
#include <sys/socket.h>

const int READ_BUFFER = 1024;
Connection::Connection(EventLoop *_loop, int fd, int conn_id, const InetAddress& local, const InetAddress& peer)
    : loop(_loop), fd(fd), conn_id(conn_id), state(connectionState::Invalid), last_active_time(TimeStamp::Now()), localAddr_(local), peerAddr_(peer)
{
    channel = std::make_unique<Channel>(loop, fd);
    readBuffer = std::make_unique<Buffer>();
    sendBuffer = std::make_unique<Buffer>();
    // 不再默认创建具体协议上下文，业务按需 setContext
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

void Connection::setOnConnectionCallback(std::function<void(const std::shared_ptr<Connection> &)> const &fn) { onConnectionCallback = fn; }
void Connection::setWriteCompleteCallback(std::function<void(const std::shared_ptr<Connection> &)> const &fn) { writeCompleteCallback = fn; }
void Connection::setHighWaterMarkCallback(std::function<void(const std::shared_ptr<Connection> &, size_t)> const &fn, size_t mark) { highWaterMarkCallback = fn; highWaterMark_ = mark; }
void Connection::setCloseCallback(std::function<void(const std::shared_ptr<Connection> &)> const &fn) { closeCallback = fn; }
void Connection::setErrorCallback(std::function<void(const std::shared_ptr<Connection> &)> const &fn) { errorCallback = fn; }

void Connection::ConnectionEstablished()
{
    std::function<void()> cb = std::bind(&Connection::HandleEvent, this); // +1
    // printf("connection setHandleEventCallback, fd: %d, 计数：%d\n", fd, shared_from_this().use_count()); // +1
    channel->setReadCallback(std::move(cb));
    channel->setWriteCallback(std::bind(&Connection::HandleWrite, this));
    channel->setCloseCallback(std::bind(&Connection::HandleClose, this));
    channel->setErrorCallback(std::bind(&Connection::HandleError, this));
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

void Connection::Send(const char *msg, size_t len) // 发送数据
{
    size_t remaining = len;
    ssize_t send_size = 0;

    // 如果发送缓冲区空，尝试直接发送
    if (sendBuffer->GetReadablebytes() == 0)
    {
        send_size = write(fd, msg, remaining);
        if (send_size >= 0)
        {
            // 说明发送了部分数据
            remaining -= static_cast<size_t>(send_size);
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
    if (highWaterMarkCallback && sendBuffer->GetReadablebytes() >= static_cast<size_t>(highWaterMark_))
        {   
            // 如果设置了高水位回调，并且当前发送缓冲区的大小超过了高水位，则调用回调函数
            // 这里的sendBuffer->GetReadablebytes()是获取当前可读字节数
            // 传入的size_t是当前发送缓冲区的大小
            // 注意：这里的回调函数可能会在其他线程中执行，所以需要注意线程安全问题
            highWaterMarkCallback(shared_from_this(), sendBuffer->GetReadablebytes());
        }
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
    // 使用 Buffer::readFd (readv) 一次尽量多读，循环直到 EAGAIN/EWOULDBLOCK
    while (true) {
        int savedErrno = 0;
        ssize_t n = readBuffer->readFd(fd, &savedErrno);
        if (n > 0) {
            // 增量解析 CRLF（行结束），暂不取走数据，只是扫描到末尾位置，便于后续上层（如 HTTP）直接使用缓冲区内容。
            const char *searchStart = readBuffer->Peek();
            while (true) {
                const char *crlf = readBuffer->findCRLF(searchStart);
                if (!crlf) break;
                // 可在需要时将行内容交给上层，这里仅扫描；避免提前 Retrieve 破坏现有 onMessage 语义
                searchStart = crlf + 2; // 跳过 "\r\n" 继续查找下一行
            }
            continue; // 继续尝试读取，直到耗尽
        } else if (n == 0) { // EOF
            printf("EOF, client fd %d disconnected\n", fd);
            HandleClose();
            break;
        } else { // n < 0
            if (savedErrno == EINTR) {
                continue; // 中断后重试
            }
            if (savedErrno == EAGAIN || savedErrno == EWOULDBLOCK) {
                // 本轮数据读完
                break;
            }
            printf("Read error(%d) on client fd %d\n", savedErrno, fd);
            HandleClose();
            break;
        }
    }
}

void Connection::WriteNonBlocking() // 非阻塞写入数据
{
    size_t remaining = sendBuffer->GetReadablebytes();
    if (remaining == 0) return;
    ssize_t send_size = write(fd, sendBuffer->Peek(), remaining);
    if (send_size == -1)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return; // 等待下次可写
        }
        LOG_ERROR << "TcpConnection::Send - TcpConnection Send ERROR";
        return;
    }
    if (send_size > 0)
    {
        remaining -= static_cast<size_t>(send_size);
        sendBuffer->Retrieve(static_cast<size_t>(send_size));
    }
    if (remaining == 0)
    {
        // 如果发送缓冲区已经清空，取消写事件监听
        channel->disableWriting();
        if (writeCompleteCallback)
            writeCompleteCallback(shared_from_this());
    }
    // 如果还有剩余数据，继续监听写事件
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
    if (closeCallback)
        closeCallback(shared_from_this());
    deleteConnectionCallback(shared_from_this());
}

void Connection::HandleError()
{
    int err = 0;
    socklen_t len = sizeof(err);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
        err = errno;
    LOG_ERROR << "TcpConnection error fd=" << fd << " err=" << err;
    if (errorCallback)
        errorCallback(shared_from_this());
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

void Connection::shutdown()
{
    if (state == connectionState::Connected)
    {
        if (!channel->isWriting() && sendBuffer->GetReadablebytes() == 0)
        {
            ::shutdown(fd, SHUT_WR);
        }
    }
}

void Connection::forceClose()
{
    if (state == connectionState::Connected || state == connectionState::Handshaking)
    {
        HandleClose();
    }
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

void Connection::setContext(const std::shared_ptr<void> &ctx) { context = ctx;}

TimeStamp Connection::GetTimeStamp() const // 获取最近一次活跃的时间戳
{
    return last_active_time;
}

void Connection::UpdateTimeStamp(const TimeStamp &ts) // 更新最近一次活跃的时间戳
{
    last_active_time = ts;
}

std::string Connection::GetlocalIpPort() const
{
    char buf[64];
    inet_ntop(AF_INET, (void *)&localAddr_.addr.sin_addr, buf, sizeof(buf));
    char out[80];
    snprintf(out, sizeof(out), "%s:%u", buf, ntohs(localAddr_.addr.sin_port));
    return out;
}

std::string Connection::GetpeerIpPort() const
{
    char buf[64];
    inet_ntop(AF_INET, (void *)&peerAddr_.addr.sin_addr, buf, sizeof(buf));
    char out[80];
    snprintf(out, sizeof(out), "%s:%u", buf, ntohs(peerAddr_.addr.sin_port));
    return out;
}