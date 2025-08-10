#pragma once

#include <arpa/inet.h>
#include "Macro.h"

class InetAddress
{
private:
    
public:
    struct sockaddr_in addr;
    socklen_t addr_len;
    
    DISALLOW_COPY_AND_MOVE(InetAddress);
    InetAddress();
    InetAddress(const char *ip, uint16_t port);
    ~InetAddress();
};