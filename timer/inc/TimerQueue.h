#pragma once
#include <unistd.h>
#include <sys/timerfd.h>

#include <set>
#include <vector>
#include <memory>
#include <utility>
#include <functional>
#include "Macro.h"
#include "TimeStamp.h"
#include "Channel.h"
#include "Timer.h"

class Timer;
class EventLoop;
class Channel;
class TimeStamp;
class TimerQueue
{
public:
    DISALLOW_COPY_AND_MOVE(TimerQueue);
    TimerQueue(EventLoop *loop);
    ~TimerQueue();

    void CreateTimerfd(); // 创建timerfd

    void ReadTimerFd(); // 读取timerfd事件
    void HandleRead();  // timerfd可读时，调用

    void ResetTimerFd(Timer *timer); // 重新设置timerfd超时时间，关注新的定时任务
    void ResetTimers();              // 将具有重复属性的定时器重新加入队列

    bool Insert(Timer *timer);                                                            // 将定时任务加入队列
    void AddTimer(TimeStamp timestamp, std::function<void()> const &cb, double interval); // 添加一个定时任务

private:
    typedef std::pair<TimeStamp, Timer *> Entry;

    EventLoop *loop_;
    int timerfd_;                      // 定时器文件描述符
    std::unique_ptr<Channel> channel_; // 定时器通道，独属资源
    std::set<Entry> timers_;           // 定时器集合
    // 注意：std::set<Entry> timers_ 使用了 std::pair<TimeStamp, Timer*> 作为元素类型
    // 其中 TimeStamp 是时间戳，Timer* 是指向 Timer 对象的指针
    // 这允许我们在定时器队列中存储时间戳和对应的定时器对象
    std::vector<Entry> active_timers_; // 激活的定时器
};