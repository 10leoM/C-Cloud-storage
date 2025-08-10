#include "net/TcpServer.h"
#include "base/Logging.h"
#include "net/TcpConnection.h"

#include <strings.h>
#include <functional>
#include <sstream>

namespace mymuduo {
namespace net {

static EventLoop* CheckLoopNotNull(EventLoop* loop)
{
    if (loop == nullptr)
    {
        std::stringstream ss;
        ss << __FILE__ << ":" << __FUNCTION__ << ":" << __LINE__ << " mainLoop is null!";
        LOG_FATAL << ss.str();
    }
    return loop;
}

TcpServer::TcpServer(EventLoop* loop,
                    const InetAddress& listenAddr,
                    const std::string& nameArg,
                    Option option)
    : loop_(CheckLoopNotNull(loop))
    , ipPort_(listenAddr.toIpPort())
    , name_(nameArg)
    , acceptor_(new Acceptor(loop, listenAddr, option == kReusePort))
    , threadPool_(new EventLoopThreadPool(loop, name_))
    , connectionCallback_()
    , messageCallback_()
    , writeCompleteCallback_()
    , threadInitCallback_()
    , started_(0)
    , nextConnId_(1)
    , connections_()
{
    // 当有新用户连接时会执行TcpServer::newConnection回调
    acceptor_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection, this, 
            std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
    for (auto& item : connections_)
    {
        TcpConnectionPtr conn(item.second);
        item.second.reset();
        // 销毁连接
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

// 设置底层subloop的个数
void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}

// 开启服务器监听
void TcpServer::start()
{
    if (started_.exchange(1) == 0) // 防止一个TcpServer对象被start多次
    {
        threadPool_->start(threadInitCallback_); // 启动底层的loop线程池
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

// 有一个新的客户端连接，acceptor会执行这个回调操作
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{
    // 轮询算法，选择一个subLoop，来管理channel
    EventLoop* ioLoop = threadPool_->getNextLoop();
    std::string connName = name_ + "-" + ipPort_ + "#" + std::to_string(nextConnId_);
    ++nextConnId_;

    std::stringstream ss;
    ss << "TcpServer::newConnection [" << name_ << "] - new connection [" 
       << connName << "] from " << peerAddr.toIpPort();
    LOG_INFO << ss.str();

    // 通过sockfd获取其绑定的本机的ip地址和端口信息
    sockaddr_in local;
    ::bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local;
    if (::getsockname(sockfd, reinterpret_cast<sockaddr*>(&local), &addrlen) < 0)
    {
        LOG_ERROR << "sockets::getLocalAddr";
    }
    InetAddress localAddr(local);

    // 根据连接成功的sockfd，创建TcpConnection连接对象
    TcpConnectionPtr conn(new TcpConnection(
                            ioLoop,
                            connName,
                            sockfd,
                            localAddr,
                            peerAddr));
    connections_[connName] = conn;
    // 下面的回调都是用户设置给TcpServer=>TcpConnection=>Channel=>Poller=>notify channel调用回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // 设置了如何关闭连接的回调
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    // 直接调用TcpConnection::connectEstablished
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
    std::stringstream ss;
    ss << "TcpServer::removeConnectionInLoop [" << name_ 
       << "] - connection " << conn->name();
    LOG_INFO << ss.str();

    connections_.erase(conn->name());
    EventLoop* ioLoop = conn->getLoop();
    ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed, conn));
}

}  // namespace net
}  // namespace mymuduo 