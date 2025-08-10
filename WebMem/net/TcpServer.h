#pragma once

#include "net/EventLoop.h"
#include "net/Acceptor.h"
#include "net/InetAddress.h"
#include "base/noncopyable.h"
#include "net/EventLoopThreadPool.h"
#include "net/Callbacks.h"
#include "net/TcpConnection.h"
#include "net/Buffer.h"

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

namespace mymuduo {
namespace net {

// TCP服务器类
class TcpServer : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    enum Option
    {
        kNoReusePort,
        kReusePort,
    };

    /**
     * @brief 构造函数
     * @param loop 事件循环
     * @param listenAddr 监听地址
     * @param nameArg 服务器名称
     * @param option 端口复用选项
     */
    TcpServer(EventLoop* loop,
              const InetAddress& listenAddr,
              const std::string& nameArg,
              Option option = kNoReusePort);
    
    ~TcpServer();

    // 设置线程初始化回调函数
    void setThreadInitCallback(const ThreadInitCallback& cb) { threadInitCallback_ = cb; }
    
    // 设置连接回调函数
    void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
    
    // 设置消息回调函数
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
    
    // 设置写完成回调函数
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }

    // 设置线程数量
    void setThreadNum(int numThreads);

    // 启动服务器
    void start();

    // 获取事件循环
    EventLoop* getLoop() const { return loop_; }

    // 获取服务器名称
    const std::string& name() const { return name_; }

    // 获取主机名
    const std::string& ipPort() const { return ipPort_; }

private:
    // 新连接到来时的回调函数
    void newConnection(int sockfd, const InetAddress& peerAddr);
    
    // 移除连接的回调函数
    void removeConnection(const TcpConnectionPtr& conn);
    
    // 在IO线程中移除连接
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    EventLoop* loop_;                          // 事件循环
    const std::string ipPort_;                 // 监听的IP和端口
    const std::string name_;                   // 服务器名称
    std::unique_ptr<Acceptor> acceptor_;       // 接收器
    std::shared_ptr<EventLoopThreadPool> threadPool_; // 线程池
    
    ConnectionCallback connectionCallback_;     // 连接回调
    MessageCallback messageCallback_;           // 消息回调
    WriteCompleteCallback writeCompleteCallback_; // 写完成回调
    ThreadInitCallback threadInitCallback_;     // 线程初始化回调
    
    std::atomic_bool started_;                 // 服务器是否已启动
    int nextConnId_;                           // 下一个连接ID
    ConnectionMap connections_;                 // 连接表
};

}  // namespace net
}  // namespace mymuduo 