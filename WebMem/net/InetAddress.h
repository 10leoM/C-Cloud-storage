#ifndef MYMUDUO_NET_INETADDRESS_H
#define MYMUDUO_NET_INETADDRESS_H

#include "base/copyable.h"
#include "base/StringPiece.h"

#include <netinet/in.h>
#include <string>

namespace mymuduo {
namespace net {

namespace sockets {
const struct sockaddr* sockaddr_cast(const struct sockaddr_in6* addr);
}

// 用于字符串参数的包装类
typedef const std::string& StringArg;

///
/// Wrapper of sockaddr_in.
///
/// This is an POD interface class.
class InetAddress : public copyable {
public:
    /// Constructs an endpoint with given port number.
    /// Mostly used in TcpServer listening.
    explicit InetAddress(uint16_t port = 0, bool loopbackOnly = false);

    /// Constructs an endpoint with given ip and port.
    /// @c ip should be "1.2.3.4"
    InetAddress(StringArg ip, uint16_t port);
    InetAddress(StringArg ip, uint16_t port, bool ipv6);
    /// Constructs an endpoint with given struct @c sockaddr_in
    /// Mostly used when accepting new connections
    explicit InetAddress(const struct sockaddr_in& addr)
        : addr_(addr)
    { }

    InetAddress(const struct sockaddr_in6& addr6)
        : addr6_(addr6)
    { }

    sa_family_t family() const { return addr_.sin_family; }
    std::string toIp() const;
    std::string toIpPort() const;
    uint16_t port() const;

    // default copy/assignment are Okay

    const struct sockaddr* getSockAddr() const { return reinterpret_cast<const struct sockaddr*>(&addr_); }
    void setSockAddr(const struct sockaddr_in& addr) { addr_ = addr; }
    const struct sockaddr* getSockAddrInet() const { return sockets::sockaddr_cast(&addr6_); }

    const struct sockaddr* getSockAddrInet6() const { return reinterpret_cast<const struct sockaddr*>(&addr6_); }
    void setSockAddrInet6(const struct sockaddr_in6& addr6) { addr6_ = addr6; }
    uint32_t ipv4NetEndian() const;
    uint16_t portNetEndian() const { return addr_.sin_port; }

    // resolve hostname to IP address, not changing port or sin_family
    // return true on success.
    // thread safe
    static bool resolve(StringArg hostname, InetAddress* result);

private:
    union
  {
    struct sockaddr_in addr_;
    struct sockaddr_in6 addr6_;
  };
};

} // namespace net
} // namespace mymuduo

#endif  // MYMUDUO_NET_INETADDRESS_H 