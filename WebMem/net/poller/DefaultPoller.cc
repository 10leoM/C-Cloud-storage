#include "../Poller.h"
#include "PollPoller.h"
#include "EPollPoller.h"

#include <stdlib.h>

namespace mymuduo {
namespace net {

Poller* Poller::newDefaultPoller(EventLoop* loop) {
    if (::getenv("MYMUDUO_USE_POLL")) {
        return new PollPoller(loop);
    } else {
        return new EPollPoller(loop); // 默认使用epoll
    }
}

}  // namespace net
}  // namespace mymuduo 