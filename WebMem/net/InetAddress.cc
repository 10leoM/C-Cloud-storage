#include "InetAddress.h"

#include "base/Logging.h"
#include "Endian.h"

#include <netdb.h>
#include <strings.h>  // bzero
#include <arpa/inet.h>

namespace mymuduo
{
namespace net
{
namespace sockets
{

// const struct sockaddr* sockaddr_cast(const struct sockaddr_in6* addr)
// {
//     return static_cast<const struct sockaddr*>(static_cast<const void*>(addr));
// }

// // const struct sockaddr* sockaddr_cast(const struct sockaddr_in* addr)
// // {
// //     return static_cast<const struct sockaddr*>(static_cast<const void*>(addr));
// // }

// const struct sockaddr_in* sockaddr_in_cast(const struct sockaddr_in6* addr)
// {
//     return static_cast<const struct sockaddr_in*>(static_cast<const void*>(addr));
// }

// const struct sockaddr_in6* sockaddr_in6_cast(const struct sockaddr* addr)
// {
//     return static_cast<const struct sockaddr_in6*>(static_cast<const void*>(addr));
// }

} // namespace sockets

static const in_addr_t kInaddrAny = INADDR_ANY;
static const in_addr_t kInaddrLoopback = INADDR_LOOPBACK;

/**
 * @brief 检查IP地址字符串是否为IPv6地址
 */
static bool isIpV6Address(const char* ip)
{
    if (::strchr(ip, ':') != nullptr)
    {
        return true;
    }
    return false;
}

InetAddress::InetAddress(uint16_t port, bool loopbackOnly)
{
    ::bzero(&addr_, sizeof addr_);
    addr_.sin_family = AF_INET;
    in_addr_t ip = loopbackOnly ? kInaddrLoopback : kInaddrAny;
    addr_.sin_addr.s_addr = sockets::hostToNetwork32(ip);
    addr_.sin_port = sockets::hostToNetwork16(port);
}

InetAddress::InetAddress(StringArg ip, uint16_t port)
{
    if (isIpV6Address(ip.c_str()))
    {
        ::bzero(&addr6_, sizeof addr6_);
        addr6_.sin6_family = AF_INET6;
        addr6_.sin6_port = sockets::hostToNetwork16(port);
        if (::inet_pton(AF_INET6, ip.c_str(), &addr6_.sin6_addr) <= 0)
        {
            LOG_ERROR << "InetAddress::InetAddress invalid IPv6 addr";
        }
    }
    else
    {
        ::bzero(&addr_, sizeof addr_);
        addr_.sin_family = AF_INET;
        addr_.sin_port = sockets::hostToNetwork16(port);
        if (::inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr) <= 0)
        {
            LOG_ERROR << "InetAddress::InetAddress invalid IPv4 addr";
        }
    }
}

InetAddress::InetAddress(StringArg ip, uint16_t port, bool ipv6)
{
    if (ipv6)
    {
        ::bzero(&addr6_, sizeof addr6_);
        addr6_.sin6_family = AF_INET6;
        addr6_.sin6_port = sockets::hostToNetwork16(port);
        if (::inet_pton(AF_INET6, ip.c_str(), &addr6_.sin6_addr) <= 0)
        {
            LOG_ERROR << "InetAddress::InetAddress invalid IPv6 addr";
        }
    }
    else
    {
        ::bzero(&addr_, sizeof addr_);
        addr_.sin_family = AF_INET;
        addr_.sin_port = sockets::hostToNetwork16(port);
        if (::inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr) <= 0)
        {
            LOG_ERROR << "InetAddress::InetAddress invalid IPv4 addr";
        }
    }
}

std::string InetAddress::toIp() const
{
    char buf[64] = "";
    if (family() == AF_INET)
    {
        ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
    }
    else if (family() == AF_INET6)
    {
        ::inet_ntop(AF_INET6, &addr6_.sin6_addr, buf, sizeof buf);
    }
    return buf;
}

std::string InetAddress::toIpPort() const
{
    char buf[64] = "";
    if (family() == AF_INET)
    {
        ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
        size_t end = ::strlen(buf);
        uint16_t port = sockets::networkToHost16(addr_.sin_port);
        snprintf(buf + end, sizeof buf - end, ":%u", port);
    }
    else if (family() == AF_INET6)
    {
        buf[0] = '[';
        ::inet_ntop(AF_INET6, &addr6_.sin6_addr, buf + 1, sizeof buf - 1);
        size_t end = ::strlen(buf);
        uint16_t port = sockets::networkToHost16(addr6_.sin6_port);
        snprintf(buf + end, sizeof buf - end, "]:%u", port);
    }
    return buf;
}

uint16_t InetAddress::port() const
{
    return sockets::networkToHost16(portNetEndian());
}

uint32_t InetAddress::ipv4NetEndian() const
{
    assert(family() == AF_INET);
    return addr_.sin_addr.s_addr;
}

bool InetAddress::resolve(StringArg hostname, InetAddress* result)
{
    assert(result != nullptr);
    struct hostent* ent = nullptr;
    struct hostent hent;
    char buf[8192];
    int herrno = 0;
    ::bzero(&hent, sizeof hent);
    int ret = gethostbyname_r(hostname.c_str(), &hent, buf, sizeof buf, &ent, &herrno);
    if (ret == 0 && ent != nullptr)
    {
        assert(ent->h_addrtype == AF_INET);
        result->addr_.sin_addr = *reinterpret_cast<struct in_addr*>(ent->h_addr);
        return true;
    }
    else
    {
        if (ret)
        {
            LOG_ERROR << "InetAddress::resolve";
        }
        return false;
    }
}
}
} // namespace mymuduo 