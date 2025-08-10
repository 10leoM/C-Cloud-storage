#ifndef MYMUDUO_NET_POLLER_H
#define MYMUDUO_NET_POLLER_H

#include <vector>
#include <map>
#include "base/Timestamp.h"

namespace mymuduo {
namespace net {

class Channel;
class EventLoop;

/// @brief IO Multiplexing的基类
/// 这个类不拥有Channel对象
class Poller {
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop* loop);
    virtual ~Poller();

    /// @brief 轮询IO事件
    /// @param timeoutMs 超时时间
    /// @param activeChannels 活动的Channel列表
    /// @return 轮询时间
    virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;

    /// @brief 更新Channel
    virtual void updateChannel(Channel* channel) = 0;

    /// @brief 移除Channel
    virtual void removeChannel(Channel* channel) = 0;

    /// @brief 是否有Channel
    virtual bool hasChannel(Channel* channel) const;

    /// @brief 获取默认的Poller
    static Poller* newDefaultPoller(EventLoop* loop);

    void assertInLoopThread() const;

protected:
    using ChannelMap = std::map<int, Channel*>;
    ChannelMap channels_;  // fd到Channel的映射

private:
    EventLoop* ownerLoop_;
};

}  // namespace net
}  // namespace mymuduo

#endif  // MYMUDUO_NET_POLLER_H 