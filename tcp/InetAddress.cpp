#include "InetAddress.h"
#include <cstring>
#include "util.h"

InetAddress::InetAddress()
{
    bzero(&addr, sizeof(addr));
    addr_len = sizeof(addr);
}

InetAddress::InetAddress(const char *ip, uint16_t port)
{
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    errif(inet_pton(AF_INET, ip, &addr.sin_addr) == -1, "inet_pton error");
    addr_len = sizeof(addr);
}

InetAddress::InetAddress(const InetAddress &other)
{
    addr = other.addr;
    addr_len = other.addr_len;
}

InetAddress &InetAddress::operator=(const InetAddress &other)
{
    if (this != &other) {
        addr = other.addr;
        addr_len = other.addr_len;
    }
    return *this;
}

InetAddress::~InetAddress()
{

}