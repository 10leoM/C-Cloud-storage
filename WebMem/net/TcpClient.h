#ifndef MYMUDUO_NET_TCPCLIENT_H
#define MYMUDUO_NET_TCPCLIENT_H

#include "net/TcpConnection.h"
#include "net/Connector.h"
#include "base/noncopyable.h"

#include <memory>
#include <mutex>
#include <string>

namespace mymuduo {
namespace net {

class EventLoop;
class InetAddress;

using ConnectorPtr = std::shared_ptr<Connector>;

///
/// TCP client类
///
class TcpClient : noncopyable {
public:
    TcpClient(EventLoop* loop,
              const InetAddress& serverAddr,
              const std::string& nameArg);
    ~TcpClient();  // 强制内联以避免虚表

    void connect();    // 连接服务器
    void disconnect(); // 断开连接
    void stop();      // 停止客户端

    TcpConnectionPtr connection() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return connection_;
    }

    EventLoop* getLoop() const { return loop_; }
    bool retry() const { return retry_; }
    void enableRetry() { retry_ = true; }

    const std::string& name() const { return name_; }

    /// 设置连接回调
    void setConnectionCallback(ConnectionCallback cb) {
        connectionCallback_ = std::move(cb);
    }

    /// 设置消息回调
    void setMessageCallback(MessageCallback cb) {
        messageCallback_ = std::move(cb);
    }

    /// 设置写完成回调
    void setWriteCompleteCallback(WriteCompleteCallback cb) {
        writeCompleteCallback_ = std::move(cb);
    }

private:
    /// 连接成功后的回调函数
    void newConnection(int sockfd);
    /// 移除连接
    void removeConnection(const TcpConnectionPtr& conn);

    EventLoop* loop_;                      // 事件循环
    const std::string name_;               // 客户端名称
    ConnectorPtr connector_;               // 连接器

    ConnectionCallback connectionCallback_;       // 连接回调
    MessageCallback messageCallback_;            // 消息回调
    WriteCompleteCallback writeCompleteCallback_;// 写完成回调

    bool retry_;    // 是否重试
    bool connect_;  // 是否连接

    // always in loop thread
    int nextConnId_;                   // 下一个连接ID
    mutable std::mutex mutex_;         // 互斥锁
    TcpConnectionPtr connection_;      // TCP连接
};

} // namespace net
} // namespace mymuduo

#endif  // MYMUDUO_NET_TCPCLIENT_H 