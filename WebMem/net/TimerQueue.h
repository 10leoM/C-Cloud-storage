#ifndef MYMUDUO_NET_TIMERQUEUE_H
#define MYMUDUO_NET_TIMERQUEUE_H

#include <set>
#include <vector>

#include "base/Mutex.h"
#include "base/Timestamp.h"
#include "Callbacks.h"
#include "Channel.h"

namespace mymuduo {
namespace net {

class EventLoop;
class Timer;
class TimerId;

///
/// A best efforts timer queue.
/// No guarantee that the callback will be on time.
///
class TimerQueue : noncopyable {
public:
    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();

    ///
    /// Schedules the callback to be run at given time,
    /// repeats if @c interval > 0.0.
    ///
    /// Must be thread safe. Usually be called from other threads.
    TimerId addTimer(TimerCallback cb,
                    Timestamp when,
                    double interval);

    void cancel(TimerId timerId);

private:
    // 定时器条目，包含到期时间和定时器指针
    using Entry = std::pair<Timestamp, Timer*>;
    // 定时器集合，按到期时间排序
    using TimerList = std::set<Entry>;
    // 激活的定时器集合
    using ActiveTimer = std::pair<Timer*, int64_t>;
    using ActiveTimerSet = std::set<ActiveTimer>;

    void addTimerInLoop(Timer* timer);
    void cancelInLoop(TimerId timerId);

    // called when timerfd alarms
    void handleRead();

    // move out all expired timers
    std::vector<Entry> getExpired(Timestamp now);
    void reset(const std::vector<Entry>& expired, Timestamp now);

    bool insert(Timer* timer);

    EventLoop* loop_;                  // 所属的事件循环
    const int timerfd_;               // 定时器文件描述符
    Channel timerfdChannel_;          // 定时器通道
    TimerList timers_;                // 按到期时间排序的定时器集合

    // for cancel()
    ActiveTimerSet activeTimers_;     // 按对象地址排序的定时器集合
    bool callingExpiredTimers_;       // 是否正在处理到期定时器
    ActiveTimerSet cancelingTimers_;  // 保存被取消的定时器
};

}  // namespace net
}  // namespace mymuduo

#endif  // MYMUDUO_NET_TIMERQUEUE_H 