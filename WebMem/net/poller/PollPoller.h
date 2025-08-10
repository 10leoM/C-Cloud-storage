#ifndef MYMUDUO_NET_POLLPOLLER_H
#define MYMUDUO_NET_POLLPOLLER_H

#include "../Poller.h"
#include <vector>

struct pollfd;

namespace mymuduo {
namespace net {

/// @brief poll(2)的封装
class PollPoller : public Poller {
public:
    PollPoller(EventLoop* loop);
    ~PollPoller() override = default;

    Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;

private:
    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;

    using PollFdList = std::vector<struct pollfd>;
    PollFdList pollfds_;
};

}  // namespace net
}  // namespace mymuduo

#endif  // MYMUDUO_NET_POLLPOLLER_H 