#include "TimerQueue.h"
#include "util.h"
#include <sys/socket.h>
#include <cstring>
#include <cassert>

TimerQueue::TimerQueue(EventLoop *loop)
    : loop_(loop), timerfd_(-1)
{
    CreateTimerfd();
    channel_ = std::make_unique<Channel>(loop_, timerfd_);
    channel_->setReadCallback(std::bind(&TimerQueue::HandleRead, this));
    channel_->enableReading(true); // 启用读事件,ET模式
}

TimerQueue::~TimerQueue()
{
    // 资源清理逻辑
    close(timerfd_);                      // 关闭timerfd文件描述符
    loop_->removeChannel(channel_.get()); // 从EventLoop中移除channel
    for (const auto &entry : timers_)
    {
        delete entry.second; // 删除定时器对象
    }
}

void TimerQueue::CreateTimerfd()
{
    errif((timerfd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC)) < 0, "timerfd_create error");
}

void TimerQueue::ReadTimerFd()
{
    uint64_t read_byte;
    ssize_t readn = read(timerfd_, &read_byte, sizeof(read_byte));
    if (readn != sizeof(read_byte))
    {
        printf("readn != sizeof(read_byte)");
    }
}

void TimerQueue::HandleRead()
{
    // std::cout << "TimerQueue::HandleRead() called." << std::endl;
    ReadTimerFd();
    active_timers_.clear();

    auto end = timers_.lower_bound(Entry(TimeStamp::Now(), reinterpret_cast<Timer *>(UINTPTR_MAX)));
    active_timers_.insert(active_timers_.end(), timers_.begin(), end);

    timers_.erase(timers_.begin(), end);
    for (auto &entry : active_timers_)
    {
        entry.second->run(); // 执行定时器回调函数
    }
    ResetTimers(); // 重置定时器
}

void TimerQueue::ResetTimerFd(Timer *timer)
{
    struct itimerspec new_, old_;
    memset(&new_, 0, sizeof(new_));
    memset(&old_, 0, sizeof(old_));

    int64_t micro_seconds_dif = timer->GetTime().GetMicroseconds() - TimeStamp::Now().GetMicroseconds();
    // std::cout<<"micro_seconds_dif: "<<micro_seconds_dif<<std::endl;
    if (micro_seconds_dif < 100)
    {
        micro_seconds_dif = 100; // 最小间隔为100微秒
    }

    new_.it_value.tv_sec = static_cast<time_t>(micro_seconds_dif / kMicrosecond2Second);
    new_.it_value.tv_nsec = static_cast<long>(micro_seconds_dif % kMicrosecond2Second * 1000); // 转换为纳秒

    errif(timerfd_settime(timerfd_, 0, &new_, &old_) == -1, "timerfd_settime error");
}

void TimerQueue::ResetTimers()
{
    for (const auto &entry : active_timers_)
    {
        if (entry.second->IsRepeat())
        {
            entry.second->ReStart(TimeStamp::Now()); // 重启定时器
            Insert(entry.second);                    // 重新插入到定时器队列
        }
        else
        {
            delete entry.second; // 删除非重复定时器
        }
    }

    if (!timers_.empty())
    {
        ResetTimerFd(timers_.begin()->second); // 重置timerfd超时时间
    }
}

void TimerQueue::AddTimer(TimeStamp timestamp, std::function<void()> const &cb, double interval)
{
    Timer *timer = new Timer(timestamp, cb, interval);
    if (Insert(timer))
    {
        ResetTimerFd(timer);
    }
}

bool TimerQueue::Insert(Timer *timer)
{
    bool reset_instantly = false;
    if (timers_.empty() || timer->GetTime() < timers_.begin()->first)
    {
        reset_instantly = true; // 如果是最早的定时器，立即重置timerfd
    }
    timers_.emplace(std::move(Entry(timer->GetTime(), timer)));
    return reset_instantly;
}