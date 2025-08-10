#pragma once
#include <sys/epoll.h>
#include <functional>
#include "Macro.h"
#include "Connection.h"
#include <memory>

class Epoll;
class EventLoop;
class Connection;
class Channel
{
private:
    EventLoop *ev; // 事件循环，裸指针管理
    int fd;
    uint32_t events;                     // 监听的事件
    uint32_t revents;                    // 实际发生的事件
    bool inEpoll;                        // 是否在 epoll 中
    std::function<void()> readCallback;  // 回调函数
    std::function<void()> writeCallback; // 回调函数

    bool tied;                           // 是否绑定了对象
    std::weak_ptr<void> tie;             // 绑定的对象，用于生命周期保护

    void HandleEventWithGuard();         // 实际的事件处理逻辑

public:
    DISALLOW_COPY_AND_MOVE(Channel);
    Channel(EventLoop *_ev, int _fd);
    ~Channel();

    void enableReading(bool ET = false); // 使fd可读并更新
    void enableWriting(bool ET = false); // 使fd可写并更新
    void disableWriting(); // 使fd不可写并更新
    bool isWriting(); // 判断是否在监听写事件
    void enableET();                     // 设置为ET模式

    int getFd();
    uint32_t getEvents();  // 获取fd的事件
    uint32_t getRevents(); // 获取fd的事件
    bool getInEpoll();     // 获取fd是否在epoll中
    void setInEpoll();     // 设置fd是否在epoll中

    void setEvents(uint32_t);                     // 设置fd的事件
    void setRevents(uint32_t);                    // 设置fd的事件
    void setReadCallback(std::function<void()> const &fn);  // 设置回调函数
    void setWriteCallback(std::function<void()> const &fn); // 设置回调函数

    void handleEvent();                           // 处理事件，调用回调函数
    void Tie(const std::shared_ptr<void> &obj);          // 绑定对象，用于生命周期保护

};
