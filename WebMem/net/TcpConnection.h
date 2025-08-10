#ifndef MYMUDUO_NET_TCPCONNECTION_H
#define MYMUDUO_NET_TCPCONNECTION_H

#include "../base/noncopyable.h"
#include "../base/StringPiece.h"
#include "../base/Types.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "InetAddress.h"

#include <memory>
#include <string>
#include <atomic>

namespace mymuduo {
namespace net {

class Channel;
class EventLoop;
class Socket;

/**
 * @brief TCP连接类，用于表示一个TCP连接
 * 
 * TcpConnection是muduo网络库中最重要的类之一，它表示一个已建立的TCP连接。
 * 它管理连接的生命周期，处理数据的收发，并提供丰富的回调接口。
 */
class TcpConnection : noncopyable,
                     public std::enable_shared_from_this<TcpConnection> {
public:
    /**
     * @brief 构造函数
     * @param loop 所属的事件循环
     * @param name 连接的名字
     * @param sockfd 已连接的socket文件描述符
     * @param localAddr 本地地址
     * @param peerAddr 对端地址
     */
    TcpConnection(EventLoop* loop,
                 const string& name,
                 int sockfd,
                 const InetAddress& localAddr,
                 const InetAddress& peerAddr);
    
    /**
     * @brief 析构函数
     */
    ~TcpConnection();

    /**
     * @brief 获取所属的事件循环
     */
    EventLoop* getLoop() const { return loop_; }
    
    /**
     * @brief 获取连接名字
     */
    const string& name() const { return name_; }
    
    /**
     * @brief 获取本地地址
     */
    const InetAddress& localAddress() const { return localAddr_; }
    
    /**
     * @brief 获取对端地址
     */
    const InetAddress& peerAddress() const { return peerAddr_; }
    
    /**
     * @brief 检查连接是否已建立
     */
    bool connected() const { return state_ == kConnected; }
    
    /**
     * @brief 检查连接是否已断开
     */
    bool disconnected() const { return state_ == kDisconnected; }

    // 设置各种回调函数
    void setConnectionCallback(const ConnectionCallback& cb) {
        connectionCallback_ = cb;
    }
    void setMessageCallback(const MessageCallback& cb) {
        messageCallback_ = cb;
    }
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) {
        writeCompleteCallback_ = cb;
    }
    void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark) {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
    }
    void setCloseCallback(const CloseCallback& cb) {
        closeCallback_ = cb;
    }

    /**
     * @brief 连接建立
     * 当TCP连接完成时由TcpServer调用
     */
    void connectEstablished();
    
    /**
     * @brief 连接销毁
     * 当TCP连接关闭时由TcpServer调用
     */
    void connectDestroyed();

    /**
     * @brief 发送数据
     * @param message 要发送的数据
     */
    void send(const void* message, int len);
    void send(const StringPiece& message);
    void send(Buffer* message);

    /**
     * @brief 关闭连接
     * 会调用shutdown(SHUT_WR)半关闭写端
     */
    void shutdown();

    /**
     * @brief 强制关闭连接
     */
    void forceClose();

    /**
     * @brief 获取输入缓冲区
     */
    Buffer* inputBuffer() { return &inputBuffer_; }
    
    /**
     * @brief 获取输出缓冲区
     */
    Buffer* outputBuffer() { return &outputBuffer_; }

    // 修改context相关方法
    void setContext(const std::shared_ptr<void>& context) { context_ = context; }
    const std::shared_ptr<void>& getContext() const { return context_; }

private:
    enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };
    void setState(StateE s) { state_ = s; }
    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();
    void sendInLoop(const StringPiece& message);
    void sendInLoop(const void* message, size_t len);
    void shutdownInLoop();
    void forceCloseInLoop();

    EventLoop* loop_;          // 所属的事件循环
    const string name_;        // 连接名字
    std::atomic<StateE> state_;  // 连接状态
    bool reading_;            // 是否正在读取数据

    std::unique_ptr<Socket> socket_;  // Socket对象
    std::unique_ptr<Channel> channel_;  // Channel对象
    const InetAddress localAddr_;  // 本地地址
    const InetAddress peerAddr_;   // 对端地址

    ConnectionCallback connectionCallback_;       // 连接建立/断开回调
    MessageCallback messageCallback_;           // 消息到达回调
    WriteCompleteCallback writeCompleteCallback_;  // 数据发送完成回调
    HighWaterMarkCallback highWaterMarkCallback_;  // 高水位回调
    CloseCallback closeCallback_;               // 连接关闭回调
    size_t highWaterMark_;                     // 高水位标记

    Buffer inputBuffer_;   // 输入缓冲区
    Buffer outputBuffer_;  // 输出缓冲区

    // 修改context成员变量类型
    std::shared_ptr<void> context_;
};

// using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

} // namespace net
} // namespace mymuduo

#endif // MYMUDUO_NET_TCPCONNECTION_H 