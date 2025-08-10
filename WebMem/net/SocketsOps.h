#ifndef MYMUDUO_NET_SOCKETSOPS_H
#define MYMUDUO_NET_SOCKETSOPS_H

#include <arpa/inet.h>

namespace mymuduo
{
namespace net
{
namespace sockets
{

///
/// 创建一个非阻塞的socket文件描述符
/// 如果发生错误，会终止程序
int createNonblockingOrDie(sa_family_t family);

///
/// 连接socket
/// @return 成功返回0，失败返回-1
int connect(int sockfd, const struct sockaddr* addr);

///
/// 绑定socket地址
/// 如果发生错误，会终止程序
void bindOrDie(int sockfd, const struct sockaddr* addr);

///
/// 监听socket
/// 如果发生错误，会终止程序
void listenOrDie(int sockfd);

///
/// 接受新连接
/// @return 成功返回非负的文件描述符，失败返回-1
int accept(int sockfd, struct sockaddr_in6* addr);

///
/// 从socket读取数据
ssize_t read(int sockfd, void *buf, size_t count);

///
/// 从socket读取数据（分散读）
ssize_t readv(int sockfd, const struct iovec *iov, int iovcnt);

///
/// 向socket写入数据
ssize_t write(int sockfd, const void *buf, size_t count);

///
/// 关闭socket
void close(int sockfd);

///
/// 关闭socket的写入端
void shutdownWrite(int sockfd);

///
/// 将socket地址转换为IP:PORT格式的字符串
void toIpPort(char* buf, size_t size, const struct sockaddr* addr);

///
/// 将socket地址转换为IP格式的字符串
void toIp(char* buf, size_t size, const struct sockaddr* addr);

///
/// 将IP和端口转换为IPv4的socket地址
void fromIpPort(const char* ip, uint16_t port, struct sockaddr_in* addr);

///
/// 将IP和端口转换为IPv6的socket地址
void fromIpPort(const char* ip, uint16_t port, struct sockaddr_in6* addr);

///
/// 获取socket错误
int getSocketError(int sockfd);

///
/// socket地址转换函数
const struct sockaddr* sockaddr_cast(const struct sockaddr_in* addr);
const struct sockaddr* sockaddr_cast(const struct sockaddr_in6* addr);
struct sockaddr* sockaddr_cast(struct sockaddr_in6* addr);
const struct sockaddr_in* sockaddr_in_cast(const struct sockaddr* addr);
const struct sockaddr_in6* sockaddr_in6_cast(const struct sockaddr* addr);

///
/// 获取socket的本地地址
struct sockaddr_in6 getLocalAddr(int sockfd);

///
/// 获取socket的对端地址
struct sockaddr_in6 getPeerAddr(int sockfd);

///
/// 判断是否是自连接
bool isSelfConnect(int sockfd);

}  // namespace sockets
}  // namespace net
}  // namespace mymuduo

#endif  // MYMUDUO_NET_SOCKETSOPS_H 