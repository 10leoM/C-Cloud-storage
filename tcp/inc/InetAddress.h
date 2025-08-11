#pragma once

#include <arpa/inet.h>
#include "Macro.h"

class InetAddress {
public:
    struct sockaddr_in addr{};
    socklen_t addr_len{};

    InetAddress();
    InetAddress(const char *ip, uint16_t port);
    InetAddress(const InetAddress &other);            // 拷贝构造
    InetAddress &operator=(const InetAddress &other); // 拷贝赋值
    ~InetAddress();
};