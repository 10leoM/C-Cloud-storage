#ifndef MYMUDUO_NET_TIMER_H
#define MYMUDUO_NET_TIMER_H

#include <atomic>
#include <functional>
#include "base/Timestamp.h"
#include "base/noncopyable.h"

namespace mymuduo {
namespace net {

///
/// Internal class for timer management.
/// 
class Timer : noncopyable {
public:
    using TimerCallback = std::function<void()>;

    Timer(TimerCallback cb, Timestamp when, double interval)
        : callback_(std::move(cb)),
          expiration_(when),
          interval_(interval),
          repeat_(interval > 0.0),
          sequence_(s_numCreated_.fetch_add(1)) {
    }

    void run() const { callback_(); }

    Timestamp expiration() const  { return expiration_; }
    bool repeat() const { return repeat_; }
    int64_t sequence() const { return sequence_; }

    void restart(Timestamp now);

    static int64_t numCreated() { return s_numCreated_.load(); }

private:
    const TimerCallback callback_;    // 定时器回调函数
    Timestamp expiration_;            // 下一次的超时时刻
    const double interval_;           // 超时时间间隔，如果是一次性定时器，该值为0
    const bool repeat_;               // 是否重复
    const int64_t sequence_;         // 定时器序号

    static std::atomic<int64_t> s_numCreated_; // 定时器计数，用于生成sequence
};

}  // namespace net
}  // namespace mymuduo

#endif  // MYMUDUO_NET_TIMER_H 