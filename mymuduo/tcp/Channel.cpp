#include "Channel.h"
#include "Epoll.h"
#include "EventLoop.h"
#include "Connection.h"
#include <assert.h>

Channel::Channel(EventLoop *_ev, int _fd)
    : ev(_ev), fd(_fd), events(0), revents(0), inEpoll(false) {}

Channel::~Channel() {}

void Channel::enableReading(bool ET) // 使fd可读
{
    if (ET)
        events |= EPOLLET | EPOLLIN;
    else
        events |= EPOLLIN; // 设置可读事件
    ev->updateChannel(this);
    inEpoll = true;
}

void Channel::enableWriting(bool ET) // 使fd可写
{
    if (ET)
        events |= EPOLLET | EPOLLOUT;
    else
        events |= EPOLLOUT;
    ev->updateChannel(this);
    inEpoll = true;
}

void Channel::disableWriting() // 使fd不可写
{
    events &= ~EPOLLOUT;
    ev->updateChannel(this);
}

void Channel::disableReading() // 使fd不可读
{
    events &= ~EPOLLIN;
    ev->updateChannel(this);
}

void Channel::disableAll() // 取消所有事件
{
    events = 0;
    ev->updateChannel(this);
}

bool Channel::isWriting()
{
    return events & EPOLLOUT;
}

int Channel::getFd()
{
    return fd;
}

void Channel::setInEpoll()
{
    if (inEpoll)
        return; // 如果已经在 epoll 中，则不需要重复设置
    // ev->updateChannel(this);    // 添加到 epoll 中,不需要update，update中有调用setInEpoll
    inEpoll = true; // 设置在 epoll 中
}

uint32_t Channel::getEvents()
{
    return events;
}

uint32_t Channel::getRevents()
{
    return revents;
}

bool Channel::getInEpoll()
{
    return inEpoll;
}

void Channel::setEvents(uint32_t events) // 设置监听的事件
{
    this->events = events;
}

void Channel::setRevents(uint32_t revents) // 设置实际发生的事件
{
    this->revents = revents;
}

void Channel::setReadCallback(std::function<void()> const &fn)
{
    // 设置回调函数
    this->readCallback = fn;
}

void Channel::setWriteCallback(std::function<void()> const &fn)
{
    // 设置回调函数
    this->writeCallback = fn;
}

void Channel::setCloseCallback(std::function<void()> const &fn)
{
    this->closeCallback = fn;
}

void Channel::setErrorCallback(std::function<void()> const &fn)
{
    this->errorCallback = fn;
}

void Channel::handleEvent() // 目前只处理读事件
{
    // 处理事件，调用回调函数, one loop per thread，所以不用再add了
    // ev->add(callback);·
    // if(this->getFd() == 4) // 如果是特定的fd，直接调用回调函数
    assert(readCallback != nullptr);
    if (tied)
    {
        std::shared_ptr<void> guard = tie.lock(); // 尝试获取绑定的对象
        // 如果绑定的对象存在，说明生命周期受保护
        HandleEventWithGuard(); // 调用实际的事件处理逻辑
    }
    else
    {
        HandleEventWithGuard(); // 如果没有绑定对象，直接处理事件
    }
    // else // 否则加入线程池执行
    //     ev->add(callback); // 将回调函数添加到线程池中执行
}

void Channel::Tie(const std::shared_ptr<void> &obj)
{
    tie = obj; // 绑定Connection对象
    tied = true;
}

void Channel::HandleEventWithGuard()
{
    // 关闭事件 (挂起且无可读)
    if ((revents & EPOLLHUP) && !(revents & EPOLLIN))
    {
        if (closeCallback)
            closeCallback();
    }
    // 错误事件
    if (revents & EPOLLERR)
    {
        if (errorCallback)
            errorCallback();
    }
    // 可读事件 (含优先/对端关闭半连接)
    if (revents & (EPOLLIN | EPOLLPRI | EPOLLRDHUP))
    {
        if (readCallback)
            readCallback();
    }
    // 可写事件
    if (revents & EPOLLOUT)
    {
        if (writeCallback)
            writeCallback();
    }
}