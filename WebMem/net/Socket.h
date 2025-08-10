#ifndef MYMUDUO_NET_SOCKET_H
#define MYMUDUO_NET_SOCKET_H

#include <netinet/tcp.h>
#include "InetAddress.h"

namespace mymuduo {
namespace net {

/**
 * @brief Socket类封装了socket文件描述符的生命周期管理
 * 
 * Socket类采用RAII方式管理socket文件描述符，在构造函数中接管文件描述符，
 * 在析构函数中关闭文件描述符。此外，Socket类还提供了一系列socket操作的
 * 成员函数。
 */
class Socket {
public:
    explicit Socket(int sockfd) ;

    // 析构函数会关闭socket
    ~Socket();

    // 禁止拷贝构造和赋值
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    int fd() const { return sockfd_; }

    /**
     * @brief 绑定socket到指定地址
     * @param addr 要绑定的地址
     */
    void bindAddress(const InetAddress& addr);

    /**
     * @brief 开始监听连接
     * @note 如果发生错误会终止程序
     */
    void listen();

    /**
     * @brief 接受新连接
     * @param peeraddr 用于存储对端地址
     * @return 新连接的socket文件描述符
     */
    int accept(InetAddress* peeraddr);

    /**
     * @brief 关闭写端
     */
    void shutdownWrite();

    /**
     * @brief 设置TCP无延迟
     * @param on 是否启用TCP无延迟
     */
    void setTcpNoDelay(bool on);

    /**
     * @brief 设置地址重用
     * @param on 是否启用地址重用
     */
    void setReuseAddr(bool on);

    /**
     * @brief 设置端口重用
     * @param on 是否启用端口重用
     */
    void setReusePort(bool on);

    /**
     * @brief 设置保持连接
     * @param on 是否启用保持连接
     */
    void setKeepAlive(bool on);

private:
    const int sockfd_;  // socket文件描述符
};

}  // namespace net
}  // namespace mymuduo

#endif  // MYMUDUO_NET_SOCKET_H 