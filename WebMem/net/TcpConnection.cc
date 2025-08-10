#include "TcpConnection.h"
#include "base/Logging.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"
#include <unistd.h>      // POSIX 标准函数（如 close）
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/tcp.h>
#include <strings.h>

using namespace mymuduo;
using namespace mymuduo::net;

TcpConnection::TcpConnection(EventLoop* loop,
                           const string& nameArg,
                           int sockfd,
                           const InetAddress& localAddr,
                           const InetAddress& peerAddr)
    : loop_(loop)
    , name_(nameArg)
    , state_(kConnecting)
    , reading_(true)
    , socket_(new Socket(sockfd))
    , channel_(new Channel(loop, sockfd))
    , localAddr_(localAddr)
    , peerAddr_(peerAddr)
    , highWaterMark_(64*1024*1024)  // 64MB
{
    // 设置通道的回调函数
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(
        std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(
        std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(
        std::bind(&TcpConnection::handleError, this));

    LOG_INFO << "TcpConnection::ctor[" << name_ << "] at " << this 
             << " fd=" << sockfd;
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection() {
    LOG_INFO << "TcpConnection::dtor[" << name_ << "] at " << this 
             << " fd=" << channel_->fd() 
             << " state=" << static_cast<int>(state_);
}

void TcpConnection::send(const void* data, int len) {
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(data, len);
        } else {
            void (TcpConnection::*fp)(const StringPiece& message) = &TcpConnection::sendInLoop;
            loop_->runInLoop(
                std::bind(fp, this, StringPiece(static_cast<const char*>(data), len)));
        }
    }
}

void TcpConnection::send(const StringPiece& message) {
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(message);
        } else {
            void (TcpConnection::*fp)(const StringPiece& message) = &TcpConnection::sendInLoop;
            loop_->runInLoop(std::bind(fp, this, message));
        }
    }
}

void TcpConnection::send(Buffer* buf) {
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(buf->peek(), buf->readableBytes());
            buf->retrieveAll();
        } else {
            void (TcpConnection::*fp)(const StringPiece& message) = &TcpConnection::sendInLoop;
            loop_->runInLoop(
                std::bind(fp, this, buf->retrieveAllAsString()));
        }
    }
}

void TcpConnection::sendInLoop(const StringPiece& message) {
    sendInLoop(message.data(), message.size());
}

void TcpConnection::sendInLoop(const void* data, size_t len) {
    loop_->assertInLoopThread();
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    // 如果连接已经关闭，就不再发送数据
    if (state_ == kDisconnected) {
        LOG_ERROR << "disconnected, give up writing";
        return;
    }
    LOG_DEBUG << "sendInLoop: data length = " << len;

    // 如果输出缓冲区为空，尝试直接发送数据
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
        nwrote = ::write(channel_->fd(), data, len);
        if (nwrote >= 0) {
            remaining = len - nwrote;
            LOG_DEBUG << "sendInLoop: wrote " << nwrote << " bytes, remaining " << remaining << " bytes";
            if (remaining == 0 && writeCompleteCallback_) {
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        } else {  // nwrote < 0
            nwrote = 0;
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                LOG_ERROR << "TcpConnection::sendInLoop error: " << strerror(errno);
                if (errno == EPIPE || errno == ECONNRESET) {
                    faultError = true;
                }
            }
        }
    }

    // 如果还有数据未发送完，添加到输出缓冲区，并开启写事件监听
    if (!faultError && remaining > 0) {
        size_t oldLen = outputBuffer_.readableBytes();
        LOG_DEBUG << "sendInLoop: append " << remaining << " bytes to output buffer, oldLen = " << oldLen;
        if (oldLen + remaining >= highWaterMark_
            && oldLen < highWaterMark_
            && highWaterMarkCallback_) {
            loop_->queueInLoop(
                std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        outputBuffer_.append(static_cast<const char*>(data) + nwrote, remaining);
        if (!channel_->isWriting()) {
            LOG_DEBUG << "sendInLoop: enable writing";
            channel_->enableWriting();
        }
        // 立即尝试再次发送
        handleWrite();
    }
}

void TcpConnection::shutdown() {
    if (state_ == kConnected) {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

void TcpConnection::shutdownInLoop() {
    loop_->assertInLoopThread();
    if (!channel_->isWriting()) {
        socket_->shutdownWrite();
    }
}

void TcpConnection::forceClose() {
    if (state_ == kConnected || state_ == kDisconnecting) {
        setState(kDisconnecting);
        loop_->queueInLoop(std::bind(&TcpConnection::forceCloseInLoop, shared_from_this()));
    }
}

void TcpConnection::forceCloseInLoop() {
    loop_->assertInLoopThread();
    if (state_ == kConnected || state_ == kDisconnecting) {
        handleClose();
    }
}

void TcpConnection::connectEstablished() {
    loop_->assertInLoopThread();
    assert(state_ == kConnecting);
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();  // 开始关注读事件

    connectionCallback_(shared_from_this());
}

void TcpConnection::connectDestroyed() {
    loop_->assertInLoopThread();
    if (state_ == kConnected) {
        setState(kDisconnected);
        channel_->disableAll();  // 停止所有事件的监听
        connectionCallback_(shared_from_this());
    }
    channel_->remove();  // 从EventLoop中移除
}

void TcpConnection::handleRead(Timestamp receiveTime) {
    loop_->assertInLoopThread();
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0) {
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    } else if (n == 0) {
        handleClose();
    } else {
        errno = savedErrno;
        LOG_ERROR << "TcpConnection::handleRead";
        handleError();
    }
}

void TcpConnection::handleWrite() {
    loop_->assertInLoopThread();
    if (channel_->isWriting()) {
        LOG_DEBUG << "handleWrite: try to write " << outputBuffer_.readableBytes() << " bytes";
        ssize_t n = ::write(channel_->fd(), outputBuffer_.peek(), outputBuffer_.readableBytes());
        if (n > 0) {
            LOG_DEBUG << "handleWrite: wrote " << n << " bytes";
            outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0) {
                LOG_DEBUG << "handleWrite: output buffer empty, disable writing";
                channel_->disableWriting();
                if (writeCompleteCallback_) {
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if (state_ == kDisconnecting) {
                    shutdownInLoop();
                }
            } else {
                LOG_DEBUG << "handleWrite: still have " << outputBuffer_.readableBytes() << " bytes to write";
                // 继续尝试写入
                loop_->queueInLoop(std::bind(&TcpConnection::handleWrite, shared_from_this()));
            }
        } else {
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                LOG_ERROR << "TcpConnection::handleWrite error: " << strerror(errno);
                if (errno == EPIPE || errno == ECONNRESET) {
                    handleClose();
                }
            }
        }
    } else {
        LOG_ERROR << "Connection fd = " << channel_->fd() << " is down, no more writing";
    }
}

void TcpConnection::handleClose() {
    loop_->assertInLoopThread();
    LOG_INFO << "TcpConnection::handleClose fd = " << channel_->fd() << " state = " << static_cast<int>(state_);
    assert(state_ == kConnected || state_ == kDisconnecting);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr guardThis(shared_from_this());
    connectionCallback_(guardThis);
    closeCallback_(guardThis);
}

void TcpConnection::handleError() {
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR << "TcpConnection::handleError name:" << name_ << " - SO_ERROR:" << err;
} 