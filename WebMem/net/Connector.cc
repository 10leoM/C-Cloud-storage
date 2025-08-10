#include "Connector.h"

#include <errno.h>
#include <string.h>

#include "base/Logging.h"
#include "Channel.h"
#include "EventLoop.h"
#include "SocketsOps.h"

namespace mymuduo {
namespace net {

const int Connector::kMaxRetryDelayMs;

// 构造函数
// 初始化成员变量
Connector::Connector(EventLoop* loop, const InetAddress& serverAddr)
    : loop_(loop),
      serverAddr_(serverAddr),
      connect_(false),
      state_(kDisconnected),
      retryDelayMs_(kInitRetryDelayMs) {
    LOG_DEBUG << "ctor[" << this << "]";
}

// 析构函数
// 确保连接已停止
Connector::~Connector() {
    LOG_DEBUG << "dtor[" << this << "]";
    assert(!channel_);
}

// 开始连接
// 可以在任意线程调用
void Connector::start() {
    connect_ = true;
    loop_->runInLoop(std::bind(&Connector::startInLoop, this));
}

// 在loop线程中启动连接
void Connector::startInLoop() {
    loop_->assertInLoopThread();
    assert(state_ == kDisconnected);
    if (connect_) {
        connect();
    } else {
        LOG_DEBUG << "do not connect";
    }
}

// 停止连接
void Connector::stop() {
    connect_ = false;
    loop_->queueInLoop(std::bind(&Connector::stopInLoop, this));
}

// 在loop线程中停止连接
void Connector::stopInLoop() {
    loop_->assertInLoopThread();
    if (state_ == kConnecting) {
        setState(kDisconnected);
        int sockfd = removeAndResetChannel();
        retry(sockfd);
    }
}

// 重新连接
void Connector::restart() {
    loop_->assertInLoopThread();
    setState(kDisconnected);
    retryDelayMs_ = kInitRetryDelayMs;
    connect_ = true;
    startInLoop();
}

// 建立连接
void Connector::connect() {
    // 创建非阻塞socket
    int sockfd = sockets::createNonblockingOrDie(serverAddr_.family());
    // 连接服务器
    int ret = sockets::connect(sockfd, reinterpret_cast<const sockaddr*>(serverAddr_.getSockAddr()));
    int savedErrno = (ret == 0) ? 0 : errno;
    switch (savedErrno) {
        case 0:
        case EINPROGRESS:  // 连接正在进行
        case EINTR:        // 被信号中断
        case EISCONN:      // 已经连接
            connecting(sockfd);  // 处理连接
            break;

        case EAGAIN:       // 临时端口不足
        case EADDRINUSE:   // 地址已被使用
        case EADDRNOTAVAIL:// 地址不可用
        case ECONNREFUSED: // 连接被拒绝
        case ENETUNREACH:  // 网络不可达
            retry(sockfd);  // 重试
            break;

        case EACCES:       // 权限不足
        case EPERM:        // 操作不允许
        case EAFNOSUPPORT: // 地址族不支持
        case EALREADY:     // 连接已经在进行
        case EBADF:        // 无效的文件描述符
        case EFAULT:       // 地址无效
        case ENOTSOCK:     // 不是socket
            LOG_SYSERR << "connect error in Connector::connect";
            sockets::close(sockfd);  // 关闭socket
            break;

        default:
            LOG_SYSERR << "Unexpected error in Connector::connect " << savedErrno;
            sockets::close(sockfd);
            break;
    }
}

// 处理正在连接的socket
void Connector::connecting(int sockfd) {
    setState(kConnecting);
    assert(!channel_);
    // 创建Channel观察socket上的事件
    channel_.reset(new Channel(loop_, sockfd));
    // 设置可写事件的回调函数
    channel_->setWriteCallback(
        std::bind(&Connector::handleWrite, this));
    // 设置错误事件的回调函数
    channel_->setErrorCallback(
        std::bind(&Connector::handleError, this));

    // 关注可写事件
    // 连接建立成功时socket可写
    channel_->enableWriting();
}

// 移除并重置Channel
int Connector::removeAndResetChannel() {
    channel_->disableAll();
    channel_->remove();
    int sockfd = channel_->fd();
    // 不能在handleWrite/handleError等回调函数中删除channel
    // 因为正在调用channel的回调函数
    loop_->queueInLoop(std::bind(&Connector::resetChannel, this));
    return sockfd;
}

// 重置Channel
void Connector::resetChannel() {
    channel_.reset();
}

// 处理可写事件
void Connector::handleWrite() {
    LOG_TRACE << "Connector::handleWrite " << state_;

    if (state_ == kConnecting) {
        int sockfd = removeAndResetChannel();
        // 获取socket错误
        int err = sockets::getSocketError(sockfd);
        if (err) {  // 有错误发生
            LOG_WARN << "Connector::handleWrite - SO_ERROR = "
                    << err << " " << strerror_tl(err);
            retry(sockfd);  // 重试
        } else if (sockets::isSelfConnect(sockfd)) {  // 自连接
            LOG_WARN << "Connector::handleWrite - Self connect";
            retry(sockfd);  // 重试
        } else {  // 连接成功
            setState(kConnected);
            if (connect_) {
                newConnectionCallback_(sockfd);  // 调用连接成功的回调函数
            } else {
                sockets::close(sockfd);  // 关闭socket
            }
        }
    } else {
        assert(state_ == kDisconnected);
    }
}

// 处理错误事件
void Connector::handleError() {
    LOG_ERROR << "Connector::handleError state=" << state_;
    if (state_ == kConnecting) {
        int sockfd = removeAndResetChannel();
        int err = sockets::getSocketError(sockfd);
        LOG_TRACE << "SO_ERROR = " << err << " " << strerror_tl(err);
        retry(sockfd);
    }
}

// 重试连接
void Connector::retry(int sockfd) {
    sockets::close(sockfd);  // 关闭socket
    setState(kDisconnected);
    if (connect_) {
        LOG_INFO << "Connector::retry - Retry connecting to "
                 << serverAddr_.toIpPort() << " in "
                 << retryDelayMs_ << " milliseconds. ";
        // 创建定时器，延迟重试
        timerId_ = loop_->runAfter(
            retryDelayMs_/1000.0,
            std::bind(&Connector::startInLoop, shared_from_this()));
        // 增加重试延迟
        retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);
    } else {
        LOG_DEBUG << "do not connect";
    }
}

}  // namespace net
}  // namespace mymuduo 