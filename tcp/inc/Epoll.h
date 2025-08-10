#pragma once
#include <sys/epoll.h>
#include <vector>
#include <cstdint>
#include "Channel.h"
#include "Macro.h"

class Channel;
class Epoll
{
private:
    int epfd;
    struct epoll_event *events;
public:
    DISALLOW_COPY_AND_MOVE(Epoll);
    Epoll();
    ~Epoll();

    void addFd(int fd, uint32_t op);
    std::vector<Channel*> poll(int timeout = -1);
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
};

