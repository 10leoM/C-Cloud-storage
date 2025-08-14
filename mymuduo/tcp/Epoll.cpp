#include "Epoll.h"
#include <unistd.h>
#include <cstring>
#include <errno.h>
#include "util.h"

#define MAXEVENT 1024
Epoll::Epoll()
{
    errif((epfd = epoll_create1(0)) == -1, "epoll_create error");
    events = new epoll_event[MAXEVENT];
    bzero(events, sizeof(epoll_event) * MAXEVENT);
}

Epoll::~Epoll()
{
    if(epfd != -1)
    {
        close(epfd);
        delete[] events;
    }
}

void Epoll::addFd(int fd, uint32_t op)
{
    epoll_event ev;
    ev.data.fd = fd;
    ev.events = op;
    errif((epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev)) == -1, "epoll_ctl error");
}

// std::vector<epoll_event> Epoll::poll(int timeout){
//     std::vector<epoll_event> activeEvents;
//     int nfds = epoll_wait(epfd, events, MAX_EVENTS, timeout);
//     errif(nfds == -1, "epoll wait error");
//     for(int i = 0; i < nfds; ++i){
//         activeEvents.push_back(events[i]);
//     }
//     return activeEvents;
// }

std::vector<Channel*> Epoll::poll(int timeout)
{
    std::vector<Channel *> activeEvents;
    int nfds;
    
    // 处理信号中断的情况，重新调用 epoll_wait
    while (true) {
        nfds = epoll_wait(epfd, events, MAXEVENT, timeout);
        if (nfds == -1) {
            if (errno == EINTR) {
                // 被信号中断，继续等待
                continue;
            } else {
                // 真正的错误
                errif(true, "epoll wait error");
            }
        }
        break; // 成功获取事件或超时，跳出循环
    }
    
    for(int i = 0; i < nfds; ++i)
    {
        Channel *ch = (Channel*)events[i].data.ptr;                 // 使用epoll_event.data.ptr指针存储 Channel 对象
        ch->setRevents(events[i].events);
        activeEvents.push_back(ch);
    }
    return activeEvents;
}

void Epoll::updateChannel(Channel *channel)
{
   int fd = channel->getFd();
    struct epoll_event ev;
    bzero(&ev, sizeof(ev));
    ev.data.ptr = channel;
    ev.events = channel->getEvents();
    if(!channel->getInEpoll()){
        errif(epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1, "epoll add error");
        channel->setInEpoll();
        // debug("Epoll: add Channel to epoll tree success, the Channel's fd is: ", fd);
    } else{
        errif(epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) == -1, "epoll modify error");
        // debug("Epoll: modify Channel in epoll tree success, the Channel's fd is: ", fd);
    }
}

void Epoll::removeChannel(Channel *channel)
{
    int fd = channel->getFd();
    if(channel->getInEpoll())
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "Epoll: remove error, the Channel's fd is: %d", fd);
        errif(epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr) == -1, buf);
        // debug("Epoll: remove Channel from epoll tree success, the Channel's fd is: ", fd);
    } 
    else 
    {
        // debug("Epoll: Channel is not in epoll tree, no need to remove, the Channel's fd is: ", fd);
    }
}