#ifndef MYMUDUO_NET_TIMERID_H
#define MYMUDUO_NET_TIMERID_H

#include "base/copyable.h"

namespace mymuduo {
namespace net {

class Timer;

///
/// An opaque identifier, for canceling Timer.
///
class TimerId : public mymuduo::copyable {
public:
    TimerId()
        : timer_(nullptr),
          sequence_(0) {
    }

    TimerId(Timer* timer, int64_t seq)
        : timer_(timer),
          sequence_(seq) {
    }

    // default copy-ctor, dtor and assignment are okay

    friend class TimerQueue;

private:
    Timer* timer_;        // 定时器指针
    int64_t sequence_;    // 定时器序号
};

}  // namespace net
}  // namespace mymuduo

#endif  // MYMUDUO_NET_TIMERID_H 