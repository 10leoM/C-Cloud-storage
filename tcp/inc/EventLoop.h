#pragma once

#include "Epoll.h"
#include "Channel.h"
#include "ThreadPool.h"
#include "Macro.h"
#include "CurrentThread.h"
#include "TimerQueue.h"
#include "TimeStamp.h"
#include "Timer.h"
#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

class Channel; // 前向声明
class Epoll;   // 前向声明
class TimerQueue;
class TimeStamp;
class Timer;
class EventLoop
{
private:
    std::unique_ptr<Epoll> ep; // epoll实例,智能指针管理
    std::atomic<bool> quit;    // 是否退出循环
    pid_t tid;                 // 记录当前线程ID

    // 任务队列机制
    std::vector<std::function<void()>> tasks; // 任务队列
    std::mutex tasks_mtx;                     // 任务队列的互斥锁

    // eventfd异步唤醒机制
    int wakeup_fd;                           // 用于异步唤醒的文件描述符
    std::unique_ptr<Channel> wakeup_channel; // 唤醒通道
    std::atomic<bool> callingfunctor;        // 是否正在处理任务

    // 定时器队列
    std::unique_ptr<TimerQueue> timer_queue;

public:
    DISALLOW_COPY_AND_MOVE(EventLoop);
    EventLoop();
    ~EventLoop();
    void loop();                     // 事件循环
    void updateChannel(Channel *ch); // 更新Channel到epoll中
    void removeChannel(Channel *ch); // 从epoll中删除Channel

    // 线程安全的任务调度接口
    void runOneFunc(const std::function<void()> fn);   // 执行任务
    void queueOneFunc(const std::function<void()> fn); // 将任务添加到队列

    // 线程判断
    bool isInLoopThread() const;
    void doToDoList();   // 执行待处理的任务列表
    void handleWakeup(); // 处理唤醒事件

    // 定时器的回调函数
    void RunAt(TimeStamp when, const std::function<void()> &cb);     // 在指定时间执行回调
    void RunAfter(double delay, const std::function<void()> &cb);    // 延迟指定时间执行回调
    void RunEvery(double interval, const std::function<void()> &cb); // 间隔指定时间重复执行回调
};