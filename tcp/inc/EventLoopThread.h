#pragma once

#include <mutex>
#include <thread>
#include <condition_variable>
#include "Macro.h"
#include "EventLoop.h"

class EventLoop;
class EventLoopThread
{
private:
    EventLoop *loop_;            // 指向由子线程创建的EventLoop，用于主线程和子线程的数据传递
    std::thread thread_;         // 子线程
    std::mutex mutex_;           // 互斥锁
    std::condition_variable cv_; // 条件变量，用于通知主线程

    // 线程运行的函数
    void ThreadFunc();

public:
    DISALLOW_COPY_AND_MOVE(EventLoopThread);
    EventLoopThread();
    ~EventLoopThread();

    // 启动线程， 使EventLoop成为IO线程
    EventLoop *StartLoop();
};