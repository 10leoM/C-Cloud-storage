#include "Timer.h"

namespace mymuduo {
namespace net {

std::atomic<int64_t> Timer::s_numCreated_;

void Timer::restart(Timestamp now) {
    if (repeat_) {
        // 如果是重复定时器，重新计算下一次超时时刻
        expiration_ = addTime(now, interval_);
    } else {
        // 一次性定时器，将超时时刻设置为一个无效值
        expiration_ = Timestamp::invalid();
    }
}

}  // namespace net
}  // namespace mymuduo 