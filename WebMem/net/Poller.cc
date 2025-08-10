#include "Poller.h"
#include "Channel.h"
#include "EventLoop.h"
#include <assert.h>

using namespace mymuduo;
using namespace mymuduo::net;

Poller::Poller(EventLoop* loop)
    : ownerLoop_(loop) {
}

Poller::~Poller() = default;

bool Poller::hasChannel(Channel* channel) const {
    assertInLoopThread();
    auto it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}

void Poller::assertInLoopThread() const {
    ownerLoop_->assertInLoopThread();
} 