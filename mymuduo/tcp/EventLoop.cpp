#include "EventLoop.h"
#include "ThreadPool.h"
#include "util.h"
#include <vector>
#include <functional>
#include <iostream>
#include <sys/eventfd.h>
#include <assert.h>

EventLoop::EventLoop() : quit(false), callingfunctor(false)
{
    ep = std::make_unique<Epoll>(); // 使用智能指针管理Epoll实例

    // 新增异步处理机制
    errif((wakeup_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) == -1, "eventfd create error"); // 创建eventfd用于异步唤醒

    wakeup_channel = std::make_unique<Channel>(this, wakeup_fd);
    wakeup_channel->setReadCallback(std::bind(&EventLoop::handleWakeup, this));
    wakeup_channel->enableReading(true);

    timer_queue = std::make_unique<TimerQueue>(this);
}

EventLoop::~EventLoop()
{
    removeChannel(wakeup_channel.get()); // 从epoll中删除wakeup_channel
    close(wakeup_fd);                    // 关闭wakeup_fd
    // delete ep;
}

void EventLoop::loop()
{
    tid = CurrentThread::tid(); // 获取当前线程ID
    while (!quit)
    {
        std::vector<Channel *> ActiveEvents = ep->poll();
        for (Channel *ch : ActiveEvents)
        {
            ch->handleEvent();
        }
        doToDoList(); // 执行待处理的任务列表
    }
}

void EventLoop::updateChannel(Channel *ch)
{
    ep->updateChannel(ch); // 更新Channel到epoll中
}

void EventLoop::removeChannel(Channel *ch)
{
    ep->removeChannel(ch); // 从epoll中删除Channel
}

// 线程安全的任务调度接口
void EventLoop::runOneFunc(const std::function<void()> fn) // 执行任务
{
    if (isInLoopThread())
    {
        fn(); // 如果是当前线程，直接执行任务
    }
    else
    {
        queueOneFunc(fn); // 否则将任务添加到队列
    }
}

void EventLoop::queueOneFunc(const std::function<void()> fn) // 将任务添加到队列
{
    {
        std::lock_guard<std::mutex> lock(tasks_mtx); // 锁定任务队列
        tasks.push_back(std::move(fn));              // 将任务添加到队列
    }
    // 如果调用当前函数的并不是当前当前EventLoop对应的的线程，将其唤醒。主要用于关闭TcpConnection
    // 由于关闭连接是由对应`TcpConnection`所发起的，但是关闭连接的操作应该由main_reactor所进行
    // 为了释放ConnectionMap的所持有的TcpConnection
    if (!callingfunctor || !isInLoopThread()) // 如果当前正在处理任务或者不是当前线程
    {
        uint64_t one = 1;
        ssize_t write_size = write(wakeup_fd, &one, sizeof(one));
        assert(write_size == sizeof(one));
    }
}

// 线程判断
bool EventLoop::isInLoopThread() const
{
    return tid == CurrentThread::tid();
}

void EventLoop::doToDoList() // 执行待处理的任务列表
{
    callingfunctor = true; // 标记正在处理任务
    std::vector<std::function<void()>> funcs;
    {
        std::lock_guard<std::mutex> lock(tasks_mtx); // 锁定任务队列
        if (tasks.empty())
        {
            callingfunctor = false; // 如果没有任务，标记处理完毕
            return;        // 如果没有任务，直接返回
        }
        funcs.swap(tasks); // 交换任务队列，避免锁的持有时间过长
    }
    for (auto &func : funcs)
    {
        func(); // 执行任务
    }
    callingfunctor = false; // 任务处理完毕
}

void EventLoop::handleWakeup() // 处理唤醒事件
{
    uint64_t one = 1;
    ssize_t read_size = read(wakeup_fd, &one, sizeof(one));
    assert(read_size == sizeof(one)); // 确保读取成功
}

void EventLoop::RunAt(TimeStamp when, const std::function<void()> &cb)
{
    timer_queue->AddTimer(std::move(when), std::move(cb), 0.0); // 在指定时间执行回调
}

void EventLoop::RunAfter(double delay, const std::function<void()> &cb)
{
    timer_queue->AddTimer(TimeStamp::AddTime(TimeStamp::Now(), delay), std::move(cb), 0.0); // 延迟指定时间执行回调
    // std::cout << "curtime: "<<TimeStamp::Now().GetMicroseconds()/kMicrosecond2Second << std::endl;
}

void EventLoop::RunEvery(double interval, const std::function<void()> &cb)
{
    timer_queue->AddTimer(TimeStamp::AddTime(TimeStamp::Now(), interval), std::move(cb), interval); // 间隔指定时间重复执行回调
}