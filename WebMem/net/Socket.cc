#include "Socket.h"
#include "SocketsOps.h"
#include "InetAddress.h"
#include "base/Logging.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>  // memset
#include <fcntl.h>

using namespace mymuduo;
using namespace mymuduo::net;

Socket::Socket(int sockfd)
    : sockfd_(sockfd)
{
    // 设置为非阻塞模式
    int flags = ::fcntl(sockfd_, F_GETFL, 0);
    flags |= O_NONBLOCK;
    int ret = ::fcntl(sockfd_, F_SETFL, flags);
    if (ret < 0) {
        LOG_ERROR << "Socket::Socket set nonblocking failed";
    }
}

Socket::~Socket() {
    sockets::close(sockfd_);
}

void Socket::bindAddress(const InetAddress& addr) {
    sockets::bindOrDie(sockfd_, addr.getSockAddrInet6());
}

void Socket::listen() {
    sockets::listenOrDie(sockfd_);
}

int Socket::accept(InetAddress* peeraddr) {
    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof addr);
    int connfd = sockets::accept(sockfd_, &addr);
    if (connfd >= 0) {
        peeraddr->setSockAddrInet6(addr);
    }
    return connfd;
}

void Socket::shutdownWrite() {
    sockets::shutdownWrite(sockfd_);
}

void Socket::setTcpNoDelay(bool on) {
    int optval = on ? 1 : 0;
    int ret = ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY,
                          &optval, static_cast<socklen_t>(sizeof optval));
    if (ret < 0 && on) {
        LOG_ERROR << "Socket::setTcpNoDelay";
    }
}

void Socket::setReuseAddr(bool on) {
    int optval = on ? 1 : 0;
    int ret = ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR,
                          &optval, static_cast<socklen_t>(sizeof optval));
    if (ret < 0 && on) {
        LOG_ERROR << "Socket::setReuseAddr";
    }
}

void Socket::setReusePort(bool on) {
#ifdef SO_REUSEPORT
    int optval = on ? 1 : 0;
    int ret = ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT,
                          &optval, static_cast<socklen_t>(sizeof optval));
    if (ret < 0 && on) {
        LOG_ERROR << "Socket::setReusePort";
    }
#else
    if (on) {
        LOG_ERROR << "SO_REUSEPORT is not supported.";
    }
#endif
}

void Socket::setKeepAlive(bool on) {
    int optval = on ? 1 : 0;
    int ret = ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE,
                          &optval, static_cast<socklen_t>(sizeof optval));
    if (ret < 0 && on) {
        LOG_ERROR << "Socket::setKeepAlive";
    }
} 