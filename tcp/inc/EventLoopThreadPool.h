#pragma once

#include "Macro.h"
#include <memory>
#include <thread>
#include <vector>

class EventLoop;
class EventLoopThread;
class EventLoopThreadPool
{
private:
    EventLoop *main_reactor_;                               // 主事件循环
    std::vector<std::unique_ptr<EventLoopThread>> threads_; // 线程池中的EventLoopThread
    std::vector<EventLoop *> loops_;                        // 线程池中的EventLoop列表

    int thread_nums_; // 线程数量
    int next_;        // 下一个分配的EventLoop索引

public:
    DISALLOW_COPY_AND_MOVE(EventLoopThreadPool);
    EventLoopThreadPool(EventLoop *loop);
    ~EventLoopThreadPool();

    void SetThreadNums(int thread_nums);

    void start();

    // 获取线程池中的EventLoop
    EventLoop *nextloop();
};